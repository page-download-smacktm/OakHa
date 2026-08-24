#ifndef ACORN_LINUX_COMPAT_H
#define ACORN_LINUX_COMPAT_H

void *memcpy(void *destination, const void *source, unsigned long length);
void *memmove(void *destination, const void *source, unsigned long length);
void *memset(void *destination, int value, unsigned long length);
int memcmp(const void *left, const void *right, unsigned long length);
unsigned long strlen(const char *text);
void __stack_chk_fail(void);
void *__memcpy_chk(void *destination, const void *source,
    unsigned long length, unsigned long destination_size);
void *__memset_chk(void *destination, int value, unsigned long length,
    unsigned long destination_size);

#endif
