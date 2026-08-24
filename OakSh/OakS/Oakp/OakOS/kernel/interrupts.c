#include "acorn/interrupts.h"
#include "acorn/serial.h"

struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char ist;
    unsigned char type_attributes;
    unsigned short offset_middle;
    unsigned int offset_high;
    unsigned int reserved;
} __attribute__((packed));

struct idt_pointer {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

extern void acorn_interrupt_stub(void);
extern void acorn_load_idt(const struct idt_pointer *pointer);

static struct idt_entry idt[256];

static void set_gate(unsigned int vector, void (*handler)(void))
{
    unsigned long address = (unsigned long)handler;
    idt[vector].offset_low = address & 0xFFFF;
    idt[vector].selector = 0x08;
    idt[vector].ist = 0;
    idt[vector].type_attributes = 0x8E;
    idt[vector].offset_middle = (address >> 16) & 0xFFFF;
    idt[vector].offset_high = (address >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

void interrupts_set_syscall_handler(unsigned int vector, void (*handler)(void))
{
    if (vector < 256 && handler != (void (*)(void))0) {
        set_gate(vector, handler);
        idt[vector].type_attributes = 0xEE;
    }
}

void interrupts_init(void)
{
    for (unsigned int vector = 0; vector < 256; ++vector)
        set_gate(vector, acorn_interrupt_stub);

    struct idt_pointer pointer = {
        .limit = sizeof(idt) - 1,
        .base = (unsigned long)idt,
    };
    acorn_load_idt(&pointer);
}

void interrupts_set_handler(unsigned int vector, void (*handler)(void))
{
    if (vector < 256 && handler != (void (*)(void))0)
        set_gate(vector, handler);
}

void acorn_exception(void)
{
    serial_write("KERNEL PANIC\nReason: CPU exception\n");
    for (;;) __asm__ volatile ("cli; hlt");
}