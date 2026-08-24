#ifndef ACORN_ENTROPY_H
#define ACORN_ENTROPY_H

int entropy_available(void);
int entropy_fill(unsigned char *output, unsigned int length);

#endif
