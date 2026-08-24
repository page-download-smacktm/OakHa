#include "acorn/linux_compat.h"
#include <bearssl_rand.h>
#include "acorn/timer.h"

void *memcpy(void *destination, const void *source, unsigned long length)
{
    unsigned char *target = (unsigned char *)destination;
    const unsigned char *origin = (const unsigned char *)source;
    for (unsigned long index = 0; index < length; ++index) target[index] = origin[index];
    return destination;
}

void *memmove(void *destination, const void *source, unsigned long length)
{
    unsigned char *target = (unsigned char *)destination;
    const unsigned char *origin = (const unsigned char *)source;
    if (target < origin) {
        for (unsigned long index = 0; index < length; ++index) target[index] = origin[index];
    } else if (target > origin) {
        for (unsigned long index = length; index != 0; --index) target[index - 1] = origin[index - 1];
    }
    return destination;
}

void *memset(void *destination, int value, unsigned long length)
{
    unsigned char *target = (unsigned char *)destination;
    for (unsigned long index = 0; index < length; ++index)
        target[index] = (unsigned char)value;
    return destination;
}

int memcmp(const void *left, const void *right, unsigned long length)
{
    const unsigned char *first = (const unsigned char *)left;
    const unsigned char *second = (const unsigned char *)right;
    for (unsigned long index = 0; index < length; ++index)
        if (first[index] != second[index]) return first[index] < second[index] ? -1 : 1;
    return 0;
}

unsigned long strlen(const char *text)
{
    unsigned long length = 0;
    while (text[length] != '\0') ++length;
    return length;
}

void __stack_chk_fail(void)
{
    for (;;) __asm__ volatile ("cli; hlt");
}

void *__memcpy_chk(void *destination, const void *source,
    unsigned long length, unsigned long destination_size)
{
    if (length > destination_size) __stack_chk_fail();
    return memcpy(destination, source, length);
}

void *__memset_chk(void *destination, int value, unsigned long length,
    unsigned long destination_size)
{
    if (length > destination_size) __stack_chk_fail();
    return memset(destination, value, length);
}

br_prng_seeder br_prng_seeder_system(const char **name)
{
    if (name != (const char **)0) *name = "oakos-external-entropy";
    return (br_prng_seeder)0;
}

long time(long *result)
{
    long seconds = (long)(timer_ticks() / 1000);
    if (result != (long *)0) *result = seconds;
    return seconds;
}
