#include "acorn/framebuffer.h"
#include "acorn/assets.h"

#define MULTIBOOT_FRAMEBUFFER_FLAG 0x00001000
#define MULTIBOOT_FRAMEBUFFER_RGB 1
#define FRAMEBUFFER_BPP 32

struct multiboot_framebuffer_info {
    unsigned int flags;
    unsigned char reserved[84];
    unsigned long address;
    unsigned int pitch;
    unsigned int width;
    unsigned int height;
    unsigned char bpp;
    unsigned char type;
    unsigned char red_position;
    unsigned char red_mask;
    unsigned char green_position;
    unsigned char green_mask;
    unsigned char blue_position;
    unsigned char blue_mask;
};

static volatile unsigned char *buffer;
static unsigned int pitch;
static unsigned int width;
static unsigned int height;
static unsigned char red_position;
static unsigned char red_mask;
static unsigned char green_position;
static unsigned char green_mask;
static unsigned char blue_position;
static unsigned char blue_mask;

static unsigned int pack_channel(unsigned int value, unsigned char position,
    unsigned char bits)
{
    unsigned int mask;
    if (bits == 0 || bits > 8) return 0;
    mask = (1u << bits) - 1;
    return ((value * mask + 127) / 255) << position;
}

static unsigned int unpack_channel(unsigned int pixel, unsigned char position,
    unsigned char bits)
{
    unsigned int mask;
    if (bits == 0 || bits > 8) return 0;
    mask = (1u << bits) - 1;
    return (((pixel >> position) & mask) * 255 + mask / 2) / mask;
}

static unsigned int pack_color(unsigned int color)
{
    return pack_channel((color >> 16) & 0xFF, red_position, red_mask) |
        pack_channel((color >> 8) & 0xFF, green_position, green_mask) |
        pack_channel(color & 0xFF, blue_position, blue_mask);
}

void framebuffer_init(unsigned long multiboot_info)
{
    struct multiboot_framebuffer_info *info =
        (struct multiboot_framebuffer_info *)multiboot_info;
    buffer = (volatile unsigned char *)0;
    width = 0;
    height = 0;
    if (multiboot_info == 0 || (info->flags & MULTIBOOT_FRAMEBUFFER_FLAG) == 0 ||
        info->type != MULTIBOOT_FRAMEBUFFER_RGB || info->bpp != FRAMEBUFFER_BPP ||
        info->address == 0 || info->pitch < info->width * 4)
        return;
    buffer = (volatile unsigned char *)info->address;
    pitch = info->pitch;
    width = info->width;
    height = info->height;
    const unsigned char *raw_info = (const unsigned char *)info;
    red_position = raw_info[112];
    red_mask = raw_info[113];
    green_position = raw_info[114];
    green_mask = raw_info[115];
    blue_position = raw_info[116];
    blue_mask = raw_info[117];
}

int framebuffer_available(void)
{
    return buffer != (volatile unsigned char *)0;
}

unsigned int framebuffer_width(void) { return width; }
unsigned int framebuffer_height(void) { return height; }

void framebuffer_draw_image(unsigned int x, unsigned int y,
    unsigned int target_width, unsigned int target_height,
    unsigned int source_width, unsigned int source_height,
    const unsigned char *pixels, const unsigned char *alpha)
{
    if (!framebuffer_available() || target_width == 0 || target_height == 0) return;
    for (unsigned int row = 0; row < target_height; ++row) {
        unsigned int source_row = row * source_height / target_height;
        for (unsigned int column = 0; column < target_width; ++column) {
            unsigned int screen_x = x + column;
            unsigned int screen_y = y + row;
            if (screen_x >= width || screen_y >= height) continue;
            unsigned int source_column = column * source_width / target_width;
            unsigned int index = source_row * source_width + source_column;
            unsigned int opacity = alpha[index];
            if (opacity == 0) continue;
            unsigned int color = (pixels[index * 3] << 16) |
                (pixels[index * 3 + 1] << 8) | pixels[index * 3 + 2];
            if (opacity < 255) {
                unsigned int background = framebuffer_read_pixel(screen_x, screen_y);
                unsigned int red = (((color >> 16) & 255) * opacity +
                    ((background >> 16) & 255) * (255 - opacity)) / 255;
                unsigned int green = (((color >> 8) & 255) * opacity +
                    ((background >> 8) & 255) * (255 - opacity)) / 255;
                unsigned int blue = ((color & 255) * opacity +
                    (background & 255) * (255 - opacity)) / 255;
                color = (red << 16) | (green << 8) | blue;
            }
            framebuffer_fill_rect(screen_x, screen_y, 1, 1, color);
        }
    }
}

#define FONT_WIDTH 7
#define FONT_HEIGHT 9
#define FONT_ADVANCE 8
#define FONT_LINE_HEIGHT 11

static unsigned char glyph(char value, unsigned int row)
{
    static const unsigned char digits[10][FONT_HEIGHT] = {
        {30,51,55,59,51,51,51,30,0}, {12,28,12,12,12,12,12,63,0},
        {30,51,3,6,12,24,48,63,0}, {30,51,3,14,3,3,51,30,0},
        {6,14,30,54,102,127,6,6,0}, {63,48,48,62,3,3,51,30,0},
        {14,24,48,62,51,51,51,30,0}, {63,3,6,12,24,24,24,24,0},
        {30,51,51,30,51,51,51,30,0}, {30,51,51,31,3,3,6,28,0}
    };
    static const unsigned char letters[26][FONT_HEIGHT] = {
        {12,30,51,51,63,51,51,51,0}, {62,51,51,62,51,51,51,62,0},
        {30,51,48,48,48,48,51,30,0}, {62,51,51,51,51,51,51,62,0},
        {63,48,48,62,48,48,48,63,0}, {63,48,48,62,48,48,48,48,0},
        {30,51,48,48,55,51,51,31,0}, {51,51,51,63,51,51,51,51,0},
        {30,12,12,12,12,12,12,30,0}, {7,3,3,3,3,51,51,30,0},
        {51,54,60,56,60,54,51,51,0}, {48,48,48,48,48,48,48,63,0},
        {99,119,127,107,99,99,99,99,0}, {51,59,63,55,51,51,51,51,0},
        {30,51,51,51,51,51,51,30,0}, {62,51,51,62,48,48,48,48,0},
        {30,51,51,51,51,59,54,29,0}, {62,51,51,62,60,54,51,51,0},
        {30,51,48,30,3,3,51,30,0}, {63,12,12,12,12,12,12,12,0},
        {51,51,51,51,51,51,51,30,0}, {51,51,51,51,51,51,30,12,0},
        {99,99,99,107,127,119,99,99,0}, {51,51,30,12,30,51,51,51,0},
        {51,51,51,30,12,12,12,12,0}, {63,3,6,12,24,48,48,63,0}
    };
    if (row >= FONT_HEIGHT) return 0;
    if (value >= '0' && value <= '9') return digits[value - '0'][row];
    if (value >= 'a' && value <= 'z') value -= 'a' - 'A';
    if (value >= 'A' && value <= 'Z') return letters[value - 'A'][row];
    if (value == '-') return row == 4 ? 63 : 0;
    if (value == ':') return row == 2 || row == 6 ? 12 : 0;
    if (value == '>') return row == 2 || row == 6 ? 48 : (row == 3 || row == 5 ? 24 : 0);
    if (value == '/') return row == 1 || row == 2 || row == 4 || row == 5 || row == 7 ? 4 : 0;
    if (value == '*') return row == 2 || row == 6 ? 8 : (row == 3 || row == 4 || row == 5 ? 28 : 0);
    if (value == ' ') return 0;
    return row == 4 ? 63 : 0;
}

void framebuffer_draw_text(unsigned int x, unsigned int y, const char *text,
    unsigned int color, unsigned int scale)
{
    if (!framebuffer_available() || text == (const char *)0 || scale == 0) return;
    while (*text != '\0') {
        if (*text == '\n') { y += FONT_LINE_HEIGHT * scale; x = 0; ++text; continue; }
        for (unsigned int row = 0; row < FONT_HEIGHT; ++row) {
            unsigned char bits = glyph(*text, row);
            for (unsigned int column = 0; column < FONT_WIDTH; ++column)
                if ((bits & (1u << (FONT_WIDTH - 1 - column))) != 0)
                    framebuffer_fill_rect(x + column * scale, y + row * scale,
                        scale, scale, color);
        }
        x += FONT_ADVANCE * scale;
        ++text;
    }
}

void framebuffer_clear(unsigned int color)
{
    framebuffer_fill_rect(0, 0, width, height, color);
}

void framebuffer_fill_rect(unsigned int x, unsigned int y,
    unsigned int rectangle_width, unsigned int rectangle_height,
    unsigned int color)
{
    if (!framebuffer_available() || x >= width || y >= height) return;
    if (rectangle_width > width - x) rectangle_width = width - x;
    if (rectangle_height > height - y) rectangle_height = height - y;
    unsigned int pixel = pack_color(color);
    for (unsigned int row = 0; row < rectangle_height; ++row) {
        volatile unsigned int *line = (volatile unsigned int *)(buffer +
            (y + row) * pitch + x * 4);
        for (unsigned int column = 0; column < rectangle_width; ++column)
            line[column] = pixel;
    }
}

unsigned int framebuffer_read_pixel(unsigned int x, unsigned int y)
{
    if (!framebuffer_available() || x >= width || y >= height) return 0;
    volatile unsigned int *pixel = (volatile unsigned int *)(buffer +
        y * pitch + x * 4);
    unsigned int value = *pixel;
    unsigned int red = unpack_channel(value, red_position, red_mask);
    unsigned int green = unpack_channel(value, green_position, green_mask);
    unsigned int blue = unpack_channel(value, blue_position, blue_mask);
    return (red << 16) | (green << 8) | blue;
}

int framebuffer_self_test(void)
{
    if (!framebuffer_available() || width < 320 || height < 200) return 0;
    framebuffer_clear(0x101820);
    framebuffer_fill_rect(0, 0, width, 56, 0x183A5A);
    framebuffer_fill_rect(32, 96, width / 3, height - 128, 0x244052);
    framebuffer_fill_rect(width / 3 + 56, 96, width / 2, 96, 0x2A5B63);
    framebuffer_fill_rect(width / 3 + 56, 224, width / 2, 184, 0x1D303A);
    framebuffer_fill_rect(width - 120, 16, 88, 24, 0x4CC9A4);
    return 1;
}
