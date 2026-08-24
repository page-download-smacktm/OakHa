#include "acorn/serial.h"

enum { COM1 = 0x3F8 };

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char value)
{
    while ((inb(COM1 + 5) & 0x20) == 0) { }
    outb(COM1, (unsigned char)value);
}

void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') serial_putc('\r');
        serial_putc(*text++);
    }
}

void serial_write_hex(unsigned long value)
{
    static const char digits[] = "0123456789ABCDEF";
    serial_write("0x");
    for (int shift = (int)(sizeof(value) * 8 - 4); shift >= 0; shift -= 4)
        serial_putc(digits[(value >> shift) & 0xF]);
}