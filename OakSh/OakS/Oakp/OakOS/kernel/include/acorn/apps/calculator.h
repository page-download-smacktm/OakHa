#ifndef ACORN_APP_CALCULATOR_H
#define ACORN_APP_CALCULATOR_H

int calculator_evaluate(const char *expression, char *output, unsigned int output_size);
void calculator_draw(const char *output);

#endif
