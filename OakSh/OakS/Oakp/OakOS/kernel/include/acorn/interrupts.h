#ifndef ACORN_INTERRUPTS_H
#define ACORN_INTERRUPTS_H

void interrupts_init(void);
void interrupts_set_handler(unsigned int vector, void (*handler)(void));
void interrupts_set_syscall_handler(unsigned int vector, void (*handler)(void));
void acorn_exception(void);

#endif