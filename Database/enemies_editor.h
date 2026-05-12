// enemies_editor.h
#ifndef ENEMIES_EDITOR_H
#define ENEMIES_EDITOR_H

#include <SDL.h>
#include "../cJSON.h"

void enemies_init(cJSON *enemies_array, int count);
void enemies_draw_list(SDL_Renderer *r, int start_y, int scroll);
void enemies_handle_click(int mx, int my, int start_y, int scroll);
void enemies_draw_edit_panel(SDL_Renderer *r, int px, int py);
int  enemies_get_scroll(void);
void enemies_adjust_scroll(int delta);
void enemies_reset_selection(void);
void enemies_handle_input(SDL_Event *evt);
int  enemies_is_edit_active(void);
void enemies_update_timer(void);
void enemies_set_window_height(int h);
void enemies_handle_buttons(int mx, int my, int px, int py);
void enemies_handle_edit_panel_click(int mx, int my, int px, int py);

#endif