#include "acorn/apps/tetris.h"
#include "acorn/framebuffer.h"
#include "acorn/entropy.h"
#include "acorn/timer.h"

#define BOARD_COLUMNS 10
#define BOARD_ROWS 20
#define CELL_SIZE 24
#define PREVIEW_CELL 16
#define PANEL_WIDTH 190
#define PANEL_GAP 40

#define COLOR_BG 0x101820
#define COLOR_PANEL 0x1D303A
#define COLOR_BOARD_BG 0x16232C
#define COLOR_TEXT 0xD8F3DC
#define COLOR_GAMEOVER 0xFF5C5C

enum { PIECE_I, PIECE_O, PIECE_T, PIECE_S, PIECE_Z, PIECE_J, PIECE_L,
    PIECE_COUNT };

/*
 * Cada peca tem 4 orientacoes, representadas como uma mascara de 16 bits
 * sobre uma grade 4x4 (bit = row * 4 + col). Pecas O/S/Z/I usam apenas
 * 1 ou 2 formas unicas, repetidas nas 4 posicoes por simplicidade.
 */
static const unsigned short piece_rotations[PIECE_COUNT][4] = {
    { 0x00F0, 0x4444, 0x00F0, 0x4444 }, /* I */
    { 0x0660, 0x0660, 0x0660, 0x0660 }, /* O */
    { 0x0072, 0x0262, 0x0270, 0x0232 }, /* T */
    { 0x003C, 0x0462, 0x003C, 0x0462 }, /* S */
    { 0x0063, 0x0264, 0x0063, 0x0264 }, /* Z */
    { 0x0071, 0x0226, 0x0470, 0x0322 }, /* J */
    { 0x0074, 0x0622, 0x0170, 0x0223 }, /* L */
};

static const unsigned int piece_colors[PIECE_COUNT] = {
    0x4CD9EC, 0xF4D35E, 0xB388EB, 0x8AE68A, 0xFF5C5C, 0x5C7CFA, 0xFFA94D,
};

static unsigned char board[BOARD_ROWS][BOARD_COLUMNS];
static int current_type;
static int current_rotation;
static int current_x;
static int current_y;
static int next_type;
static unsigned long score;
static unsigned int level;
static unsigned int lines_cleared_total;
static int game_over;
static unsigned long drop_counter;
static unsigned int rng_state;
static int rng_seeded;

/*
 * Mesmo esquema de aleatoriedade usado no minesweeper: xorshift32 semeado
 * com RDTSC (sempre disponivel em x86, nao depende do timer do kernel
 * ja estar avancando) reforcado por RDRAND quando o hardware suporta.
 * Ver kernel/apps/minesweeper.c para a explicacao detalhada do porque
 * dessa escolha em vez de depender so de timer_ticks()/RDRAND.
 */
static unsigned long long tetris_read_tsc(void)
{
    unsigned int low;
    unsigned int high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((unsigned long long)high << 32) | low;
}

static void tetris_rng_seed(void)
{
    unsigned long long tsc = tetris_read_tsc();
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

static unsigned int tetris_rng_next(void)
{
    if (!rng_seeded) tetris_rng_seed();
    unsigned long long tsc = tetris_read_tsc();
    rng_state ^= (unsigned int)tsc;
    rng_state ^= (unsigned int)timer_ticks();
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    if (rng_state == 0) rng_state = 0x2545F491u;
    return rng_state;
}

static unsigned int tetris_rng_below(unsigned int bound)
{
    unsigned int limit = (0xFFFFFFFFu / bound) * bound;
    unsigned int value;
    do {
        value = tetris_rng_next();
    } while (value >= limit);
    return value % bound;
}

static int tetris_random_piece(void)
{
    return (int)tetris_rng_below(PIECE_COUNT);
}

static int piece_cell(int type, int rotation, int row, int col)
{
    unsigned short mask = piece_rotations[type][rotation];
    return (mask & (unsigned short)(1u << (row * 4 + col))) != 0;
}

static int piece_fits(int type, int rotation, int px, int py)
{
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            if (!piece_cell(type, rotation, row, col)) continue;
            int board_col = px + col;
            int board_row = py + row;
            if (board_col < 0 || board_col >= BOARD_COLUMNS ||
                board_row < 0 || board_row >= BOARD_ROWS) return 0;
            if (board[board_row][board_col] != 0) return 0;
        }
    return 1;
}

static void spawn_piece(void)
{
    current_type = next_type;
    next_type = tetris_random_piece();
    current_rotation = 0;
    current_x = (BOARD_COLUMNS - 4) / 2;
    current_y = 0;
    if (!piece_fits(current_type, current_rotation, current_x, current_y))
        game_over = 1;
}

static void clear_lines(void)
{
    static const unsigned int line_points[5] = { 0, 40, 100, 300, 1200 };
    unsigned int cleared = 0;
    for (int row = BOARD_ROWS - 1; row >= 0; --row) {
        int full = 1;
        for (int col = 0; col < BOARD_COLUMNS; ++col)
            if (board[row][col] == 0) { full = 0; break; }
        if (!full) continue;
        ++cleared;
        for (int move_row = row; move_row > 0; --move_row)
            for (int col = 0; col < BOARD_COLUMNS; ++col)
                board[move_row][col] = board[move_row - 1][col];
        for (int col = 0; col < BOARD_COLUMNS; ++col)
            board[0][col] = 0;
        ++row; /* reavalia a mesma linha, agora preenchida pela de cima */
    }
    if (cleared == 0) return;
    if (cleared > 4) cleared = 4;
    score += (unsigned long)line_points[cleared] * (level + 1);
    lines_cleared_total += cleared;
    level = lines_cleared_total / 10;
}

static void lock_piece(void)
{
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            if (piece_cell(current_type, current_rotation, row, col)) {
                int board_row = current_y + row;
                int board_col = current_x + col;
                if (board_row >= 0 && board_row < BOARD_ROWS &&
                    board_col >= 0 && board_col < BOARD_COLUMNS)
                    board[board_row][board_col] =
                        (unsigned char)(current_type + 1);
            }
    clear_lines();
    spawn_piece();
}

static unsigned long drop_interval(void)
{
    unsigned long interval = 700UL - (unsigned long)level * 40UL;
    return interval < 100UL ? 100UL : interval;
}

void tetris_app_init(void)
{
    for (int row = 0; row < BOARD_ROWS; ++row)
        for (int col = 0; col < BOARD_COLUMNS; ++col)
            board[row][col] = 0;
    score = 0;
    level = 0;
    lines_cleared_total = 0;
    game_over = 0;
    drop_counter = 0;
    next_type = tetris_random_piece();
    spawn_piece();
}

void tetris_app_key(int value)
{
    if (value == 'r' || value == 'R') {
        tetris_app_init();
        return;
    }
    if (game_over) return;
    if (value == 'a' || value == 'A') {
        if (piece_fits(current_type, current_rotation, current_x - 1,
            current_y))
            --current_x;
    } else if (value == 'd' || value == 'D') {
        if (piece_fits(current_type, current_rotation, current_x + 1,
            current_y))
            ++current_x;
    } else if (value == 's' || value == 'S') {
        if (piece_fits(current_type, current_rotation, current_x,
            current_y + 1)) {
            ++current_y;
            ++score;
        } else lock_piece();
        drop_counter = 0;
    } else if (value == 'w' || value == 'W') {
        int next_rotation = (current_rotation + 1) % 4;
        if (piece_fits(current_type, next_rotation, current_x, current_y))
            current_rotation = next_rotation;
    } else if (value == ' ' || value == '\n') {
        while (piece_fits(current_type, current_rotation, current_x,
            current_y + 1))
            ++current_y;
        lock_piece();
        drop_counter = 0;
    }
}

void tetris_app_tick(void)
{
    if (game_over) return;
    ++drop_counter;
    if (drop_counter < drop_interval()) return;
    drop_counter = 0;
    if (piece_fits(current_type, current_rotation, current_x, current_y + 1))
        ++current_y;
    else
        lock_piece();
}

static unsigned int board_x(void)
{
    unsigned int content_width = BOARD_COLUMNS * CELL_SIZE + PANEL_GAP +
        PANEL_WIDTH;
    return (framebuffer_width() - content_width) / 2;
}

static unsigned int board_y(void)
{
    return (framebuffer_height() - BOARD_ROWS * CELL_SIZE) / 2 + 18;
}

static void format_number(char *output, unsigned int capacity,
    unsigned long value)
{
    char digits[20];
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

static void draw_label_number(unsigned int x, unsigned int y,
    const char *label, unsigned long value)
{
    char buffer[48];
    unsigned int index = 0;
    while (label[index] != '\0' && index + 1 < sizeof(buffer)) {
        buffer[index] = label[index];
        ++index;
    }
    char number[20];
    format_number(number, sizeof(number), value);
    unsigned int number_index = 0;
    while (number[number_index] != '\0' && index + 1 < sizeof(buffer))
        buffer[index++] = number[number_index++];
    buffer[index] = '\0';
    framebuffer_draw_text(x, y, buffer, COLOR_TEXT, 1);
}

void tetris_app_draw_fullscreen(void)
{
    unsigned int left = board_x();
    unsigned int top = board_y();
    unsigned int panel_x = left + BOARD_COLUMNS * CELL_SIZE + PANEL_GAP;

    framebuffer_clear(COLOR_BG);
    framebuffer_draw_text(24, 24, "TETRIS", COLOR_TEXT, 2);
    framebuffer_draw_text(24, 54,
        "A/D move   S soft drop   W rotate   SPACE hard drop   R restart   CTRL+C shell",
        COLOR_TEXT, 1);

    framebuffer_fill_rect(left - 4, top - 4, BOARD_COLUMNS * CELL_SIZE + 8,
        BOARD_ROWS * CELL_SIZE + 8, COLOR_PANEL);
    for (unsigned int row = 0; row < BOARD_ROWS; ++row)
        for (unsigned int column = 0; column < BOARD_COLUMNS; ++column) {
            unsigned int x = left + column * CELL_SIZE;
            unsigned int y = top + row * CELL_SIZE;
            unsigned int cell_value = board[row][column];
            unsigned int color = cell_value != 0 ?
                piece_colors[cell_value - 1] : COLOR_BOARD_BG;
            framebuffer_fill_rect(x, y, CELL_SIZE - 2, CELL_SIZE - 2, color);
        }

    if (!game_over)
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col) {
                if (!piece_cell(current_type, current_rotation, row, col))
                    continue;
                int board_row = current_y + row;
                int board_col = current_x + col;
                if (board_row < 0 || board_row >= BOARD_ROWS ||
                    board_col < 0 || board_col >= BOARD_COLUMNS) continue;
                unsigned int x = left + (unsigned int)board_col * CELL_SIZE;
                unsigned int y = top + (unsigned int)board_row * CELL_SIZE;
                framebuffer_fill_rect(x, y, CELL_SIZE - 2, CELL_SIZE - 2,
                    piece_colors[current_type]);
            }

    framebuffer_fill_rect(panel_x - 4, top - 4, PANEL_WIDTH + 8, 180,
        COLOR_PANEL);
    framebuffer_draw_text(panel_x, top, "NEXT", COLOR_TEXT, 1);
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            unsigned int x = panel_x + (unsigned int)col * PREVIEW_CELL;
            unsigned int y = top + 20 + (unsigned int)row * PREVIEW_CELL;
            unsigned int color = piece_cell(next_type, 0, row, col) ?
                piece_colors[next_type] : COLOR_BOARD_BG;
            framebuffer_fill_rect(x, y, PREVIEW_CELL - 2, PREVIEW_CELL - 2,
                color);
        }

    draw_label_number(panel_x, top + 100, "Score: ", score);
    draw_label_number(panel_x, top + 120, "Level: ", level);
    draw_label_number(panel_x, top + 140, "Lines: ", lines_cleared_total);

    if (game_over)
        framebuffer_draw_text(24, framebuffer_height() - 28,
            "GAME OVER - press R to restart", COLOR_GAMEOVER, 1);
}