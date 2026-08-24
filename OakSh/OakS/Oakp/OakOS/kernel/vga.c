#include "acorn/vga.h"

static volatile unsigned short *const video = (unsigned short *)0xB8000;
static unsigned int row;
static unsigned int column;
static const unsigned char color = 0x07;

void vga_init(void)
{
    row = 0;
    column = 0;
    for (unsigned int index = 0; index < 80 * 25; ++index)
        video[index] = ((unsigned short)color << 8) | ' ';
}

void vga_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            column = 0;
            ++row;
        } else {
            video[row * 80 + column] = ((unsigned short)color << 8) | (unsigned char)*text;
            ++column;
            if (column == 80) { column = 0; ++row; }
        }
        if (row == 25) row = 0;
        ++text;
    }
}

void vga_put_char(char value)
{
    char text[2] = { value, '\0' };
    vga_write(text);
}

void vga_backspace(void)
{
    if (column == 0 && row == 0) return;
    if (column == 0) {
        --row;
        column = 79;
    } else {
        --column;
    }
    video[row * 80 + column] = ((unsigned short)color << 8) | ' ';
}