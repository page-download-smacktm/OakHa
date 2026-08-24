#include "acorn/mouse.h"
#include "acorn/framebuffer.h"
#include "acorn/pic.h"

static unsigned char packet[3];
static unsigned int packet_index;
static int cursor_x;
static int cursor_y;
static unsigned char buttons;
static int changed;
static volatile unsigned long irq_count;
static volatile unsigned long packet_count;
static volatile unsigned long byte_count;

static inline void mouse_packet_byte(unsigned char value)
{
    ++byte_count;
    if (packet_index == 0 && (value & 0xC8) != 0x08) return;
    packet[packet_index++] = value;
    if (packet_index == 3) {
        int next_x = cursor_x + (signed char)packet[1];
        int next_y = cursor_y - (signed char)packet[2];
        int max_x = (int)framebuffer_width() - 1;
        int max_y = (int)framebuffer_height() - 1;
        cursor_x = next_x < 0 ? 0 : (next_x > max_x ? max_x : next_x);
        cursor_y = next_y < 0 ? 0 : (next_y > max_y ? max_y : next_y);
        buttons = packet[0] & 7;
        changed = 1;
        ++packet_count;
        packet_index = 0;
    }
}

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

static void wait_input_clear(void)
{
    for (unsigned long count = 0; count < 100000; ++count)
        if ((inb(0x64) & 2) == 0) return;
}

static int wait_output_full(void)
{
    for (unsigned long count = 0; count < 100000; ++count)
        if ((inb(0x64) & 1) != 0) return 1;
    return 0;
}

static int mouse_command(unsigned char value)
{
    wait_input_clear();
    outb(0x64, 0xD4);
    wait_input_clear();
    outb(0x60, value);
    if (!wait_output_full()) return 0;
    unsigned char status = inb(0x64);
    unsigned char response = inb(0x60);
    return (status & 0x20) != 0 && response == 0xFA;
}

void mouse_init(void)
{
    packet_index = 0;
    buttons = 0;
    irq_count = 0;
    packet_count = 0;
    byte_count = 0;
    cursor_x = framebuffer_width() / 2;
    cursor_y = framebuffer_height() / 2;
    changed = 1;
    wait_input_clear();
    outb(0x64, 0xA7);
    while ((inb(0x64) & 1) != 0) {
        (void)inb(0x60);
    }
    outb(0x64, 0xA8);
    wait_input_clear();
    outb(0x64, 0x20);
    if (!wait_output_full()) return;
    unsigned char status = inb(0x60);
    status |= 2;
    wait_input_clear();
    outb(0x64, 0x60);
    wait_input_clear();
    outb(0x60, (status | 2) & (unsigned char)~0x20);
    (void)mouse_command(0xF6);
    (void)mouse_command(0xF4);
}

void acorn_mouse_irq(void)
{
    unsigned char status = inb(0x64);
    ++irq_count;
    if ((status & 1) != 0 && (status & 0x20) != 0)
        mouse_packet_byte(inb(0x60));
    pic_send_slave_eoi();
}

void mouse_poll(void)
{
    while ((inb(0x64) & 1) != 0) {
        unsigned char status = inb(0x64);
        if ((status & 0x20) != 0)
            mouse_packet_byte(inb(0x60));
        else
            (void)inb(0x60);
    }
}

int mouse_moved(void)
{
    int result = changed;
    changed = 0;
    return result;
}

int mouse_x(void) { return cursor_x; }
int mouse_y(void) { return cursor_y; }
unsigned char mouse_buttons(void) { return buttons; }
unsigned long mouse_irq_count(void) { return irq_count; }
unsigned long mouse_packet_count(void) { return packet_count; }
unsigned long mouse_byte_count(void) { return byte_count; }
