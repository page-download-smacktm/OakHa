#ifndef ACORN_NETWORK_H
#define ACORN_NETWORK_H

void network_init(void);
void network_poll(void);
int network_available(void);
unsigned int network_ipv4(void);
unsigned int network_gateway(void);
unsigned int network_dns(void);
int network_arp_request(unsigned int address);
int network_dns_lookup(const char *name, char *output, unsigned int capacity);
int network_tcp_connect(int connection, unsigned int address, unsigned short port);
long network_tcp_send(int connection, const void *data, unsigned int length);
long network_tcp_receive(int connection, void *data, unsigned int capacity);
int network_tcp_close(int connection);
int network_http_get(const char *host, char *output, unsigned int capacity);
int network_self_test(void);

#endif
