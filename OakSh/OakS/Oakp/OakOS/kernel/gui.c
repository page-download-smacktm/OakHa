#include "acorn/gui.h"
#include "acorn/dillo_platform.h"
#include "acorn/framebuffer.h"
#include "acorn/assets.h"
#include "acorn/mouse.h"
#include "acorn/serial.h"
#include "acorn/apps/files.h"
#include "acorn/apps/snake.h"
#include "acorn/apps/minesweeper.h"
#include "acorn/apps/tetris.h"
#include "acorn/apps/pong.h"
#include "acorn/apps/shell.h"
#include "acorn/vga.h"

#define COLOR_BG 0x20342E
#define COLOR_BAR 0x126B45
#define COLOR_PANEL 0x2F6B52
#define COLOR_WINDOW 0x315A59
#define COLOR_TITLE 0x328B78
#define COLOR_TEXT 0xF2FFF0
#define COLOR_CURSOR 0xF4D35E

static unsigned int terminal_x;
static char terminal_line[80];
static unsigned int terminal_length;
static unsigned long last_mouse_packets;
static unsigned long gui_ticks;
static char terminal_output[80];
static unsigned int terminal_output_length;
static unsigned int gui_mode;
static int shutdown_in_progress;
static unsigned long shutdown_started_tick;
static unsigned int cursor_saved[18][17];
static unsigned int cursor_drawn_x;
static unsigned int cursor_drawn_y;
static int cursor_visible;
static unsigned char previous_mouse_buttons;
static char raw_history[18][80];
static unsigned int raw_history_length;
static unsigned int gui_apps;
static unsigned int gui_open_tabs;
static int gui_window_maximized;
static int gui_window_minimized;
static unsigned int gui_current_app;
static char gui_egna_title[32];
static char gui_egna_path[64];

enum { GUI_DESKTOP, GUI_FILES, GUI_SNAKE, GUI_TERMINAL, GUI_FULLSCREEN_SNAKE,
    GUI_FULLSCREEN_MINESWEEPER, GUI_FULLSCREEN_TETRIS, GUI_FULLSCREEN_PONG };
enum { KEY_F1 = 0x80, KEY_F2, KEY_F3 };
enum { APP_SNAKE = 1, APP_MINESWEEPER = 2, APP_TETRIS = 4, APP_PONG = 8, APP_EGNA = 16 };

static void draw_cursor(void);
static void draw_desktop(void);
static void draw_window_chrome(const char *title);
static void draw_active_cursor(void);
static void draw_taskbar(void);
static void enter_fullscreen_snake(void);
static void enter_fullscreen_minesweeper(void);
static void enter_fullscreen_tetris(void);
static void enter_fullscreen_pong(void);

static unsigned int app_bit(const char *name)
{
    if (name == (const char *)0 || name[0] == '\0') return 0;
    if (name[0] == 's') return APP_SNAKE;
    if (name[0] == 'm') return APP_MINESWEEPER;
    if (name[0] == 't') return APP_TETRIS;
    if (name[0] == 'e' || name[0] == 'E') return APP_EGNA;
    return APP_PONG;
}

static void register_egna_app(const char *name, const char *path)
{
    unsigned int index = 0;
    if (name != (const char *)0) {
        while (name[index] != '\0' && index + 1 < sizeof(gui_egna_title)) ++index;
        for (unsigned int character = 0; character < index; ++character)
            gui_egna_title[character] = name[character];
        gui_egna_title[index] = '\0';
    }
    if (path != (const char *)0) {
        unsigned int length = 0;
        while (path[length] != '\0' && length + 1 < sizeof(gui_egna_path)) ++length;
        for (unsigned int character = 0; character < length; ++character)
            gui_egna_path[character] = path[character];
        gui_egna_path[length] = '\0';
    }
    gui_apps |= APP_EGNA;
}

static void add_gui_app(const char *name)
{
    if (name == (const char *)0 || name[0] == '\0') return;
    if (name[0] == 'e' || name[0] == 'E' || name[0] == '/' ||
        (name[0] != 's' && name[0] != 'm' && name[0] != 't' &&
        name[0] != 'p' && name[0] != 'P')) {
        register_egna_app(name, name);
        return;
    }
    gui_apps |= app_bit(name);
}

static void launch_gui_app(const char *name)
{
    unsigned int bit = app_bit(name);
    add_gui_app(name);
    gui_current_app = bit;
    gui_open_tabs |= gui_current_app;
    gui_window_minimized = 0;
    if (bit == APP_EGNA) {
        register_egna_app(name, name);
        gui_mode = GUI_DESKTOP;
        draw_desktop();
        return;
    }
    if (name[0] == 's') enter_fullscreen_snake();
    else if (name[0] == 'm') enter_fullscreen_minesweeper();
    else if (name[0] == 't') enter_fullscreen_tetris();
    else enter_fullscreen_pong();
}

static void open_gui_app(const char *name)
{
    unsigned int bit = app_bit(name);
    gui_current_app = bit;
    gui_open_tabs |= gui_current_app;
    gui_window_minimized = 0;
    if (bit == APP_EGNA) {
        register_egna_app(name, name);
        gui_mode = GUI_DESKTOP;
        draw_desktop();
        return;
    }
    if (name[0] == 's') {
        enter_fullscreen_snake();
        return;
    }
    launch_gui_app(name);
}

static void restore_gui_app(void)
{
    gui_window_minimized = 0;
    if (gui_current_app == APP_SNAKE) {
        gui_mode = GUI_FULLSCREEN_SNAKE;
        snake_app_draw_fullscreen();
        draw_window_chrome("SNAKE");
        return;
    }
    if (gui_current_app == APP_MINESWEEPER) {
        gui_mode = GUI_FULLSCREEN_MINESWEEPER;
        minesweeper_app_draw_fullscreen();
        draw_window_chrome("MINESWEEPER");
        draw_active_cursor();
        return;
    }
    if (gui_current_app == APP_TETRIS) {
        gui_mode = GUI_FULLSCREEN_TETRIS;
        tetris_app_draw_fullscreen();
        draw_window_chrome("TETRIS");
        return;
    }
    if (gui_current_app == APP_PONG) {
        gui_mode = GUI_FULLSCREEN_PONG;
        pong_app_draw_fullscreen();
        draw_window_chrome("PONG");
        return;
    }
    gui_mode = GUI_DESKTOP;
    draw_desktop();
}

static void draw_active_cursor(void)
{
    cursor_visible = 0;
    draw_cursor();
}

static void draw_window_chrome(const char *title)
{
    unsigned int width = framebuffer_width();
    unsigned int button_y = 6;
    framebuffer_fill_rect(0, 0, width, 32, COLOR_BAR);
    framebuffer_draw_text(14, 10, title, COLOR_TEXT, 1);
    framebuffer_fill_rect(width - 48, button_y, 20, 20, COLOR_TITLE);
    framebuffer_fill_rect(width - 24, button_y, 20, 20, 0xB84A4A);
    framebuffer_draw_text(width - 42, 12, "_", COLOR_TEXT, 1);
    framebuffer_draw_text(width - 18, 11, "X", COLOR_TEXT, 1);
    draw_taskbar();
}

static void draw_taskbar(void)
{
    unsigned int y = framebuffer_height() - 30;
    unsigned int x = 318;
    framebuffer_fill_rect(0, y, framebuffer_width(), 30, COLOR_BAR);
    framebuffer_draw_text(18, y + 10, "OAK", COLOR_TEXT, 1);
    if (gui_open_tabs & APP_SNAKE) {
        unsigned int width = 74;
        framebuffer_fill_rect(x, y + 4, width, 22,
            gui_current_app == APP_SNAKE ? COLOR_TITLE : 0x234B3D);
        framebuffer_draw_text(x + 10, y + 11, "SNAKE", COLOR_TEXT, 1);
        x += width + 4;
    }
    if (gui_open_tabs & APP_MINESWEEPER) {
        unsigned int width = 112;
        framebuffer_fill_rect(x, y + 4, width, 22,
            gui_current_app == APP_MINESWEEPER ? COLOR_TITLE : 0x234B3D);
        framebuffer_draw_text(x + 10, y + 11, "MINESWEEPER", COLOR_TEXT, 1);
        x += width + 4;
    }
    if (gui_open_tabs & APP_TETRIS) {
        unsigned int width = 74;
        framebuffer_fill_rect(x, y + 4, width, 22,
            gui_current_app == APP_TETRIS ? COLOR_TITLE : 0x234B3D);
        framebuffer_draw_text(x + 10, y + 11, "TETRIS", COLOR_TEXT, 1);
        x += width + 4;
    }
    if (gui_open_tabs & APP_PONG) {
        unsigned int width = 66;
        framebuffer_fill_rect(x, y + 4, width, 22,
            gui_current_app == APP_PONG ? COLOR_TITLE : 0x234B3D);
        framebuffer_draw_text(x + 10, y + 11, "PONG", COLOR_TEXT, 1);
    }
}

static const char cursor_shape[18][18] = {
    "    B            ", "    BB           ", "    BWB          ",
    "    BWWB         ", "    BWWWB        ", "    BWWWWB       ",
    "    BWWWWWB      ", "    BWWWWWWB     ", "    BWWWWWWWB    ",
    "    BWWWWWWWWB   ", "    BWWWWWWWWWB  ", "    BWWWWWWWWWWB ",
    "    BWWWWWWBBB   ", "    BWWWBB       ", "    BWWB         ",
    "    BB           ", "                 ", "                 "
};

static void reset_apps(void)
{
    shell_init(terminal_line, sizeof(terminal_line), terminal_output,
        sizeof(terminal_output), &terminal_length);
    terminal_x = 0;
    snake_app_init();
}

static void enter_terminal(void)
{
    gui_mode = GUI_TERMINAL;
    reset_apps();
    vga_init();
    raw_history_length = 0;
    vga_write("OakOS raw terminal\nType help for commands.\n\n> ");
    framebuffer_clear(0x050505);
    framebuffer_draw_text(24, 24, "OakOS raw terminal", COLOR_TEXT, 2);
    framebuffer_draw_text(24, 52, "Type help for commands. ret-grafic returns to GUI.", COLOR_TEXT, 1);
    framebuffer_draw_text(24, 84, "> ", COLOR_CURSOR, 1);
}

static void enter_fullscreen_snake(void)
{
    gui_mode = GUI_FULLSCREEN_SNAKE;
    gui_current_app = APP_SNAKE;
    gui_window_minimized = 0;
    snake_app_init();
    dillo_platform_clear_events();
    cursor_visible = 0;
    snake_app_draw_fullscreen();
    draw_window_chrome("SNAKE");
    draw_taskbar();
    draw_active_cursor();
}

static void enter_fullscreen_minesweeper(void)
{
    gui_mode = GUI_FULLSCREEN_MINESWEEPER;
    gui_current_app = APP_MINESWEEPER;
    gui_window_minimized = 0;
    minesweeper_app_init();
    dillo_platform_clear_events();
    cursor_visible = 0;
    previous_mouse_buttons = 0;
    minesweeper_app_draw_fullscreen();
    draw_window_chrome("MINESWEEPER");
    draw_taskbar();
    draw_active_cursor();
}

static void enter_fullscreen_tetris(void)
{
    gui_mode = GUI_FULLSCREEN_TETRIS;
    gui_current_app = APP_TETRIS;
    gui_window_minimized = 0;
    tetris_app_init();
    dillo_platform_clear_events();
    cursor_visible = 0;
    tetris_app_draw_fullscreen();
    draw_window_chrome("TETRIS");
    draw_taskbar();
    draw_active_cursor();
}

static void enter_fullscreen_pong(void)
{
    gui_mode = GUI_FULLSCREEN_PONG;
    gui_current_app = APP_PONG;
    gui_window_minimized = 0;
    pong_app_init();
    dillo_platform_clear_events();
    cursor_visible = 0;
    pong_app_draw_fullscreen();
    draw_window_chrome("PONG");
    draw_taskbar();
    draw_active_cursor();
}

static void raw_history_add(const char *text)
{
    unsigned int index = 0;
    if (text[0] == '\0') return;
    if (raw_history_length == 18) {
        for (unsigned int row = 1; row < 18; ++row)
            for (unsigned int column = 0; column < 80; ++column)
                raw_history[row - 1][column] = raw_history[row][column];
        --raw_history_length;
    }
    while (text[index] != '\0' && index + 1 < 80) {
        raw_history[raw_history_length][index] = text[index];
        ++index;
    }
    raw_history[raw_history_length][index] = '\0';
    ++raw_history_length;
}

static void draw_raw_terminal(void)
{
    unsigned int y = 24;
    framebuffer_clear(0x050505);
    framebuffer_draw_text(24, y, "OakOS raw terminal", COLOR_TEXT, 2);
    y += 28;
    framebuffer_draw_text(24, y, "Type help. ret-grafic returns to GUI.", COLOR_TEXT, 1);
    y += 18;
    for (unsigned int row = 0; row < raw_history_length; ++row) {
        framebuffer_draw_text(24, y, raw_history[row], COLOR_TEXT, 1);
        y += 10;
    }
    const char *directory = shell_current_directory();
    unsigned int directory_length = 0;
    while (directory[directory_length] != '\0') ++directory_length;
    framebuffer_draw_text(24, y, "> ", COLOR_CURSOR, 1);
    framebuffer_draw_text(36, y, directory, COLOR_CURSOR, 1);
    framebuffer_draw_text(42 + directory_length * 6, y, " $ ", COLOR_CURSOR, 1);
    framebuffer_draw_text(66 + directory_length * 6, y, terminal_line, COLOR_TEXT, 1);
}

static void text_copy(char *destination, const char *source)
{
    unsigned int index = 0;
    while (source[index] != '\0' && index + 1 < sizeof(terminal_output)) {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
    terminal_output_length = index;
}
static void draw_cursor(void)
{
    unsigned int x = (unsigned int)mouse_x();
    unsigned int y = (unsigned int)mouse_y();
    if (cursor_visible) {
        for (unsigned int row = 0; row < 18; ++row)
            for (unsigned int column = 0; column < 17; ++column)
                if (cursor_shape[row][column] != ' ')
                    framebuffer_fill_rect(cursor_drawn_x + column,
                        cursor_drawn_y + row, 1, 1, cursor_saved[row][column]);
    }
    for (unsigned int row = 0; row < 18; ++row) {
        for (unsigned int column = 0; column < 17; ++column) {
            unsigned int color;
            if (cursor_shape[row][column] == 'B') color = 0x050505;
            else if (cursor_shape[row][column] == 'W') color = 0xF7F7F2;
            else continue;
            cursor_saved[row][column] = framebuffer_read_pixel(x + column, y + row);
            framebuffer_fill_rect(x + column, y + row, 1, 1, color);
        }
    }
    cursor_drawn_x = x;
    cursor_drawn_y = y;
    cursor_visible = 1;
}

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(unsigned short port, unsigned short value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void)
{
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

struct acpi_sdt_header {
    char signature[4];
    unsigned int length;
    unsigned char revision;
    unsigned char checksum;
    char oem_id[6];
    char oem_table_id[8];
    unsigned int oem_revision;
    unsigned int creator_id;
    unsigned int creator_revision;
} __attribute__((packed));

static unsigned char acpi_checksum(const void *memory, unsigned int length)
{
    const unsigned char *bytes = (const unsigned char *)memory;
    unsigned int sum = 0;
    for (unsigned int index = 0; index < length; ++index)
        sum += bytes[index];
    return (unsigned char)(sum & 0xFF);
}

static unsigned long acpi_find_rsdp(void)
{
    for (unsigned long address = 0xE0000; address < 0x100000; address += 16) {
        const unsigned char *region = (const unsigned char *)address;
        if (region[0] != 'R' || region[1] != 'S' || region[2] != 'D' ||
            region[3] != ' ' || region[4] != 'P' || region[5] != 'T' ||
            region[6] != 'R' || region[7] != ' ') {
            continue;
        }
        if (acpi_checksum(region, 20) == 0) return address;
        if (address + 36 <= 0x100000) {
            unsigned int length = (unsigned int)(region[10] | (region[11] << 8) |
                (region[12] << 16) | (region[13] << 24));
            if (length >= 36 && acpi_checksum(region, length) == 0) return address;
        }
    }
    return 0;
}

static unsigned long long acpi_rsdp_table_address(unsigned long rsdp_address)
{
    if (rsdp_address == 0) return 0;
    const unsigned char *rsdp = (const unsigned char *)rsdp_address;
    if (rsdp[15] >= 2) {
        unsigned long long value = 0;
        value |= (unsigned long long)rsdp[24];
        value |= (unsigned long long)rsdp[25] << 8;
        value |= (unsigned long long)rsdp[26] << 16;
        value |= (unsigned long long)rsdp[27] << 24;
        value |= (unsigned long long)rsdp[28] << 32;
        value |= (unsigned long long)rsdp[29] << 40;
        value |= (unsigned long long)rsdp[30] << 48;
        value |= (unsigned long long)rsdp[31] << 56;
        return value;
    }
    return (unsigned long long)(rsdp[16] | (rsdp[17] << 8) |
        (rsdp[18] << 16) | (rsdp[19] << 24));
}

static unsigned int acpi_sdt_length(unsigned long long table_address)
{
    if (table_address == 0) return 0;
    const struct acpi_sdt_header *table = (const struct acpi_sdt_header *)table_address;
    return table->length;
}

static int acpi_table_matches(unsigned long long table_address, const char *signature)
{
    if (table_address == 0) return 0;
    const struct acpi_sdt_header *table = (const struct acpi_sdt_header *)table_address;
    return table->signature[0] == signature[0] &&
        table->signature[1] == signature[1] &&
        table->signature[2] == signature[2] &&
        table->signature[3] == signature[3];
}

static void acpi_find_fadt_table(unsigned long long table_address,
    unsigned int entry_size, unsigned int *out_entry_count,
    unsigned long long *out_fadt_address)
{
    *out_entry_count = 0;
    *out_fadt_address = 0;
    if (table_address == 0) return;
    unsigned int length = acpi_sdt_length(table_address);
    if (length < sizeof(struct acpi_sdt_header)) return;
    unsigned int table_count = (length - sizeof(struct acpi_sdt_header)) / entry_size;
    const unsigned char *table = (const unsigned char *)table_address;
    for (unsigned int index = 0; index < table_count; ++index) {
        unsigned long long entry_address = 0;
        const unsigned char *entry_ptr = table + sizeof(struct acpi_sdt_header) + index * entry_size;
        if (entry_size == 8) {
            entry_address = (unsigned long long)
                (entry_ptr[0] | (entry_ptr[1] << 8) | (entry_ptr[2] << 16) | (entry_ptr[3] << 24) |
                 ((unsigned long long)entry_ptr[4] << 32) | ((unsigned long long)entry_ptr[5] << 40) |
                 ((unsigned long long)entry_ptr[6] << 48) | ((unsigned long long)entry_ptr[7] << 56));
        } else {
            entry_address = (unsigned long long)(
                entry_ptr[0] | (entry_ptr[1] << 8) | (entry_ptr[2] << 16) | (entry_ptr[3] << 24));
        }
        if (entry_address == 0) continue;
        if (acpi_table_matches(entry_address, "FACP")) {
            *out_fadt_address = entry_address;
            *out_entry_count = table_count;
            return;
        }
    }
}

static unsigned short acpi_fadt_pm1a_cnt_block(unsigned long long fadt_address)
{
    if (fadt_address == 0) return 0;
    const unsigned char *fadt = (const unsigned char *)fadt_address;
    if (acpi_sdt_length(fadt_address) < 0x40) return 0;
    return (unsigned short)(fadt[0x32] | (fadt[0x33] << 8));
}

static unsigned short acpi_fadt_pm1b_cnt_block(unsigned long long fadt_address)
{
    if (fadt_address == 0) return 0;
    const unsigned char *fadt = (const unsigned char *)fadt_address;
    if (acpi_sdt_length(fadt_address) < 0x40) return 0;
    return (unsigned short)(fadt[0x34] | (fadt[0x35] << 8));
}

static unsigned long long acpi_fadt_dsdt_address(unsigned long long fadt_address)
{
    if (fadt_address == 0) return 0;
    const unsigned char *fadt = (const unsigned char *)fadt_address;
    unsigned int length = acpi_sdt_length(fadt_address);
    if (length < 0x2C) return 0;
    unsigned long long dsdt = (unsigned long long)(
        fadt[0x28] | (fadt[0x29] << 8) | (fadt[0x2A] << 16) | (fadt[0x2B] << 24));
    if (length >= 0x50) {
        unsigned long long x_dsdt = (unsigned long long)
            (fadt[0x50] | (fadt[0x51] << 8) | (fadt[0x52] << 16) | (fadt[0x53] << 24) |
             ((unsigned long long)fadt[0x54] << 32) | ((unsigned long long)fadt[0x55] << 40) |
             ((unsigned long long)fadt[0x56] << 48) | ((unsigned long long)fadt[0x57] << 56));
        if (x_dsdt != 0) dsdt = x_dsdt;
    }
    return dsdt;
}

static int acpi_parse_s5_slp_typ(unsigned long long dsdt_address, unsigned short *out_slp_typ)
{
    if (dsdt_address == 0 || out_slp_typ == (unsigned short *)0) return 0;
    *out_slp_typ = 0;

    const unsigned char *dsdt = (const unsigned char *)dsdt_address;
    unsigned int length = acpi_sdt_length(dsdt_address);
    if (length < 4) return 0;

    for (unsigned int index = 0; index + 5 < length; ++index) {
        if (dsdt[index] != '_' || dsdt[index + 1] != 'S' || dsdt[index + 2] != '5')
            continue;
        if (dsdt[index + 3] != 0 && dsdt[index + 3] != '\n' && dsdt[index + 3] != '\r')
            continue;

        unsigned int scan = index + 4;
        while (scan + 2 < length) {
            if (dsdt[scan] == 0x0A || dsdt[scan] == 0x0B || dsdt[scan] == 0x0C) {
                unsigned short candidate = (unsigned short)(dsdt[scan + 1] | (dsdt[scan + 2] << 8));
                if (candidate != 0) {
                    *out_slp_typ = candidate;
                    return 1;
                }
            }
            ++scan;
        }
        break;
    }
    return 0;
}

static unsigned short acpi_make_s5_value(unsigned short slp_typ)
{
    const unsigned short slp_en = 1u << 13;
    return (unsigned short)((slp_typ << 10) | slp_en);
}

static int acpi_shutdown_s5(void)
{
    unsigned long rsdp_address = acpi_find_rsdp();
    if (rsdp_address == 0) {
        serial_write("shutdown: no ACPI RSDP found\n");
        return 0;
    }

    unsigned long long rsdt_or_xsdt = acpi_rsdp_table_address(rsdp_address);
    if (rsdt_or_xsdt == 0) {
        serial_write("shutdown: ACPI root table missing\n");
        return 0;
    }

    unsigned int entry_size = 8;
    unsigned int table_length = acpi_sdt_length(rsdt_or_xsdt);
    if (table_length >= sizeof(struct acpi_sdt_header) &&
        acpi_table_matches(rsdt_or_xsdt, "RSDT")) {
        entry_size = 4;
    } else if (acpi_table_matches(rsdt_or_xsdt, "XSDT")) {
        entry_size = 8;
    }

    unsigned long long fadt_address = 0;
    unsigned int ignored_count = 0;
    acpi_find_fadt_table(rsdt_or_xsdt, entry_size, &ignored_count, &fadt_address);
    if (fadt_address == 0) {
        serial_write("shutdown: no ACPI FADT found\n");
        return 0;
    }

    unsigned short pm1a_cnt_block = acpi_fadt_pm1a_cnt_block(fadt_address);
    unsigned short pm1b_cnt_block = acpi_fadt_pm1b_cnt_block(fadt_address);
    if (pm1a_cnt_block == 0 && pm1b_cnt_block == 0) {
        serial_write("shutdown: ACPI PM1_CNT block missing\n");
        return 0;
    }

    unsigned long long dsdt_address = acpi_fadt_dsdt_address(fadt_address);
    unsigned short slp_typ = 0;
    if (!acpi_parse_s5_slp_typ(dsdt_address, &slp_typ)) {
        serial_write("shutdown: no valid _S5 DSDT profile\n");
        return 0;
    }

    unsigned short sleep_value = acpi_make_s5_value(slp_typ);
    serial_write("shutdown: issuing ACPI S5 request\n");
    __asm__ volatile ("cli");
    if (pm1a_cnt_block != 0) {
        outw((unsigned short)pm1a_cnt_block, sleep_value);
        io_wait();
    }
    if (pm1b_cnt_block != 0) {
        outw((unsigned short)pm1b_cnt_block, sleep_value);
        io_wait();
    }
    __asm__ volatile ("sti");
    return 1;
}

static int power_off_system(void)
{
    if (shutdown_in_progress) return 1;
    if (acpi_shutdown_s5()) {
        shutdown_in_progress = 1;
        shutdown_started_tick = gui_ticks;
        serial_write("shutdown: request sent\n");
        return 1;
    }

    shutdown_in_progress = 0;
    serial_write("shutdown: request failed\n");
    return 0;
}

static void draw_desktop(void)
{
    unsigned int screen_width = framebuffer_width();
    unsigned int screen_height = framebuffer_height();
    cursor_visible = 0;
    framebuffer_clear(COLOR_BG);
    framebuffer_draw_image(0, 56, screen_width, screen_height - 56,
        652, 432, oakos_background_pixels, oakos_background_alpha);
    framebuffer_fill_rect(0, 0, screen_width, 56, COLOR_BAR);
    framebuffer_draw_text(24, 18, "OAKOS DESKTOP", COLOR_TEXT, 2);
    framebuffer_fill_rect(screen_width - 120, 16, 88, 24, 0x4CC9A4);
    framebuffer_draw_text(screen_width - 112, 24, "DESLIGAR", COLOR_BG, 1);
    framebuffer_fill_rect(26, 78, 270, screen_height - 110, COLOR_PANEL);
    framebuffer_fill_rect(26, 78, 270, 32, COLOR_TITLE);
    framebuffer_draw_text(40, 88, "WORKSPACE", COLOR_TEXT, 1);
    framebuffer_draw_text(48, 140, "FILES", COLOR_TEXT, 2);
    framebuffer_draw_text(48, 174, "TERMINAL", COLOR_TEXT, 2);
    framebuffer_draw_text(48, 208, "SETTINGS", COLOR_TEXT, 2);
    framebuffer_fill_rect(42, 246, 56, 42, COLOR_TITLE);
    framebuffer_draw_image(42, 246, 56, 42, 56, 42,
        oakos_shell_icon_pixels, oakos_shell_icon_alpha);
    framebuffer_draw_text(40, 300, "APPS", COLOR_CURSOR, 1);
    if (gui_apps & APP_SNAKE) {
        framebuffer_draw_image(42, 320, 56, 42, 56, 42,
            oakos_snake_icon_pixels, oakos_snake_icon_alpha);
        framebuffer_draw_text(42, 370, "SNAKE", COLOR_TEXT, 1);
    }
    if (gui_apps & APP_MINESWEEPER) {
        framebuffer_draw_image(112, 320, 56, 42, 56, 42,
            oakos_minesweeper_icon_pixels, oakos_minesweeper_icon_alpha);
        framebuffer_draw_text(112, 370, "MINES", COLOR_TEXT, 1);
    }
    if (gui_apps & APP_TETRIS) {
        framebuffer_draw_image(182, 320, 56, 42, 56, 42,
            oakos_tetris_icon_pixels, oakos_tetris_icon_alpha);
        framebuffer_draw_text(182, 370, "TETRIS", COLOR_TEXT, 1);
    }
    if (gui_apps & APP_PONG) {
        framebuffer_draw_image(42, 398, 56, 42, 56, 42,
            oakos_pong_icon_pixels, oakos_pong_icon_alpha);
        framebuffer_draw_text(42, 448, "PONG", COLOR_TEXT, 1);
    }
    if (gui_apps & APP_EGNA) {
        framebuffer_fill_rect(182, 398, 56, 42, COLOR_TITLE);
        framebuffer_draw_text(188, 448, gui_egna_title[0] != '\0' ? gui_egna_title : "EGNA", COLOR_TEXT, 1);
    }
    if (gui_mode != GUI_DESKTOP)
        framebuffer_fill_rect(318, 78, 410, 76, COLOR_TITLE);
    if (gui_mode == GUI_FILES) files_app_draw();
    if (gui_mode == GUI_SNAKE && !gui_window_minimized) {
        snake_app_draw();
        framebuffer_fill_rect(618, 184, 24, 20, COLOR_TITLE);
        framebuffer_fill_rect(650, 184, 24, 20, COLOR_TITLE);
        framebuffer_fill_rect(682, 184, 24, 20, 0xB84A4A);
        framebuffer_draw_text(626, 190, "_", COLOR_TEXT, 1);
        framebuffer_draw_text(658, 189, "[]", COLOR_TEXT, 1);
        framebuffer_draw_text(690, 189, "X", COLOR_TEXT, 1);
    }
    draw_taskbar();
    draw_cursor();
}

void gui_init(void)
{
    terminal_x = 0;
    terminal_length = 0;
    terminal_line[0] = '\0';
    shell_init(terminal_line, sizeof(terminal_line), terminal_output,
        sizeof(terminal_output), &terminal_length);
    gui_mode = GUI_DESKTOP;
    gui_apps = 0;
    gui_open_tabs = 0;
    gui_window_maximized = 0;
    gui_window_minimized = 0;
    gui_current_app = 0;
    snake_app_init();
    gui_ticks = 0;
    last_mouse_packets = 0;
    shutdown_in_progress = 0;
    shutdown_started_tick = 0;
    if (framebuffer_available()) draw_desktop();
}

void gui_keyboard_input(int value)
{
    dillo_platform_push_key(value);
    if (gui_mode == GUI_TERMINAL) {
        char command_text[80];
        unsigned int command_length = 0;
        command_text[0] = '\0';
        if (value == '\n') {
            command_text[command_length++] = '>';
            command_text[command_length++] = ' ';
            while (terminal_line[command_length - 2] != '\0' &&
                command_length + 1 < sizeof(command_text)) {
                command_text[command_length] = terminal_line[command_length - 2];
                ++command_length;
            }
            command_text[command_length] = '\0';
        }
        int command_mode = shell_handle_key(value, terminal_line,
            sizeof(terminal_line), &terminal_length, terminal_output,
            sizeof(terminal_output));
        if (value == '\b') vga_backspace();
        else if (value == '\n') {
            vga_write("\n");
            if (command_mode == SHELL_RETURN_GRAPHIC) {
                gui_mode = GUI_DESKTOP;
                reset_apps();
                draw_desktop();
            } else if (command_mode == SHELL_SHOW_GUI) {
                add_gui_app(shell_requested_app());
                gui_mode = GUI_DESKTOP;
                draw_desktop();
            } else if (command_mode == SHELL_RUN_APP) {
                launch_gui_app(shell_requested_app());
            } else {
                raw_history_add(command_text);
                raw_history_add(terminal_output);
                vga_write(terminal_output);
                vga_write("\n> ");
            }
        } else if (value >= 32 && value <= 126) vga_put_char((char)value);
        if (gui_mode == GUI_TERMINAL) draw_raw_terminal();
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_SNAKE) {
        if (value == 0x03) {
            gui_mode = GUI_TERMINAL;
            dillo_platform_clear_events();
            enter_terminal();
            return;
        }
        snake_app_key(value);
        snake_app_draw_fullscreen();
        draw_window_chrome("SNAKE");
        draw_active_cursor();
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_MINESWEEPER) {
        if (value == 0x03) {
            dillo_platform_clear_events();
            enter_terminal();
            return;
        }
        minesweeper_app_key(value);
        minesweeper_app_draw_fullscreen();
        draw_window_chrome("MINESWEEPER");
        draw_active_cursor();
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_TETRIS) {
        if (value == 0x03) {
            dillo_platform_clear_events();
            enter_terminal();
            return;
        }
        tetris_app_key(value);
        tetris_app_draw_fullscreen();
        draw_window_chrome("TETRIS");
        draw_active_cursor();
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_PONG) {
        if (value == 0x03) {
            dillo_platform_clear_events();
            enter_terminal();
            return;
        }
        pong_app_key(value);
        pong_app_draw_fullscreen();
        draw_window_chrome("PONG");
        draw_active_cursor();
        return;
    }
    if (value == KEY_F1) {
        enter_terminal();
        return;
    }
    if (value == KEY_F3) {
        enter_fullscreen_snake();
        text_copy(terminal_output, "snake: WASD move, Q quit");
        return;
    }
    if (value == 0x1B) {
        gui_mode = GUI_DESKTOP;
        reset_apps();
        text_copy(terminal_output, "desktop ready");
        draw_desktop();
        return;
    }
    if (gui_mode == GUI_SNAKE) {
        snake_app_key(value);
        if (value == 'q' || value == 'Q') { gui_mode = GUI_DESKTOP; text_copy(terminal_output, "snake closed"); }
        draw_desktop();
        return;
    }
    int command_mode = shell_handle_key(value, terminal_line,
        sizeof(terminal_line), &terminal_length, terminal_output,
        sizeof(terminal_output));
    if (command_mode == SHELL_RUN_APP) {
        launch_gui_app(shell_requested_app());
    } else if (command_mode == SHELL_SHOW_GUI) {
        add_gui_app(shell_requested_app());
        gui_mode = GUI_DESKTOP;
    } else if (command_mode >= GUI_DESKTOP && command_mode <= GUI_SNAKE) {
        gui_mode = command_mode;
        if (gui_mode == GUI_SNAKE) snake_app_init();
    }
    terminal_x = terminal_length * 6;
    draw_desktop();
}

void gui_mouse_input(int x, int y, unsigned char buttons)
{
    dillo_platform_push_mouse(x, y, buttons);
    if (gui_mode == GUI_TERMINAL) return;
    if (gui_window_minimized) {
        if ((buttons & 1) != 0 && y >= (int)framebuffer_height() - 30) {
            unsigned int tab_x = 318;
            if (gui_open_tabs & APP_SNAKE) {
                if (x >= (int)tab_x && x < (int)tab_x + 74) { gui_current_app = APP_SNAKE; restore_gui_app(); return; }
                tab_x += 78;
            }
            if (gui_open_tabs & APP_MINESWEEPER) {
                if (x >= (int)tab_x && x < (int)tab_x + 112) { gui_current_app = APP_MINESWEEPER; restore_gui_app(); return; }
                tab_x += 116;
            }
            if (gui_open_tabs & APP_TETRIS) {
                if (x >= (int)tab_x && x < (int)tab_x + 74) { gui_current_app = APP_TETRIS; restore_gui_app(); return; }
                tab_x += 78;
            }
            if (gui_open_tabs & APP_PONG) {
                if (x >= (int)tab_x && x < (int)tab_x + 66) { gui_current_app = APP_PONG; restore_gui_app(); return; }
            }
            previous_mouse_buttons = 0;
            return;
        }
        if ((buttons & 1) != 0 && y >= 320 && y < 362) {
            if ((gui_apps & APP_SNAKE) && x >= 42 && x < 98) { open_gui_app("snake"); return; }
            if ((gui_apps & APP_MINESWEEPER) && x >= 112 && x < 168) { open_gui_app("minesweeper"); return; }
            if ((gui_apps & APP_TETRIS) && x >= 182 && x < 238) { open_gui_app("tetris"); return; }
        }
        if ((buttons & 1) != 0 && y >= 398 && y < 440 && (gui_apps & APP_PONG) && x >= 42 && x < 98) {
            open_gui_app("pong");
            return;
        }
        if ((buttons & 1) != 0 && y < 246 && y > 288) {
            if (x >= 42 && x < 98) {
                enter_terminal();
                return;
            }
        }
        return;
    }
    if ((gui_mode == GUI_FULLSCREEN_SNAKE ||
        gui_mode == GUI_FULLSCREEN_MINESWEEPER ||
        gui_mode == GUI_FULLSCREEN_TETRIS || gui_mode == GUI_FULLSCREEN_PONG) &&
        (buttons & 1) != 0) {
        if (y >= (int)framebuffer_height() - 30) {
            unsigned int tab_x = 318;
            if (gui_open_tabs & APP_SNAKE) {
                if (x >= (int)tab_x && x < (int)tab_x + 74) { gui_current_app = APP_SNAKE; restore_gui_app(); return; }
                tab_x += 78;
            }
            if (gui_open_tabs & APP_MINESWEEPER) {
                if (x >= (int)tab_x && x < (int)tab_x + 112) { gui_current_app = APP_MINESWEEPER; restore_gui_app(); return; }
                tab_x += 116;
            }
            if (gui_open_tabs & APP_TETRIS) {
                if (x >= (int)tab_x && x < (int)tab_x + 74) { gui_current_app = APP_TETRIS; restore_gui_app(); return; }
                tab_x += 78;
            }
            if (gui_open_tabs & APP_PONG) {
                if (x >= (int)tab_x && x < (int)tab_x + 66) { gui_current_app = APP_PONG; restore_gui_app(); return; }
            }
        }
        if (y < 32) {
            if (x >= (int)framebuffer_width() - 24) {
                if (gui_current_app != 0) gui_open_tabs &= ~gui_current_app;
                gui_current_app = 0;
                if (gui_open_tabs & APP_SNAKE) gui_current_app = APP_SNAKE;
                else if (gui_open_tabs & APP_MINESWEEPER) gui_current_app = APP_MINESWEEPER;
                else if (gui_open_tabs & APP_TETRIS) gui_current_app = APP_TETRIS;
                else if (gui_open_tabs & APP_PONG) gui_current_app = APP_PONG;
                gui_mode = GUI_DESKTOP;
                gui_window_minimized = 0;
                draw_desktop();
                return;
            }
            if (x >= (int)framebuffer_width() - 48 &&
                x < (int)framebuffer_width() - 24) {
                gui_window_minimized = 1;
                draw_desktop();
                return;
            }
        }
    }
    if (gui_mode == GUI_SNAKE && (buttons & 1) != 0 && y >= 184 && y < 204) {
        if (x >= 682 && x < 706) {
            if (gui_current_app != 0) gui_open_tabs &= ~gui_current_app;
            gui_current_app = 0;
            if (gui_open_tabs & APP_SNAKE) gui_current_app = APP_SNAKE;
            else if (gui_open_tabs & APP_MINESWEEPER) gui_current_app = APP_MINESWEEPER;
            else if (gui_open_tabs & APP_TETRIS) gui_current_app = APP_TETRIS;
            else if (gui_open_tabs & APP_PONG) gui_current_app = APP_PONG;
            gui_mode = GUI_DESKTOP;
            gui_window_minimized = 0;
        } else if (x >= 618 && x < 642) {
            gui_window_minimized = 1;
        } else return;
        draw_desktop();
        return;
    }
    if ((gui_apps & APP_EGNA) && y >= 398 && y < 440 && x >= 182 && x < 238) {
        if (gui_egna_path[0] != '\0') {
            if (framebuffer_available()) shell_set_output(terminal_output, sizeof(terminal_output), "EGNA launched");
            gui_current_app = APP_EGNA;
            gui_open_tabs |= APP_EGNA;
            gui_window_minimized = 0;
            gui_mode = GUI_DESKTOP;
            draw_desktop();
        }
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_TETRIS || gui_mode == GUI_FULLSCREEN_PONG) {
        if (mouse_moved() && !gui_window_minimized) draw_active_cursor();
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_MINESWEEPER) {
        minesweeper_app_mouse(x, y, buttons, previous_mouse_buttons);
        previous_mouse_buttons = buttons;
        minesweeper_app_draw_fullscreen();
        draw_cursor();
        return;
    }
    if ((buttons & 1) == 0) return;
    if (x >= (int)framebuffer_width() - 120 && x < (int)framebuffer_width() - 32 &&
        y >= 16 && y < 40) {
        if (gui_mode == GUI_DESKTOP) {
            if (power_off_system()) {
                return;
            }
            serial_write("shutdown: returning to desktop\n");
        }
        gui_mode = GUI_DESKTOP;
        reset_apps();
        shell_set_output(terminal_output, sizeof(terminal_output), "app closed");
        draw_desktop();
        return;
    }
    if (x >= 318 && y >= 180 && y < 480) {
        enter_terminal();
        return;
    }
    if (gui_mode != GUI_DESKTOP) return;
    if (y >= (int)framebuffer_height() - 30) {
        unsigned int tab_x = 318;
        if (gui_open_tabs & APP_SNAKE) {
            if (x >= (int)tab_x && x < (int)tab_x + 74) { gui_current_app = APP_SNAKE; restore_gui_app(); return; }
            tab_x += 78;
        }
        if (gui_open_tabs & APP_MINESWEEPER) {
            if (x >= (int)tab_x && x < (int)tab_x + 112) { gui_current_app = APP_MINESWEEPER; restore_gui_app(); return; }
            tab_x += 116;
        }
        if (gui_open_tabs & APP_TETRIS) {
            if (x >= (int)tab_x && x < (int)tab_x + 74) { gui_current_app = APP_TETRIS; restore_gui_app(); return; }
            tab_x += 78;
        }
        if (gui_open_tabs & APP_PONG) {
            if (x >= (int)tab_x && x < (int)tab_x + 66) { gui_current_app = APP_PONG; restore_gui_app(); return; }
        }
        return;
    }
    if (y >= 320 && y < 362) {
        if ((gui_apps & APP_SNAKE) && x >= 42 && x < 98) { open_gui_app("snake"); return; }
        if ((gui_apps & APP_MINESWEEPER) && x >= 112 && x < 168) { open_gui_app("minesweeper"); return; }
        if ((gui_apps & APP_TETRIS) && x >= 182 && x < 238) { open_gui_app("tetris"); return; }
    }
    if (y >= 398 && y < 440 && (gui_apps & APP_PONG) && x >= 42 && x < 98) {
        open_gui_app("pong");
        return;
    }
    if (y < 246 || y > 288) return;
    if (x >= 42 && x < 98) {
        enter_terminal();
        return;
    }
    else return;
    text_copy(terminal_output, "shell");
    draw_desktop();
}

void gui_tick(void)
{
    if (!framebuffer_available()) return;
    ++gui_ticks;
    if (shutdown_in_progress) {
        if (gui_ticks - shutdown_started_tick > 3000) {
            serial_write("shutdown: ACPI request did not power off; returning to desktop\n");
            shutdown_in_progress = 0;
            gui_mode = GUI_DESKTOP;
            reset_apps();
            draw_desktop();
        }
        return;
    }
    if (gui_mode == GUI_TERMINAL) return;
    if (gui_mode == GUI_FULLSCREEN_SNAKE) {
        if (gui_ticks % 80 == 0) {
            snake_app_tick();
            if (!gui_window_minimized) {
                snake_app_draw_fullscreen();
                draw_window_chrome("SNAKE");
                draw_active_cursor();
            }
        }
        if (mouse_moved()) {
            unsigned int old_mode = gui_mode;
            gui_mouse_input(mouse_x(), mouse_y(), mouse_buttons());
            if (gui_mode == old_mode) {
                if (gui_window_minimized) draw_cursor();
                else draw_active_cursor();
            }
        }
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_MINESWEEPER) {
        minesweeper_app_tick();
        if (gui_ticks % 80 == 0) {
            if (!gui_window_minimized) {
                minesweeper_app_draw_fullscreen();
                draw_window_chrome("MINESWEEPER");
                draw_active_cursor();
            }
        }
        if (mouse_moved()) {
            unsigned int old_mode = gui_mode;
            gui_mouse_input(mouse_x(), mouse_y(), mouse_buttons());
            if (gui_mode == old_mode) {
                if (gui_window_minimized) draw_cursor();
                else draw_active_cursor();
            }
        }
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_TETRIS) {
        tetris_app_tick();
        if (gui_ticks % 16 == 0 && !gui_window_minimized) { tetris_app_draw_fullscreen(); draw_window_chrome("TETRIS"); draw_active_cursor(); }
        if (mouse_moved()) {
            unsigned int old_mode = gui_mode;
            gui_mouse_input(mouse_x(), mouse_y(), mouse_buttons());
            if (gui_mode == old_mode) {
                if (gui_window_minimized) draw_cursor();
                else draw_active_cursor();
            }
        }
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_PONG) {
        if (gui_ticks % 16 == 0) {
            pong_app_tick();
            if (!gui_window_minimized) {
                pong_app_draw_fullscreen();
                draw_window_chrome("PONG");
                draw_active_cursor();
            }
        }
        if (mouse_moved()) {
            unsigned int old_mode = gui_mode;
            gui_mouse_input(mouse_x(), mouse_y(), mouse_buttons());
            if (gui_mode == old_mode) {
                if (gui_window_minimized) draw_cursor();
                else draw_active_cursor();
            }
        }
        return;
    }
    if (gui_mode == GUI_SNAKE && gui_ticks % 80 == 0) {
        if (!gui_window_minimized) draw_cursor();
        snake_app_tick();
        if (!gui_window_minimized) {
            snake_app_draw_update();
            draw_cursor();
        }
    }
    if (mouse_moved()) {
        unsigned int old_mode = gui_mode;
        gui_mouse_input(mouse_x(), mouse_y(), mouse_buttons());
        if (gui_mode == old_mode) draw_cursor();
        if (mouse_packet_count() != last_mouse_packets) {
            last_mouse_packets = mouse_packet_count();
            serial_write("mouse event: x=");
            serial_write_hex((unsigned long)mouse_x());
            serial_write(" y=");
            serial_write_hex((unsigned long)mouse_y());
            serial_write(" packets=");
            serial_write_hex(last_mouse_packets);
            serial_write("\n");
        }
    }
}

int gui_self_test(void)
{
    return framebuffer_available() && framebuffer_width() >= 320 &&
        framebuffer_height() >= 200;
}