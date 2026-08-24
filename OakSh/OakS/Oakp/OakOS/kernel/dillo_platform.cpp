#include "acorn/dillo_platform.h"
#include "acorn/framebuffer.h"

enum { DILLO_EVENT_QUEUE_SIZE = 64 };

static struct dillo_platform_event event_queue[DILLO_EVENT_QUEUE_SIZE];
static unsigned int event_head;
static unsigned int event_tail;
static unsigned int clip_x;
static unsigned int clip_y;
static unsigned int clip_width;
static unsigned int clip_height;

static void push_event(const struct dillo_platform_event *event)
{
    unsigned int next = (event_head + 1) % DILLO_EVENT_QUEUE_SIZE;
    if (next == event_tail) return;
    event_queue[event_head] = *event;
    event_head = next;
}

extern "C" int dillo_platform_available(void)
{
    return framebuffer_available();
}

extern "C" unsigned int dillo_platform_width(void)
{
    return framebuffer_width();
}

extern "C" unsigned int dillo_platform_height(void)
{
    return framebuffer_height();
}

extern "C" void dillo_platform_clear(unsigned int color)
{
    framebuffer_clear(color);
}

extern "C" void dillo_platform_fill_rect(unsigned int x, unsigned int y,
    unsigned int width, unsigned int height, unsigned int color)
{
    if (x < clip_x) {
        width -= x < clip_x && width > clip_x - x ? clip_x - x : width;
        x = clip_x;
    }
    if (y < clip_y) {
        height -= y < clip_y && height > clip_y - y ? clip_y - y : height;
        y = clip_y;
    }
    if (x >= clip_x + clip_width || y >= clip_y + clip_height) return;
    if (width > clip_x + clip_width - x) width = clip_x + clip_width - x;
    if (height > clip_y + clip_height - y) height = clip_y + clip_height - y;
    if (width == 0 || height == 0) return;
    framebuffer_fill_rect(x, y, width, height, color);
}

extern "C" void dillo_platform_draw_text(unsigned int x, unsigned int y,
    const char *text, unsigned int color, unsigned int scale)
{
    framebuffer_draw_text(x, y, text, color, scale);
}

extern "C" void dillo_platform_begin_frame(void)
{
    clip_x = 0;
    clip_y = 0;
    clip_width = framebuffer_width();
    clip_height = framebuffer_height();
}

extern "C" void dillo_platform_set_clip(unsigned int x, unsigned int y,
    unsigned int width, unsigned int height)
{
    clip_x = x;
    clip_y = y;
    clip_width = width;
    clip_height = height;
}

extern "C" void dillo_platform_end_frame(void)
{
}

extern "C" void dillo_platform_push_key(int value)
{
    struct dillo_platform_event event = { DILLO_PLATFORM_EVENT_KEY, value,
        0, 0, 0 };
    push_event(&event);
}

extern "C" void dillo_platform_push_mouse(int x, int y, unsigned char buttons)
{
    struct dillo_platform_event event = { DILLO_PLATFORM_EVENT_MOUSE, 0,
        x, y, buttons };
    push_event(&event);
}

extern "C" int dillo_platform_poll_event(struct dillo_platform_event *event)
{
    if (event == (struct dillo_platform_event *)0 || event_head == event_tail)
        return 0;
    *event = event_queue[event_tail];
    event_tail = (event_tail + 1) % DILLO_EVENT_QUEUE_SIZE;
    return 1;
}

extern "C" void dillo_platform_clear_events(void)
{
    event_tail = event_head;
}

extern "C" int dillo_platform_self_test(void)
{
    struct dillo_platform_event event;
    dillo_platform_clear_events();
    dillo_platform_push_key('d');
    dillo_platform_push_mouse(12, 34, 1);
    if (!dillo_platform_poll_event(&event) ||
        event.type != DILLO_PLATFORM_EVENT_KEY || event.value != 'd') return 0;
    if (!dillo_platform_poll_event(&event) ||
        event.type != DILLO_PLATFORM_EVENT_MOUSE || event.x != 12 ||
        event.y != 34 || event.buttons != 1) return 0;
    if (dillo_platform_poll_event(&event)) return 0;
    dillo_platform_clear_events();
    return 1;
}
