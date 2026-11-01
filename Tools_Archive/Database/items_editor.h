// items_editor.h
#ifndef ITEMS_EDITOR_H
#define ITEMS_EDITOR_H

#include <SDL.h>
#include "../cJSON.h"

void items_init(cJSON *json_array, int count);
void items_draw_list(SDL_Renderer *renderer, int y_offset, int scroll);
int  items_get_total_height();
void items_handle_click(int mx, int my, int y_offset, int scroll);
void items_draw_edit_panel(SDL_Renderer *renderer, int px, int py);
void items_handle_input(SDL_Event *evt);
void items_save_to_file();
int  items_is_edit_active();
void items_reset_selection();
void items_adjust_scroll(int delta);
int  items_get_scroll();
void items_update_timer(void);
void items_set_window_height(int h);

#endif