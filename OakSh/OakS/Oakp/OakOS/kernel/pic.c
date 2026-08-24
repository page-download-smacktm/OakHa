#include "acorn/pic.h"

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

void pic_init(void)
{
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();
    outb(0x21, 0xF8);
    outb(0xA1, 0xFF);
}

void pic_send_eoi(void)
{
    outb(0x20, 0x20);
}

void pic_send_slave_eoi(void)
{
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}