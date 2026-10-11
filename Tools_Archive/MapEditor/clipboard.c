// clipboard.c
#include "clipboard.h"
#include <stdlib.h>
#include <string.h>

// Геометрические константы редактора (дублируем для независимости модуля)
#define MAP_X 300
#define MAP_Y 34
#define TILE_SIZE 48

void clipboard_init(Clipboard *cb) {
    memset(cb, 0, sizeof(Clipboard));
}

void clipboard_free(Clipboard *cb) {
    free(cb->tiles); free(cb->tiles2);
    free(cb->rot); free(cb->rot2);
    free(cb->mirror_x); free(cb->mirror_x2);
    free(cb->mirror_y); free(cb->mirror_y2);
    free(cb->cell_type);
    memset(cb, 0, sizeof(Clipboard));
}

bool clipboard_is_empty(const Clipboard *cb) {
    return cb->width == 0 || cb->height == 0;
}

static int clamp(int v, int min, int max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static void normalize_rect(int *x1, int *y1, int *x2, int *y2) {
    if (*x1 > *x2) { int t = *x1; *x1 = *x2; *x2 = t; }
    if (*y1 > *y2) { int t = *y1; *y1 = *y2; *y2 = t; }
}

void clipboard_copy(Clipboard *cb, const Map *map, int x1, int y1, int x2, int y2, int active_layer) {
    clipboard_free(cb);
    normalize_rect(&x1, &y1, &x2, &y2);
    x1 = clamp(x1, 0, map->width - 1);
    y1 = clamp(y1, 0, map->height - 1);
    x2 = clamp(x2, 0, map->width - 1);
    y2 = clamp(y2, 0, map->height - 1);

    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;
    if (w <= 0 || h <= 0) return;

    cb->width = w; cb->height = h;
    int sz = w * h;
    cb->tiles     = (int*)malloc(sz * sizeof(int));
    cb->tiles2    = (int*)malloc(sz * sizeof(int));
    cb->rot       = (int*)malloc(sz * sizeof(int));
    cb->rot2      = (int*)malloc(sz * sizeof(int));
    cb->mirror_x  = (int*)malloc(sz * sizeof(int));
    cb->mirror_x2 = (int*)malloc(sz * sizeof(int));
    cb->mirror_y  = (int*)malloc(sz * sizeof(int));
    cb->mirror_y2 = (int*)malloc(sz * sizeof(int));
    cb->cell_type = (int*)malloc(sz * sizeof(int));

    // Указатели на данные активного слоя
    const int *src_tiles   = (active_layer == 0) ? map->tiles : map->tiles2;
    const int *src_rot     = (active_layer == 0) ? map->rot : map->rot2;
    const int *src_mirror_x = (active_layer == 0) ? map->mirror_x : map->mirror_x2;
    const int *src_mirror_y = (active_layer == 0) ? map->mirror_y : map->mirror_y2;

    for (int y = y1, dy = 0; y <= y2; y++, dy++) {
        for (int x = x1, dx = 0; x <= x2; x++, dx++) {
            int src_idx = x * map->height + y;
            int dst_idx = dx * h + dy;

            int tile = src_tiles[src_idx];
            cb->tiles[dst_idx]  = tile;
            cb->tiles2[dst_idx] = tile;   // оба слоя получают одно и то же

            int r = src_rot[src_idx];
            cb->rot[dst_idx]  = r;
            cb->rot2[dst_idx] = r;

            int mx = src_mirror_x[src_idx];
            cb->mirror_x[dst_idx]  = mx;
            cb->mirror_x2[dst_idx] = mx;

            int my = src_mirror_y[src_idx];
            cb->mirror_y[dst_idx]  = my;
            cb->mirror_y2[dst_idx] = my;

            // cell_type копируем из общего массива
            cb->cell_type[dst_idx] = map->cell_type[src_idx];
        }
    }
}

void clipboard_paste(const Clipboard *cb, Map *map, int dest_x, int dest_y, int layer) {
    if (clipboard_is_empty(cb)) return;
    int w = cb->width, h = cb->height;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int tx = dest_x + dx;
            int ty = dest_y + dy;
            if (tx < 0 || tx >= map->width || ty < 0 || ty >= map->height) continue;
            int dst_idx = tx * map->height + ty;
            int src_idx = dx * h + dy;

            if (layer == 0) {
                map->tiles[dst_idx]     = cb->tiles[src_idx];
                map->rot[dst_idx]       = cb->rot[src_idx];
                map->mirror_x[dst_idx]  = cb->mirror_x[src_idx];
                map->mirror_y[dst_idx]  = cb->mirror_y[src_idx];
                map->cell_type[dst_idx] = cb->cell_type[src_idx];
            } else {
                map->tiles2[dst_idx]    = cb->tiles2[src_idx];
                map->rot2[dst_idx]      = cb->rot2[src_idx];
                map->mirror_x2[dst_idx] = cb->mirror_x2[src_idx];
                map->mirror_y2[dst_idx] = cb->mirror_y2[src_idx];
                // cell_type не трогаем
            }
        }
    }
}

void clipboard_draw_preview(const Clipboard *cb, SDL_Renderer *rend, SDL_Texture **tiles,
                            int tile_count, int cam_x, int cam_y, float zoom,
                            int mouse_tx, int mouse_ty) {
    if (clipboard_is_empty(cb) || !tiles) return;
    float ts = TILE_SIZE * zoom;
    for (int dy = 0; dy < cb->height; dy++) {
        for (int dx = 0; dx < cb->width; dx++) {
            int idx = dx * cb->height + dy;
            int tile_id = cb->tiles[idx];
            if (tile_id < 0 || tile_id >= tile_count) continue;
            SDL_Texture *tex = tiles[tile_id];
            SDL_FRect dst = {
                MAP_X + (mouse_tx + dx) * ts - cam_x * zoom,
                MAP_Y + (mouse_ty + dy) * ts - cam_y * zoom,
                ts, ts
            };
            SDL_SetTextureAlphaMod(tex, 120);
            SDL_RenderCopyF(rend, tex, NULL, &dst);
            SDL_SetTextureAlphaMod(tex, 255);
        }
    }
}