#include "acorn/gui.h"
#include "acorn/dillo_platform.h"
#include "acorn/framebuffer.h"
#include "acorn/mouse.h"
#include "acorn/serial.h"
#include "acorn/apps/files.h"
#include "acorn/apps/snake.h"
#include "acorn/apps/minesweeper.h"
#include "acorn/apps/tetris.h"
#include "acorn/apps/pong.h"
#include "acorn/apps/shell.h"
#include "acorn/vga.h"

#define COLOR_BG 0x101820
#define COLOR_BAR 0x183A5A
#define COLOR_PANEL 0x244052
#define COLOR_WINDOW 0x1D303A
#define COLOR_TITLE 0x2A5B63
#define COLOR_TEXT 0xD8F3DC
#define COLOR_CURSOR 0xF4D35E

static unsigned int terminal_x;
static char terminal_line[80];
static unsigned int terminal_length;
static unsigned long last_mouse_packets;
static unsigned long gui_ticks;
static char terminal_output[80];
static unsigned int terminal_output_length;
static unsigned int gui_mode;
static unsigned int cursor_saved[18][17];
static unsigned int cursor_drawn_x;
static unsigned int cursor_drawn_y;
static int cursor_visible;
static unsigned char previous_mouse_buttons;
static char raw_history[18][80];
static unsigned int raw_history_length;

enum { GUI_DESKTOP, GUI_FILES, GUI_SNAKE, GUI_TERMINAL, GUI_FULLSCREEN_SNAKE,
    GUI_FULLSCREEN_MINESWEEPER, GUI_FULLSCREEN_TETRIS, GUI_FULLSCREEN_PONG };
enum { KEY_F1 = 0x80, KEY_F2, KEY_F3 };

static void draw_cursor(void);

static const char cursor_shape[18][18] = {
    "    B            ", "    BB           ", "    BWB          ",
    "    BWWB         ", "    BWWWB        ", "    BWWWWB       ",
    "    BWWWWWB      ", "    BWWWWWWB     ", "    BWWWWWWWB    ",
    "    BWWWWWWWWB   ", "    BWWWWWWWWWB  ", "    BWWWWWBBBBBB ",
    "    BWWBWWB      ", "    BWBWWB       ", "    BB WWB       ",
    "                 ", "                 ", "                 "
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
    snake_app_init();
    dillo_platform_clear_events();
    cursor_visible = 0;
    snake_app_draw_fullscreen();
}

static void enter_fullscreen_minesweeper(void)
{
    gui_mode = GUI_FULLSCREEN_MINESWEEPER;
    minesweeper_app_init();
    dillo_platform_clear_events();
    cursor_visible = 0;
    previous_mouse_buttons = 0;
    minesweeper_app_draw_fullscreen();
    draw_cursor();
}

static void enter_fullscreen_tetris(void)
{
    gui_mode = GUI_FULLSCREEN_TETRIS;
    tetris_app_init();
    dillo_platform_clear_events();
    cursor_visible = 0;
    tetris_app_draw_fullscreen();
}

static void enter_fullscreen_pong(void)
{
    gui_mode = GUI_FULLSCREEN_PONG;
    pong_app_init();
    dillo_platform_clear_events();
    cursor_visible = 0;
    pong_app_draw_fullscreen();
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

static void draw_desktop(void)
{
    unsigned int screen_width = framebuffer_width();
    unsigned int screen_height = framebuffer_height();
    cursor_visible = 0;
    framebuffer_clear(COLOR_BG);
    framebuffer_fill_rect(0, 0, screen_width, 56, COLOR_BAR);
    framebuffer_draw_text(24, 18, "OAKOS DESKTOP", COLOR_TEXT, 2);
    framebuffer_fill_rect(screen_width - 120, 16, 88, 24, 0x4CC9A4);
    framebuffer_draw_text(screen_width - 108, 24, "CLOSE", COLOR_BG, 1);
    framebuffer_fill_rect(26, 78, 270, screen_height - 110, COLOR_PANEL);
    framebuffer_fill_rect(26, 78, 270, 32, COLOR_TITLE);
    framebuffer_draw_text(40, 88, "WORKSPACE", COLOR_TEXT, 1);
    framebuffer_draw_text(48, 140, "FILES", COLOR_TEXT, 2);
    framebuffer_draw_text(48, 174, "TERMINAL", COLOR_TEXT, 2);
    framebuffer_draw_text(48, 208, "SETTINGS", COLOR_TEXT, 2);
    framebuffer_fill_rect(42, 246, 56, 42, COLOR_TITLE);
    framebuffer_fill_rect(182, 246, 56, 42, COLOR_TITLE);
    framebuffer_draw_text(50, 258, "SHELL", COLOR_TEXT, 1);
    framebuffer_draw_text(190, 258, "F3", COLOR_TEXT, 1);
    framebuffer_draw_text(40, 300, "APPS", COLOR_CURSOR, 1);
    if (gui_mode != GUI_DESKTOP)
        framebuffer_fill_rect(318, 78, 410, 76, COLOR_TITLE);
    if (gui_mode == GUI_FILES) files_app_draw();
    if (gui_mode == GUI_SNAKE) snake_app_draw();
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
    snake_app_init();
    gui_ticks = 0;
    last_mouse_packets = 0;
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
            } else if (command_mode == SHELL_RUN_APP) {
                if (shell_requested_app()[0] == 's') enter_fullscreen_snake();
                else if (shell_requested_app()[0] == 't') enter_fullscreen_tetris();
                else if (shell_requested_app()[0] == 'p') enter_fullscreen_pong();
                else enter_fullscreen_minesweeper();
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
        draw_cursor();
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
        return;
    }
    if (value == KEY_F1) {
        enter_terminal();
        return;
    }
    if (value == KEY_F3) {
        gui_mode = GUI_SNAKE;
        snake_app_init();
        text_copy(terminal_output, "snake: WASD move, Q quit");
        draw_desktop();
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
        if (shell_requested_app()[0] == 's') enter_fullscreen_snake();
        else if (shell_requested_app()[0] == 't') enter_fullscreen_tetris();
        else if (shell_requested_app()[0] == 'p') enter_fullscreen_pong();
        else enter_fullscreen_minesweeper();
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
    if (gui_mode == GUI_FULLSCREEN_TETRIS) return;
    if (gui_mode == GUI_FULLSCREEN_PONG) return;
    if (gui_mode == GUI_FULLSCREEN_MINESWEEPER) {
        minesweeper_app_mouse(x, y, buttons, previous_mouse_buttons);
        previous_mouse_buttons = buttons;
        minesweeper_app_draw_fullscreen();
        draw_cursor();
        return;
    }
    if ((buttons & 1) == 0) return;
    if (gui_mode != GUI_DESKTOP && x >= (int)framebuffer_width() - 120 &&
        x < (int)framebuffer_width() - 32 && y >= 16 && y < 40) {
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
    if (y < 246 || y > 288) return;
    if (x >= 42 && x < 98) {
        enter_terminal();
        return;
    }
    else if (x >= 182 && x < 238) gui_mode = GUI_SNAKE;
    else return;
    text_copy(terminal_output, gui_mode == GUI_SNAKE ?
        "snake: WASD move, Q quit" : "shell");
    draw_desktop();
}

void gui_tick(void)
{
    if (!framebuffer_available()) return;
    ++gui_ticks;
    if (gui_mode == GUI_TERMINAL) return;
    if (gui_mode == GUI_FULLSCREEN_SNAKE) {
        if (gui_ticks % 80 == 0) {
            snake_app_tick();
            snake_app_draw_fullscreen();
        }
        if (mouse_moved())
            dillo_platform_push_mouse(mouse_x(), mouse_y(), mouse_buttons());
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_MINESWEEPER) {
        minesweeper_app_tick();
        if (gui_ticks % 80 == 0) {
            minesweeper_app_draw_fullscreen();
            draw_cursor();
        }
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_TETRIS) {
        tetris_app_tick();
        if (gui_ticks % 16 == 0) tetris_app_draw_fullscreen();
        return;
    }
    if (gui_mode == GUI_FULLSCREEN_PONG) {
        if (gui_ticks % 16 == 0) {
            pong_app_tick();
            pong_app_draw_fullscreen();
        }
        return;
    }
    if (gui_mode == GUI_SNAKE && gui_ticks % 80 == 0) {
        draw_cursor();
        snake_app_tick();
        snake_app_draw_update();
        draw_cursor();
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