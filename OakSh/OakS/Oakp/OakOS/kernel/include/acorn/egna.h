#ifndef ACORN_EGNA_H
#define ACORN_EGNA_H

#define EGNA_FLAG_HAS_ICON 0x00000001u
#define EGNA_FLAG_HAS_SHELL 0x00000002u
#define EGNA_FLAG_HAS_CODE 0x00000004u
#define EGNA_FLAG_GUI_READY 0x00000008u

struct egna_package {
    char name[32];
    char shell_command[128];
    unsigned int flags;
    unsigned int icon_length;
    unsigned int icon_width;
    unsigned int icon_height;
    unsigned int code_length;
};

enum {
    EGNA_PAYLOAD_NONE = 0,
    EGNA_PAYLOAD_BYTECODE = 1,
    EGNA_PAYLOAD_ELF = 2
};

int egna_load(const char *path, struct egna_package *package);
int egna_has_icon(const char *path);
int egna_is_gui_capable(const char *path);
int egna_get_name(const char *path, char *output, unsigned int output_size);
int egna_get_shell_command(const char *path, char *output, unsigned int output_size);
int egna_run_shell(const char *path, char *output, unsigned int output_size);
int egna_payload_type(const char *path, unsigned int *type);
int egna_extract_icon(const char *path, void *buffer, unsigned int buffer_size,
    unsigned int *icon_length);
int egna_extract_payload(const char *path, void *buffer, unsigned int buffer_size,
    unsigned int *payload_length);
int egna_run(const char *path, char *output, unsigned int output_size);

#endif
