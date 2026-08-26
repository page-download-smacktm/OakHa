#ifndef ACORN_APPS_MINESWEEPER_H
#define ACORN_APPS_MINESWEEPER_H

void minesweeper_app_init(void);
void minesweeper_app_key(int value);
void minesweeper_app_mouse(int x, int y, unsigned char buttons,
	unsigned char previous_buttons);
void minesweeper_app_tick(void);
void minesweeper_app_draw_fullscreen(void);

#endif
