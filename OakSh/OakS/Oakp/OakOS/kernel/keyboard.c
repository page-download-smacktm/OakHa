#include "acorn/keyboard.h"
#include "acorn/pic.h"
#include "acorn/serial.h"
#include "acorn/gui.h"

enum { KEYBOARD_BUFFER_SIZE = 128 };

static char input_buffer[KEYBOARD_BUFFER_SIZE];
static unsigned int input_head;
static unsigned int input_tail;
static int shift_pressed;
static int ctrl_pressed;

static const char keymap[] =
    "\0\0331234567890-="
    "\b\tqwertyuiop[]\n\0asdfghjkl;'`"
    "\0\\zxcvbnm,./\0\0\0 ";

static char shifted_value(unsigned char scancode, char value)
{
    switch (scancode) {
    case 0x02: return '!';
    case 0x03: return '@';
    case 0x04: return '#';
    case 0x05: return '$';
    case 0x06: return '%';
    case 0x07: return '^';
    case 0x08: return '&';
    case 0x09: return '*';
    case 0x0A: return '(';
    case 0x0B: return ')';
    case 0x0C: return '_';
    case 0x0D: return '+';
    case 0x0E: return '{';
    case 0x0F: return '}';
    case 0x1A: return '{';
    case 0x1B: return '}';
    case 0x27: return ':';
    case 0x28: return '"';
    case 0x29: return '~';
    case 0x2B: return '|';
    case 0x2C: return '<';
    case 0x2D: return '>';
    case 0x2E: return '?';
    case 0x33: return '<';
    case 0x34: return '>';
    case 0x35: return '?';
    default: return value;
    }
}

static void buffer_put(char value)
{
    unsigned int next = (input_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next == input_tail) return;
    input_buffer[input_head] = value;
    input_head = next;
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void keyboard_init(void)
{
    input_head = 0;
    input_tail = 0;
    shift_pressed = 0;
    ctrl_pressed = 0;
}

void acorn_keyboard_irq(void)
{
    unsigned char scancode = inb(0x60);
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
    } else if (scancode == 0x1D) {
        ctrl_pressed = 1;
    } else if (scancode == 0x9D) {
        ctrl_pressed = 0;
    } else if ((scancode & 0x80) == 0) {
        if (scancode >= 0x3B && scancode <= 0x3D) {
            gui_keyboard_input(0x80 + scancode - 0x3B);
            pic_send_eoi();
            return;
        }
        if (scancode == 0x01) {
            gui_keyboard_input(0x1B);
            pic_send_eoi();
            return;
        }
        char value = scancode < sizeof(keymap) ? keymap[scancode] : 0;
        if (value != 0) {
            if (ctrl_pressed && (value == 's' || value == 'S')) value = 0x13;
            else if (ctrl_pressed && (value == 'c' || value == 'C')) value = 0x03;
            if (shift_pressed) value = shifted_value(scancode, value);
            if (shift_pressed && value >= 'a' && value <= 'z') value -= 'a' - 'A';
            buffer_put(value);
            gui_keyboard_input(value);
            serial_write("keyboard scancode: ");
            serial_write_hex(scancode);
            serial_write(" shift: ");
            serial_write(shift_pressed ? "1 value: " : "0 value: ");
            serial_write("keyboard char: ");
            if (value == '\n') serial_write("\\n");
            else if (value == '\b') serial_write("\\b");
            else {
                char output[2] = { value, '\0' };
                serial_write(output);
            }
            serial_write("\n");
        }
    }
    pic_send_eoi();
}

int keyboard_pending(void)
{
    return input_head != input_tail;
}

int keyboard_read(void)
{
    if (!keyboard_pending()) return -1;
    char value = input_buffer[input_tail];
    input_tail = (input_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return (unsigned char)value;
}

int keyboard_self_test(void)
{
    return keymap[0x1E] == 'a' && keymap[0x30] == 'b' &&
        keymap[0x39] == ' ' && keymap[0x1C] == '\n';
}