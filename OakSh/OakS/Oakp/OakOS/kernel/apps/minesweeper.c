#include "acorn/apps/minesweeper.h"
#include "acorn/framebuffer.h"

#define BOARD_COLUMNS 10
#define BOARD_ROWS 8
#define CELL_SIZE 32
#define MINE_COUNT 12
#define RESULT_DURATION_TICKS 5000

#define COLOR_BG 0x101820
#define COLOR_PANEL 0x1D303A
#define COLOR_CELL 0x244052
#define COLOR_REVEALED 0xD8F3DC
#define COLOR_TEXT 0xD8F3DC
#define COLOR_CURSOR 0xF4D35E
#define COLOR_MINE 0xFF5C5C

static unsigned char mines[BOARD_ROWS][BOARD_COLUMNS];
static unsigned char revealed[BOARD_ROWS][BOARD_COLUMNS];
static unsigned char marked[BOARD_ROWS][BOARD_COLUMNS];
static unsigned int cursor_x;
static unsigned int cursor_y;
static unsigned int revealed_count;
static unsigned int marked_count;
static int game_over;
static int game_won;
static unsigned long result_ticks;

static unsigned int board_x(void)
{
    return (framebuffer_width() - BOARD_COLUMNS * CELL_SIZE) / 2;
}

static unsigned int board_y(void)
{
    return (framebuffer_height() - BOARD_ROWS * CELL_SIZE) / 2 + 18;
}

static unsigned int adjacent_mines(unsigned int x, unsigned int y)
{
    unsigned int count = 0;
    for (int row = -1; row <= 1; ++row)
        for (int column = -1; column <= 1; ++column) {
            int next_x = (int)x + column;
            int next_y = (int)y + row;
            if (next_x >= 0 && next_x < BOARD_COLUMNS && next_y >= 0 &&
                next_y < BOARD_ROWS && mines[next_y][next_x]) ++count;
        }
    return count;
}

static void reveal_cell(unsigned int x, unsigned int y)
{
    if (x >= BOARD_COLUMNS || y >= BOARD_ROWS || revealed[y][x] || marked[y][x])
        return;
    revealed[y][x] = 1;
    ++revealed_count;
    if (mines[y][x]) {
        game_over = 1;
        result_ticks = 0;
        return;
    }
    if (adjacent_mines(x, y) == 0)
        for (int row = -1; row <= 1; ++row)
            for (int column = -1; column <= 1; ++column) {
                int next_x = (int)x + column;
                int next_y = (int)y + row;
                if (next_x >= 0 && next_x < BOARD_COLUMNS && next_y >= 0 &&
                    next_y < BOARD_ROWS)
                    reveal_cell((unsigned int)next_x, (unsigned int)next_y);
            }
}

static void check_win(void)
{
    if (!game_over && !game_won &&
        revealed_count >= BOARD_COLUMNS * BOARD_ROWS - MINE_COUNT) {
        game_won = 1;
        result_ticks = 0;
    }
}

void minesweeper_app_init(void)
{
    unsigned int placed = 0;
    for (unsigned int row = 0; row < BOARD_ROWS; ++row)
        for (unsigned int column = 0; column < BOARD_COLUMNS; ++column) {
            mines[row][column] = 0;
            revealed[row][column] = 0;
            marked[row][column] = 0;
        }
    while (placed < MINE_COUNT) {
        unsigned int position = (placed * 37 + 11) %
            (BOARD_COLUMNS * BOARD_ROWS);
        unsigned int row = position / BOARD_COLUMNS;
        unsigned int column = position % BOARD_COLUMNS;
        if (!mines[row][column]) {
            mines[row][column] = 1;
            ++placed;
        }
    }
    cursor_x = BOARD_COLUMNS / 2;
    cursor_y = BOARD_ROWS / 2;
    revealed_count = 0;
    marked_count = 0;
    game_over = 0;
    game_won = 0;
    result_ticks = 0;
}

void minesweeper_app_key(int value)
{
    if (value == 'r' || value == 'R') {
        minesweeper_app_init();
        return;
    }
    if (game_over || game_won) return;
    if (value == 'w' && cursor_y != 0) --cursor_y;
    else if (value == 's' && cursor_y + 1 < BOARD_ROWS) ++cursor_y;
    else if (value == 'a' && cursor_x != 0) --cursor_x;
    else if (value == 'd' && cursor_x + 1 < BOARD_COLUMNS) ++cursor_x;
    else if (value == ' ' || value == '\n' || value == 'x' || value == 'X')
        reveal_cell(cursor_x, cursor_y);
    else if (value == 'f' || value == 'F') {
        if (!revealed[cursor_y][cursor_x]) {
            marked[cursor_y][cursor_x] = !marked[cursor_y][cursor_x];
            if (marked[cursor_y][cursor_x]) ++marked_count;
            else if (marked_count != 0) --marked_count;
        }
    }
    check_win();
}

void minesweeper_app_mouse(int x, int y, unsigned char buttons,
    unsigned char previous_buttons)
{
    unsigned int left = board_x();
    unsigned int top = board_y();
    if (x < (int)left || y < (int)top ||
        x >= (int)(left + BOARD_COLUMNS * CELL_SIZE) ||
        y >= (int)(top + BOARD_ROWS * CELL_SIZE)) return;
    cursor_x = ((unsigned int)x - left) / CELL_SIZE;
    cursor_y = ((unsigned int)y - top) / CELL_SIZE;
    if (game_over || game_won || buttons == previous_buttons) return;
    if ((buttons & 1) != 0) reveal_cell(cursor_x, cursor_y);
    else if ((buttons & 2) != 0 && !revealed[cursor_y][cursor_x]) {
        marked[cursor_y][cursor_x] = !marked[cursor_y][cursor_x];
        if (marked[cursor_y][cursor_x]) ++marked_count;
        else if (marked_count != 0) --marked_count;
    }
    check_win();
}

void minesweeper_app_tick(void)
{
    check_win();
    if (game_over || game_won) {
        ++result_ticks;
        if (result_ticks >= RESULT_DURATION_TICKS)
            minesweeper_app_init();
    }
}

void minesweeper_app_draw_fullscreen(void)
{
    unsigned int left = board_x();
    unsigned int top = board_y();
    framebuffer_clear(COLOR_BG);
    framebuffer_draw_text(24, 24, "MINESWEEPER", COLOR_TEXT, 2);
    framebuffer_draw_text(24, 54, "Left click reveal   Right click mark   WASD move   X/SPACE reveal   R restart   CTRL+C shell",
        COLOR_TEXT, 1);
    for (unsigned int row = 0; row < BOARD_ROWS; ++row)
        for (unsigned int column = 0; column < BOARD_COLUMNS; ++column) {
            unsigned int x = left + column * CELL_SIZE;
            unsigned int y = top + row * CELL_SIZE;
            unsigned int color = revealed[row][column] ? COLOR_REVEALED : COLOR_CELL;
            framebuffer_fill_rect(x, y, CELL_SIZE - 2, CELL_SIZE - 2, color);
            if (column == cursor_x && row == cursor_y)
                framebuffer_fill_rect(x + 2, y + 2, CELL_SIZE - 6, 3, COLOR_CURSOR);
            if (game_over && mines[row][column])
                framebuffer_draw_text(x + 9, y + 8, "B", COLOR_MINE, 2);
            else if (marked[row][column])
                framebuffer_draw_text(x + 11, y + 8, "F", COLOR_MINE, 2);
            else if (revealed[row][column] && mines[row][column])
                framebuffer_draw_text(x + 9, y + 8, "*", COLOR_MINE, 2);
            else if (revealed[row][column]) {
                unsigned int count = adjacent_mines(column, row);
                if (count != 0) {
                    char digit[2] = { (char)('0' + count), '\0' };
                    framebuffer_draw_text(x + 11, y + 8, digit, COLOR_PANEL, 2);
                }
            }
        }
    if (game_over) framebuffer_draw_text(24, framebuffer_height() - 28,
        "GAME OVER - restarting", COLOR_MINE, 1);
    else if (game_won) framebuffer_draw_text(24, framebuffer_height() - 28,
        "YOU WIN - restarting", COLOR_CURSOR, 1);
    else {
        char status[32] = "Marked: 0";
        status[8] = (char)('0' + marked_count % 10);
        framebuffer_draw_text(24, framebuffer_height() - 28, status,
            COLOR_TEXT, 1);
    }
}