#include "acorn/apps/pong.h"
#include "acorn/framebuffer.h"
#include "acorn/entropy.h"
#include "acorn/timer.h"

#define COLOR_BG 0x0B1220
#define COLOR_TEXT 0xD8F3DC
#define COLOR_NET 0x244052
#define COLOR_BALL 0xF4D35E
#define COLOR_PADDLE_LEFT 0x4CD9EC
#define COLOR_PADDLE_RIGHT 0xFF7F50
#define COLOR_GAMEOVER 0xFF5C5C

enum {
    PONG_TOP_OFFSET = 96,
    PONG_BOTTOM_OFFSET = 40,
    PONG_SIDE_MARGIN = 32,
    PONG_PADDLE_WIDTH = 14,
    PONG_PADDLE_HEIGHT = 90,
    PONG_PADDLE_SPEED = 7,
    PONG_HOLD_GRACE = 5,
    PONG_BALL_SIZE = 14,
    PONG_WIN_SCORE = 7,
    PONG_BALL_SPEED_START = 6,
    PONG_BALL_SPEED_MAX = 16,
    PONG_BALL_SPEEDUP = 1,
};

static int left_paddle_y;
static int right_paddle_y;
static int left_hold_dir;
static int right_hold_dir;
static unsigned long left_hold_tick;
static unsigned long right_hold_tick;
static unsigned long pong_ticks;
static int ball_x;
static int ball_y;
static int ball_dx;
static int ball_dy;
static unsigned int left_score;
static unsigned int right_score;
static int game_over;
static int winner;
static unsigned int rng_state;
static int rng_seeded;

/*
 * Mesmo esquema de aleatoriedade usado no minesweeper/tetris: xorshift32
 * semeado com RDTSC, reforcado por RDRAND quando disponivel e pelo
 * contador de ticks do timer, para variar o saque da bola sem depender
 * de um gerador global.
 */
static unsigned long long pong_read_tsc(void)
{
    unsigned int low;
    unsigned int high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((unsigned long long)high << 32) | low;
}

static void pong_rng_seed(void)
{
    unsigned long long tsc = pong_read_tsc();
    unsigned int seed = (unsigned int)(tsc ^ (tsc >> 32));
    unsigned int entropy_word = 0;
    if (entropy_fill((unsigned char *)&entropy_word, sizeof(entropy_word)))
        seed ^= entropy_word;
    seed ^= (unsigned int)timer_ticks();
    seed ^= (unsigned int)(unsigned long)&rng_state;
    seed ^= 0x9E3779B9u;
    if (seed == 0) seed = 0x2545F491u;
    rng_state = seed;
    rng_seeded = 1;
}

static unsigned int pong_rng_next(void)
{
    if (!rng_seeded) pong_rng_seed();
    unsigned long long tsc = pong_read_tsc();
    rng_state ^= (unsigned int)tsc;
    rng_state ^= (unsigned int)timer_ticks();
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    if (rng_state == 0) rng_state = 0x2545F491u;
    return rng_state;
}

static unsigned int field_left(void) { return PONG_SIDE_MARGIN; }
static unsigned int field_top(void) { return PONG_TOP_OFFSET; }

static unsigned int field_width(void)
{
    unsigned int width = framebuffer_width();
    unsigned int usable = width > 2 * PONG_SIDE_MARGIN ?
        width - 2 * PONG_SIDE_MARGIN : 200;
    return usable;
}

static unsigned int field_height(void)
{
    unsigned int height = framebuffer_height();
    unsigned int usable = height > PONG_TOP_OFFSET + PONG_BOTTOM_OFFSET ?
        height - PONG_TOP_OFFSET - PONG_BOTTOM_OFFSET : 200;
    return usable;
}

static int paddle_max_y(void)
{
    int usable = (int)field_height() - PONG_PADDLE_HEIGHT;
    return usable > 0 ? usable : 0;
}

static void serve_ball(int towards_left)
{
    unsigned int roll = pong_rng_next();
    ball_x = (int)field_width() / 2 - PONG_BALL_SIZE / 2;
    ball_y = (int)field_height() / 2 - PONG_BALL_SIZE / 2;
    ball_dx = towards_left ? -PONG_BALL_SPEED_START : PONG_BALL_SPEED_START;
    ball_dy = (roll & 1) ? 4 : -4;
    if (roll & 2) ball_dy = -ball_dy;
}

void pong_app_init(void)
{
    left_paddle_y = paddle_max_y() / 2;
    right_paddle_y = paddle_max_y() / 2;
    left_hold_dir = 0;
    right_hold_dir = 0;
    left_hold_tick = 0;
    right_hold_tick = 0;
    pong_ticks = 0;
    left_score = 0;
    right_score = 0;
    game_over = 0;
    winner = 0;
    serve_ball((pong_rng_next() & 1) != 0);
}

void pong_app_key(int value)
{
    if (value == 'r' || value == 'R') {
        pong_app_init();
        return;
    }
    if (game_over) return;
    if (value == 'w' || value == 'W') {
        left_hold_dir = -1;
        left_hold_tick = pong_ticks;
    } else if (value == 's' || value == 'S') {
        left_hold_dir = 1;
        left_hold_tick = pong_ticks;
    } else if (value == 'i' || value == 'I') {
        right_hold_dir = -1;
        right_hold_tick = pong_ticks;
    } else if (value == 'k' || value == 'K') {
        right_hold_dir = 1;
        right_hold_tick = pong_ticks;
    }
}

/*
 * O teclado so entrega eventos de tecla pressionada (com auto-repeat de
 * hardware enquanto o usuario segura), sem evento de soltar. Por isso a
 * raquete continua se movendo a cada tick enquanto os eventos de repeticao
 * continuarem chegando (segurar a tecla) e para pouco depois de soltar,
 * usando uma janela de tolerancia (PONG_HOLD_GRACE ticks) para nao parar
 * entre dois pulsos de auto-repeat.
 */
static void apply_paddle_hold(void)
{
    int max_y = paddle_max_y();
    if (left_hold_dir != 0) {
        if (pong_ticks - left_hold_tick <= PONG_HOLD_GRACE) {
            left_paddle_y += left_hold_dir * PONG_PADDLE_SPEED;
            if (left_paddle_y < 0) left_paddle_y = 0;
            if (left_paddle_y > max_y) left_paddle_y = max_y;
        } else {
            left_hold_dir = 0;
        }
    }
    if (right_hold_dir != 0) {
        if (pong_ticks - right_hold_tick <= PONG_HOLD_GRACE) {
            right_paddle_y += right_hold_dir * PONG_PADDLE_SPEED;
            if (right_paddle_y < 0) right_paddle_y = 0;
            if (right_paddle_y > max_y) right_paddle_y = max_y;
        } else {
            right_hold_dir = 0;
        }
    }
}

static int clamp_speed(int value)
{
    if (value > PONG_BALL_SPEED_MAX) return PONG_BALL_SPEED_MAX;
    if (value < -PONG_BALL_SPEED_MAX) return -PONG_BALL_SPEED_MAX;
    return value;
}

void pong_app_tick(void)
{
    ++pong_ticks;
    if (game_over) return;
    apply_paddle_hold();

    int width = (int)field_width();
    int height = (int)field_height();
    int next_x = ball_x + ball_dx;
    int next_y = ball_y + ball_dy;

    if (next_y < 0) {
        next_y = -next_y;
        ball_dy = -ball_dy;
    } else if (next_y + PONG_BALL_SIZE > height) {
        next_y = 2 * (height - PONG_BALL_SIZE) - next_y;
        ball_dy = -ball_dy;
    }

    if (ball_dx < 0 && next_x <= PONG_PADDLE_WIDTH &&
        next_y + PONG_BALL_SIZE >= left_paddle_y &&
        next_y <= left_paddle_y + PONG_PADDLE_HEIGHT) {
        next_x = PONG_PADDLE_WIDTH;
        ball_dx = clamp_speed(-ball_dx + PONG_BALL_SPEEDUP);
        int center = left_paddle_y + PONG_PADDLE_HEIGHT / 2;
        int offset = (next_y + PONG_BALL_SIZE / 2) - center;
        ball_dy = clamp_speed(ball_dy + offset / 12);
    } else if (ball_dx > 0 && next_x + PONG_BALL_SIZE >= width - PONG_PADDLE_WIDTH &&
        next_y + PONG_BALL_SIZE >= right_paddle_y &&
        next_y <= right_paddle_y + PONG_PADDLE_HEIGHT) {
        next_x = width - PONG_PADDLE_WIDTH - PONG_BALL_SIZE;
        ball_dx = clamp_speed(-ball_dx - PONG_BALL_SPEEDUP);
        int center = right_paddle_y + PONG_PADDLE_HEIGHT / 2;
        int offset = (next_y + PONG_BALL_SIZE / 2) - center;
        ball_dy = clamp_speed(ball_dy + offset / 12);
    }

    if (next_x < -PONG_BALL_SIZE) {
        ++right_score;
        if (right_score >= PONG_WIN_SCORE) { game_over = 1; winner = 2; }
        else serve_ball(0);
        return;
    }
    if (next_x > width + PONG_BALL_SIZE) {
        ++left_score;
        if (left_score >= PONG_WIN_SCORE) { game_over = 1; winner = 1; }
        else serve_ball(1);
        return;
    }

    ball_x = next_x;
    ball_y = next_y;
}

static void format_number(char *output, unsigned int capacity,
    unsigned int value)
{
    char digits[10];
    unsigned int count = 0;
    if (value == 0) digits[count++] = '0';
    while (value != 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    }
    unsigned int index = 0;
    while (count != 0 && index + 1 < capacity) output[index++] = digits[--count];
    output[index] = '\0';
}

void pong_app_draw_fullscreen(void)
{
    unsigned int screen_width = framebuffer_width();
    unsigned int left = field_left();
    unsigned int top = field_top();
    unsigned int width = field_width();
    unsigned int height = field_height();

    framebuffer_clear(COLOR_BG);
    framebuffer_draw_text(24, 24, "PONG", COLOR_TEXT, 2);
    framebuffer_draw_text(24, 54,
        "W/S move esquerda   I/K move direita   R reinicia   CTRL+C shell",
        COLOR_TEXT, 1);

    framebuffer_fill_rect(left, top, width, height, 0x101C2A);
    for (unsigned int y = 0; y + 12 < height; y += 24)
        framebuffer_fill_rect(left + width / 2 - 2, top + y, 4, 12, COLOR_NET);

    char left_text[16];
    char right_text[16];
    format_number(left_text, sizeof(left_text), left_score);
    format_number(right_text, sizeof(right_text), right_score);
    framebuffer_draw_text(left + width / 2 - 60, top - 34, left_text,
        COLOR_PADDLE_LEFT, 2);
    framebuffer_draw_text(left + width / 2 + 44, top - 34, right_text,
        COLOR_PADDLE_RIGHT, 2);

    framebuffer_fill_rect(left, top + (unsigned int)left_paddle_y,
        PONG_PADDLE_WIDTH, PONG_PADDLE_HEIGHT, COLOR_PADDLE_LEFT);
    framebuffer_fill_rect(left + width - PONG_PADDLE_WIDTH,
        top + (unsigned int)right_paddle_y, PONG_PADDLE_WIDTH,
        PONG_PADDLE_HEIGHT, COLOR_PADDLE_RIGHT);

    framebuffer_fill_rect(left + (unsigned int)ball_x,
        top + (unsigned int)ball_y, PONG_BALL_SIZE, PONG_BALL_SIZE,
        COLOR_BALL);

    if (game_over) {
        const char *message = winner == 1 ?
            "JOGADOR DA ESQUERDA VENCEU - R reinicia" :
            "JOGADOR DA DIREITA VENCEU - R reinicia";
        unsigned int text_x = screen_width > 480 ? (screen_width - 480) / 2 : 24;
        framebuffer_draw_text(text_x, top + height / 2 - 8, message,
            COLOR_GAMEOVER, 1);
    }
}