#ifndef ACORN_SERIAL_H
#define ACORN_SERIAL_H
void serial_init(void);
void serial_write(const char *text);
void serial_write_hex(unsigned long value);
#endif