#ifndef ACORN_VGA_H
#define ACORN_VGA_H
void vga_init(void);
void vga_write(const char *text);
void vga_put_char(char value);
void vga_backspace(void);
#endif