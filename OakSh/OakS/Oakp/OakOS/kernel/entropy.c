#include "acorn/entropy.h"

static unsigned int cpuid_ecx(void)
{
    unsigned int eax = 1;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    return ecx;
}

static int rdrand_supported(void)
{
    return (cpuid_ecx() & (1U << 30)) != 0;
}

static int rdrand32(unsigned int *value)
{
    unsigned char ready = 0;
    for (unsigned int attempt = 0; attempt < 10; ++attempt) {
        __asm__ volatile ("rdrand %0; setc %1"
            : "=a"(*value), "=qm"(ready));
        if (ready) return 1;
    }
    return 0;
}

int entropy_available(void)
{
    return rdrand_supported();
}

int entropy_fill(unsigned char *output, unsigned int length)
{
    if (output == (unsigned char *)0 || !rdrand_supported()) return 0;
    for (unsigned int index = 0; index < length; index += 4) {
        unsigned int value;
        unsigned int remaining = length - index;
        if (!rdrand32(&value)) return 0;
        for (unsigned int byte = 0; byte < remaining && byte < 4; ++byte)
            output[index + byte] = (unsigned char)(value >> (byte * 8));
    }
    return 1;
}
