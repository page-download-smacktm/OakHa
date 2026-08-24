#include "acorn/framebuffer.h"

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
    unsigned char reserved2;
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
static unsigned char green_position;
static unsigned char blue_position;

static unsigned int pack_color(unsigned int color)
{
    unsigned int red = (color >> 16) & 0xFF;
    unsigned int green = (color >> 8) & 0xFF;
    unsigned int blue = color & 0xFF;
    return (red << red_position) | (green << green_position) |
        (blue << blue_position);
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
    red_position = info->red_position;
    green_position = info->green_position;
    blue_position = info->blue_position;
}

int framebuffer_available(void)
{
    return buffer != (volatile unsigned char *)0;
}

unsigned int framebuffer_width(void) { return width; }
unsigned int framebuffer_height(void) { return height; }

static unsigned char glyph(char value, unsigned int row)
{
    static const unsigned char digits[10][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12}
    };
    static const unsigned char letters[26][7] = {
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
        {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
    };
    if (value >= '0' && value <= '9') return digits[value - '0'][row];
    if (value >= 'a' && value <= 'z') value -= 'a' - 'A';
    if (value >= 'A' && value <= 'Z') return letters[value - 'A'][row];
    if (value == '-') return row == 3 ? 31 : 0;
    if (value == ':') return row == 2 || row == 5 ? 4 : 0;
    if (value == '>') return row == 2 || row == 4 ? 16 : (row == 3 ? 8 : 0);
    if (value == '/') return row == 1 || row == 2 || row == 4 || row == 5 ? 4 : 0;
    if (value == ' ') return 0;
    return row == 3 ? 31 : 0;
}

void framebuffer_draw_text(unsigned int x, unsigned int y, const char *text,
    unsigned int color, unsigned int scale)
{
    if (!framebuffer_available() || text == (const char *)0 || scale == 0) return;
    while (*text != '\0') {
        if (*text == '\n') { y += 8 * scale; x = 0; ++text; continue; }
        for (unsigned int row = 0; row < 7; ++row) {
            unsigned char bits = glyph(*text, row);
            for (unsigned int column = 0; column < 5; ++column)
                if ((bits & (1u << (4 - column))) != 0)
                    framebuffer_fill_rect(x + column * scale, y + row * scale,
                        scale, scale, color);
        }
        x += 6 * scale;
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
    unsigned int red = (value >> red_position) & 0xFF;
    unsigned int green = (value >> green_position) & 0xFF;
    unsigned int blue = (value >> blue_position) & 0xFF;
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
