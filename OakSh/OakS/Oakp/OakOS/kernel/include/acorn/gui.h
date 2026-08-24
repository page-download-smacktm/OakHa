#ifndef ACORN_GUI_H
#define ACORN_GUI_H

void gui_init(void);
void gui_tick(void);
void gui_keyboard_input(int value);
void gui_mouse_input(int x, int y, unsigned char buttons);
int gui_self_test(void);

#endif
