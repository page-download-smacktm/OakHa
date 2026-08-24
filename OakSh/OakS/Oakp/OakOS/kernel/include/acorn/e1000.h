#ifndef ACORN_E1000_H
#define ACORN_E1000_H

void e1000_init(void);
int e1000_available(void);
void e1000_get_mac(unsigned char *mac);
int e1000_send(const void *data, unsigned int length);
int e1000_receive(void *data, unsigned int capacity);
int e1000_self_test(void);

#endif