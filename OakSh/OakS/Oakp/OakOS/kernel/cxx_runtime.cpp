#include "acorn/memory.h"

extern "C" int __cxa_atexit(void (*function)(void *), void *argument,
    void *dso_handle)
{
    (void)function;
    (void)argument;
    (void)dso_handle;
    return 0;
}

extern "C" int __cxa_guard_acquire(unsigned long long *guard)
{
    return *guard == 0;
}

extern "C" void __cxa_guard_release(unsigned long long *guard)
{
    *guard = 1;
}

extern "C" void __cxa_guard_abort(unsigned long long *guard)
{
    *guard = 0;
}

extern "C" void __cxa_pure_virtual(void)
{
    for (;;) __asm__ volatile ("cli; hlt");
}

void *operator new(unsigned long size)
{
    return kmalloc(size);
}

void *operator new[](unsigned long size)
{
    return kmalloc(size);
}

void operator delete(void *pointer) noexcept
{
    (void)pointer;
}

void operator delete[](void *pointer) noexcept
{
    (void)pointer;
}

void operator delete(void *pointer, unsigned long size) noexcept
{
    (void)pointer;
    (void)size;
}

void operator delete[](void *pointer, unsigned long size) noexcept
{
    (void)pointer;
    (void)size;
}
