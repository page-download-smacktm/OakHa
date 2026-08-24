#ifndef ACORN_APP_SHELL_H
#define ACORN_APP_SHELL_H

#define SHELL_RETURN_GRAPHIC (-2)
#define SHELL_RUN_APP (-3)

void shell_init(char *line, unsigned int line_size, char *output,
    unsigned int output_size, unsigned int *length);
void shell_set_output(char *output, unsigned int output_size, const char *text);
const char *shell_current_directory(void);
const char *shell_requested_app(void);
int shell_handle_key(int value, char *line, unsigned int line_size,
    unsigned int *length, char *output, unsigned int output_size);

#endif
