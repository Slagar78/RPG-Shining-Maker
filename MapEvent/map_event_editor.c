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

#define MAX_ROOF_EVENTS 128

// ─── Событие крыши (теперь с конкретными координатами триггера) ──
typedef struct {
    int tile_id;
    int trigger_x, trigger_y;   // клетка, по которой кликнули для выбора тайла
    int start_x, start_y;
    int end_x, end_y;
    int exit_x, exit_y;   // клетка-выход (закрывает крышу)
} RoofEvent;

// ─── Список карт ──────────────────────────────
typedef struct {
    Map *maps;
    int map_count;
    int current_map;
    int map_list_scroll;
} MapList;

// ─── Главный редактор ─────────────────────────
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

    RoofEvent roof_events[MAX_ROOF_EVENTS];
    int roof_event_count;
    int selected_roof_event;   // -1 = none
    int edit_field;            // 0=Tile ID, 1=Start, 2=End
    char input_buf[32];

    bool left_panel_collapsed;
} Editor;

// ─── Прототипы ────────────────────────────────
Map* current_map(Editor *ed);
void get_logical_mouse(Editor *ed, int *mx, int *my);
void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, SDL_Color color);
void save_events_to_json(Editor *ed, const char *folder);
void load_events_from_json(Editor *ed, const char *folder);

static void draw_roof_field(Editor *ed, const char *label, int field_idx, int line_y);
static void check_roof_field_click(Editor *ed, int field_idx, int line_y, int mx, int my);

// ─── Вспомогательные функции ──────────────────
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

// ─── Инициализация ────────────────────────────
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
    ed->roof_event_count = 0;
    ed->selected_roof_event = -1;
    ed->edit_field = -1;
    ed->input_buf[0] = '\0';
    ed->left_panel_collapsed = false;
}

// ─── Тайлсет ──────────────────────────────────
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
    int palette_cols = 8;
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

// ─── Карты ────────────────────────────────────
Map* current_map(Editor *ed) {
    if (ed->map_list.map_count == 0) return NULL;
    return &ed->map_list.maps[ed->map_list.current_map];
}

void load_map_list(Editor *ed) {
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
    if (ed->map_list.map_count > 0) {
        load_tileset(ed, ed->map_list.maps[ed->map_list.current_map].tileset_path);
        load_events_from_json(ed, ed->map_list.maps[ed->map_list.current_map].folder);
        if (ed->roof_event_count == 0) {
            char check_path[512];
            snprintf(check_path, sizeof(check_path), "../data/maps/%s/events.json",
                     ed->map_list.maps[ed->map_list.current_map].folder);
            FILE *test = fopen(check_path, "r");
            if (!test) {
                save_events_to_json(ed, ed->map_list.maps[ed->map_list.current_map].folder);
            } else {
                fclose(test);
            }
        }
    }
}

// ─── События (events.json) ─────────────────────
void load_events_from_json(Editor *ed, const char *folder) {
    ed->roof_event_count = 0;
    ed->selected_roof_event = -1;
    ed->edit_field = -1;
    ed->input_buf[0] = '\0';

    char path[512];
    snprintf(path, sizeof(path), "../data/maps/%s/events.json", folder);
    FILE *f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *data = malloc(len+1);
    fread(data, 1, len, f); data[len] = '\0'; fclose(f);
    cJSON *arr = cJSON_Parse(data);
    free(data);
    if (!arr || !cJSON_IsArray(arr)) {
        if (arr) cJSON_Delete(arr);
        return;
    }
    int count = cJSON_GetArraySize(arr);
    for (int i = 0; i < count && ed->roof_event_count < MAX_ROOF_EVENTS; i++) {
        cJSON *ev = cJSON_GetArrayItem(arr, i);
        if (!ev) continue;
        cJSON *tid = cJSON_GetObjectItem(ev, "tile_id");
        cJSON *sx  = cJSON_GetObjectItem(ev, "start_x");
        cJSON *sy  = cJSON_GetObjectItem(ev, "start_y");
        cJSON *ex  = cJSON_GetObjectItem(ev, "end_x");
        cJSON *ey  = cJSON_GetObjectItem(ev, "end_y");

        // Новые поля (могут отсутствовать в старых файлах)
        cJSON *tx = cJSON_GetObjectItem(ev, "trigger_x");
        cJSON *ty = cJSON_GetObjectItem(ev, "trigger_y");

        if (tid && sx && sy && ex && ey) {
            RoofEvent *re = &ed->roof_events[ed->roof_event_count++];
            re->tile_id = tid->valueint;
            re->start_x = sx->valueint;
            re->start_y = sy->valueint;
            re->end_x   = ex->valueint;
            re->end_y   = ey->valueint;
            // Если координаты триггера не указаны, ставим -1
            re->trigger_x = (tx && cJSON_IsNumber(tx)) ? tx->valueint : -1;
            re->trigger_y = (ty && cJSON_IsNumber(ty)) ? ty->valueint : -1;
            cJSON *ex_x = cJSON_GetObjectItem(ev, "exit_x");
            cJSON *ex_y = cJSON_GetObjectItem(ev, "exit_y");
            re->exit_x = (ex_x && cJSON_IsNumber(ex_x)) ? ex_x->valueint : -1;
            re->exit_y = (ex_y && cJSON_IsNumber(ex_y)) ? ex_y->valueint : -1;
        }
    }
    cJSON_Delete(arr);
}

void save_events_to_json(Editor *ed, const char *folder) {
    char dir[512];
    snprintf(dir, sizeof(dir), "../data/maps/%s", folder);
    CreateDirectoryA(dir, NULL);
    char filename[768];
    snprintf(filename, sizeof(filename), "%s/events.json", dir);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ed->roof_event_count; i++) {
        RoofEvent *re = &ed->roof_events[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddNumberToObject(ev, "tile_id", re->tile_id);
        cJSON_AddNumberToObject(ev, "start_x", re->start_x);
        cJSON_AddNumberToObject(ev, "start_y", re->start_y);
        cJSON_AddNumberToObject(ev, "end_x",   re->end_x);
        cJSON_AddNumberToObject(ev, "end_y",   re->end_y);
        // Сохраняем координаты триггера (даже если -1, для полноты)
        cJSON_AddNumberToObject(ev, "trigger_x", re->trigger_x);
        cJSON_AddNumberToObject(ev, "trigger_y", re->trigger_y);
        cJSON_AddNumberToObject(ev, "exit_x", re->exit_x);
        cJSON_AddNumberToObject(ev, "exit_y", re->exit_y);
        cJSON_AddItemToArray(root, ev);
    }
    char *str = cJSON_Print(root);
    FILE *f = fopen(filename, "w");
    if (f) { fputs(str, f); fclose(f); }
    cJSON_Delete(root);
    free(str);
}

// ─── Отрисовка карты + рамки (красная для триггера, зелёная для зоны) ──
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

    // Красная рамка вокруг конкретной клетки-триггера (если задана)
    if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
        RoofEvent *re = &ed->roof_events[ed->selected_roof_event];
        if (re->trigger_x >= 0 && re->trigger_y >= 0) {
            int tx = re->trigger_x;
            int ty = re->trigger_y;
            if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                SDL_FRect tile_dst = {
                    MAP_X + (tx * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (ty * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile, scaled_tile
            };
            SDL_SetRenderDrawColor(ed->renderer, 255, 0, 0, 255);
            // Рисуем жирную красную рамку (несколько проходов со смещением)
               for (int dx = -1; dx <= 1; dx++) {
               for (int dy = -1; dy <= 1; dy++) {
               SDL_FRect thick_rect = {
               tile_dst.x + dx,
               tile_dst.y + dy,
               tile_dst.w,
               tile_dst.h
                };
                  SDL_RenderDrawRectF(ed->renderer, &thick_rect);
                    }
                }
            }
        }
    }

    // Голубая рамка вокруг клетки-выхода
    if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
        RoofEvent *re = &ed->roof_events[ed->selected_roof_event];
        if (re->exit_x >= 0 && re->exit_y >= 0) {
            int ex_x = re->exit_x;
            int ex_y = re->exit_y;
            if (ex_x >= 0 && ex_x < map->width && ex_y >= 0 && ex_y < map->height) {
                SDL_FRect ex_dst = {
                    MAP_X + (ex_x * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (ex_y * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile, scaled_tile
                };
                SDL_SetRenderDrawColor(ed->renderer, 0, 150, 255, 255);  // голубой
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        SDL_FRect thick = { ex_dst.x + dx, ex_dst.y + dy, ex_dst.w, ex_dst.h };
                        SDL_RenderDrawRectF(ed->renderer, &thick);
                    }
                }
            }
        }
    }

    // Зелёная рамка зоны крыши (цельный прямоугольник)
    if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
        RoofEvent *re = &ed->roof_events[ed->selected_roof_event];
        int x1 = re->start_x < re->end_x ? re->start_x : re->end_x;
        int y1 = re->start_y < re->end_y ? re->start_y : re->end_y;
        int x2 = re->start_x > re->end_x ? re->start_x : re->end_x;
        int y2 = re->start_y > re->end_y ? re->start_y : re->end_y;

        SDL_FRect zone = {
            MAP_X + (x1 * TILE_SIZE - ed->cam_x) * zoom,
            MAP_Y + (y1 * TILE_SIZE - ed->cam_y) * zoom,
            (x2 - x1 + 1) * scaled_tile,
            (y2 - y1 + 1) * scaled_tile
        };
        SDL_SetRenderDrawColor(ed->renderer, 0, 255, 0, 255);
        // Жирная зелёная рамка
        for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
        SDL_FRect thick_zone = {
            zone.x + dx,
            zone.y + dy,
            zone.w,
            zone.h
            };
            SDL_RenderDrawRectF(ed->renderer, &thick_zone);
        }
    }
}

    SDL_RenderSetClipRect(ed->renderer, NULL);

    // Подсказка под курсором
    int mx, my;
    get_logical_mouse(ed, &mx, &my);
    if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
        float world_x = (mx - MAP_X) / zoom + ed->cam_x;
        float world_y = (my - MAP_Y) / zoom + ed->cam_y;
        int tx = (int)(world_x / TILE_SIZE);
        int ty = (int)(world_y / TILE_SIZE);
        if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
            int idx = tx * map->height + ty;
            int tile_id = map->tiles[idx];
            char info[64];
            snprintf(info, sizeof(info), "(%d,%d) tile=%d", tx, ty, tile_id);
            draw_text_centered(ed->renderer, ed->font, info, MAP_X + MAP_VISIBLE_W - 80, MAP_Y - 12, (SDL_Color){255,255,255,255});
        }
    }

    // Скроллбары
    float map_pixel_w = map->width * TILE_SIZE;
    float map_pixel_h = map->height * TILE_SIZE;
    float view_w = MAP_VISIBLE_W / zoom;
    float view_h = MAP_VISIBLE_H / zoom;

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

// ─── Правая панель ────────────────────────────
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

    SDL_Rect save_btn = { pan_x+10, buttons_y, RIGHT_PANEL_W-20, 28 };
    bool hover_save = (mx >= save_btn.x && mx < save_btn.x+save_btn.w && my >= save_btn.y && my < save_btn.y+save_btn.h);
    SDL_Color save_bg = hover_save ? (SDL_Color){140,140,140,255} : (SDL_Color){100,100,100,255};
    if (ed->save_blink_active) save_bg = (SDL_Color){220,80,80,255};
    SDL_SetRenderDrawColor(ed->renderer, save_bg.r, save_bg.g, save_bg.b, 255);
    SDL_RenderFillRect(ed->renderer, &save_btn);
    draw_text_centered(ed->renderer, ed->font, "Save Events", save_btn.x+save_btn.w/2, save_btn.y+save_btn.h/2, (SDL_Color){255,255,255,255});

    draw_text_centered(ed->renderer, ed->font, "Click map: tile", pan_x+RIGHT_PANEL_W/2, buttons_y - 20, (SDL_Color){200,200,200,255});
    draw_text_centered(ed->renderer, ed->font, "Start/End: select+click", pan_x+RIGHT_PANEL_W/2, buttons_y - 5, (SDL_Color){200,200,200,255});
}

// ─── Вспомогательные функции для левой панели ──
static void draw_roof_field(Editor *ed, const char *label, int field_idx, int line_y) {
    SDL_Rect lbl_rect = {10, line_y, 65, 20};
    draw_text_centered(ed->renderer, ed->font, label, lbl_rect.x + lbl_rect.w/2, lbl_rect.y + lbl_rect.h/2,
                       (SDL_Color){200, 200, 200, 255});

    SDL_Rect fld_rect = {90, line_y, 130, 20};
    bool active = (ed->edit_field == field_idx);
    SDL_SetRenderDrawColor(ed->renderer, active ? 255 : 200, active ? 255 : 200, active ? 255 : 200, 255);
    SDL_RenderFillRect(ed->renderer, &fld_rect);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(ed->renderer, &fld_rect);

    char buf[32];
    if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
        RoofEvent *re = &ed->roof_events[ed->selected_roof_event];
        if (field_idx == 0)
            snprintf(buf, sizeof(buf), "%d", re->tile_id);
        else if (field_idx == 1)
            snprintf(buf, sizeof(buf), "%d,%d", re->start_x, re->start_y);
        else if (field_idx == 2)
            snprintf(buf, sizeof(buf), "%d,%d", re->end_x, re->end_y);
        else if (field_idx == 3)
            snprintf(buf, sizeof(buf), "%d,%d", re->exit_x, re->exit_y);
    }
    if (active && ed->input_buf[0])
        snprintf(buf, sizeof(buf), "%s", ed->input_buf);
    draw_text_centered(ed->renderer, ed->font, buf, fld_rect.x + fld_rect.w/2, fld_rect.y + fld_rect.h/2,
                       (SDL_Color){0, 0, 0, 255});
}

static void check_roof_field_click(Editor *ed, int field_idx, int line_y, int mx, int my) {
    SDL_Rect fld = {90, line_y, 130, 20};
    if (mx >= fld.x && mx < fld.x + fld.w && my >= fld.y && my < fld.y + fld.h) {
        ed->edit_field = field_idx;
        if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
            RoofEvent *re = &ed->roof_events[ed->selected_roof_event];
            if (field_idx == 0)
                snprintf(ed->input_buf, sizeof(ed->input_buf), "%d", re->tile_id);
            else if (field_idx == 1)
                snprintf(ed->input_buf, sizeof(ed->input_buf), "%d,%d", re->start_x, re->start_y);
            else if (field_idx == 2)
                snprintf(ed->input_buf, sizeof(ed->input_buf), "%d,%d", re->end_x, re->end_y);
            else if (field_idx == 3)
                snprintf(ed->input_buf, sizeof(ed->input_buf), "%d,%d", re->exit_x, re->exit_y);
        }
        SDL_StartTextInput();
    }
}

// ─── Левая панель (Roof Events, сворачиваемая) ──
void render_left_panel(Editor *ed) {
    SDL_Rect bg = {0, 0, LEFT_PANEL_W, WINDOW_H};
    SDL_SetRenderDrawColor(ed->renderer, 50,50,50,255);
    SDL_RenderFillRect(ed->renderer, &bg);

    // Заголовок и кнопка сворачивания
    int y = 10;
    draw_text_centered(ed->renderer, ed->font, "ROOF EVENTS", LEFT_PANEL_W/2 - 20, y, (SDL_Color){255,255,255,255});

    SDL_Rect collapse_btn = { LEFT_PANEL_W - 40, 0, 30, 24 };
    if (ed->left_panel_collapsed) {
        draw_text_centered(ed->renderer, ed->font, "+", collapse_btn.x+15, collapse_btn.y+12, (SDL_Color){255,255,255,255});
    } else {
        draw_text_centered(ed->renderer, ed->font, "-", collapse_btn.x+15, collapse_btn.y+12, (SDL_Color){255,255,255,255});
    }

    if (ed->left_panel_collapsed) {
        return;
    }

    y = 35;
    SDL_Rect add_btn = {10, y, LEFT_PANEL_W-20, 24};
    SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
    SDL_RenderFillRect(ed->renderer, &add_btn);
    draw_text_centered(ed->renderer, ed->font, "Add Roof Event", add_btn.x+add_btn.w/2, add_btn.y+add_btn.h/2, (SDL_Color){255,255,255,255});
    y += 30;

    SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
    SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
    y += 5;

    int list_start_y = y;
    int list_h = 200;
    SDL_Rect list_clip = {10, list_start_y, LEFT_PANEL_W-20, list_h};
    SDL_RenderSetClipRect(ed->renderer, &list_clip);

    for (int i = 0; i < ed->roof_event_count; i++) {
        RoofEvent *re = &ed->roof_events[i];
        char buf[128];
        snprintf(buf, sizeof(buf), "Tile %d  (%d,%d)-(%d,%d)",
                 re->tile_id, re->start_x, re->start_y, re->end_x, re->end_y);
        SDL_Color col = (i == ed->selected_roof_event) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,255,255};
        SDL_Rect item_rect = {10, list_start_y + i*18, LEFT_PANEL_W-20, 18};
        if (i == ed->selected_roof_event) {
            SDL_SetRenderDrawColor(ed->renderer, 80,80,120,255);
            SDL_RenderFillRect(ed->renderer, &item_rect);
        }
        draw_text_centered(ed->renderer, ed->font, buf, LEFT_PANEL_W/2, item_rect.y+9, col);
    }
    SDL_RenderSetClipRect(ed->renderer, NULL);
    y = list_start_y + list_h + 5;

    SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
    SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
    y += 5;

    if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
        int edit_y = y;

        draw_roof_field(ed, "Tile ID", 0, edit_y);
        edit_y += 22;
        draw_roof_field(ed, "Start", 1, edit_y);
        edit_y += 22;
        draw_roof_field(ed, "End", 2, edit_y);
        edit_y += 22;
        // Поле Exit
        draw_roof_field(ed, "Exit", 3, edit_y);
        edit_y += 22;

        SDL_Rect del_btn = {10, edit_y, LEFT_PANEL_W-20, 24};
        SDL_SetRenderDrawColor(ed->renderer, 180, 80, 80, 255);
        SDL_RenderFillRect(ed->renderer, &del_btn);
        draw_text_centered(ed->renderer, ed->font, "Delete Event", del_btn.x+del_btn.w/2, del_btn.y+del_btn.h/2, (SDL_Color){255,255,255,255});
    }
}

// ─── Обработка ввода (исправленная) ──────────
void handle_input(Editor *ed, bool *running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *running = false; return; }

        // ================== Текстовый ввод ==================
        if (ed->edit_field != -1) {
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    int len = strlen(ed->input_buf);
                    if (len > 0) ed->input_buf[len-1] = '\0';
                }
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                    if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
                        RoofEvent *re = &ed->roof_events[ed->selected_roof_event];
                        if (ed->edit_field == 0) {
                            re->tile_id = atoi(ed->input_buf);
                        } else if (ed->edit_field == 1) {
                            int x, y;
                            if (sscanf(ed->input_buf, "%d,%d", &x, &y) == 2) {
                                re->start_x = x;
                                re->start_y = y;
                            }
                        } else if (ed->edit_field == 2) {
                            int x, y;
                            if (sscanf(ed->input_buf, "%d,%d", &x, &y) == 2) {
                                re->end_x = x;
                                re->end_y = y;
                            }
                        } else if (ed->edit_field == 3) {
                            int x, y;
                            if (sscanf(ed->input_buf, "%d,%d", &x, &y) == 2) {
                                re->exit_x = x;
                                re->exit_y = y;
                            }
                        }
                    }
                    ed->edit_field = -1;
                    ed->input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
                else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    ed->edit_field = -1;
                    ed->input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }
            else if (e.type == SDL_TEXTINPUT) {
                if (strspn(e.text.text, "0123456789,-") == strlen(e.text.text)) {
                    if (strlen(ed->input_buf) < 30) {
                        strcat(ed->input_buf, e.text.text);
                    }
                }
            }
        }

        // ================== Колесо мыши ==================
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
            if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                int max_visible = (WINDOW_H - 60 - 35 - 10) / 20;
                int total = ed->map_list.map_count;
                int max_scroll = (total > max_visible) ? (total - max_visible) : 0;
                ed->map_list.map_list_scroll -= e.wheel.y;
                if (ed->map_list.map_list_scroll < 0) ed->map_list.map_list_scroll = 0;
                if (ed->map_list.map_list_scroll > max_scroll) ed->map_list.map_list_scroll = max_scroll;
            }
        }

        // ================== Правая кнопка (панорама) ==================
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
            if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                ed->panning = 1;
                ed->pan_start_x = e.button.x;
                ed->pan_start_y = e.button.y;
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT)
            ed->panning = 0;

        // ================== Левая кнопка: нажатие ==================
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = e.button.x, my = e.button.y;

            // Сброс поля ввода при клике вне полей и карты
            if (ed->edit_field != -1) {
                if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                    // клик по карте – обработаем ниже
                } else if (!(mx >= 0 && mx < LEFT_PANEL_W)) {
                    ed->edit_field = -1;
                    ed->input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }

            // Левая панель
            if (mx >= 0 && mx < LEFT_PANEL_W) {
                SDL_Rect collapse_btn = { LEFT_PANEL_W - 40, 0, 30, 24 };
                if (mx >= collapse_btn.x && mx < collapse_btn.x+collapse_btn.w &&
                    my >= collapse_btn.y && my < collapse_btn.y+collapse_btn.h) {
                    ed->left_panel_collapsed = !ed->left_panel_collapsed;
                    return;
                }

                if (ed->left_panel_collapsed) return;

                int y_off = 35;
                SDL_Rect add_btn = {10, y_off, LEFT_PANEL_W-20, 24};
                if (my >= add_btn.y && my < add_btn.y+add_btn.h) {
                    if (ed->roof_event_count < MAX_ROOF_EVENTS) {
                        RoofEvent *re = &ed->roof_events[ed->roof_event_count++];
                        re->tile_id = 0;
                        re->trigger_x = re->trigger_y = -1;
                        re->exit_x = -1;
                        re->exit_y = -1;
                        re->start_x = 0; re->start_y = 0;
                        re->end_x = 1; re->end_y = 1;
                        ed->selected_roof_event = ed->roof_event_count - 1;
                        ed->edit_field = -1;
                        ed->input_buf[0] = '\0';
                    }
                    return;
                }
                y_off += 30 + 5;
                int list_start_y = y_off;
                int list_h = 200;
                if (my >= list_start_y && my < list_start_y + list_h) {
                    int idx = (my - list_start_y) / 18;
                    if (idx >= 0 && idx < ed->roof_event_count) {
                        ed->selected_roof_event = idx;
                        ed->edit_field = -1;
                        ed->input_buf[0] = '\0';
                        return;
                    }
                }
                if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
                    int edit_y = list_start_y + list_h + 5 + 5;
                    check_roof_field_click(ed, 0, edit_y, mx, my);
                    check_roof_field_click(ed, 1, edit_y + 22, mx, my);
                    check_roof_field_click(ed, 2, edit_y + 44, mx, my);
                    check_roof_field_click(ed, 3, edit_y + 66, mx, my);   // Exit

                    SDL_Rect del_btn = {10, edit_y + 88, LEFT_PANEL_W-20, 24};
                    if (mx >= del_btn.x && mx < del_btn.x+del_btn.w && my >= del_btn.y && my < del_btn.y+del_btn.h) {
                        for (int i = ed->selected_roof_event; i < ed->roof_event_count-1; i++)
                            ed->roof_events[i] = ed->roof_events[i+1];
                        ed->roof_event_count--;
                        ed->selected_roof_event = -1;
                        ed->edit_field = -1;
                        SDL_StopTextInput();
                        return;
                    }
                }
                return;
            }

            // Правая панель
            if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                SDL_Rect save_btn = { WINDOW_W - RIGHT_PANEL_W + 10, WINDOW_H - 60, RIGHT_PANEL_W - 20, 28 };
                if (mx >= save_btn.x && mx < save_btn.x+save_btn.w && my >= save_btn.y && my < save_btn.y+save_btn.h) {
                    Map *cur = current_map(ed);
                    if (cur) {
                        save_events_to_json(ed, cur->folder);
                        ed->save_blink_active = true;
                        ed->save_blink_time = SDL_GetTicks();
                    }
                    return;
                }
                int line_h = 20, list_start_y = 35;
                int max_visible = (WINDOW_H - 60 - list_start_y - 10) / line_h;
                int start_idx = ed->map_list.map_list_scroll;
                int end_idx = (start_idx + max_visible < ed->map_list.map_count) ? start_idx + max_visible : ed->map_list.map_count;
                for (int i = start_idx; i < end_idx; i++) {
                    int row_y = list_start_y + (i - ed->map_list.map_list_scroll) * line_h;
                    if (my >= row_y - 10 && my < row_y + 10) {
                        ed->map_list.current_map = i;
                        load_tileset(ed, ed->map_list.maps[i].tileset_path);
                        load_events_from_json(ed, ed->map_list.maps[i].folder);
                        return;
                    }
                }
            }

            // Клик по карте (установка тайла/координат)
            if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                Map *map = current_map(ed);
                if (map && ed->selected_roof_event >= 0 && ed->roof_event_count > 0) {
                    float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
                    float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
                    int tx = (int)(world_x / TILE_SIZE);
                    int ty = (int)(world_y / TILE_SIZE);
                    if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                        RoofEvent *re = &ed->roof_events[ed->selected_roof_event];
                        if (ed->edit_field == -1 || ed->edit_field == 0) {
                            // Сохраняем tile_id И координаты триггера
                            re->tile_id = map->tiles[tx * map->height + ty];
                            re->trigger_x = tx;
                            re->trigger_y = ty;
                            if (ed->edit_field == 0) {
                                ed->edit_field = -1;
                                ed->input_buf[0] = '\0';
                                SDL_StopTextInput();
                            }
                        } else if (ed->edit_field == 1) {
                            re->start_x = tx;
                            re->start_y = ty;
                            ed->edit_field = -1;
                            ed->input_buf[0] = '\0';
                            SDL_StopTextInput();
                        } else if (ed->edit_field == 2) {
                            re->end_x = tx;
                            re->end_y = ty;
                            ed->edit_field = -1;
                            ed->input_buf[0] = '\0';
                            SDL_StopTextInput();
                        } else if (ed->edit_field == 3) {
                            re->exit_x = tx;
                            re->exit_y = ty;
                            ed->edit_field = -1;
                            ed->input_buf[0] = '\0';
                            SDL_StopTextInput();
                        }
                    }
                }
            }

            // Захват ползунков скроллбаров
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

        // ================== Левая кнопка: отпускание ==================
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            ed->scrollbar_drag_v = false;
            ed->scrollbar_drag_h = false;
        }
    }

    // ================== Непрерывные действия ==================
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