#ifndef ACORN_PIC_H
#define ACORN_PIC_H

void pic_init(void);
void pic_send_eoi(void);
void pic_send_slave_eoi(void);

#endif