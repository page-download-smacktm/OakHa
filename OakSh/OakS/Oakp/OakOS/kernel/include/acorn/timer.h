#ifndef ACORN_TIMER_H
#define ACORN_TIMER_H

void timer_init(void);
void acorn_timer_irq(void);
unsigned long timer_ticks(void);

#endif