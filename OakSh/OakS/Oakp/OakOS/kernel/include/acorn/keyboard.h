#ifndef ACORN_KEYBOARD_H
#define ACORN_KEYBOARD_H

void keyboard_init(void);
void acorn_keyboard_irq(void);
int keyboard_pending(void);
int keyboard_read(void);
int keyboard_self_test(void);

#endif