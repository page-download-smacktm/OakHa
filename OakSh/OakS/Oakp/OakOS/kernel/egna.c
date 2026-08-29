#include "acorn/egna.h"
#include "acorn/fs.h"
#include "acorn/memory.h"
#include "acorn/process.h"

#define EGNA_MAGIC "EGNA"
#define EGNA_VERSION 1u
#define EGNA_BYTECODE_MAGIC "EGBC"

struct egna_disk_header {
    char magic[4];
    unsigned int version;
    unsigned int flags;
    char name[32];
    char shell_command[128];
    unsigned int icon_length;
    unsigned int icon_width;
    unsigned int icon_height;
    unsigned int code_length;
};

static void copy_text(char *destination, unsigned int size, const char *source)
{
    unsigned int index = 0;
    if (size == 0) return;
    while (source[index] != '\0' && index + 1 < size) {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static int is_elf_payload(const unsigned char *data, unsigned int length)
{
    return length >= 4 && data[0] == 0x7F && data[1] == 'E' &&
        data[2] == 'L' && data[3] == 'F';
}

static int is_bytecode_payload(const unsigned char *data, unsigned int length)
{
    return length >= 4 && data[0] == 'E' && data[1] == 'G' &&
        data[2] == 'B' && data[3] == 'C';
}

static int read_package_file(const char *path, unsigned char *buffer,
    unsigned int buffer_size, unsigned int *file_size)
{
    long size = 0;
    if (path == (const char *)0 || buffer == (unsigned char *)0 ||
        buffer_size < sizeof(struct egna_disk_header) || file_size == (unsigned int *)0)
        return 0;
    if (!fs_exists(path)) return 0;
    size = fs_read(path, buffer, (unsigned long)buffer_size);
    if (size <= 0) return 0;
    *file_size = (unsigned int)size;
    return 1;
}

int egna_load(const char *path, struct egna_package *package)
{
    struct egna_disk_header header;
    unsigned char buffer[32768];
    unsigned int file_size = 0;
    if (package == (struct egna_package *)0 || path == (const char *)0)
        return 0;
    if (!read_package_file(path, buffer, sizeof(buffer), &file_size)) return 0;
    if (file_size < sizeof(header)) return 0;
    for (unsigned int index = 0; index < sizeof(header); ++index)
        ((unsigned char *)&header)[index] = buffer[index];
    if (header.magic[0] != 'E' || header.magic[1] != 'G' ||
        header.magic[2] != 'N' || header.magic[3] != 'A') return 0;
    if (header.version != EGNA_VERSION) return 0;

    package->flags = header.flags;
    copy_text(package->name, sizeof(package->name), header.name);
    copy_text(package->shell_command, sizeof(package->shell_command), header.shell_command);
    package->icon_length = header.icon_length;
    package->icon_width = header.icon_width;
    package->icon_height = header.icon_height;
    package->code_length = header.code_length;

    if (package->icon_length != 0) package->flags |= EGNA_FLAG_HAS_ICON;
    if (package->shell_command[0] != '\0') package->flags |= EGNA_FLAG_HAS_SHELL;
    if (package->code_length != 0) package->flags |= EGNA_FLAG_HAS_CODE;
    if (package->icon_length != 0 && package->icon_width > 0 && package->icon_height > 0)
        package->flags |= EGNA_FLAG_GUI_READY;
    else if (package->icon_length != 0)
        package->flags &= ~EGNA_FLAG_GUI_READY;

    if (sizeof(header) + package->icon_length + package->code_length > file_size)
        return 0;
    return 1;
}

int egna_has_icon(const char *path)
{
    struct egna_package package;
    return egna_load(path, &package) && (package.flags & EGNA_FLAG_HAS_ICON) != 0;
}

int egna_is_gui_capable(const char *path)
{
    struct egna_package package;
    return egna_load(path, &package) && (package.flags & EGNA_FLAG_GUI_READY) != 0;
}

int egna_get_name(const char *path, char *output, unsigned int output_size)
{
    struct egna_package package;
    if (!egna_load(path, &package) || output == (char *)0 || output_size == 0)
        return 0;
    copy_text(output, output_size, package.name);
    return 1;
}

int egna_get_shell_command(const char *path, char *output, unsigned int output_size)
{
    struct egna_package package;
    if (!egna_load(path, &package) || output == (char *)0 || output_size == 0)
        return 0;
    copy_text(output, output_size, package.shell_command);
    return 1;
}

int egna_extract_icon(const char *path, void *buffer, unsigned int buffer_size,
    unsigned int *icon_length)
{
    struct egna_package package;
    unsigned char file[32768];
    unsigned int file_size = 0;
    if (!egna_load(path, &package) || buffer == (void *)0 || package.icon_length == 0)
        return 0;
    if (!read_package_file(path, file, sizeof(file), &file_size)) return 0;
    if (buffer_size < package.icon_length) return 0;
    if (icon_length != (unsigned int *)0) *icon_length = package.icon_length;
    for (unsigned int index = 0; index < package.icon_length; ++index)
        ((unsigned char *)buffer)[index] = file[sizeof(struct egna_disk_header) + index];
    return 1;
}

int egna_extract_payload(const char *path, void *buffer, unsigned int buffer_size,
    unsigned int *payload_length)
{
    struct egna_package package;
    unsigned char file[32768];
    unsigned int file_size = 0;
    unsigned long offset = 0;
    if (!egna_load(path, &package) || buffer == (void *)0 || package.code_length == 0)
        return 0;
    if (!read_package_file(path, file, sizeof(file), &file_size)) return 0;
    offset = sizeof(struct egna_disk_header) + package.icon_length;
    if (buffer_size < package.code_length) return 0;
    if (payload_length != (unsigned int *)0) *payload_length = package.code_length;
    for (unsigned int index = 0; index < package.code_length; ++index)
        ((unsigned char *)buffer)[index] = file[offset + index];
    return 1;
}

int egna_payload_type(const char *path, unsigned int *type)
{
    struct egna_package package;
    unsigned char file[32768];
    unsigned int file_size = 0;
    unsigned long offset = 0;
    if (!egna_load(path, &package) || package.code_length == 0) return 0;
    if (!read_package_file(path, file, sizeof(file), &file_size)) return 0;
    offset = sizeof(struct egna_disk_header) + package.icon_length;
    if (type == (unsigned int *)0) return 0;
    if (is_elf_payload(file + offset, package.code_length)) {
        *type = EGNA_PAYLOAD_ELF;
        return 1;
    }
    if (is_bytecode_payload(file + offset, package.code_length)) {
        *type = EGNA_PAYLOAD_BYTECODE;
        return 1;
    }
    *type = EGNA_PAYLOAD_NONE;
    return 1;
}

int egna_run_shell(const char *path, char *output, unsigned int output_size)
{
    struct egna_package package;
    char command[128];
    if (!egna_load(path, &package) || output == (char *)0 || output_size == 0)
        return 0;
    if ((package.flags & EGNA_FLAG_HAS_SHELL) == 0) {
        copy_text(output, output_size, "EGNA has no shell entry");
        return 0;
    }
    copy_text(command, sizeof(command), package.shell_command);
    copy_text(output, output_size, command);
    return 1;
}

int egna_run(const char *path, char *output, unsigned int output_size)
{
    struct egna_package package;
    unsigned int type = EGNA_PAYLOAD_NONE;
    unsigned int payload_size = 0;
    unsigned char *payload = (unsigned char *)0;
    if (!egna_load(path, &package) || output == (char *)0 || output_size == 0)
        return 0;
    if (package.shell_command[0] != '\0') {
        copy_text(output, output_size, package.shell_command);
        return 1;
    }
    if (!egna_payload_type(path, &type) || package.code_length == 0) {
        copy_text(output, output_size, "EGNA: no runnable payload");
        return 0;
    }
    payload = (unsigned char *)kmalloc((unsigned long)package.code_length);
    if (payload == (unsigned char *)0) {
        copy_text(output, output_size, "EGNA: payload allocation failed");
        return 0;
    }
    if (!egna_extract_payload(path, payload, package.code_length, &payload_size)) {
        copy_text(output, output_size, "EGNA: payload extraction failed");
        return 0;
    }
    if (type == EGNA_PAYLOAD_ELF) {
        struct process *process = process_create((void (*)(void))1);
        if (process == (struct process *)0 || !process_load_elf(process, payload, payload_size)) {
            copy_text(output, output_size, "EGNA: ELF load failed");
            return 0;
        }
        process_run_user(process);
        copy_text(output, output_size, "EGNA: ELF payload launched");
        return 1;
    }
    if (type == EGNA_PAYLOAD_BYTECODE) {
        copy_text(output, output_size, "EGNA: bytecode payload loaded");
        return 1;
    }
    copy_text(output, output_size, "EGNA: unsupported payload type");
    return 0;
}
