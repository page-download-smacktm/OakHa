#include "acorn/apps/snake.h"
#include "acorn/framebuffer.h"

#define COLOR_WINDOW 0x1D303A
#define COLOR_TITLE 0x2A5B63
#define COLOR_TEXT 0xD8F3DC
#define COLOR_CURSOR 0xF4D35E

enum { SNAKE_COLUMNS = 36, SNAKE_ROWS = 21, SNAKE_CELL = 10,
    SNAKE_MAX_LENGTH = 64 };

static unsigned int snake_x[SNAKE_MAX_LENGTH];
static unsigned int snake_y[SNAKE_MAX_LENGTH];
static unsigned int snake_length;
static int snake_dx;
static int snake_dy;
static int snake_game_over;
static unsigned int food_x;
static unsigned int food_y;
static unsigned int snake_score;

void snake_app_init(void)
{
    snake_x[0] = 18; snake_y[0] = 10;
    snake_x[1] = 17; snake_y[1] = 10;
    snake_x[2] = 16; snake_y[2] = 10;
    snake_x[3] = 15; snake_y[3] = 10;
    snake_length = 4;
    snake_dx = 1;
    snake_dy = 0;
    snake_game_over = 0;
    food_x = 28;
    food_y = 10;
    snake_score = 0;
}

void snake_app_key(int value)
{
    if (value == 'r' || value == 'R') {
        snake_app_init();
        return;
    }
    if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
    if (value == 'w' && snake_dy == 0) { snake_dx = 0; snake_dy = -1; }
    if (value == 's' && snake_dy == 0) { snake_dx = 0; snake_dy = 1; }
    if (value == 'a' && snake_dx == 0) { snake_dx = -1; snake_dy = 0; }
    if (value == 'd' && snake_dx == 0) { snake_dx = 1; snake_dy = 0; }
}

void snake_app_tick(void)
{
    unsigned int next_x;
    unsigned int next_y;
    int next_x_position;
    int next_y_position;
    if (snake_game_over) return;
    next_x_position = (int)snake_x[0] + snake_dx;
    next_y_position = (int)snake_y[0] + snake_dy;
    if (next_x_position < 0) next_x_position = SNAKE_COLUMNS - 1;
    if (next_x_position >= SNAKE_COLUMNS) next_x_position = 0;
    if (next_y_position < 0) next_y_position = SNAKE_ROWS - 1;
    if (next_y_position >= SNAKE_ROWS) next_y_position = 0;
    next_x = (unsigned int)next_x_position;
    next_y = (unsigned int)next_y_position;
    int eating = next_x == food_x && next_y == food_y;
    for (unsigned int index = 1; index < snake_length; ++index)
        if (snake_x[index] == next_x && snake_y[index] == next_y) {
            snake_game_over = 1;
            return;
        }
    if (eating && snake_length < SNAKE_MAX_LENGTH) {
        ++snake_length;
        ++snake_score;
        food_x = (food_x + 11) % SNAKE_COLUMNS;
        food_y = (food_y + 7) % SNAKE_ROWS;
    }
    for (unsigned int index = snake_length - 1; index != 0; --index) {
        snake_x[index] = snake_x[index - 1];
        snake_y[index] = snake_y[index - 1];
    }
    snake_x[0] = next_x;
    snake_y[0] = next_y;
}

void snake_app_draw(void)
{
    framebuffer_fill_rect(318, 180, 410, 300, COLOR_WINDOW);
    framebuffer_fill_rect(318, 180, 410, 30, COLOR_TITLE);
    framebuffer_draw_text(334, 190, "SNAKE", COLOR_TEXT, 2);
    framebuffer_draw_text(540, 190, snake_game_over ? "GAME OVER R" : "WASD MOVE", COLOR_TEXT, 1);
    for (unsigned int index = 0; index < snake_length; ++index)
        framebuffer_fill_rect(340 + snake_x[index] * SNAKE_CELL,
            230 + snake_y[index] * SNAKE_CELL, SNAKE_CELL - 1, SNAKE_CELL - 1,
            index == 0 ? COLOR_CURSOR : 0x4CC9A4);
    framebuffer_fill_rect(340 + food_x * SNAKE_CELL,
        230 + food_y * SNAKE_CELL, SNAKE_CELL - 1, SNAKE_CELL - 1, 0xFF5C5C);
}

void snake_app_draw_update(void)
{
    framebuffer_fill_rect(340, 230, SNAKE_COLUMNS * SNAKE_CELL,
        SNAKE_ROWS * SNAKE_CELL,
        COLOR_WINDOW);
    for (unsigned int index = 0; index < snake_length; ++index)
        framebuffer_fill_rect(340 + snake_x[index] * SNAKE_CELL,
            230 + snake_y[index] * SNAKE_CELL, SNAKE_CELL - 1, SNAKE_CELL - 1,
            index == 0 ? COLOR_CURSOR : 0x4CC9A4);
    framebuffer_fill_rect(340 + food_x * SNAKE_CELL,
        230 + food_y * SNAKE_CELL, SNAKE_CELL - 1, SNAKE_CELL - 1, 0xFF5C5C);
}

void snake_app_draw_fullscreen(void)
{
    unsigned int width = framebuffer_width();
    unsigned int height = framebuffer_height();
    unsigned int origin_x = (width - SNAKE_COLUMNS * SNAKE_CELL) / 2;
    unsigned int origin_y = (height - SNAKE_ROWS * SNAKE_CELL) / 2;
    framebuffer_clear(COLOR_WINDOW);
    framebuffer_draw_text(24, 24, "SNAKE", COLOR_TEXT, 2);
    framebuffer_draw_text(24, 54, "WASD move   R restart   CTRL+C return to shell",
        COLOR_TEXT, 1);
    for (unsigned int index = 0; index < snake_length; ++index)
        framebuffer_fill_rect(origin_x + snake_x[index] * SNAKE_CELL,
            origin_y + snake_y[index] * SNAKE_CELL, SNAKE_CELL - 1,
            SNAKE_CELL - 1, index == 0 ? COLOR_CURSOR : 0x4CC9A4);
    framebuffer_fill_rect(origin_x + food_x * SNAKE_CELL,
        origin_y + food_y * SNAKE_CELL, SNAKE_CELL - 1, SNAKE_CELL - 1,
        0xFF5C5C);
}
