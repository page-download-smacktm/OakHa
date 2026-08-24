#include "acorn/socket.h"
#include "acorn/network.h"

enum { SOCKET_MAX = 8, SOCKET_BASE = 3, AF_INET = 2, SOCK_STREAM = 1 };

static int socket_connection(int descriptor)
{
    return descriptor - SOCKET_BASE;
}

struct socket_state {
    int used;
    int connected;
    unsigned int address;
    unsigned short port;
};

static struct socket_state sockets[SOCKET_MAX];

int socket_open(int domain, int type, int protocol)
{
    (void)protocol;
    if (domain != AF_INET || type != SOCK_STREAM) return -1;
    for (int index = 0; index < SOCKET_MAX; ++index) {
        if (!sockets[index].used) {
            sockets[index].used = 1;
            sockets[index].connected = 0;
            return SOCKET_BASE + index;
        }
    }
    return -1;
}

static struct socket_state *get_socket(int descriptor)
{
    int index = descriptor - SOCKET_BASE;
    if (index < 0 || index >= SOCKET_MAX || !sockets[index].used) return 0;
    return &sockets[index];
}

int socket_connect(int descriptor, unsigned int address, unsigned short port)
{
    struct socket_state *socket = get_socket(descriptor);
    if (socket == 0 || !network_tcp_connect(socket_connection(descriptor), address, port)) return -1;
    socket->address = address;
    socket->port = port;
    socket->connected = 1;
    return 0;
}

long socket_send(int descriptor, const void *data, unsigned long length)
{
    struct socket_state *socket = get_socket(descriptor);
    if (socket == 0 || !socket->connected || length > 1024) return -1;
    return network_tcp_send(socket_connection(descriptor), data, (unsigned int)length);
}

long socket_receive(int descriptor, void *data, unsigned long capacity)
{
    struct socket_state *socket = get_socket(descriptor);
    if (socket == 0 || !socket->connected || capacity > 1024) return -1;
    return network_tcp_receive(socket_connection(descriptor), data, (unsigned int)capacity);
}

int socket_close(int descriptor)
{
    struct socket_state *socket = get_socket(descriptor);
    if (socket == 0) return -1;
    if (socket->connected && network_tcp_close(socket_connection(descriptor)) < 0) return -1;
    socket->used = 0;
    socket->connected = 0;
    return 0;
}