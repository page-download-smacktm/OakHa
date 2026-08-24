#ifndef OAK_POSIX_H
#define OAK_POSIX_H

#include "libc.h"

typedef long ssize_t;
typedef long off_t;
typedef unsigned int mode_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;

#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_TRUNC 01000
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define AF_INET 2
#define SOCK_STREAM 1

static inline uid_t getuid(void) { return (uid_t)oak_getuid(); }
static inline gid_t getgid(void) { return (gid_t)oak_getgid(); }
static inline ssize_t write(int fd, const void *data, unsigned long length)
{
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) return -1;
    return (ssize_t)oak_write((const char *)data, length);
}
static inline int mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return (int)oak_mkdir(path);
}
static inline int creat(const char *path, mode_t mode)
{
    (void)mode;
    return (int)oak_create(path);
}
static inline int socket(int domain, int type, int protocol)
{
    return (int)oak_socket(domain, type, protocol);
}
static inline int connect(int descriptor, unsigned int address,
    unsigned short port)
{
    return (int)oak_connect(descriptor, address, port);
}
static inline ssize_t send(int descriptor, const void *data, unsigned long length,
    int flags)
{
    (void)flags;
    return (ssize_t)oak_send(descriptor, data, length);
}
static inline ssize_t recv(int descriptor, void *data, unsigned long capacity,
    int flags)
{
    (void)flags;
    return (ssize_t)oak_recv(descriptor, data, capacity);
}
static inline int close(int descriptor) { return (int)oak_close(descriptor); }

#endif