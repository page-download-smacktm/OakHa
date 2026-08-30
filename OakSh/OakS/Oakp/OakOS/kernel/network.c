#include "acorn/network.h"
#include "acorn/e1000.h"
#include "acorn/timer.h"

#define ETHERNET_HEADER 14
#define ARP_PACKET_SIZE 28
#define IPV4_HEADER_SIZE 20
#define UDP_HEADER_SIZE 8
#define FRAME_SIZE 1514
#define DNS_PORT 53
#define DNS_CLIENT_PORT 49152
#define TCP_CLIENT_PORT 49153
#define TCP_CONNECTIONS 8
#define TCP_RETRANSMIT_TICKS 30
#define TCP_MAX_RETRIES 3

#define ETHERNET_ARP 0x0806
#define ETHERNET_IPV4 0x0800
#define ARP_REQUEST 1
#define ARP_REPLY 2

static const unsigned char broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static unsigned int local_address = 0x0A00020F;
static unsigned int gateway_address = 0x0A000202;
static unsigned int dns_address = 0x0A000203;
static unsigned char local_mac[6];
static unsigned char gateway_mac[6];
static unsigned int arp_gateway_ready;
static unsigned int network_ready;
static unsigned char frame[FRAME_SIZE];
static unsigned short dns_id;
static unsigned int dns_answer;
static int dns_answer_ready;
struct tcp_state {
    unsigned int sequence;
    unsigned int acknowledged;
    unsigned int remote_address;
    unsigned short remote_port;
    int synack_ready;
    unsigned char data[1024];
    unsigned int data_length;
    unsigned char pending_data[1024];
    unsigned int pending_sequence;
    unsigned int pending_acknowledged;
    unsigned int pending_length;
    unsigned long pending_tick;
    unsigned long pending_timeout;
    unsigned int pending_retries;
    int pending_fin;
    int closing;
    int remote_closed;
};

static struct tcp_state tcp_connections[TCP_CONNECTIONS];
static struct tcp_state *tcp_active;

static unsigned short read16(const unsigned char *data);
static unsigned int read32(const unsigned char *data);

#define tcp_sequence (tcp_active->sequence)
#define tcp_acknowledged (tcp_active->acknowledged)
#define tcp_remote_address (tcp_active->remote_address)
#define tcp_remote_port (tcp_active->remote_port)
#define tcp_synack_ready (tcp_active->synack_ready)
#define tcp_data (tcp_active->data)
#define tcp_data_length (tcp_active->data_length)

static int tcp_select(int connection)
{
    if (connection < 0 || connection >= TCP_CONNECTIONS) return 0;
    tcp_active = &tcp_connections[connection];
    return 1;
}

static unsigned short tcp_local_port(void)
{
    for (int index = 0; index < TCP_CONNECTIONS; ++index)
        if (&tcp_connections[index] == tcp_active)
            return (unsigned short)(TCP_CLIENT_PORT + index);
    return TCP_CLIENT_PORT;
}

static int tcp_connection_for_packet(const unsigned char *packet,
    unsigned int tcp_offset)
{
    unsigned int remote_address = read32(packet + 26);
    unsigned int remote_port = read16(packet + tcp_offset);
    unsigned int local_port = read16(packet + tcp_offset + 2);
    for (int index = 0; index < TCP_CONNECTIONS; ++index) {
        struct tcp_state *connection = &tcp_connections[index];
        if (connection->remote_address != 0 &&
            connection->remote_address == remote_address &&
            connection->remote_port == remote_port &&
            local_port == (unsigned int)(TCP_CLIENT_PORT + index))
            return index;
    }
    return -1;
}

static unsigned short read16(const unsigned char *data)
{
    return (unsigned short)(((unsigned short)data[0] << 8) | data[1]);
}

static unsigned int read32(const unsigned char *data)
{
    return ((unsigned int)data[0] << 24) | ((unsigned int)data[1] << 16) |
        ((unsigned int)data[2] << 8) | data[3];
}

static void write16(unsigned char *data, unsigned short value)
{
    data[0] = (unsigned char)(value >> 8);
    data[1] = (unsigned char)value;
}

static void write32(unsigned char *data, unsigned int value)
{
    data[0] = (unsigned char)(value >> 24);
    data[1] = (unsigned char)(value >> 16);
    data[2] = (unsigned char)(value >> 8);
    data[3] = (unsigned char)value;
}

static void copy_bytes(unsigned char *destination, const unsigned char *source,
    unsigned int length)
{
    for (unsigned int index = 0; index < length; ++index)
        destination[index] = source[index];
}

static unsigned short checksum(const unsigned char *data, unsigned int length)
{
    unsigned long sum = 0;
    for (unsigned int index = 0; index + 1 < length; index += 2)
        sum += read16(data + index);
    if ((length & 1) != 0) sum += (unsigned int)data[length - 1] << 8;
    while ((sum >> 16) != 0) sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)~sum;
}

static unsigned short transport_checksum(const unsigned char *data,
    unsigned int length, unsigned int destination, unsigned char protocol)
{
    unsigned long sum = (local_address >> 16) + (local_address & 0xFFFF) +
        (destination >> 16) + (destination & 0xFFFF) + protocol + length;
    for (unsigned int index = 0; index + 1 < length; index += 2)
        sum += read16(data + index);
    if ((length & 1) != 0) sum += (unsigned int)data[length - 1] << 8;
    while ((sum >> 16) != 0) sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)~sum;
}

static int send_arp(unsigned int target)
{
    for (unsigned int index = 0; index < 6; ++index) {
        frame[index] = broadcast_mac[index];
        frame[6 + index] = local_mac[index];
    }
    write16(frame + 12, ETHERNET_ARP);
    write16(frame + 14, 1);
    write16(frame + 16, ETHERNET_IPV4);
    frame[18] = 6;
    frame[19] = 4;
    write16(frame + 20, ARP_REQUEST);
    copy_bytes(frame + 22, local_mac, 6);
    write32(frame + 28, local_address);
    for (unsigned int index = 0; index < 6; ++index) frame[32 + index] = 0;
    write32(frame + 38, target);
    return e1000_send(frame, 42);
}

static int send_udp(const unsigned char *destination, unsigned int address,
    unsigned short source_port, unsigned short destination_port,
    const unsigned char *payload, unsigned int payload_length)
{
    unsigned int ip_length = IPV4_HEADER_SIZE + UDP_HEADER_SIZE + payload_length;
    unsigned int length = ETHERNET_HEADER + ip_length;
    if (length > FRAME_SIZE) return 0;
    copy_bytes(frame, destination, 6);
    copy_bytes(frame + 6, local_mac, 6);
    write16(frame + 12, ETHERNET_IPV4);
    frame[14] = 0x45;
    frame[15] = 0;
    write16(frame + 16, (unsigned short)ip_length);
    write16(frame + 18, 0);
    write16(frame + 20, 0x4000);
    frame[22] = 64;
    frame[23] = 17;
    write16(frame + 24, 0);
    write32(frame + 26, local_address);
    write32(frame + 30, address);
    write16(frame + 24, checksum(frame + 14, IPV4_HEADER_SIZE));
    write16(frame + 34, source_port);
    write16(frame + 36, destination_port);
    write16(frame + 38, (unsigned short)(UDP_HEADER_SIZE + payload_length));
    write16(frame + 40, 0);
    copy_bytes(frame + 42, payload, payload_length);
    return e1000_send(frame, length);
}

static int send_tcp(const unsigned char *destination, unsigned int address,
    unsigned short port, unsigned short flags, unsigned int sequence,
    unsigned int acknowledge)
{
    unsigned int tcp_length = 20;
    unsigned int ip_length = IPV4_HEADER_SIZE + tcp_length;
    unsigned int length = ETHERNET_HEADER + ip_length;
    copy_bytes(frame, destination, 6);
    copy_bytes(frame + 6, local_mac, 6);
    write16(frame + 12, ETHERNET_IPV4);
    frame[14] = 0x45;
    frame[15] = 0;
    write16(frame + 16, (unsigned short)ip_length);
    write16(frame + 18, 0);
    write16(frame + 20, 0x4000);
    frame[22] = 64;
    frame[23] = 6;
    write16(frame + 24, 0);
    write32(frame + 26, local_address);
    write32(frame + 30, address);
    write16(frame + 24, checksum(frame + 14, IPV4_HEADER_SIZE));
    write16(frame + 34, tcp_local_port());
    write16(frame + 36, port);
    write32(frame + 38, sequence);
    write32(frame + 42, acknowledge);
    frame[46] = 0x50;
    frame[47] = (unsigned char)flags;
    write16(frame + 48, 4096);
    write16(frame + 50, 0);
    write16(frame + 52, 0);
    write16(frame + 50, transport_checksum(frame + 34, tcp_length,
        address, 6));
    return e1000_send(frame, length);
}

static int send_tcp_data(const unsigned char *destination, unsigned int address,
    unsigned short port, unsigned int sequence, unsigned int acknowledge,
    const unsigned char *data, unsigned int data_length)
{
    unsigned int tcp_length = 20 + data_length;
    unsigned int ip_length = IPV4_HEADER_SIZE + tcp_length;
    unsigned int length = ETHERNET_HEADER + ip_length;
    if (length > FRAME_SIZE) return 0;
    copy_bytes(frame, destination, 6);
    copy_bytes(frame + 6, local_mac, 6);
    write16(frame + 12, ETHERNET_IPV4);
    frame[14] = 0x45;
    frame[15] = 0;
    write16(frame + 16, (unsigned short)ip_length);
    write16(frame + 18, 0);
    write16(frame + 20, 0x4000);
    frame[22] = 64;
    frame[23] = 6;
    write16(frame + 24, 0);
    write32(frame + 26, local_address);
    write32(frame + 30, address);
    write16(frame + 24, checksum(frame + 14, IPV4_HEADER_SIZE));
    write16(frame + 34, tcp_local_port());
    write16(frame + 36, port);
    write32(frame + 38, sequence);
    write32(frame + 42, acknowledge);
    frame[46] = 0x50;
    frame[47] = 0x18;
    write16(frame + 48, 4096);
    write16(frame + 50, 0);
    write16(frame + 52, 0);
    copy_bytes(frame + 54, data, data_length);
    write16(frame + 50, transport_checksum(frame + 34, tcp_length,
        address, 6));
    return e1000_send(frame, length);
}

static void receive_arp(const unsigned char *packet)
{
    if (read16(packet + 12) != ETHERNET_ARP || read16(packet + 20) != ARP_REPLY)
        return;
    if (read32(packet + 28) != gateway_address) return;
    copy_bytes(gateway_mac, packet + 22, 6);
    arp_gateway_ready = 1;
}

static void receive_ipv4(const unsigned char *packet, unsigned int length)
{
    if (length < ETHERNET_HEADER + IPV4_HEADER_SIZE ||
        read16(packet + 12) != ETHERNET_IPV4) return;
    unsigned int header_length = (packet[14] & 0x0F) * 4;
    if (header_length < IPV4_HEADER_SIZE ||
        length < ETHERNET_HEADER + header_length) return;
    if (packet[23] == 6 && length >= ETHERNET_HEADER + header_length + 20) {
        unsigned int tcp_offset = ETHERNET_HEADER + header_length;
        int connection = tcp_connection_for_packet(packet, tcp_offset);
        if (connection >= 0) tcp_select(connection);
        unsigned short flags = read16(packet + tcp_offset + 12) & 0x01FF;
        if (connection >= 0 && tcp_synack_ready &&
            tcp_active->pending_length != 0 &&
            read32(packet + tcp_offset + 8) >=
                tcp_active->pending_sequence + tcp_active->pending_length) {
            tcp_active->pending_length = 0;
            tcp_active->pending_retries = 0;
            if (tcp_active->pending_fin) {
                tcp_active->pending_fin = 0;
                tcp_active->synack_ready = 0;
                tcp_active->remote_address = 0;
                tcp_active->remote_port = 0;
            }
        }
        if (connection >= 0 && read32(packet + 26) == tcp_remote_address &&
            read32(packet + 30) == local_address &&
            read16(packet + tcp_offset) == tcp_remote_port &&
            read16(packet + tcp_offset + 2) == TCP_CLIENT_PORT &&
            (flags & 0x12) == 0x12 &&
            read32(packet + tcp_offset + 4) == tcp_sequence) {
            tcp_acknowledged = read32(packet + tcp_offset + 4) + 1;
            tcp_synack_ready = 1;
        } else if (connection >= 0 && tcp_synack_ready &&
            read32(packet + 26) == tcp_remote_address &&
            read32(packet + 30) == local_address &&
            read16(packet + tcp_offset) == tcp_remote_port &&
            read16(packet + tcp_offset + 2) == TCP_CLIENT_PORT) {
            unsigned int tcp_header_length = ((packet[tcp_offset + 12] >> 4) * 4);
            unsigned int payload_offset = tcp_offset + tcp_header_length;
            if (tcp_header_length < 20 || payload_offset > length) return;
            if (payload_offset < length && tcp_data_length == 0 &&
                read32(packet + tcp_offset + 4) == tcp_acknowledged) {
                tcp_data_length = length - payload_offset;
                if (tcp_data_length > sizeof(tcp_data))
                    tcp_data_length = sizeof(tcp_data);
                tcp_acknowledged += tcp_data_length;
                copy_bytes(tcp_data, packet + payload_offset, tcp_data_length);
            }
            if (payload_offset < length &&
                read32(packet + tcp_offset + 4) < tcp_acknowledged) {
                send_tcp(gateway_mac, tcp_remote_address, tcp_remote_port, 0x10,
                    tcp_sequence, tcp_acknowledged);
            }
            if ((flags & 0x01) != 0) {
                ++tcp_acknowledged;
                send_tcp(gateway_mac, tcp_remote_address, tcp_remote_port, 0x10,
                    tcp_sequence, tcp_acknowledged);
                tcp_active->remote_closed = 1;
            }
        }
    } else if (packet[23] == 17 && length >= ETHERNET_HEADER + header_length + UDP_HEADER_SIZE) {
        unsigned int udp_offset = ETHERNET_HEADER + header_length;
        unsigned short udp_length = read16(packet + udp_offset + 4);
        if (udp_length >= UDP_HEADER_SIZE &&
            udp_offset + udp_length <= length &&
            read16(packet + udp_offset + 2) == DNS_CLIENT_PORT &&
            read16(packet + udp_offset + 8) == dns_id &&
            (packet[udp_offset + 10] & 0x80) != 0) {
            unsigned int answer_count = read16(packet + udp_offset + 14);
            unsigned int cursor = udp_offset + 20;
            while (cursor < length && packet[cursor] != 0) {
                if ((packet[cursor] & 0xC0) == 0xC0) {
                    cursor += 2;
                    break;
                }
                cursor += packet[cursor] + 1;
            }
            if (cursor >= length) return;
            if (packet[cursor] == 0) ++cursor;
            if (cursor + 4 > length) return;
            cursor += 4;
            for (unsigned int answer = 0; answer < answer_count; ++answer) {
                while (cursor < length && packet[cursor] != 0) {
                    if ((packet[cursor] & 0xC0) == 0xC0) {
                        cursor += 2;
                        break;
                    }
                    cursor += packet[cursor] + 1;
                }
                if (cursor >= length || packet[cursor] == 0) ++cursor;
                if (cursor + 10 > length) return;
                unsigned short type = read16(packet + cursor);
                unsigned short data_length = read16(packet + cursor + 8);
                cursor += 10;
                if (type == 1 && data_length == 4 && cursor + 4 <= length) {
                    dns_answer = read32(packet + cursor);
                    dns_answer_ready = 1;
                    return;
                }
                cursor += data_length;
            }
        }
    }
}

void network_init(void)
{
    e1000_get_mac(local_mac);
    network_ready = e1000_available();
    arp_gateway_ready = 0;
    tcp_active = &tcp_connections[0];
}

static void network_retransmit(void)
{
    unsigned long now = timer_ticks();
    for (int index = 0; index < TCP_CONNECTIONS; ++index) {
        struct tcp_state *connection = &tcp_connections[index];
        if (!connection->synack_ready || connection->pending_length == 0 ||
            now - connection->pending_tick < connection->pending_timeout)
            continue;
        tcp_select(index);
        if (connection->pending_retries >= TCP_MAX_RETRIES) {
            connection->pending_length = 0;
            connection->pending_fin = 0;
            connection->synack_ready = 0;
            connection->remote_address = 0;
            connection->remote_port = 0;
            continue;
        }
        if (connection->pending_fin) {
            send_tcp(gateway_mac, tcp_remote_address, tcp_remote_port, 0x11,
                connection->pending_sequence, connection->pending_acknowledged);
        } else {
            send_tcp_data(gateway_mac, tcp_remote_address, tcp_remote_port,
                connection->pending_sequence, connection->pending_acknowledged,
                connection->pending_data, connection->pending_length);
        }
        connection->pending_tick = now;
        if (connection->pending_timeout < TCP_RETRANSMIT_TICKS * 4)
            connection->pending_timeout *= 2;
        ++connection->pending_retries;
    }
}

void network_poll(void)
{
    int length;
    if (!network_ready) return;
    network_retransmit();
    while ((length = e1000_receive(frame, sizeof(frame))) > 0) {
        if (length >= ETHERNET_HEADER && read16(frame + 12) == ETHERNET_ARP)
            receive_arp(frame);
        else if (length >= ETHERNET_HEADER) receive_ipv4(frame, (unsigned int)length);
    }
}

int network_available(void) { return network_ready; }
unsigned int network_ipv4(void) { return local_address; }
unsigned int network_gateway(void) { return gateway_address; }
unsigned int network_dns(void) { return dns_address; }

int network_arp_request(unsigned int address)
{
    if (!network_ready) return 0;
    if (address == gateway_address && arp_gateway_ready) return 1;
    return send_arp(address);
}

static unsigned int text_length(const char *text)
{
    unsigned int length = 0;
    while (text[length] != '\0') ++length;
    return length;
}

static void ip_text(char *output, unsigned int capacity, unsigned int address)
{
    unsigned int offset = 0;
    for (unsigned int part = 0; part < 4; ++part) {
        unsigned int value = (address >> (24 - part * 8)) & 0xFF;
        unsigned int divisor = 100;
        int started = 0;
        while (divisor != 0) {
            unsigned int digit = value / divisor;
            value %= divisor;
            divisor /= 10;
            if (digit != 0 || started || divisor == 0) {
                if (offset + 1 >= capacity) break;
                output[offset++] = (char)('0' + digit);
                started = 1;
            }
        }
        if (part != 3 && offset + 1 < capacity) output[offset++] = '.';
    }
    if (capacity != 0) output[offset < capacity ? offset : capacity - 1] = '\0';
}

int network_dns_lookup(const char *name, char *output, unsigned int capacity)
{
    unsigned char query[256];
    unsigned int name_length;
    unsigned int offset = 12;
    unsigned int label_start = 0;
    if (!network_ready || name == (const char *)0 || output == (char *)0 ||
        capacity == 0) return 0;
    name_length = text_length(name);
    if (name_length == 0 || name_length > 200) return 0;
    query[0] = 0x4A;
    query[1] = 0x11;
    query[2] = 1;
    query[3] = 0;
    query[4] = 0;
    query[5] = 1;
    query[6] = query[7] = query[8] = query[9] = query[10] = query[11] = 0;
    for (unsigned int index = 0; index <= name_length; ++index) {
        if (name[index] == '.' || name[index] == '\0') {
            if (index == label_start || index - label_start > 63) return 0;
            query[offset++] = (unsigned char)(index - label_start);
            for (unsigned int letter = label_start; letter < index; ++letter)
                query[offset++] = (unsigned char)name[letter];
            label_start = index + 1;
        }
    }
    query[offset++] = 0;
    write16(query + offset, 1);
    offset += 2;
    write16(query + offset, 1);
    offset += 2;
    if (!arp_gateway_ready && !send_arp(gateway_address)) return 0;
    for (unsigned long wait = timer_ticks(); timer_ticks() - wait < 100; ) {
        network_poll();
        if (arp_gateway_ready) break;
    }
    if (!arp_gateway_ready) return 0;
    dns_id = 0x4A11;
    dns_answer_ready = 0;
    if (!send_udp(gateway_mac, dns_address, DNS_CLIENT_PORT, DNS_PORT,
        query, offset)) return 0;
    for (unsigned long wait = timer_ticks(); timer_ticks() - wait < 200; ) {
        network_poll();
        if (dns_answer_ready) {
            ip_text(output, capacity, dns_answer);
            return 1;
        }
    }
    return 0;
}

int network_tcp_connect(int connection, unsigned int address, unsigned short port)
{
    if (!tcp_select(connection) || !network_ready || port == 0 || address == 0 ||
        tcp_synack_ready || tcp_active->closing)
        return 0;
    if (!arp_gateway_ready) {
        if (!send_arp(gateway_address)) return 0;
        unsigned long wait = timer_ticks();
        while (timer_ticks() - wait < 100) {
            network_poll();
            if (arp_gateway_ready) break;
        }
        if (!arp_gateway_ready) return 0;
    }
    tcp_sequence = 0x13572468;
    tcp_remote_address = address;
    tcp_remote_port = port;
    tcp_data_length = 0;
    tcp_synack_ready = 0;
    tcp_active->closing = 0;
    tcp_active->remote_closed = 0;
    tcp_active->pending_length = 0;
    if (!send_tcp(gateway_mac, address, port, 0x02, tcp_sequence, 0)) return 0;
    for (unsigned long wait = timer_ticks(); timer_ticks() - wait < 200; ) {
        tcp_select(connection);
        network_poll();
        tcp_select(connection);
        if (tcp_synack_ready) {
            send_tcp(gateway_mac, address, port, 0x10, tcp_sequence + 1,
                tcp_acknowledged);
            tcp_sequence += 1;
            return 1;
        }
    }
    return 0;
}

long network_tcp_send(int connection, const void *data, unsigned int length)
{
    if (!tcp_select(connection) || !network_ready || data == (const void *)0 || length == 0 ||
        length > sizeof(tcp_data) || !tcp_synack_ready ||
        tcp_active->pending_length != 0)
        return -1;
    if (!send_tcp_data(gateway_mac, tcp_remote_address, tcp_remote_port,
        tcp_sequence, tcp_acknowledged, (const unsigned char *)data, length))
        return -1;
    tcp_sequence += length;
    copy_bytes(tcp_active->pending_data, (const unsigned char *)data, length);
    tcp_active->pending_sequence = tcp_sequence - length;
    tcp_active->pending_acknowledged = tcp_acknowledged;
    tcp_active->pending_length = length;
    tcp_active->pending_tick = timer_ticks();
    tcp_active->pending_timeout = TCP_RETRANSMIT_TICKS;
    tcp_active->pending_retries = 0;
    tcp_active->pending_fin = 0;
    return (long)length;
}

long network_tcp_receive(int connection, void *data, unsigned int capacity)
{
    if (!tcp_select(connection) || !network_ready || data == (void *)0 || capacity == 0 ||
        !tcp_synack_ready)
        return -1;
    if (tcp_active->remote_closed) return 0;
    network_poll();
    tcp_select(connection);
    if (tcp_data_length == 0) return 0;
    unsigned int length = tcp_data_length < capacity ? tcp_data_length : capacity;
    copy_bytes((unsigned char *)data, tcp_data, length);
    if (length < tcp_data_length) {
        for (unsigned int index = length; index < tcp_data_length; ++index)
            tcp_data[index - length] = tcp_data[index];
    }
    send_tcp(gateway_mac, tcp_remote_address, tcp_remote_port, 0x10,
        tcp_sequence, tcp_acknowledged);
    tcp_data_length -= length;
    return (long)length;
}

int network_tcp_close(int connection)
{
    if (!tcp_select(connection) || !network_ready || !tcp_synack_ready) return -1;
    if (!send_tcp(gateway_mac, tcp_remote_address, tcp_remote_port, 0x11,
        tcp_sequence, tcp_acknowledged))
        return -1;
    tcp_active->pending_sequence = tcp_sequence;
    tcp_active->pending_acknowledged = tcp_acknowledged;
    tcp_active->pending_length = 1;
    tcp_active->pending_tick = timer_ticks();
    tcp_active->pending_timeout = TCP_RETRANSMIT_TICKS;
    tcp_active->pending_retries = 0;
    tcp_active->pending_fin = 1;
    tcp_active->closing = 1;
    ++tcp_sequence;
    tcp_data_length = 0;
    return 0;
}

static int parse_http_target(const char *target, char *host, unsigned int host_capacity,
    unsigned short *port, char *path, unsigned int path_capacity)
{
    const char *cursor = target;
    unsigned int host_length = 0;
    unsigned int path_length = 0;
    unsigned int port_value = 80;
    if (target == (const char *)0 || host == (char *)0 || path == (char *)0 ||
        port == (unsigned short *)0) return 0;
    if (cursor[0] == 'h' && cursor[1] == 't' && cursor[2] == 't' && cursor[3] == 'p' &&
        cursor[4] == ':' && cursor[5] == '/' && cursor[6] == '/') cursor += 7;
    else if (cursor[0] == 'h' && cursor[1] == 't' && cursor[2] == 't' && cursor[3] == 'p' &&
        cursor[4] == 's' && cursor[5] == ':' && cursor[6] == '/' && cursor[7] == '/') {
        return 0;
    }
    while (cursor[host_length] != '\0' && cursor[host_length] != ':' &&
        cursor[host_length] != '/' && host_length + 1 < host_capacity)
        ++host_length;
    if (host_length == 0) return 0;
    for (unsigned int index = 0; index < host_length; ++index)
        host[index] = cursor[index];
    host[host_length] = '\0';
    if (cursor[host_length] == ':') {
        const char *port_cursor = cursor + host_length + 1;
        unsigned int port_index = 0;
        while (port_cursor[port_index] != '\0' && port_cursor[port_index] != '/' &&
            port_index < 6) {
            if (port_cursor[port_index] < '0' || port_cursor[port_index] > '9') return 0;
            port_value = port_value * 10 + (unsigned int)(port_cursor[port_index] - '0');
            ++port_index;
        }
        if (port_value == 0 || port_value > 65535) return 0;
        *port = (unsigned short)port_value;
        cursor = port_cursor + port_index;
    } else {
        *port = 80;
        cursor = cursor + host_length;
    }
    if (cursor[0] == '/') {
        while (cursor[path_length] != '\0' && path_length + 1 < path_capacity) {
            path[path_length] = cursor[path_length];
            ++path_length;
        }
        path[path_length] = '\0';
    } else {
        path[0] = '/';
        path[1] = '\0';
    }
    return 1;
}

int network_http_get(const char *host, char *output, unsigned int capacity)
{
    unsigned char request[512];
    unsigned int request_length = 0;
    unsigned int address;
    char hostname[128];
    char path[256];
    unsigned short port = 80;
    if (host == (const char *)0 || output == (char *)0 || capacity == 0)
        return 0;
    if (!parse_http_target(host, hostname, sizeof(hostname), &port, path, sizeof(path)))
        return 0;
    if (!network_dns_lookup(hostname, output, capacity)) return 0;
    address = dns_answer;
    const char *prefix = "GET ";
    const char *suffix = " HTTP/1.0\r\nHost: ";
    const char *tail = "\r\nConnection: close\r\n\r\n";
    for (unsigned int index = 0; prefix[index] != '\0'; ++index)
        request[request_length++] = (unsigned char)prefix[index];
    for (unsigned int index = 0; path[index] != '\0'; ++index)
        if (request_length + 1 < sizeof(request)) request[request_length++] = (unsigned char)path[index];
    for (unsigned int index = 0; suffix[index] != '\0'; ++index)
        if (request_length + 1 < sizeof(request)) request[request_length++] = (unsigned char)suffix[index];
    for (unsigned int index = 0; hostname[index] != '\0'; ++index)
        if (request_length + 1 < sizeof(request)) request[request_length++] = (unsigned char)hostname[index];
    if (port != 80) {
        char port_text[8];
        unsigned int port_length = 0;
        if (port < 10) port_text[port_length++] = (char)('0' + port);
        else {
            unsigned int value = port;
            do {
                port_text[port_length++] = (char)('0' + (value % 10));
                value /= 10;
            } while (value != 0);
            for (unsigned int index = 0; index < port_length / 2; ++index) {
                char tmp = port_text[index];
                port_text[index] = port_text[port_length - 1 - index];
                port_text[port_length - 1 - index] = tmp;
            }
        }
        request[request_length++] = ':';
        for (unsigned int index = 0; index < port_length; ++index)
            if (request_length + 1 < sizeof(request)) request[request_length++] = (unsigned char)port_text[index];
    }
    for (unsigned int index = 0; tail[index] != '\0'; ++index)
        if (request_length + 1 < sizeof(request)) request[request_length++] = (unsigned char)tail[index];
    tcp_data_length = 0;
    if (!network_tcp_connect(0, address, port)) return 0;
    if (network_tcp_send(0, request, request_length) < 0) return 0;
    for (unsigned long wait = timer_ticks(); timer_ticks() - wait < 300; ) {
        long received = network_tcp_receive(0, (unsigned char *)output,
            capacity - 1);
        if (received > 0) {
            unsigned int length = (unsigned int)received;
            output[length] = '\0';
            network_tcp_close(0);
            return 1;
        }
    }
    network_tcp_close(0);
    return 0;
}

int network_self_test(void)
{
    return network_ready && local_address != 0 && gateway_address != 0 &&
        dns_address != 0 && checksum((const unsigned char *)"", 0) == 0xFFFF;
}
