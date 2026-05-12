// actors_editor.h
#ifndef ACTORS_EDITOR_H
#define ACTORS_EDITOR_H

#include <SDL.h>
#include "../cJSON.h"

void actors_init(cJSON *json_array, int count);
void actors_draw_list(SDL_Renderer *renderer, int y_offset, int scroll);
int  actors_get_total_height(void);
void actors_handle_click(int mx, int my, int y_offset, int scroll);
void actors_draw_edit_panel(SDL_Renderer *renderer, int px, int py);
void actors_handle_input(SDL_Event *evt);
void actors_save_to_file(void);
int  actors_is_edit_active(void);
void actors_reset_selection(void);
void actors_adjust_scroll(int delta);
int  actors_get_scroll(void);
void actors_reload(void);
void actors_set_window_height(int h);
void actors_update_timer(void);

#endif