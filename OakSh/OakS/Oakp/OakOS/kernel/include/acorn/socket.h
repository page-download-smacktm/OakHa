#ifndef ACORN_SOCKET_H
#define ACORN_SOCKET_H

int socket_open(int domain, int type, int protocol);
int socket_connect(int descriptor, unsigned int address, unsigned short port);
long socket_send(int descriptor, const void *data, unsigned long length);
long socket_receive(int descriptor, void *data, unsigned long capacity);
int socket_close(int descriptor);

#endif