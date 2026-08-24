#include "acorn/apps/files.h"
#include "acorn/framebuffer.h"

#define COLOR_WINDOW 0x1D303A
#define COLOR_TITLE 0x2A5B63
#define COLOR_TEXT 0xD8F3DC

void files_app_draw(void)
{
    framebuffer_fill_rect(318, 180, 410, 300, COLOR_WINDOW);
    framebuffer_fill_rect(318, 180, 410, 30, COLOR_TITLE);
    framebuffer_draw_text(334, 190, "FILES", COLOR_TEXT, 2);
    framebuffer_draw_text(338, 236, "PERSISTENT FILES", COLOR_TEXT, 1);
    framebuffer_draw_text(338, 264, "/tmp/hello", COLOR_TEXT, 2);
    framebuffer_draw_text(338, 296, "/persistent", COLOR_TEXT, 2);
    framebuffer_draw_text(338, 328, "/persistent-dir", COLOR_TEXT, 2);
}
