#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <windows.h>
#include <commdlg.h>
#include "map.h"
#include "cJSON.h"

// ─── Геометрия окна ───────────────────────────
#define WINDOW_W 1280
#define WINDOW_H 720
#define TOOLBAR_H 34

#define LEFT_PANEL_W 300
#define RIGHT_PANEL_W 200
#define SCROLLBAR_SIZE 48

#define MAP_X LEFT_PANEL_W
#define MAP_Y TOOLBAR_H
#define MAP_VISIBLE_W (WINDOW_W - LEFT_PANEL_W - RIGHT_PANEL_W - SCROLLBAR_SIZE)
#define MAP_VISIBLE_H (WINDOW_H - TOOLBAR_H - SCROLLBAR_SIZE)

#define TILE_SIZE 48
#define FONT_SIZE 16

// ─── Структуры данных ───
typedef struct {
    Map *maps;
    int map_count;
    int current_map;
    int map_list_scroll;
} MapList;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    SDL_Texture **tiles;
    int           tile_count;
    char          tileset_path[256];
    bool          tileset_loaded;

    MapList map_list;

    float cam_x, cam_y;
    int   panning;
    Sint32 pan_start_x, pan_start_y;

    float zoom;
    bool show_layer1;
    bool show_layer2;

    bool save_blink_active;
    Uint32 save_blink_time;

    bool scrollbar_drag_h;
    bool scrollbar_drag_v;
    int  scroll_drag_mouse_offset_x;
    int  scroll_drag_mouse_offset_y;
} Editor;

// ─── Прототипы ────────────────────────────────
Map* current_map(Editor *ed);
void find_first_tileset_path(char *out, size_t out_len);
void get_logical_mouse(Editor *ed, int *mx, int *my);
void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, SDL_Color color);

// ─── Реализация ──────────────────────────────

void get_logical_mouse(Editor *ed, int *mx, int *my) {
    SDL_GetMouseState(mx, my);
    int win_w, win_h;
    SDL_GetWindowSize(ed->window, &win_w, &win_h);
    if (win_w != WINDOW_W || win_h != WINDOW_H) {
        *mx = (int)((float)*mx * WINDOW_W / win_w + 0.5f);
        *my = (int)((float)*my * WINDOW_H / win_h + 0.5f);
    }
}

void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst = { cx - surf->w/2, cy - surf->h/2, surf->w, surf->h };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void editor_init(Editor *ed) {
    memset(ed, 0, sizeof(Editor));
    ed->cam_x = ed->cam_y = 0;
    ed->panning = 0;
    ed->save_blink_active = false;
    ed->map_list.maps = NULL;
    ed->map_list.map_count = 0;
    ed->map_list.current_map = 0;
    ed->map_list.map_list_scroll = 0;
    ed->show_layer1 = true;
    ed->show_layer2 = true;
    ed->zoom = 1.0f;
    ed->scrollbar_drag_h = false;
    ed->scrollbar_drag_v = false;
}

void free_tileset(Editor *ed) {
    if (ed->tiles) {
        for (int i = 0; i < ed->tile_count; i++) SDL_DestroyTexture(ed->tiles[i]);
        free(ed->tiles);
        ed->tiles = NULL;
    }
    ed->tile_count = 0;
    ed->tileset_loaded = false;
}

int load_tileset(Editor *ed, const char *path) {
    free_tileset(ed);

    char full_tileset[512];
    if (path[0] && path[1] == ':')
        snprintf(full_tileset, sizeof(full_tileset), "%s", path);
    else
        snprintf(full_tileset, sizeof(full_tileset), "../%s", path);

    SDL_Surface *surface = IMG_Load(full_tileset);
    if (!surface) return 0;

    int cols = surface->w / TILE_SIZE;
    int rows = surface->h / TILE_SIZE;
    int palette_cols = 8;                    // ← ширина полосы как в оригинале
    int strips = cols / palette_cols;

    ed->tile_count = cols * rows;
    ed->tiles = (SDL_Texture**)malloc(ed->tile_count * sizeof(SDL_Texture*));

    int idx = 0;
    for (int strip = 0; strip < strips; strip++) {
        int start_col = strip * palette_cols;
        int end_col = start_col + palette_cols;
        for (int r = 0; r < rows; r++) {
            for (int c = start_col; c < end_col; c++) {
                SDL_Rect src = { c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE };
                SDL_Surface *tile_surf = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
                SDL_BlitSurface(surface, &src, tile_surf, NULL);
                ed->tiles[idx++] = SDL_CreateTextureFromSurface(ed->renderer, tile_surf);
                SDL_FreeSurface(tile_surf);
            }
        }
    }

    SDL_FreeSurface(surface);
    get_relative_path(path, ed->tileset_path, sizeof(ed->tileset_path));
    ed->tileset_loaded = true;
    return 1;
}

Map* current_map(Editor *ed) {
    if (ed->map_list.map_count == 0) return NULL;
    return &ed->map_list.maps[ed->map_list.current_map];
}

// Загрузка списка карт из entries.json
void load_map_list(Editor *ed) {
    // Освобождаем предыдущие карты
    for (int i = 0; i < ed->map_list.map_count; i++) map_free(&ed->map_list.maps[i]);
    free(ed->map_list.maps);
    ed->map_list.maps = NULL;
    ed->map_list.map_count = 0;
    ed->map_list.current_map = 0;

    FILE *f = fopen("../data/maps/entries.json", "r");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *data = malloc(len+1);
    if (!data) { fclose(f); return; }
    fread(data, 1, len, f); data[len] = '\0'; fclose(f);
    cJSON *entries = cJSON_Parse(data);
    free(data);
    if (!entries) return;

    int count = cJSON_GetArraySize(entries);
    for (int i = 0; i < count; i++) {
        cJSON *entry = cJSON_GetArrayItem(entries, i);
        cJSON *folder_json = cJSON_GetObjectItem(entry, "folder");
        cJSON *name_json = cJSON_GetObjectItem(entry, "name");
        if (!folder_json || !name_json) continue;
        const char *folder = folder_json->valuestring;
        const char *name = name_json->valuestring;

        char layout_path[512];
        snprintf(layout_path, sizeof(layout_path), "../data/maps/%s/layout.json", folder);
        Map temp;
        if (map_load_from_json(&temp, layout_path)) {
            safe_strcpy(temp.name, sizeof(temp.name), name);
            safe_strcpy(temp.folder, sizeof(temp.folder), folder);
            ed->map_list.map_count++;
            ed->map_list.maps = realloc(ed->map_list.maps, ed->map_list.map_count * sizeof(Map));
            ed->map_list.maps[ed->map_list.map_count - 1] = temp;
        }
    }
    cJSON_Delete(entries);
}

// Отрисовка карты
void render_map(Editor *ed) {
    Map *map = current_map(ed);
    if (!map || !ed->tileset_loaded) return;

    float zoom = ed->zoom;
    float scaled_tile = TILE_SIZE * zoom;

    SDL_Rect map_area = { MAP_X, MAP_Y, MAP_VISIBLE_W, MAP_VISIBLE_H };
    SDL_RenderSetClipRect(ed->renderer, &map_area);

    int start_x = (int)(ed->cam_x / TILE_SIZE);
    int start_y = (int)(ed->cam_y / TILE_SIZE);
    int end_x = start_x + (int)(MAP_VISIBLE_W / scaled_tile) + 1;
    int end_y = start_y + (int)(MAP_VISIBLE_H / scaled_tile) + 1;
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;
    if (end_x > map->width) end_x = map->width;
    if (end_y > map->height) end_y = map->height;

    // Первый слой
    if (ed->show_layer1) {
        for (int x = start_x; x < end_x; x++) {
            for (int y = start_y; y < end_y; y++) {
                int idx = x * map->height + y;
                int tile_id = map->tiles[idx];
                if (tile_id < 0 || tile_id >= ed->tile_count) continue;

                SDL_Texture *tex = ed->tiles[tile_id];
                double angle = map->rot[idx] * 90.0;
                SDL_RendererFlip flip = SDL_FLIP_NONE;
                if (map->mirror_x[idx]) flip |= SDL_FLIP_HORIZONTAL;
                if (map->mirror_y[idx]) flip |= SDL_FLIP_VERTICAL;

                SDL_FRect dst = {
                    MAP_X + (x * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (y * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile, scaled_tile
                };
                SDL_FPoint center = { scaled_tile / 2.0f, scaled_tile / 2.0f };
                SDL_RenderCopyExF(ed->renderer, tex, NULL, &dst, angle, &center, flip);
            }
        }
    }

    // Второй слой (полупрозрачный, если видим оба)
    if (ed->show_layer2) {
        for (int x = start_x; x < end_x; x++) {
            for (int y = start_y; y < end_y; y++) {
                int idx = x * map->height + y;
                int tile_id = map->tiles2[idx];
                if (tile_id < 0 || tile_id >= ed->tile_count) continue;

                SDL_Texture *tex = ed->tiles[tile_id];
                double angle = map->rot2[idx] * 90.0;
                SDL_RendererFlip flip = SDL_FLIP_NONE;
                if (map->mirror_x2[idx]) flip |= SDL_FLIP_HORIZONTAL;
                if (map->mirror_y2[idx]) flip |= SDL_FLIP_VERTICAL;

                SDL_FRect dst = {
                    MAP_X + (x * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (y * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile, scaled_tile
                };
                SDL_FPoint center = { scaled_tile / 2.0f, scaled_tile / 2.0f };
                if (ed->show_layer1) {
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureAlphaMod(tex, 96);
                }
                SDL_RenderCopyExF(ed->renderer, tex, NULL, &dst, angle, &center, flip);
                if (ed->show_layer1) {
                    SDL_SetTextureAlphaMod(tex, 255);
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
                }
            }
        }
    }

    SDL_RenderSetClipRect(ed->renderer, NULL);

    // Полосы прокрутки
    float map_pixel_w = map->width * TILE_SIZE;
    float map_pixel_h = map->height * TILE_SIZE;
    float view_w = MAP_VISIBLE_W / zoom;
    float view_h = MAP_VISIBLE_H / zoom;

    // Вертикальный скроллбар
    {
        SDL_Rect track = { MAP_X + MAP_VISIBLE_W + 2, MAP_Y, SCROLLBAR_SIZE - 4, MAP_VISIBLE_H };
        SDL_SetRenderDrawColor(ed->renderer, 200,200,200,255);
        SDL_RenderFillRect(ed->renderer, &track);
        if (map_pixel_h > view_h) {
            float thumb_h = (view_h / map_pixel_h) * MAP_VISIBLE_H;
            if (thumb_h < 8) thumb_h = 8;
            float max_y = map_pixel_h - view_h;
            float thumb_y = MAP_Y + (max_y > 0 ? (ed->cam_y / max_y) * (MAP_VISIBLE_H - thumb_h) : 0);
            SDL_Rect thumb = { track.x+2, (int)thumb_y, track.w-4, (int)thumb_h-4 };
            SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }
    }
    // Горизонтальный скроллбар
    {
        SDL_Rect track = { MAP_X + 2, MAP_Y + MAP_VISIBLE_H, MAP_VISIBLE_W - 4, SCROLLBAR_SIZE };
        SDL_SetRenderDrawColor(ed->renderer, 200,200,200,255);
        SDL_RenderFillRect(ed->renderer, &track);
        if (map_pixel_w > view_w) {
            float thumb_w = (view_w / map_pixel_w) * MAP_VISIBLE_W;
            if (thumb_w < 8) thumb_w = 8;
            float max_x = map_pixel_w - view_w;
            float thumb_x = MAP_X + (max_x > 0 ? (ed->cam_x / max_x) * (MAP_VISIBLE_W - thumb_w) : 0);
            SDL_Rect thumb = { (int)thumb_x+2, track.y+2, (int)thumb_w-4, SCROLLBAR_SIZE-4 };
            SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }
    }
}

// Правая панель
void render_right_panel(Editor *ed) {
    int pan_x = WINDOW_W - RIGHT_PANEL_W;
    SDL_Rect bg = { pan_x, 0, RIGHT_PANEL_W, WINDOW_H };
    SDL_SetRenderDrawColor(ed->renderer, 60,60,60,255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "MAPS", pan_x+RIGHT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    int mx, my;
    get_logical_mouse(ed, &mx, &my);

    int line_h = 20;
    int list_start_y = 35;
    int buttons_y = WINDOW_H - 60;
    int list_max_h = buttons_y - list_start_y - 10;
    int max_visible = list_max_h / line_h;
    if (max_visible < 1) max_visible = 1;

    SDL_Rect list_clip = { pan_x+5, list_start_y, RIGHT_PANEL_W-10, list_max_h };
    SDL_RenderSetClipRect(ed->renderer, &list_clip);

    int total_rows = ed->map_list.map_count;
    int max_scroll = (total_rows > max_visible) ? (total_rows - max_visible) : 0;
    if (ed->map_list.map_list_scroll < 0) ed->map_list.map_list_scroll = 0;
    if (ed->map_list.map_list_scroll > max_scroll) ed->map_list.map_list_scroll = max_scroll;

    for (int i = ed->map_list.map_list_scroll; i < total_rows && i < ed->map_list.map_list_scroll + max_visible; i++) {
        int row_y = list_start_y + (i - ed->map_list.map_list_scroll) * line_h;
        SDL_Color col = (i == ed->map_list.current_map) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,255,255};
        draw_text_centered(ed->renderer, ed->font, ed->map_list.maps[i].name,
                          pan_x + RIGHT_PANEL_W/2, row_y + line_h/2, col);
    }

    // Скроллбар списка
    if (total_rows > max_visible) {
        int bar_x = pan_x + RIGHT_PANEL_W - 10, bar_w = 6;
        SDL_Rect track = { bar_x, list_start_y, bar_w, list_max_h };
        SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
        SDL_RenderFillRect(ed->renderer, &track);
        float thumb_h = (float)max_visible / total_rows * list_max_h;
        if (thumb_h < 12) thumb_h = 12;
        int thumb_y = list_start_y + (int)((list_max_h - thumb_h) * ((float)ed->map_list.map_list_scroll / max_scroll));
        SDL_Rect thumb = { bar_x, thumb_y, bar_w, (int)thumb_h };
        SDL_SetRenderDrawColor(ed->renderer, 180,180,180,255);
        SDL_RenderFillRect(ed->renderer, &thumb);
    }

    SDL_RenderSetClipRect(ed->renderer, NULL);

    // Кнопка Save
    SDL_Rect save_btn = { pan_x+10, buttons_y, RIGHT_PANEL_W-20, 28 };
    bool hover_save = (mx >= save_btn.x && mx < save_btn.x+save_btn.w && my >= save_btn.y && my < save_btn.y+save_btn.h);
    SDL_Color save_bg = hover_save ? (SDL_Color){140,140,140,255} : (SDL_Color){100,100,100,255};
    if (ed->save_blink_active) save_bg = (SDL_Color){220,80,80,255};
    SDL_SetRenderDrawColor(ed->renderer, save_bg.r, save_bg.g, save_bg.b, 255);
    SDL_RenderFillRect(ed->renderer, &save_btn);
    draw_text_centered(ed->renderer, ed->font, "Save", save_btn.x+save_btn.w/2, save_btn.y+save_btn.h/2, (SDL_Color){255,255,255,255});
}

// Левая панель (пока пустая)
void render_left_panel(Editor *ed) {
    SDL_Rect bg = {0, 0, LEFT_PANEL_W, WINDOW_H};
    SDL_SetRenderDrawColor(ed->renderer, 50,50,50,255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "EVENTS", LEFT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});
    draw_text_centered(ed->renderer, ed->font, "(no tools yet)", LEFT_PANEL_W/2, 250, (SDL_Color){150,150,150,255});
}

// Обработка ввода
void handle_input(Editor *ed, bool *running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *running = false; return; }

        // Колесо мыши: зум (Ctrl+колесо) или скролл списка карт
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my;
            get_logical_mouse(ed, &mx, &my);
            if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                    ed->zoom += e.wheel.y * 0.1f;
                    if (ed->zoom < 0.1f) ed->zoom = 0.1f;
                    if (ed->zoom > 2.0f) ed->zoom = 2.0f;
                }
            }
            // Прокрутка списка карт в правой панели
            if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                int max_visible = (WINDOW_H - 60 - 35 - 10) / 20;
                int total = ed->map_list.map_count;
                int max_scroll = (total > max_visible) ? (total - max_visible) : 0;
                ed->map_list.map_list_scroll -= e.wheel.y;
                if (ed->map_list.map_list_scroll < 0) ed->map_list.map_list_scroll = 0;
                if (ed->map_list.map_list_scroll > max_scroll) ed->map_list.map_list_scroll = max_scroll;
            }
        }

        // Правая кнопка: начало панорамирования (с Ctrl)
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
            if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                ed->panning = 1;
                ed->pan_start_x = e.button.x;
                ed->pan_start_y = e.button.y;
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT)
            ed->panning = 0;

        // Отпускание левой кнопки – сброс перетаскивания скроллбаров
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            ed->scrollbar_drag_v = false;
            ed->scrollbar_drag_h = false;
        }

        // Левая кнопка: интерфейс
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = e.button.x, my = e.button.y;
            // Правая панель
            if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                SDL_Rect save_btn = { WINDOW_W - RIGHT_PANEL_W + 10, WINDOW_H - 60, RIGHT_PANEL_W - 20, 28 };
                if (mx >= save_btn.x && mx < save_btn.x+save_btn.w && my >= save_btn.y && my < save_btn.y+save_btn.h) {
                    Map *cur = current_map(ed);
                    if (cur) {
                        map_save_to_json(cur, cur->folder);
                        ed->save_blink_active = true;
                        ed->save_blink_time = SDL_GetTicks();
                    }
                    return;
                }
                // Список карт
                int line_h = 20, list_start_y = 35;
                int max_visible = (WINDOW_H - 60 - list_start_y - 10) / line_h;
                int start_idx = ed->map_list.map_list_scroll;
                int end_idx = (start_idx + max_visible < ed->map_list.map_count) ? start_idx + max_visible : ed->map_list.map_count;
                for (int i = start_idx; i < end_idx; i++) {
                    int row_y = list_start_y + (i - ed->map_list.map_list_scroll) * line_h;
                    if (my >= row_y - 10 && my < row_y + 10) {
                        ed->map_list.current_map = i;
                        load_tileset(ed, ed->map_list.maps[i].tileset_path);
                        return;
                    }
                }
            }
            // Вертикальный скроллбар (только захват ползунка, без клика по треку)
            if (mx >= MAP_X + MAP_VISIBLE_W && mx < MAP_X + MAP_VISIBLE_W + SCROLLBAR_SIZE &&
                my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                Map *map = current_map(ed);
                if (map) {
                    float view_h = MAP_VISIBLE_H / ed->zoom;
                    float max_cam_y = map->height * TILE_SIZE - view_h;
                    if (max_cam_y > 0) {
                        float thumb_h = (view_h / (map->height * TILE_SIZE)) * MAP_VISIBLE_H;
                        if (thumb_h < 8) thumb_h = 8;
                        float thumb_y = MAP_Y + (ed->cam_y / max_cam_y) * (MAP_VISIBLE_H - thumb_h);
                        if (my >= thumb_y && my <= thumb_y + thumb_h) {
                            ed->scrollbar_drag_v = true;
                            ed->scroll_drag_mouse_offset_y = my - (int)thumb_y;
                        }
                    }
                }
            }
            // Горизонтальный скроллбар (только захват ползунка)
            else if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W &&
                     my >= MAP_Y + MAP_VISIBLE_H && my < MAP_Y + MAP_VISIBLE_H + SCROLLBAR_SIZE) {
                Map *map = current_map(ed);
                if (map) {
                    float view_w = MAP_VISIBLE_W / ed->zoom;
                    float max_cam_x = map->width * TILE_SIZE - view_w;
                    if (max_cam_x > 0) {
                        float thumb_w = (view_w / (map->width * TILE_SIZE)) * MAP_VISIBLE_W;
                        if (thumb_w < 8) thumb_w = 8;
                        float thumb_x = MAP_X + (ed->cam_x / max_cam_x) * (MAP_VISIBLE_W - thumb_w);
                        if (mx >= thumb_x && mx <= thumb_x + thumb_w) {
                            ed->scrollbar_drag_h = true;
                            ed->scroll_drag_mouse_offset_x = mx - (int)thumb_x;
                        }
                    }
                }
            }
        }
    }

    // Перетаскивание скроллбаров (если захвачены)
    if (ed->scrollbar_drag_v) {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        Map *map = current_map(ed);
        if (map) {
            float view_h = MAP_VISIBLE_H / ed->zoom;
            float max_cam_y = map->height * TILE_SIZE - view_h;
            if (max_cam_y > 0) {
                float thumb_h = (view_h / (map->height * TILE_SIZE)) * MAP_VISIBLE_H;
                if (thumb_h < 8) thumb_h = 8;
                float range = MAP_VISIBLE_H - thumb_h;
                float local_y = my - MAP_Y - ed->scroll_drag_mouse_offset_y;
                if (local_y < 0) local_y = 0;
                if (local_y > range) local_y = range;
                ed->cam_y = (local_y / range) * max_cam_y;
            }
        }
    }
    if (ed->scrollbar_drag_h) {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        Map *map = current_map(ed);
        if (map) {
            float view_w = MAP_VISIBLE_W / ed->zoom;
            float max_cam_x = map->width * TILE_SIZE - view_w;
            if (max_cam_x > 0) {
                float thumb_w = (view_w / (map->width * TILE_SIZE)) * MAP_VISIBLE_W;
                if (thumb_w < 8) thumb_w = 8;
                float range = MAP_VISIBLE_W - thumb_w;
                float local_x = mx - MAP_X - ed->scroll_drag_mouse_offset_x;
                if (local_x < 0) local_x = 0;
                if (local_x > range) local_x = range;
                ed->cam_x = (local_x / range) * max_cam_x;
            }
        }
    }

    // Панорамирование (Ctrl+правая кнопка)
    if (ed->panning) {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        float dx = (mx - ed->pan_start_x) / ed->zoom;
        float dy = (my - ed->pan_start_y) / ed->zoom;
        ed->cam_x -= dx; ed->cam_y -= dy;
        ed->pan_start_x = mx; ed->pan_start_y = my;
        Map *map = current_map(ed);
        if (map) {
            float max_x = map->width * TILE_SIZE - MAP_VISIBLE_W / ed->zoom;
            float max_y = map->height * TILE_SIZE - MAP_VISIBLE_H / ed->zoom;
            if (max_x < 0) max_x = 0;
            if (max_y < 0) max_y = 0;
            if (ed->cam_x < 0) ed->cam_x = 0;
            if (ed->cam_x > max_x) ed->cam_x = max_x;
            if (ed->cam_y < 0) ed->cam_y = 0;
            if (ed->cam_y > max_y) ed->cam_y = max_y;
        }
    }
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    Editor ed;
    editor_init(&ed);

    ed.window = SDL_CreateWindow("Map Event Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    ed.renderer = SDL_CreateRenderer(ed.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ed.renderer, WINDOW_W, WINDOW_H);

    ed.font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", FONT_SIZE);
    if (!ed.font) ed.font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", FONT_SIZE);
    if (!ed.font) return 1;

    CreateDirectoryA("../data/maps", NULL);
    load_map_list(&ed);
    if (ed.map_list.map_count > 0) {
        load_tileset(&ed, ed.map_list.maps[ed.map_list.current_map].tileset_path);
    }

    bool running = true;
    while (running) {
        handle_input(&ed, &running);

        Uint32 now = SDL_GetTicks();
        if (ed.save_blink_active && now - ed.save_blink_time >= 150)
            ed.save_blink_active = false;

        SDL_SetRenderDrawColor(ed.renderer, 30,30,30,255);
        SDL_RenderClear(ed.renderer);

        render_left_panel(&ed);
        render_map(&ed);
        render_right_panel(&ed);

        SDL_RenderPresent(ed.renderer);
        SDL_Delay(16);
    }

    free_tileset(&ed);
    TTF_CloseFont(ed.font);
    for (int i = 0; i < ed.map_list.map_count; i++) map_free(&ed.map_list.maps[i]);
    free(ed.map_list.maps);
    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}