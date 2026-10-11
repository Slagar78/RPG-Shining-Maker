// clipboard.h
#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "map.h"           // теперь Map известен всем

typedef struct {
    int width, height;
    int *tiles;
    int *tiles2;
    int *rot;
    int *rot2;
    int *mirror_x;
    int *mirror_x2;
    int *mirror_y;
    int *mirror_y2;
    int *cell_type;
} Clipboard;

void clipboard_init(Clipboard *cb);
void clipboard_free(Clipboard *cb);
bool clipboard_is_empty(const Clipboard *cb);
void clipboard_copy(Clipboard *cb, const Map *map, int x1, int y1, int x2, int y2, int active_layer);
void clipboard_paste(const Clipboard *cb, Map *map, int dest_x, int dest_y, int layer);
void clipboard_draw_preview(const Clipboard *cb, SDL_Renderer *rend, SDL_Texture **tiles,
                            int tile_count, int cam_x, int cam_y, float zoom,
                            int mouse_tx, int mouse_ty);

#endif