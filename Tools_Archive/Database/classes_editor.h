// classes_editor.h
#ifndef CLASSES_EDITOR_H
#define CLASSES_EDITOR_H

#include <SDL.h>
#include "../cJSON.h"

void classes_init(cJSON *json_array, int count);
void classes_draw_list(SDL_Renderer *renderer, int y_offset, int scroll);
int  classes_get_total_height(void);
void classes_handle_click(int mx, int my, int y_offset, int scroll);
void classes_draw_edit_panel(SDL_Renderer *renderer, int px, int py);
void classes_handle_input(SDL_Event *evt);
void classes_save_to_file(void);
int  classes_is_edit_active(void);
void classes_reset_selection(void);
void classes_adjust_scroll(int delta);
int  classes_get_scroll(void);
void classes_reload(void);
void classes_update_timer(void);
void classes_set_window_height(int h);
void enemies_clamp_scroll(int start_y);

#endif