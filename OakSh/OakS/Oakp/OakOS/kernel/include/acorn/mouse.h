#ifndef ACORN_MOUSE_H
#define ACORN_MOUSE_H

void mouse_init(void);
void acorn_mouse_irq(void);
void mouse_poll(void);
int mouse_moved(void);
int mouse_x(void);
int mouse_y(void);
unsigned char mouse_buttons(void);
unsigned long mouse_irq_count(void);
unsigned long mouse_packet_count(void);
unsigned long mouse_byte_count(void);

#endif
