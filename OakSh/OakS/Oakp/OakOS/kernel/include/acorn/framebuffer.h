#ifndef ACORN_FRAMEBUFFER_H
#define ACORN_FRAMEBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

void framebuffer_init(unsigned long multiboot_info);
int framebuffer_available(void);
unsigned int framebuffer_width(void);
unsigned int framebuffer_height(void);
void framebuffer_clear(unsigned int color);
void framebuffer_fill_rect(unsigned int x, unsigned int y,
    unsigned int width, unsigned int height, unsigned int color);
unsigned int framebuffer_read_pixel(unsigned int x, unsigned int y);
void framebuffer_draw_text(unsigned int x, unsigned int y, const char *text,
    unsigned int color, unsigned int scale);
int framebuffer_self_test(void);

#ifdef __cplusplus
}
#endif

#endif
