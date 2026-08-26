#include "acorn/apps/shell.h"
#include "acorn/apps/calculator.h"
#include "acorn/fs.h"
#include "acorn/e1000.h"
#include "acorn/network.h"

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

static int equals(const char *left, const char *right)
{
    unsigned int index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        ++index;
    }
    return left[index] == right[index];
}

static int starts_with(const char *text, const char *prefix)
{
    unsigned int index = 0;
    while (prefix[index] != '\0') {
        if (text[index] != prefix[index]) return 0;
        ++index;
    }
    return 1;
}

static char current_directory[33] = "/";
static char previous_directory[33] = "/";
static char download_buffer[1024];
static char requested_app[33];

static int download_http_file(const char *path, const char *host)
{
    char *body = download_buffer;
    long response_length = 0;
    unsigned int index;
    if (!network_http_get(host, download_buffer, sizeof(download_buffer))) return 0;
    while (response_length < (long)sizeof(download_buffer) &&
        download_buffer[response_length] != '\0') ++response_length;
    for (index = 0; index + 3 < (unsigned int)response_length; ++index) {
        if (download_buffer[index] == '\r' && download_buffer[index + 1] == '\n' &&
            download_buffer[index + 2] == '\r' && download_buffer[index + 3] == '\n') {
            body = download_buffer + index + 4;
            response_length -= (long)(index + 4);
            break;
        }
    }
    if (!fs_exists(path) && fs_create(path) != 0) return 0;
    return fs_write(path, body, (unsigned long)response_length) == response_length;
}
static void resolve_path(char *destination, unsigned int size, const char *path)
{
        if (path[0] == '/') {
            copy_text(destination, size, path);
            return;
        }
        if (equals(current_directory, "/")) {
            destination[0] = '/';
            copy_text(destination + 1, size - 1, path);
        } else {
            copy_text(destination, size, current_directory);
            unsigned int length = 0;
            while (destination[length] != '\0') ++length;
            if (length + 1 < size) {
                destination[length++] = '/';
                copy_text(destination + length, size - length, path);
            }
        }
}

void shell_set_output(char *output, unsigned int output_size, const char *text)
{
    copy_text(output, output_size, text);
}

const char *shell_current_directory(void)
{
    return current_directory;
}

const char *shell_requested_app(void)
{
    return requested_app;
}

void shell_init(char *line, unsigned int line_size, char *output,
    unsigned int output_size, unsigned int *length)
{
    if (line_size != 0) line[0] = '\0';
    copy_text(output, output_size, "shell ready");
    current_directory[0] = '/';
    current_directory[1] = '\0';
    previous_directory[0] = '/';
    previous_directory[1] = '\0';
    requested_app[0] = '\0';
    *length = 0;
}

static int run_command(const char *line, char *output, unsigned int output_size)
{
    char result[24];
    char buffer[64];
    if (starts_with(line, "run ")) {
        const char *name = line + 4;
        if (equals(name, "snake") || equals(name, "snake.elf") ||
            equals(name, "minesweeper") || equals(name, "minesweeper.elf") ||
            equals(name, "tetris") || equals(name, "tetris.elf")) {
            if (starts_with(name, "mine")) {
                copy_text(requested_app, sizeof(requested_app), "minesweeper");
                copy_text(output, output_size, "starting minesweeper");
            } else if (starts_with(name, "tetris")) {
                copy_text(requested_app, sizeof(requested_app), "tetris");
                copy_text(output, output_size, "starting tetris");
            } else {
                copy_text(requested_app, sizeof(requested_app), "snake");
                copy_text(output, output_size, "starting snake");
            }
            return SHELL_RUN_APP;
        }
        copy_text(output, output_size, "run: application unavailable");
    } else if (equals(line, "help")) {
        copy_text(output, output_size,
            "help run(snake/minesweeper/tetris) cd ls show mkdir calc net dns tcp http curl browse ret-grafic");
    } else if (equals(line, "net")) {
        copy_text(output, output_size,
            e1000_available() ? "network: e1000 ready" : "network: no e1000");
    } else if (equals(line, "net arp")) {
        copy_text(output, output_size,
            network_arp_request(network_gateway()) ?
            "network: arp request sent" : "network: arp unavailable");
    } else if (starts_with(line, "dns ")) {
        copy_text(output, output_size, "dns: resolving");
        if (!network_dns_lookup(line + 4, output, output_size))
            copy_text(output, output_size, "dns: lookup failed");
    } else if (equals(line, "tcp")) {
        copy_text(output, output_size,
            network_tcp_connect(0, network_gateway(), 80) ?
            "tcp: connection established" : "tcp: connection failed");
    } else if (starts_with(line, "http ")) {
        if (!network_http_get(line + 5, output, output_size))
            copy_text(output, output_size, "http: request failed");
    } else if (starts_with(line, "curl ")) {
        const char *address = line + 5;
        if (starts_with(address, "-o ")) {
            const char *path = address + 3;
            unsigned int path_length = 0;
            while (path[path_length] != ' ' && path[path_length] != '\0') ++path_length;
            if (path[path_length] == ' ') {
                char file_path[33];
                char host[64];
                unsigned int host_length = 0;
                if (path_length >= sizeof(file_path)) {
                    copy_text(output, output_size, "curl: path too long");
                    return 0;
                }
                for (unsigned int index = 0; index < path_length; ++index)
                    file_path[index] = path[index];
                file_path[path_length] = '\0';
                path += path_length + 1;
                if (starts_with(path, "https://")) {
                    copy_text(output, output_size, "curl: https unavailable");
                } else {
                    if (starts_with(path, "http://")) path += 7;
                    while (path[host_length] != '\0' && host_length + 1 < sizeof(host)) {
                        host[host_length] = path[host_length];
                        ++host_length;
                    }
                    host[host_length] = '\0';
                    copy_text(output, output_size,
                        download_http_file(file_path, host) ?
                        "curl: file saved" : "curl: download failed");
                }
            } else copy_text(output, output_size, "curl: use -o /file host");
        } else if (starts_with(address, "https://")) {
            copy_text(output, output_size, "curl: https unavailable");
        } else {
            if (starts_with(address, "http://")) address += 7;
            if (!network_http_get(address, output, output_size))
                copy_text(output, output_size, "curl: request failed");
        }
    } else if (starts_with(line, "browse ")) {
        const char *address = line + 7;
        if (starts_with(address, "https://")) {
            copy_text(output, output_size, "browse: https unavailable");
        } else {
            if (starts_with(address, "http://")) address += 7;
            if (!network_http_get(address, output, output_size))
                copy_text(output, output_size, "browse: page unavailable");
        }
    } else if (equals(line, "ret-grafic")) {
        copy_text(output, output_size, "returning to graphics");
        return SHELL_RETURN_GRAPHIC;
    } else if (equals(line, "ls")) {
        if (fs_list(current_directory, output, output_size) < 0)
            copy_text(output, output_size, "directory unavailable");
    } else if (starts_with(line, "cd ")) {
        char path[33];
        resolve_path(path, sizeof(path), line + 3);
        if (fs_is_directory(path)) {
            copy_text(previous_directory, sizeof(previous_directory), current_directory);
            copy_text(current_directory, sizeof(current_directory), path);
            copy_text(output, output_size, "directory changed");
        } else copy_text(output, output_size, "directory not found");
    } else if (equals(line, "rp")) {
        char path[33];
        copy_text(path, sizeof(path), current_directory);
        copy_text(current_directory, sizeof(current_directory), previous_directory);
        copy_text(previous_directory, sizeof(previous_directory), path);
        copy_text(output, output_size, "returned to previous directory");
    } else if (starts_with(line, "mkdir ")) {
        char path[33];
        resolve_path(path, sizeof(path), line + 6);
        copy_text(output, output_size,
            fs_mkdir(path) == 0 ? "directory created" : "mkdir failed");
    } else if (starts_with(line, "show ") || starts_with(line, "cat ")) {
        unsigned int offset = line[1] == 'h' ? 5 : 4;
        long length = fs_read(line + offset, buffer, sizeof(buffer) - 1);
        if (length < 0) copy_text(output, output_size, "file not found");
        else { buffer[length] = '\0'; copy_text(output, output_size, buffer); }
    } else if (starts_with(line, "calc ")) {
        if (calculator_evaluate(line + 5, result, sizeof(result)))
            copy_text(output, output_size, result);
        else copy_text(output, output_size, "invalid expression");
    } else copy_text(output, output_size, "unknown command: help");
    return 0;
}

int shell_handle_key(int value, char *line, unsigned int line_size,
    unsigned int *length, char *output, unsigned int output_size)
{
    if (value == '\b') {
        if (*length != 0) --*length;
    } else if (value == '\n') {
        line[*length] = '\0';
        int mode = run_command(line, output, output_size);
        *length = 0;
        if (line_size != 0) line[0] = '\0';
        return mode;
    } else if (value >= 32 && value <= 126 && *length + 1 < line_size) {
        line[(*length)++] = (char)value;
        line[*length] = '\0';
    }
    return -1;
}