#include "acorn/timer.h"
#include "acorn/pic.h"
#include "acorn/serial.h"
#include "acorn/gui.h"
#include "acorn/mouse.h"
#include "acorn/network.h"

static volatile unsigned long ticks;

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void timer_init(void)
{
    unsigned short divisor = 1193;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);
}

void acorn_timer_irq(void)
{
    ++ticks;
    mouse_poll();
    network_poll();
    gui_tick();
    pic_send_eoi();
}

unsigned long timer_ticks(void)
{
    return ticks;
}