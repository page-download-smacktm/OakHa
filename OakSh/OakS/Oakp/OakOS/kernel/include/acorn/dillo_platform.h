#ifndef ACORN_DILLO_PLATFORM_H
#define ACORN_DILLO_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

int dillo_platform_available(void);
unsigned int dillo_platform_width(void);
unsigned int dillo_platform_height(void);
void dillo_platform_clear(unsigned int color);
void dillo_platform_fill_rect(unsigned int x, unsigned int y,
    unsigned int width, unsigned int height, unsigned int color);
void dillo_platform_draw_text(unsigned int x, unsigned int y,
    const char *text, unsigned int color, unsigned int scale);
void dillo_platform_begin_frame(void);
void dillo_platform_set_clip(unsigned int x, unsigned int y,
    unsigned int width, unsigned int height);
void dillo_platform_end_frame(void);

enum dillo_platform_event_type {
    DILLO_PLATFORM_EVENT_NONE,
    DILLO_PLATFORM_EVENT_KEY,
    DILLO_PLATFORM_EVENT_MOUSE
};

struct dillo_platform_event {
    enum dillo_platform_event_type type;
    int value;
    int x;
    int y;
    unsigned char buttons;
};

void dillo_platform_push_key(int value);
void dillo_platform_push_mouse(int x, int y, unsigned char buttons);
int dillo_platform_poll_event(struct dillo_platform_event *event);
void dillo_platform_clear_events(void);
int dillo_platform_self_test(void);

#ifdef __cplusplus
}
#endif

#endif
