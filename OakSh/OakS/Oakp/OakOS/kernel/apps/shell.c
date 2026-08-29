#include "acorn/apps/shell.h"
#include "acorn/apps/calculator.h"
#include "acorn/egna.h"
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

static int ends_with(const char *text, const char *suffix)
{
    unsigned int text_length = 0;
    unsigned int suffix_length = 0;
    while (text[text_length] != '\0') ++text_length;
    while (suffix[suffix_length] != '\0') ++suffix_length;
    if (suffix_length > text_length) return 0;
    for (unsigned int index = 0; index < suffix_length; ++index) {
        if (text[text_length - suffix_length + index] != suffix[index]) return 0;
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

static int run_egna_package(const char *path, int show_in_gui,
    char *output, unsigned int output_size)
{
    char full_path[64];
    char package_name[32];
    char shell_command[128];
    struct egna_package package;
    resolve_path(full_path, sizeof(full_path), path);
    if (!fs_exists(full_path)) {
        copy_text(output, output_size, "EGNA: file not found");
        return 0;
    }
    if (!egna_load(full_path, &package)) {
        copy_text(output, output_size, "EGNA: invalid package");
        return 0;
    }
    if (package.name[0] == '\0') {
        if (!egna_get_name(full_path, package_name, sizeof(package_name)))
            copy_text(package_name, sizeof(package_name), "unknown");
    }
    if ((package.flags & EGNA_FLAG_GUI_READY) != 0 && show_in_gui) {
        copy_text(requested_app, sizeof(requested_app), package.name[0] != '\0' ?
            package.name : path);
        copy_text(output, output_size, "EGNA: package queued for GUI");
        return SHELL_SHOW_GUI;
    }
    if ((package.flags & EGNA_FLAG_HAS_SHELL) != 0) {
        egna_get_shell_command(full_path, shell_command, sizeof(shell_command));
        copy_text(output, output_size, shell_command[0] != '\0' ? shell_command :
            "EGNA: shell command loaded");
        return SHELL_RUN_APP;
    }
    copy_text(output, output_size, "EGNA: shell-only package");
    return 0;
}

static int run_command(const char *line, char *output, unsigned int output_size)
{
    char result[24];
    char buffer[64];
    if (starts_with(line, "run ") || starts_with(line, "show-gui ")) {
        int show_in_gui = starts_with(line, "show-gui ");
        const char *name = line + (show_in_gui ? 9 : 4);
        if (name[0] != '\0' && name[0] != ' ' && name[0] != '\t') {
            unsigned int length = 0;
            while (name[length] != '\0' && name[length] != ' ' && name[length] != '\t' &&
                length + 1 < 64) ++length;
            char app_name[64];
            if (length >= sizeof(app_name)) length = sizeof(app_name) - 1;
            for (unsigned int index = 0; index < length; ++index) app_name[index] = name[index];
            app_name[length] = '\0';
            if (ends_with(app_name, ".egna") || starts_with(app_name, "./") ||
                starts_with(app_name, "/")) {
                return run_egna_package(app_name, show_in_gui, output, output_size);
            }
        }
        if (equals(name, "snake") || equals(name, "snake.elf") ||
            equals(name, "minesweeper") || equals(name, "minesweeper.elf") ||
            equals(name, "tetris") || equals(name, "tetris.elf") ||
            equals(name, "pong") || equals(name, "pong.elf")) {
            if (starts_with(name, "mine")) {
                copy_text(requested_app, sizeof(requested_app), "minesweeper");
                copy_text(output, output_size, show_in_gui ?
                    "minesweeper added to GUI" : "starting minesweeper");
            } else if (starts_with(name, "tetris")) {
                copy_text(requested_app, sizeof(requested_app), "tetris");
                copy_text(output, output_size, show_in_gui ?
                    "tetris added to GUI" : "starting tetris");
            } else if (starts_with(name, "pong")) {
                copy_text(requested_app, sizeof(requested_app), "pong");
                copy_text(output, output_size, show_in_gui ?
                    "pong added to GUI" : "starting pong");
            } else {
                copy_text(requested_app, sizeof(requested_app), "snake");
                copy_text(output, output_size, show_in_gui ?
                    "snake added to GUI" : "starting snake");
            }
            return show_in_gui ? SHELL_SHOW_GUI : SHELL_RUN_APP;
        }
        copy_text(output, output_size, "run: application unavailable");
    } else if (equals(line, "help")) {
        copy_text(output, output_size,
            "help run(app.egna/snake/minesweeper/tetris/pong) show-gui(app.egna/app) curl -o /file.egna http://host download-egna -o /file.egna http://host cd ls show mkdir calc net dns tcp http curl browse ret-grafic");
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
                    if (download_http_file(file_path, host)) {
                        if (ends_with(file_path, ".egna")) {
                            char app_name[32];
                            unsigned int slash = 0;
                            while (file_path[slash] != '\0') ++slash;
                            while (slash > 0 && file_path[slash - 1] != '/') --slash;
                            copy_text(app_name, sizeof(app_name), file_path + slash);
                            copy_text(requested_app, sizeof(requested_app), app_name);
                            copy_text(output, output_size, "curl: EGNA saved and added to desktop");
                            return SHELL_SHOW_GUI;
                        }
                        copy_text(output, output_size, "curl: file saved");
                    } else {
                        copy_text(output, output_size, "curl: download failed");
                    }
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
    } else if (starts_with(line, "download-egna ") || starts_with(line, "install-egna ")) {
        const char *spec = line + (starts_with(line, "download-egna ") ? 15 : 14);
        char file_path[64];
        char host[64];
        unsigned int host_length = 0;
        if (starts_with(spec, "-o ")) {
            const char *path = spec + 3;
            unsigned int path_length = 0;
            while (path[path_length] != ' ' && path[path_length] != '\0') ++path_length;
            if (path[path_length] == ' ') {
                const char *url = path + path_length + 1;
                for (unsigned int index = 0; index < path_length && index + 1 < sizeof(file_path); ++index)
                    file_path[index] = path[index];
                file_path[path_length] = '\0';
                if (starts_with(url, "https://")) {
                    copy_text(output, output_size, "download-egna: https unavailable");
                } else {
                    if (starts_with(url, "http://")) url += 7;
                    while (url[host_length] != '\0' && host_length + 1 < sizeof(host) &&
                        url[host_length] != '/' && url[host_length] != ' ') {
                        host[host_length] = url[host_length];
                        ++host_length;
                    }
                    host[host_length] = '\0';
                    if (download_http_file(file_path, host)) {
                        char app_name[32];
                        unsigned int slash = 0;
                        while (file_path[slash] != '\0') ++slash;
                        while (slash > 0 && file_path[slash - 1] != '/') --slash;
                        copy_text(app_name, sizeof(app_name), file_path + slash);
                        copy_text(requested_app, sizeof(requested_app), app_name);
                        copy_text(output, output_size, "EGNA downloaded and added to desktop");
                        return SHELL_SHOW_GUI;
                    }
                    copy_text(output, output_size, "download-egna: failed");
                }
            } else copy_text(output, output_size, "download-egna: use -o /path file.egna http://host");
        } else {
            const char *url = spec;
            char target_name[32];
            unsigned int target_length = 0;
            while (url[target_length] != ' ' && url[target_length] != '\0') ++target_length;
            if (url[target_length] == ' ') {
                for (unsigned int index = 0; index < target_length && index + 1 < sizeof(target_name); ++index)
                    target_name[index] = url[index];
                target_name[target_length] = '\0';
                copy_text(file_path, sizeof(file_path), "/");
                if (target_length > 0) {
                    copy_text(file_path + 1, sizeof(file_path) - 1, target_name);
                }
                if (download_http_file(file_path, target_name)) {
                    char app_name[32];
                    unsigned int slash = 0;
                    while (file_path[slash] != '\0') ++slash;
                    while (slash > 0 && file_path[slash - 1] != '/') --slash;
                    copy_text(app_name, sizeof(app_name), file_path + slash);
                    copy_text(requested_app, sizeof(requested_app), app_name);
                    copy_text(output, output_size, "EGNA downloaded and added to desktop");
                    return SHELL_SHOW_GUI;
                }
                copy_text(output, output_size, "download-egna: failed");
            } else copy_text(output, output_size, "download-egna: needs URL and target");
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