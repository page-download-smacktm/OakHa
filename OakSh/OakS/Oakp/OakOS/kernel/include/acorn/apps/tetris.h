#ifndef ACORN_APPS_TETRIS_H
#define ACORN_APPS_TETRIS_H

void tetris_app_init(void);
void tetris_app_key(int value);
void tetris_app_tick(void);
void tetris_app_draw_fullscreen(void);

#endif