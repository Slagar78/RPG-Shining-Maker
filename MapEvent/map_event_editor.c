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
#define MAX_TILE_CHANGES 64
#define MAX_STAIRS 64
#define MAX_WARPS 64


typedef struct {
    int start_x, start_y;
    int end_x, end_y;
    int direction;
} StairEvent;

typedef struct {
    int trigger_x, trigger_y;   // клетка-активатор
    char target_map[64];        // папка карты назначения
    int target_x, target_y;     // точка появления
    int facing;                 // направление после телепорта (2=вниз, 4=влево, 6=вправо, 8=вверх)
} WarpEvent;

typedef struct {
    int trigger_x, trigger_y;   // клетка-триггер (куда наступает игрок)
    int new_tile_id;            // ID нового тайла
    int sample_x, sample_y;     // клетка, с которой взят образец тайла (для подсветки)
} TileChangeEvent;

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

    TileChangeEvent tile_changes[MAX_TILE_CHANGES];
    int tile_change_count;
    int selected_tile_change;   // -1 = none
    int tc_edit_field;          // 0=Trigger, 1=New Tile ID
    char tc_input_buf[32];
    bool tc_section_collapsed;
    int tc_section_y;

    int roof_event_scroll;
    int tile_change_scroll;
    SDL_Rect roof_list_rect;
    SDL_Rect tc_list_rect;

    StairEvent stair_events[MAX_STAIRS];
    int stair_event_count;
    int selected_stair;         // -1 = none
    int stair_edit_field;       // 0=Start, 1=End
    char stair_input_buf[32];
    bool stair_section_collapsed;
    int stair_section_y;
    int stair_event_scroll;
    SDL_Rect stair_list_rect;

    // Warp events
    WarpEvent warp_events[MAX_WARPS];
    int warp_event_count;
    int selected_warp;          // -1 = ничего не выбрано
    int warp_edit_field;        // 0=Trigger, 1=Target Map, 2=Target Pos, 3=Facing
    char warp_input_buf[64];
    bool warp_section_collapsed;
    int warp_section_y;
    int warp_event_scroll;
    SDL_Rect warp_list_rect;
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
    ed->left_panel_collapsed = true;
    ed->tile_change_count = 0;
    ed->selected_tile_change = -1;
    ed->tc_edit_field = -1;
    ed->tc_input_buf[0] = '\0';
    ed->tc_section_collapsed = true;   // по умолчанию свёрнуто
    ed->tc_section_y = 0;
    ed->roof_event_scroll = 0;
    ed->tile_change_scroll = 0;
    memset(&ed->roof_list_rect, 0, sizeof(ed->roof_list_rect));
    memset(&ed->tc_list_rect, 0, sizeof(ed->tc_list_rect));

    ed->stair_event_count = 0;
    ed->selected_stair = -1;
    ed->stair_edit_field = -1;
    ed->stair_input_buf[0] = '\0';
    ed->stair_section_collapsed = true;
    ed->stair_section_y = 0;
    ed->stair_event_scroll = 0;
    memset(&ed->stair_list_rect, 0, sizeof(ed->stair_list_rect));

    ed->warp_event_count = 0;
    ed->selected_warp = -1;
    ed->warp_edit_field = -1;
    ed->warp_input_buf[0] = '\0';
    ed->warp_section_collapsed = true;
    ed->warp_section_y = 0;
    ed->warp_event_scroll = 0;
    memset(&ed->warp_list_rect, 0, sizeof(ed->warp_list_rect));
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
    // Сбрасываем все события
    ed->roof_event_count = 0;
    ed->tile_change_count = 0;
    ed->stair_event_count = 0;
    ed->selected_roof_event = -1;
    ed->selected_tile_change = -1;
    ed->selected_stair = -1;
    ed->edit_field = -1;
    ed->tc_edit_field = -1;
    ed->stair_edit_field = -1;
    ed->input_buf[0] = '\0';
    ed->tc_input_buf[0] = '\0';
    ed->stair_input_buf[0] = '\0';

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
    for (int i = 0; i < count; i++) {
        cJSON *ev = cJSON_GetArrayItem(arr, i);
        if (!ev) continue;

        cJSON *type_json = cJSON_GetObjectItem(ev, "type");
        const char *type = type_json ? type_json->valuestring : "roof"; // по умолчанию крыша

        if (strcmp(type, "roof") == 0) {
            if (ed->roof_event_count < MAX_ROOF_EVENTS) {
                cJSON *tid = cJSON_GetObjectItem(ev, "tile_id");
                cJSON *sx  = cJSON_GetObjectItem(ev, "start_x");
                cJSON *sy  = cJSON_GetObjectItem(ev, "start_y");
                cJSON *ex  = cJSON_GetObjectItem(ev, "end_x");
                cJSON *ey  = cJSON_GetObjectItem(ev, "end_y");
                cJSON *tx = cJSON_GetObjectItem(ev, "trigger_x");
                cJSON *ty = cJSON_GetObjectItem(ev, "trigger_y");
                cJSON *ex_x = cJSON_GetObjectItem(ev, "exit_x");
                cJSON *ex_y = cJSON_GetObjectItem(ev, "exit_y");

                if (tid && sx && sy && ex && ey) {
                    RoofEvent *re = &ed->roof_events[ed->roof_event_count++];
                    re->tile_id = tid->valueint;
                    re->start_x = sx->valueint; re->start_y = sy->valueint;
                    re->end_x = ex->valueint;   re->end_y = ey->valueint;
                    re->trigger_x = (tx && cJSON_IsNumber(tx)) ? tx->valueint : -1;
                    re->trigger_y = (ty && cJSON_IsNumber(ty)) ? ty->valueint : -1;
                    re->exit_x = (ex_x && cJSON_IsNumber(ex_x)) ? ex_x->valueint : -1;
                    re->exit_y = (ex_y && cJSON_IsNumber(ex_y)) ? ex_y->valueint : -1;
                }
            }
        } else if (strcmp(type, "tile_change") == 0) {
            if (ed->tile_change_count < MAX_TILE_CHANGES) {
                cJSON *tx = cJSON_GetObjectItem(ev, "trigger_x");
                cJSON *ty = cJSON_GetObjectItem(ev, "trigger_y");
                cJSON *ntid = cJSON_GetObjectItem(ev, "new_tile_id");
                if (tx && ty && ntid) {
                    TileChangeEvent *tc = &ed->tile_changes[ed->tile_change_count++];
                    tc->trigger_x = tx->valueint;
                    tc->trigger_y = ty->valueint;
                    tc->new_tile_id = ntid->valueint;
                    cJSON *sx = cJSON_GetObjectItem(ev, "sample_x");
                    cJSON *sy = cJSON_GetObjectItem(ev, "sample_y");
                    tc->sample_x = (sx && cJSON_IsNumber(sx)) ? sx->valueint : -1;
                    tc->sample_y = (sy && cJSON_IsNumber(sy)) ? sy->valueint : -1;
                }
            }
        } else if (strcmp(type, "stairs") == 0) {
            if (ed->stair_event_count < MAX_STAIRS) {
                cJSON *sx = cJSON_GetObjectItem(ev, "start_x");
                cJSON *sy = cJSON_GetObjectItem(ev, "start_y");
                cJSON *ex = cJSON_GetObjectItem(ev, "end_x");
                cJSON *ey = cJSON_GetObjectItem(ev, "end_y");
                if (sx && sy && ex && ey) {
                    StairEvent *se = &ed->stair_events[ed->stair_event_count++];
                    se->start_x = sx->valueint;
                    se->start_y = sy->valueint;
                    se->end_x = ex->valueint;
                    se->end_y = ey->valueint;
                    cJSON *dir = cJSON_GetObjectItem(ev, "direction");
                    se->direction = (dir && cJSON_IsNumber(dir)) ? dir->valueint : 0;
                }
            }
        } else if (strcmp(type, "warp") == 0) {
            if (ed->warp_event_count < MAX_WARPS) {
                cJSON *tx = cJSON_GetObjectItem(ev, "trigger_x");
                cJSON *ty = cJSON_GetObjectItem(ev, "trigger_y");
                cJSON *tmap = cJSON_GetObjectItem(ev, "target_map");
                cJSON *tox = cJSON_GetObjectItem(ev, "target_x");
                cJSON *toy = cJSON_GetObjectItem(ev, "target_y");
                cJSON *facing = cJSON_GetObjectItem(ev, "facing");
                if (tx && ty && tmap && tox && toy && facing) {
                    WarpEvent *we = &ed->warp_events[ed->warp_event_count++];
                    we->trigger_x = tx->valueint;
                    we->trigger_y = ty->valueint;
                    snprintf(we->target_map, sizeof(we->target_map), "%s", tmap->valuestring);
                    we->target_map[63] = '\0';
                    we->target_x = tox->valueint;
                    we->target_y = toy->valueint;
                    we->facing = facing->valueint;
                }
            }
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

    // Сохраняем крыши
    for (int i = 0; i < ed->roof_event_count; i++) {
        RoofEvent *re = &ed->roof_events[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "roof");
        cJSON_AddNumberToObject(ev, "tile_id", re->tile_id);
        cJSON_AddNumberToObject(ev, "start_x", re->start_x);
        cJSON_AddNumberToObject(ev, "start_y", re->start_y);
        cJSON_AddNumberToObject(ev, "end_x", re->end_x);
        cJSON_AddNumberToObject(ev, "end_y", re->end_y);
        cJSON_AddNumberToObject(ev, "trigger_x", re->trigger_x);
        cJSON_AddNumberToObject(ev, "trigger_y", re->trigger_y);
        cJSON_AddNumberToObject(ev, "exit_x", re->exit_x);
        cJSON_AddNumberToObject(ev, "exit_y", re->exit_y);
        cJSON_AddItemToArray(root, ev);
    }

    // Сохраняем замены тайлов
    for (int i = 0; i < ed->tile_change_count; i++) {
        TileChangeEvent *tc = &ed->tile_changes[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "tile_change");
        cJSON_AddNumberToObject(ev, "trigger_x", tc->trigger_x);
        cJSON_AddNumberToObject(ev, "trigger_y", tc->trigger_y);
        cJSON_AddNumberToObject(ev, "new_tile_id", tc->new_tile_id);
        cJSON_AddNumberToObject(ev, "sample_x", tc->sample_x);
        cJSON_AddNumberToObject(ev, "sample_y", tc->sample_y);
        cJSON_AddItemToArray(root, ev);
    }

    // Сохраняем лестницы
    for (int i = 0; i < ed->stair_event_count; i++) {
        StairEvent *se = &ed->stair_events[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "stairs");
        cJSON_AddNumberToObject(ev, "start_x", se->start_x);
        cJSON_AddNumberToObject(ev, "start_y", se->start_y);
        cJSON_AddNumberToObject(ev, "end_x", se->end_x);
        cJSON_AddNumberToObject(ev, "end_y", se->end_y);
        cJSON_AddNumberToObject(ev, "direction", se->direction);
        cJSON_AddItemToArray(root, ev);
    }

    // Сохраняем варпы
    for (int i = 0; i < ed->warp_event_count; i++) {
        WarpEvent *we = &ed->warp_events[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "warp");
        cJSON_AddNumberToObject(ev, "trigger_x", we->trigger_x);
        cJSON_AddNumberToObject(ev, "trigger_y", we->trigger_y);
        cJSON_AddStringToObject(ev, "target_map", we->target_map);
        cJSON_AddNumberToObject(ev, "target_x", we->target_x);
        cJSON_AddNumberToObject(ev, "target_y", we->target_y);
        cJSON_AddNumberToObject(ev, "facing", we->facing);
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

    // === Подсветка для Roof Events ===
    // Красная рамка вокруг конкретной клетки-триггера
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
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        SDL_FRect thick_rect = { tile_dst.x + dx, tile_dst.y + dy, tile_dst.w, tile_dst.h };
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
            int ex_x = re->exit_x, ex_y = re->exit_y;
            if (ex_x >= 0 && ex_x < map->width && ex_y >= 0 && ex_y < map->height) {
                SDL_FRect ex_dst = {
                    MAP_X + (ex_x * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (ex_y * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile, scaled_tile
                };
                SDL_SetRenderDrawColor(ed->renderer, 0, 150, 255, 255);
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        SDL_FRect thick = { ex_dst.x + dx, ex_dst.y + dy, ex_dst.w, ex_dst.h };
                        SDL_RenderDrawRectF(ed->renderer, &thick);
                    }
                }
            }
        }
    }

    // Зелёная рамка зоны крыши
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
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                SDL_FRect thick_zone = { zone.x + dx, zone.y + dy, zone.w, zone.h };
                SDL_RenderDrawRectF(ed->renderer, &thick_zone);
            }
        }
    }

    // === Подсветка для Tile Changes ===
    if (ed->selected_tile_change >= 0 && ed->selected_tile_change < ed->tile_change_count) {
        TileChangeEvent *tc = &ed->tile_changes[ed->selected_tile_change];

        // Жёлтая рамка вокруг клетки-триггера
        if (tc->trigger_x >= 0 && tc->trigger_y >= 0) {
            int ttx = tc->trigger_x, tty = tc->trigger_y;
            if (ttx >= 0 && ttx < map->width && tty >= 0 && tty < map->height) {
                SDL_FRect tdst = {
                    MAP_X + (ttx * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (tty * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile, scaled_tile
                };
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255); // жёлтый
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        SDL_FRect r = { tdst.x + dx, tdst.y + dy, tdst.w, tdst.h };
                        SDL_RenderDrawRectF(ed->renderer, &r);
                    }
                }
            }
        }

        // Оранжевая рамка вокруг клетки-образца (откуда взят новый тайл)
        if (tc->new_tile_id >= 0 && tc->sample_x >= 0 && tc->sample_y >= 0) {
            int ox = tc->sample_x, oy = tc->sample_y;
            if (ox >= 0 && ox < map->width && oy >= 0 && oy < map->height) {
                SDL_FRect odst = {
                    MAP_X + (ox * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (oy * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile, scaled_tile
                };
                SDL_SetRenderDrawColor(ed->renderer, 255, 165, 0, 255); // оранжевый
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        SDL_FRect r = { odst.x + dx, odst.y + dy, odst.w, odst.h };
                        SDL_RenderDrawRectF(ed->renderer, &r);
                    }
                }
            }
        }
    }

    // === Подсветка для Stairs (диагональные линии) ===
    if (ed->selected_stair >= 0 && ed->selected_stair < ed->stair_event_count) {
        StairEvent *se = &ed->stair_events[ed->selected_stair];
        int x1 = se->start_x, y1 = se->start_y;
        int x2 = se->end_x,   y2 = se->end_y;

        // Определяем направление
        int dx = (x2 > x1) ? 1 : ((x2 < x1) ? -1 : 0);
        int dy = (y2 > y1) ? 1 : ((y2 < y1) ? -1 : 0);
        int steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
        if (steps == 0) steps = 1; // одна клетка

        // Для каждой клетки вдоль диагонали рисуем жирную линию
        SDL_SetRenderDrawColor(ed->renderer, 0, 120, 255, 255); // синий
        for (int i = 0; i <= steps; i++) {
            int cx = x1 + i * dx;
            int cy = y1 + i * dy;
            if (cx < 0 || cx >= map->width || cy < 0 || cy >= map->height) continue;

            // Экранные координаты центра клетки
            float center_x = MAP_X + (cx * TILE_SIZE - ed->cam_x + TILE_SIZE/2.0f) * zoom;
            float center_y = MAP_Y + (cy * TILE_SIZE - ed->cam_y + TILE_SIZE/2.0f) * zoom;
            float half = (TILE_SIZE / 2.0f) * zoom;

            // Определяем, какой символ рисовать: '/' или '\'
            bool forward_slash = (se->direction == 1); // теперь 1 = '/', 0 = '\'

            // Рисуем жирную линию (толщина 3 пикселя)
            for (int t = -1; t <= 1; t++) {
                if (forward_slash) {
                    SDL_RenderDrawLineF(ed->renderer,
                        center_x - half, center_y + half + t,
                        center_x + half, center_y - half + t);
                } else {
                    SDL_RenderDrawLineF(ed->renderer,
                        center_x - half, center_y - half + t,
                        center_x + half, center_y + half + t);
                }
            }
        }
    }

    // === Подсветка для Warp Events ===
    if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
        WarpEvent *we = &ed->warp_events[ed->selected_warp];
        if (we->trigger_x >= 0 && we->trigger_y >= 0) {
            int wx = we->trigger_x, wy = we->trigger_y;
            if (wx >= 0 && wx < map->width && wy >= 0 && wy < map->height) {
                SDL_FRect tile_dst = {
                    MAP_X + (wx * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (wy * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile, scaled_tile
                };

                // Полупрозрачная фиолетовая заливка
                SDL_SetRenderDrawBlendMode(ed->renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ed->renderer, 200, 0, 200, 80);
                SDL_RenderFillRectF(ed->renderer, &tile_dst);

                // Жирная фиолетовая рамка
                SDL_SetRenderDrawColor(ed->renderer, 255, 0, 255, 255);
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        SDL_FRect r = { tile_dst.x + dx, tile_dst.y + dy, tile_dst.w, tile_dst.h };
                        SDL_RenderDrawRectF(ed->renderer, &r);
                    }
                }
                SDL_SetRenderDrawBlendMode(ed->renderer, SDL_BLENDMODE_NONE);

                // Жёлтая стрелка направления
                float cx = tile_dst.x + scaled_tile/2.0f;
                float cy = tile_dst.y + scaled_tile/2.0f;
                float sz = scaled_tile * 0.3f;
                SDL_FPoint arrow[3];
                switch (we->facing) {
                    case 2: arrow[0] = (SDL_FPoint){cx, cy + sz}; arrow[1] = (SDL_FPoint){cx - sz, cy - sz/2}; arrow[2] = (SDL_FPoint){cx + sz, cy - sz/2}; break;
                    case 4: arrow[0] = (SDL_FPoint){cx - sz, cy}; arrow[1] = (SDL_FPoint){cx + sz/2, cy - sz}; arrow[2] = (SDL_FPoint){cx + sz/2, cy + sz}; break;
                    case 6: arrow[0] = (SDL_FPoint){cx + sz, cy}; arrow[1] = (SDL_FPoint){cx - sz/2, cy - sz}; arrow[2] = (SDL_FPoint){cx - sz/2, cy + sz}; break;
                    case 8: arrow[0] = (SDL_FPoint){cx, cy - sz}; arrow[1] = (SDL_FPoint){cx - sz, cy + sz/2}; arrow[2] = (SDL_FPoint){cx + sz, cy + sz/2}; break;
                    default: arrow[0]=arrow[1]=arrow[2] = (SDL_FPoint){0,0}; break;
                }
                if (we->facing >= 2 && we->facing <= 8) {
                    SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                    SDL_RenderDrawLinesF(ed->renderer, arrow, 3);
                    SDL_RenderDrawLineF(ed->renderer, arrow[2].x, arrow[2].y, arrow[0].x, arrow[0].y);
                }
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

static void draw_tc_field(Editor *ed, const char *label, int field_idx, int line_y) {
    SDL_Rect lbl_rect = {10, line_y, 65, 20};
    draw_text_centered(ed->renderer, ed->font, label, lbl_rect.x + lbl_rect.w/2, lbl_rect.y + lbl_rect.h/2,
                       (SDL_Color){200, 200, 200, 255});
    SDL_Rect fld_rect = {90, line_y, 130, 20};
    bool active = (ed->tc_edit_field == field_idx);
    SDL_SetRenderDrawColor(ed->renderer, active ? 255 : 200, active ? 255 : 200, active ? 255 : 200, 255);
    SDL_RenderFillRect(ed->renderer, &fld_rect);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(ed->renderer, &fld_rect);

    char buf[32] = "";
    if (ed->selected_tile_change >= 0 && ed->selected_tile_change < ed->tile_change_count) {
        TileChangeEvent *te = &ed->tile_changes[ed->selected_tile_change];
        if (field_idx == 0)
            snprintf(buf, sizeof(buf), "%d,%d", te->trigger_x, te->trigger_y);
        else if (field_idx == 1)
            snprintf(buf, sizeof(buf), "%d", te->new_tile_id);
    }
    if (active && ed->tc_input_buf[0])
        snprintf(buf, sizeof(buf), "%s", ed->tc_input_buf);
    draw_text_centered(ed->renderer, ed->font, buf, fld_rect.x + fld_rect.w/2, fld_rect.y + fld_rect.h/2,
                       (SDL_Color){0, 0, 0, 255});
}

static void check_tc_click(Editor *ed, int field_idx, int line_y, int mx, int my) {
    SDL_Rect fld = {90, line_y, 130, 20};
    if (mx >= fld.x && mx < fld.x + fld.w && my >= fld.y && my < fld.y + fld.h) {
        ed->tc_edit_field = field_idx;
        if (ed->selected_tile_change >= 0 && ed->selected_tile_change < ed->tile_change_count) {
            TileChangeEvent *te = &ed->tile_changes[ed->selected_tile_change];
            if (field_idx == 0)
                snprintf(ed->tc_input_buf, sizeof(ed->tc_input_buf), "%d,%d", te->trigger_x, te->trigger_y);
            else if (field_idx == 1)
                snprintf(ed->tc_input_buf, sizeof(ed->tc_input_buf), "%d", te->new_tile_id);
        }
        SDL_StartTextInput();
    }
}

static void draw_stair_field(Editor *ed, const char *label, int field_idx, int line_y) {
    SDL_Rect lbl_rect = {10, line_y, 65, 20};
    draw_text_centered(ed->renderer, ed->font, label, lbl_rect.x + lbl_rect.w/2, lbl_rect.y + lbl_rect.h/2,
                       (SDL_Color){200, 200, 200, 255});
    SDL_Rect fld_rect = {90, line_y, 130, 20};
    bool active = (ed->stair_edit_field == field_idx);
    SDL_SetRenderDrawColor(ed->renderer, active ? 255 : 200, active ? 255 : 200, active ? 255 : 200, 255);
    SDL_RenderFillRect(ed->renderer, &fld_rect);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(ed->renderer, &fld_rect);

    char buf[32] = "";
    if (ed->selected_stair >= 0 && ed->selected_stair < ed->stair_event_count) {
        StairEvent *se = &ed->stair_events[ed->selected_stair];
        if (field_idx == 0)
            snprintf(buf, sizeof(buf), "%d,%d", se->start_x, se->start_y);
        else if (field_idx == 1)
            snprintf(buf, sizeof(buf), "%d,%d", se->end_x, se->end_y);
    }
    if (active && ed->stair_input_buf[0])
        snprintf(buf, sizeof(buf), "%s", ed->stair_input_buf);
    draw_text_centered(ed->renderer, ed->font, buf, fld_rect.x + fld_rect.w/2, fld_rect.y + fld_rect.h/2,
                       (SDL_Color){0, 0, 0, 255});
}

static void check_stair_click(Editor *ed, int field_idx, int line_y, int mx, int my) {
    SDL_Rect fld = {90, line_y, 130, 20};
    if (mx >= fld.x && mx < fld.x + fld.w && my >= fld.y && my < fld.y + fld.h) {
        ed->stair_edit_field = field_idx;
        if (ed->selected_stair >= 0 && ed->selected_stair < ed->stair_event_count) {
            StairEvent *se = &ed->stair_events[ed->selected_stair];
            if (field_idx == 0)
                snprintf(ed->stair_input_buf, sizeof(ed->stair_input_buf), "%d,%d", se->start_x, se->start_y);
            else if (field_idx == 1)
                snprintf(ed->stair_input_buf, sizeof(ed->stair_input_buf), "%d,%d", se->end_x, se->end_y);
        }
        SDL_StartTextInput();
    }
}

static void draw_warp_field(Editor *ed, const char *label, int field_idx, int line_y) {
    SDL_Rect lbl_rect = {10, line_y, 65, 20};
    draw_text_centered(ed->renderer, ed->font, label, lbl_rect.x + lbl_rect.w/2, lbl_rect.y + lbl_rect.h/2,
                       (SDL_Color){200, 200, 200, 255});

    SDL_Rect fld_rect = {90, line_y, 130, 20};
    bool active = (ed->warp_edit_field == field_idx);

    if (field_idx == 1) {
        // ========== Target Map – выбор стрелками ==========
        // Фон поля
        SDL_SetRenderDrawColor(ed->renderer, active ? 220 : 200, 200, 200, 255);
        SDL_RenderFillRect(ed->renderer, &fld_rect);
        SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(ed->renderer, &fld_rect);

        // Кнопки-стрелки
        SDL_Rect left_arrow  = { fld_rect.x, fld_rect.y, 18, fld_rect.h };
        SDL_Rect right_arrow = { fld_rect.x + fld_rect.w - 18, fld_rect.y, 18, fld_rect.h };
        SDL_Rect name_rect   = { left_arrow.x + left_arrow.w, fld_rect.y,
                                 right_arrow.x - (left_arrow.x + left_arrow.w), fld_rect.h };

        SDL_SetRenderDrawColor(ed->renderer, 80, 80, 80, 255);
        SDL_RenderFillRect(ed->renderer, &left_arrow);
        SDL_RenderFillRect(ed->renderer, &right_arrow);
        draw_text_centered(ed->renderer, ed->font, "<", left_arrow.x + left_arrow.w/2, left_arrow.y + left_arrow.h/2,
                           (SDL_Color){255,255,255,255});
        draw_text_centered(ed->renderer, ed->font, ">", right_arrow.x + right_arrow.w/2, right_arrow.y + right_arrow.h/2,
                           (SDL_Color){255,255,255,255});

        // Имя карты из entries.json
        char display_name[64] = "???";
        if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
            WarpEvent *we = &ed->warp_events[ed->selected_warp];
            for (int i = 0; i < ed->map_list.map_count; i++) {
                if (strcmp(ed->map_list.maps[i].folder, we->target_map) == 0) {
                    snprintf(display_name, sizeof(display_name), "%s", ed->map_list.maps[i].name);
                    break;
                }
            }
            if (display_name[0] == '?' && we->target_map[0] != '\0')
                snprintf(display_name, sizeof(display_name), "%s", we->target_map);
        }
        draw_text_centered(ed->renderer, ed->font, display_name, name_rect.x + name_rect.w/2, name_rect.y + name_rect.h/2,
                           (SDL_Color){0,0,0,255});
    } else {
        // ========== Остальные поля – текстовый ввод ==========
        SDL_SetRenderDrawColor(ed->renderer, active ? 255 : 200, active ? 255 : 200, active ? 255 : 200, 255);
        SDL_RenderFillRect(ed->renderer, &fld_rect);
        SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(ed->renderer, &fld_rect);

        char buf[64] = "";
        if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
            WarpEvent *we = &ed->warp_events[ed->selected_warp];
            if (field_idx == 0)
                snprintf(buf, sizeof(buf), "%d,%d", we->trigger_x, we->trigger_y);
            else if (field_idx == 2)
                snprintf(buf, sizeof(buf), "%d,%d", we->target_x, we->target_y);
            else if (field_idx == 3)
                snprintf(buf, sizeof(buf), "%d", we->facing);
        }
        if (active && ed->warp_input_buf[0])
            snprintf(buf, sizeof(buf), "%s", ed->warp_input_buf);
        draw_text_centered(ed->renderer, ed->font, buf, fld_rect.x + fld_rect.w/2, fld_rect.y + fld_rect.h/2,
                           (SDL_Color){0, 0, 0, 255});
    }
}

static void check_warp_click(Editor *ed, int field_idx, int line_y, int mx, int my) {
    SDL_Rect fld = {90, line_y, 130, 20};
    if (mx >= fld.x && mx < fld.x + fld.w && my >= fld.y && my < fld.y + fld.h) {
        if (field_idx == 1) {
            // Активируем поле (для клавиш влево/вправо)
            ed->warp_edit_field = 1;
            SDL_StopTextInput(); // текстовый ввод не нужен

            SDL_Rect left_arrow  = { fld.x, fld.y, 18, fld.h };
            SDL_Rect right_arrow = { fld.x + fld.w - 18, fld.y, 18, fld.h };

            if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
                WarpEvent *we = &ed->warp_events[ed->selected_warp];
                int cur = -1;
                for (int i = 0; i < ed->map_list.map_count; i++) {
                    if (strcmp(ed->map_list.maps[i].folder, we->target_map) == 0) { cur = i; break; }
                }

                if (mx >= left_arrow.x && mx < left_arrow.x + left_arrow.w) {
                    if (ed->map_list.map_count > 0) {
                        if (cur == -1) cur = 0;
                        else cur = (cur - 1 + ed->map_list.map_count) % ed->map_list.map_count;
                        snprintf(we->target_map, sizeof(we->target_map), "%s", ed->map_list.maps[cur].folder);
                    }
                } else if (mx >= right_arrow.x && mx < right_arrow.x + right_arrow.w) {
                    if (ed->map_list.map_count > 0) {
                        if (cur == -1) cur = 0;
                        else cur = (cur + 1) % ed->map_list.map_count;
                        snprintf(we->target_map, sizeof(we->target_map), "%s", ed->map_list.maps[cur].folder);
                    }
                }
            }
            return; // выходим, не переходя к текстовому вводу
        }

        // Остальные поля – стандартный текстовый ввод
        ed->warp_edit_field = field_idx;
        if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
            WarpEvent *we = &ed->warp_events[ed->selected_warp];
            if (field_idx == 0)
                snprintf(ed->warp_input_buf, sizeof(ed->warp_input_buf), "%d,%d", we->trigger_x, we->trigger_y);
            else if (field_idx == 2)
                snprintf(ed->warp_input_buf, sizeof(ed->warp_input_buf), "%d,%d", we->target_x, we->target_y);
            else if (field_idx == 3)
                snprintf(ed->warp_input_buf, sizeof(ed->warp_input_buf), "%d", we->facing);
        }
        SDL_StartTextInput();
    }
}


// ─── Левая панель ──
void render_left_panel(Editor *ed) {
    SDL_Rect bg = {0, 0, LEFT_PANEL_W, WINDOW_H};
    SDL_SetRenderDrawColor(ed->renderer, 50,50,50,255);
    SDL_RenderFillRect(ed->renderer, &bg);

    int y = 10;

    // ====================== ROOF EVENTS ======================
    // Заголовок Roof Events (всегда видим)
    SDL_Rect roof_header = {10, y, LEFT_PANEL_W-20, 24};
    SDL_SetRenderDrawColor(ed->renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(ed->renderer, &roof_header);
    draw_text_centered(ed->renderer, ed->font, "ROOF EVENTS", LEFT_PANEL_W/2, y+12, (SDL_Color){255,255,255,255});

    // Кнопка сворачивания Roof Events
    SDL_Rect roof_collapse_btn = { LEFT_PANEL_W - 35, y + 1, 26, 22 };
    SDL_SetRenderDrawColor(ed->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(ed->renderer, &roof_collapse_btn);
    draw_text_centered(ed->renderer, ed->font, ed->left_panel_collapsed ? "+" : "—",
                       roof_collapse_btn.x + roof_collapse_btn.w/2, roof_collapse_btn.y + roof_collapse_btn.h/2,
                       (SDL_Color){255,255,255,255});
    y += 26;

    // Содержимое Roof Events (только если развёрнуто)
    if (!ed->left_panel_collapsed) {
        SDL_Rect add_btn = {10, y, LEFT_PANEL_W-20, 24};
        SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
        SDL_RenderFillRect(ed->renderer, &add_btn);
        draw_text_centered(ed->renderer, ed->font, "Add Roof Event", add_btn.x+add_btn.w/2, add_btn.y+add_btn.h/2,
                           (SDL_Color){255,255,255,255});
        y += 30;

        SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
        SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
        y += 5;

        int list_start_y = y;
        int list_h = 200;
        int item_h = 18;
        int max_visible = 10;                     // ровно 10 элементов
        ed->roof_list_rect = (SDL_Rect){10, list_start_y, LEFT_PANEL_W-20, list_h};

        int max_scroll = (ed->roof_event_count > max_visible) ? ed->roof_event_count - max_visible : 0;
        if (ed->roof_event_scroll < 0) ed->roof_event_scroll = 0;
        if (ed->roof_event_scroll > max_scroll) ed->roof_event_scroll = max_scroll;

        SDL_Rect list_clip = {10, list_start_y, LEFT_PANEL_W-20, list_h};
        SDL_RenderSetClipRect(ed->renderer, &list_clip);
        int start_idx = ed->roof_event_scroll;
        int end_idx = (start_idx + max_visible < ed->roof_event_count) ? start_idx + max_visible : ed->roof_event_count;
        for (int i = start_idx; i < end_idx; i++) {
            RoofEvent *re = &ed->roof_events[i];
            char buf[128];
            snprintf(buf, sizeof(buf), "Tile %d  (%d,%d)-(%d,%d)",
                     re->tile_id, re->start_x, re->start_y, re->end_x, re->end_y);
            SDL_Color col = (i == ed->selected_roof_event) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,255,255};
            SDL_Rect item_rect = {10, list_start_y + (i - start_idx) * item_h, LEFT_PANEL_W-20, item_h};
            if (i == ed->selected_roof_event) {
                SDL_SetRenderDrawColor(ed->renderer, 80,80,120,255);
                SDL_RenderFillRect(ed->renderer, &item_rect);
            }
            draw_text_centered(ed->renderer, ed->font, buf, LEFT_PANEL_W/2, item_rect.y + item_h/2, col);
        }
        SDL_RenderSetClipRect(ed->renderer, NULL);

        // Скроллбар
        if (ed->roof_event_count > max_visible) {
            int bar_x = LEFT_PANEL_W - 12, bar_w = 6;
            SDL_Rect track = { bar_x, list_start_y, bar_w, list_h };
            SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
            SDL_RenderFillRect(ed->renderer, &track);
            float thumb_h = (float)max_visible / ed->roof_event_count * list_h;
            if (thumb_h < 12) thumb_h = 12;
            int thumb_y = list_start_y + (int)((list_h - thumb_h) * ((float)ed->roof_event_scroll / max_scroll));
            SDL_Rect thumb = { bar_x, thumb_y, bar_w, (int)thumb_h };
            SDL_SetRenderDrawColor(ed->renderer, 180,180,180,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }

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
            draw_roof_field(ed, "Exit", 3, edit_y);
            edit_y += 22;

            SDL_Rect del_btn = {10, edit_y, LEFT_PANEL_W-20, 24};
            SDL_SetRenderDrawColor(ed->renderer, 180, 80, 80, 255);
            SDL_RenderFillRect(ed->renderer, &del_btn);
            draw_text_centered(ed->renderer, ed->font, "Delete Event", del_btn.x+del_btn.w/2, del_btn.y+del_btn.h/2,
                               (SDL_Color){255,255,255,255});
            y = edit_y + 24;
        }
    }

    // ====================== TILE CHANGES ======================
    y += 10;  // небольшой отступ между секциями
    ed->tc_section_y = y;   // запоминаем для обработки кликов

    SDL_Rect tc_header = {10, y, LEFT_PANEL_W-20, 24};
    SDL_SetRenderDrawColor(ed->renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(ed->renderer, &tc_header);
    draw_text_centered(ed->renderer, ed->font, "TILE CHANGES", LEFT_PANEL_W/2, y+12, (SDL_Color){255,255,255,255});

    SDL_Rect tc_collapse_btn = { LEFT_PANEL_W - 35, y + 1, 26, 22 };
    SDL_SetRenderDrawColor(ed->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(ed->renderer, &tc_collapse_btn);
    draw_text_centered(ed->renderer, ed->font, ed->tc_section_collapsed ? "+" : "—",
                       tc_collapse_btn.x + tc_collapse_btn.w/2, tc_collapse_btn.y + tc_collapse_btn.h/2,
                       (SDL_Color){255,255,255,255});
    y += 26;

    if (!ed->tc_section_collapsed) {
        SDL_Rect tc_add_btn = {10, y, LEFT_PANEL_W-20, 24};
        SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
        SDL_RenderFillRect(ed->renderer, &tc_add_btn);
        draw_text_centered(ed->renderer, ed->font, "Add Tile Change", tc_add_btn.x+tc_add_btn.w/2, tc_add_btn.y+tc_add_btn.h/2,
                           (SDL_Color){255,255,255,255});
        y += 30;

        SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
        SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
        y += 5;

        int tc_list_start_y = y;
        int tc_list_h = 100;
        int tc_item_h = 18;
        int tc_max_visible = 5;                  // ровно 5 элементов
        ed->tc_list_rect = (SDL_Rect){10, tc_list_start_y, LEFT_PANEL_W-20, tc_list_h};

        int tc_max_scroll = (ed->tile_change_count > tc_max_visible) ? ed->tile_change_count - tc_max_visible : 0;
        if (ed->tile_change_scroll < 0) ed->tile_change_scroll = 0;
        if (ed->tile_change_scroll > tc_max_scroll) ed->tile_change_scroll = tc_max_scroll;

        SDL_Rect tc_list_clip = {10, tc_list_start_y, LEFT_PANEL_W-20, tc_list_h};
        SDL_RenderSetClipRect(ed->renderer, &tc_list_clip);
        int tc_start = ed->tile_change_scroll;
        int tc_end = (tc_start + tc_max_visible < ed->tile_change_count) ? tc_start + tc_max_visible : ed->tile_change_count;
        for (int i = tc_start; i < tc_end; i++) {
            TileChangeEvent *te = &ed->tile_changes[i];
            char buf[64];
            snprintf(buf, sizeof(buf), "(%d,%d) -> Tile %d", te->trigger_x, te->trigger_y, te->new_tile_id);
            SDL_Color col = (i == ed->selected_tile_change) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,255,255};
            SDL_Rect item_rect = {10, tc_list_start_y + (i - tc_start) * tc_item_h, LEFT_PANEL_W-20, tc_item_h};
            if (i == ed->selected_tile_change) {
                SDL_SetRenderDrawColor(ed->renderer, 80,80,120,255);
                SDL_RenderFillRect(ed->renderer, &item_rect);
            }
            draw_text_centered(ed->renderer, ed->font, buf, LEFT_PANEL_W/2, item_rect.y + tc_item_h/2, col);
        }
        SDL_RenderSetClipRect(ed->renderer, NULL);

        // Скроллбар
        if (ed->tile_change_count > tc_max_visible) {
            int bar_x = LEFT_PANEL_W - 12, bar_w = 6;
            SDL_Rect track = { bar_x, tc_list_start_y, bar_w, tc_list_h };
            SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
            SDL_RenderFillRect(ed->renderer, &track);
            float thumb_h = (float)tc_max_visible / ed->tile_change_count * tc_list_h;
            if (thumb_h < 12) thumb_h = 12;
            int thumb_y = tc_list_start_y + (int)((tc_list_h - thumb_h) * ((float)ed->tile_change_scroll / tc_max_scroll));
            SDL_Rect thumb = { bar_x, thumb_y, bar_w, (int)thumb_h };
            SDL_SetRenderDrawColor(ed->renderer, 180,180,180,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }

        y = tc_list_start_y + tc_list_h + 5;

        SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
        SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
        y += 5;

        if (ed->selected_tile_change >= 0 && ed->selected_tile_change < ed->tile_change_count) {
            int edit_y = y;
            draw_tc_field(ed, "Trigger", 0, edit_y);
            edit_y += 22;
            draw_tc_field(ed, "New Tile", 1, edit_y);
            edit_y += 22;

            SDL_Rect del_btn = {10, edit_y, LEFT_PANEL_W-20, 24};
            SDL_SetRenderDrawColor(ed->renderer, 180, 80, 80, 255);
            SDL_RenderFillRect(ed->renderer, &del_btn);
            draw_text_centered(ed->renderer, ed->font, "Delete", del_btn.x+del_btn.w/2, del_btn.y+del_btn.h/2,
                               (SDL_Color){255,255,255,255});
        }
    }

    // ====================== STAIRS ======================
    y += 10;
    ed->stair_section_y = y;

    SDL_Rect stair_header = {10, y, LEFT_PANEL_W-20, 24};
    SDL_SetRenderDrawColor(ed->renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(ed->renderer, &stair_header);
    draw_text_centered(ed->renderer, ed->font, "STAIRS", LEFT_PANEL_W/2, y+12, (SDL_Color){255,255,255,255});

    SDL_Rect stair_collapse_btn = { LEFT_PANEL_W - 35, y + 1, 26, 22 };
    SDL_SetRenderDrawColor(ed->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(ed->renderer, &stair_collapse_btn);
    draw_text_centered(ed->renderer, ed->font, ed->stair_section_collapsed ? "+" : "—",
                       stair_collapse_btn.x + stair_collapse_btn.w/2, stair_collapse_btn.y + stair_collapse_btn.h/2,
                       (SDL_Color){255,255,255,255});
    y += 26;

    if (!ed->stair_section_collapsed) {
        SDL_Rect stair_add_btn = {10, y, LEFT_PANEL_W-20, 24};
        SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
        SDL_RenderFillRect(ed->renderer, &stair_add_btn);
        draw_text_centered(ed->renderer, ed->font, "Add Stairs", stair_add_btn.x+stair_add_btn.w/2, stair_add_btn.y+stair_add_btn.h/2,
                           (SDL_Color){255,255,255,255});
        y += 30;

        SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
        SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
        y += 5;

        int stair_list_start_y = y;
        int stair_list_h = 100;
        int stair_item_h = 18;
        int stair_max_visible = 5;                  // 5 элементов
        ed->stair_list_rect = (SDL_Rect){10, stair_list_start_y, LEFT_PANEL_W-20, stair_list_h};

        int stair_max_scroll = (ed->stair_event_count > stair_max_visible) ? ed->stair_event_count - stair_max_visible : 0;
        if (ed->stair_event_scroll < 0) ed->stair_event_scroll = 0;
        if (ed->stair_event_scroll > stair_max_scroll) ed->stair_event_scroll = stair_max_scroll;

        SDL_Rect stair_list_clip = {10, stair_list_start_y, LEFT_PANEL_W-20, stair_list_h};
        SDL_RenderSetClipRect(ed->renderer, &stair_list_clip);
        int stair_start = ed->stair_event_scroll;
        int stair_end = (stair_start + stair_max_visible < ed->stair_event_count) ? stair_start + stair_max_visible : ed->stair_event_count;
        for (int i = stair_start; i < stair_end; i++) {
            StairEvent *se = &ed->stair_events[i];
            char buf[64];
            snprintf(buf, sizeof(buf), "(%d,%d)->(%d,%d)", se->start_x, se->start_y, se->end_x, se->end_y);
            SDL_Color col = (i == ed->selected_stair) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,255,255};
            SDL_Rect item_rect = {10, stair_list_start_y + (i - stair_start) * stair_item_h, LEFT_PANEL_W-20, stair_item_h};
            if (i == ed->selected_stair) {
                SDL_SetRenderDrawColor(ed->renderer, 80,80,120,255);
                SDL_RenderFillRect(ed->renderer, &item_rect);
            }
            draw_text_centered(ed->renderer, ed->font, buf, LEFT_PANEL_W/2, item_rect.y + stair_item_h/2, col);
        }
        SDL_RenderSetClipRect(ed->renderer, NULL);

        if (ed->stair_event_count > stair_max_visible) {
            int bar_x = LEFT_PANEL_W - 12, bar_w = 6;
            SDL_Rect track = { bar_x, stair_list_start_y, bar_w, stair_list_h };
            SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
            SDL_RenderFillRect(ed->renderer, &track);
            float thumb_h = (float)stair_max_visible / ed->stair_event_count * stair_list_h;
            if (thumb_h < 12) thumb_h = 12;
            int thumb_y = stair_list_start_y + (int)((stair_list_h - thumb_h) * ((float)ed->stair_event_scroll / stair_max_scroll));
            SDL_Rect thumb = { bar_x, thumb_y, bar_w, (int)thumb_h };
            SDL_SetRenderDrawColor(ed->renderer, 180,180,180,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }

        y = stair_list_start_y + stair_list_h + 5;

        SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
        SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
        y += 5;

        if (ed->selected_stair >= 0 && ed->selected_stair < ed->stair_event_count) {
            int edit_y = y;
            draw_stair_field(ed, "Start", 0, edit_y);
            edit_y += 22;
            draw_stair_field(ed, "End", 1, edit_y);
            edit_y += 22;

            // Кнопки выбора направления лестницы
            StairEvent *se = &ed->stair_events[ed->selected_stair];
            int dir_btn_y = edit_y;
            SDL_Rect slash_btn = {10, dir_btn_y, 50, 20};
            SDL_Rect bslash_btn = {70, dir_btn_y, 50, 20};

            SDL_SetRenderDrawColor(ed->renderer, se->direction == 1 ? 100 : 70, 200, 100, 255);
            SDL_RenderFillRect(ed->renderer, &slash_btn);
            draw_text_centered(ed->renderer, ed->font, "/", slash_btn.x+slash_btn.w/2, slash_btn.y+slash_btn.h/2,
                               (SDL_Color){255,255,255,255});

            SDL_SetRenderDrawColor(ed->renderer, se->direction == 0 ? 100 : 70, 200, 100, 255);
            SDL_RenderFillRect(ed->renderer, &bslash_btn);
            draw_text_centered(ed->renderer, ed->font, "\\", bslash_btn.x+bslash_btn.w/2, bslash_btn.y+bslash_btn.h/2,
                               (SDL_Color){255,255,255,255});

            edit_y += 22;  // сдвиг для кнопки Delete

            SDL_Rect del_btn = {10, edit_y, LEFT_PANEL_W-20, 24};
            SDL_SetRenderDrawColor(ed->renderer, 180, 80, 80, 255);
            SDL_RenderFillRect(ed->renderer, &del_btn);
            draw_text_centered(ed->renderer, ed->font, "Delete", del_btn.x+del_btn.w/2, del_btn.y+del_btn.h/2,
                               (SDL_Color){255,255,255,255});
        }
    }

    // ====================== WARP EVENTS ======================
    y += 10;
    ed->warp_section_y = y;

    SDL_Rect warp_header = {10, y, LEFT_PANEL_W-20, 24};
    SDL_SetRenderDrawColor(ed->renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(ed->renderer, &warp_header);
    draw_text_centered(ed->renderer, ed->font, "WARP EVENTS", LEFT_PANEL_W/2, y+12, (SDL_Color){255,255,255,255});

    SDL_Rect warp_collapse_btn = { LEFT_PANEL_W - 35, y + 1, 26, 22 };
    SDL_SetRenderDrawColor(ed->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(ed->renderer, &warp_collapse_btn);
    draw_text_centered(ed->renderer, ed->font, ed->warp_section_collapsed ? "+" : "—",
                       warp_collapse_btn.x + warp_collapse_btn.w/2, warp_collapse_btn.y + warp_collapse_btn.h/2,
                       (SDL_Color){255,255,255,255});
    y += 26;

    if (!ed->warp_section_collapsed) {
        SDL_Rect warp_add_btn = {10, y, LEFT_PANEL_W-20, 24};
        SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
        SDL_RenderFillRect(ed->renderer, &warp_add_btn);
        draw_text_centered(ed->renderer, ed->font, "Add Warp", warp_add_btn.x+warp_add_btn.w/2, warp_add_btn.y+warp_add_btn.h/2,
                           (SDL_Color){255,255,255,255});
        y += 30;

        SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
        SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
        y += 5;

        int warp_list_start_y = y;
        int warp_list_h = 100;
        int warp_item_h = 18;
        int warp_max_visible = 5;
        ed->warp_list_rect = (SDL_Rect){10, warp_list_start_y, LEFT_PANEL_W-20, warp_list_h};

        int warp_max_scroll = (ed->warp_event_count > warp_max_visible) ? ed->warp_event_count - warp_max_visible : 0;
        if (ed->warp_event_scroll < 0) ed->warp_event_scroll = 0;
        if (ed->warp_event_scroll > warp_max_scroll) ed->warp_event_scroll = warp_max_scroll;

        SDL_Rect warp_list_clip = {10, warp_list_start_y, LEFT_PANEL_W-20, warp_list_h};
        SDL_RenderSetClipRect(ed->renderer, &warp_list_clip);
        int warp_start = ed->warp_event_scroll;
        int warp_end = (warp_start + warp_max_visible < ed->warp_event_count) ? warp_start + warp_max_visible : ed->warp_event_count;
        for (int i = warp_start; i < warp_end; i++) {
            WarpEvent *we = &ed->warp_events[i];
            char buf[128];
            snprintf(buf, sizeof(buf), "(%d,%d) -> %s (%d,%d)", we->trigger_x, we->trigger_y, we->target_map, we->target_x, we->target_y);
            SDL_Color col = (i == ed->selected_warp) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,255,255};
            SDL_Rect item_rect = {10, warp_list_start_y + (i - warp_start) * warp_item_h, LEFT_PANEL_W-20, warp_item_h};
            if (i == ed->selected_warp) {
                SDL_SetRenderDrawColor(ed->renderer, 80,80,120,255);
                SDL_RenderFillRect(ed->renderer, &item_rect);
            }
            draw_text_centered(ed->renderer, ed->font, buf, LEFT_PANEL_W/2, item_rect.y + warp_item_h/2, col);
        }
        SDL_RenderSetClipRect(ed->renderer, NULL);

        if (ed->warp_event_count > warp_max_visible) {
            int bar_x = LEFT_PANEL_W - 12, bar_w = 6;
            SDL_Rect track = { bar_x, warp_list_start_y, bar_w, warp_list_h };
            SDL_SetRenderDrawColor(ed->renderer, 90,90,90,255);
            SDL_RenderFillRect(ed->renderer, &track);
            float thumb_h = (float)warp_max_visible / ed->warp_event_count * warp_list_h;
            if (thumb_h < 12) thumb_h = 12;
            int thumb_y = warp_list_start_y + (int)((warp_list_h - thumb_h) * ((float)ed->warp_event_scroll / warp_max_scroll));
            SDL_Rect thumb = { bar_x, thumb_y, bar_w, (int)thumb_h };
            SDL_SetRenderDrawColor(ed->renderer, 180,180,180,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }

        y = warp_list_start_y + warp_list_h + 5;

        SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
        SDL_RenderDrawLine(ed->renderer, 10, y, LEFT_PANEL_W-10, y);
        y += 5;

        if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
            int edit_y = y;
            draw_warp_field(ed, "Trigger", 0, edit_y); edit_y += 22;
            draw_warp_field(ed, "Targ.Map", 1, edit_y); edit_y += 22;
            draw_warp_field(ed, "Targ.Pos", 2, edit_y); edit_y += 22;
            draw_warp_field(ed, "Facing", 3, edit_y); edit_y += 22;

            SDL_Rect del_btn = {10, edit_y, LEFT_PANEL_W-20, 24};
            SDL_SetRenderDrawColor(ed->renderer, 180, 80, 80, 255);
            SDL_RenderFillRect(ed->renderer, &del_btn);
            draw_text_centered(ed->renderer, ed->font, "Delete", del_btn.x+del_btn.w/2, del_btn.y+del_btn.h/2,
                               (SDL_Color){255,255,255,255});
        }
    }

    SDL_RenderSetClipRect(ed->renderer, NULL);
}


// ─── Обработка ввода ──────────────────────────
void handle_input(Editor *ed, bool *running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *running = false; return; }

        // ================== Текстовый ввод (Roof Events) ==================
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

        // ================== Текстовый ввод (Tile Changes) ==================
        if (ed->tc_edit_field != -1) {
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    int len = strlen(ed->tc_input_buf);
                    if (len > 0) ed->tc_input_buf[len-1] = '\0';
                }
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                    if (ed->selected_tile_change >= 0 && ed->selected_tile_change < ed->tile_change_count) {
                        TileChangeEvent *tc = &ed->tile_changes[ed->selected_tile_change];
                        if (ed->tc_edit_field == 0) {
                            int x, y;
                            if (sscanf(ed->tc_input_buf, "%d,%d", &x, &y) == 2) {
                                tc->trigger_x = x;
                                tc->trigger_y = y;
                            }
                        } else if (ed->tc_edit_field == 1) {
                            tc->new_tile_id = atoi(ed->tc_input_buf);
                        }
                    }
                    ed->tc_edit_field = -1;
                    ed->tc_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
                else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    ed->tc_edit_field = -1;
                    ed->tc_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }
            else if (e.type == SDL_TEXTINPUT) {
                if (strspn(e.text.text, "0123456789,-") == strlen(e.text.text)) {
                    if (strlen(ed->tc_input_buf) < 30) {
                        strcat(ed->tc_input_buf, e.text.text);
                    }
                }
            }
        }

        // ================== Текстовый ввод (Stairs) ==================
        if (ed->stair_edit_field != -1) {
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    int len = strlen(ed->stair_input_buf);
                    if (len > 0) ed->stair_input_buf[len-1] = '\0';
                }
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                    if (ed->selected_stair >= 0 && ed->selected_stair < ed->stair_event_count) {
                        StairEvent *se = &ed->stair_events[ed->selected_stair];
                        if (ed->stair_edit_field == 0) {
                            int x, y;
                            if (sscanf(ed->stair_input_buf, "%d,%d", &x, &y) == 2) {
                                se->start_x = x; se->start_y = y;
                            }
                        } else if (ed->stair_edit_field == 1) {
                            int x, y;
                            if (sscanf(ed->stair_input_buf, "%d,%d", &x, &y) == 2) {
                                se->end_x = x; se->end_y = y;
                            }
                        }
                    }
                    ed->stair_edit_field = -1;
                    ed->stair_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
                else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    ed->stair_edit_field = -1;
                    ed->stair_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }
            else if (e.type == SDL_TEXTINPUT) {
                if (strspn(e.text.text, "0123456789,-") == strlen(e.text.text)) {
                    if (strlen(ed->stair_input_buf) < 30) {
                        strcat(ed->stair_input_buf, e.text.text);
                    }
                }
            }
        }

                // ================== Текстовый ввод (Warps) ==================
        if (ed->warp_edit_field != -1) {
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    int len = strlen(ed->warp_input_buf);
                    if (len > 0) ed->warp_input_buf[len-1] = '\0';
                }
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                    if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
                        WarpEvent *we = &ed->warp_events[ed->selected_warp];
                        if (ed->warp_edit_field == 0) {
                            int x, y; sscanf(ed->warp_input_buf, "%d,%d", &x, &y);
                            we->trigger_x = x; we->trigger_y = y;
                        } else if (ed->warp_edit_field == 1) {
                            snprintf(we->target_map, sizeof(we->target_map), "%s", ed->warp_input_buf);
                            we->target_map[63] = '\0';
                        } else if (ed->warp_edit_field == 2) {
                            int x, y; sscanf(ed->warp_input_buf, "%d,%d", &x, &y);
                            we->target_x = x; we->target_y = y;
                        } else if (ed->warp_edit_field == 3) {
                            we->facing = atoi(ed->warp_input_buf);
                        }
                    }
                    ed->warp_edit_field = -1;
                    ed->warp_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
                
                else if (ed->warp_edit_field == 1 &&
                         (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_RIGHT)) {
                    if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
                        WarpEvent *we = &ed->warp_events[ed->selected_warp];
                        int cur = -1;
                        for (int i = 0; i < ed->map_list.map_count; i++) {
                            if (strcmp(ed->map_list.maps[i].folder, we->target_map) == 0) { cur = i; break; }
                        }
                        if (ed->map_list.map_count > 0) {
                            if (cur == -1) cur = 0;
                            else if (e.key.keysym.sym == SDLK_LEFT)
                                cur = (cur - 1 + ed->map_list.map_count) % ed->map_list.map_count;
                            else
                                cur = (cur + 1) % ed->map_list.map_count;
                            snprintf(we->target_map, sizeof(we->target_map), "%s", ed->map_list.maps[cur].folder);
                        }
                    }
                }
                
                else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    ed->warp_edit_field = -1;
                    ed->warp_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }
            else if (e.type == SDL_TEXTINPUT) {
                if (strlen(ed->warp_input_buf) < 63) {
                    // Для target_map разрешаем буквы, цифры и '_'
                    if (ed->warp_edit_field == 1) {
                        if (isalnum(e.text.text[0]) || e.text.text[0] == '_')
                            strcat(ed->warp_input_buf, e.text.text);
                    } else {
                        if (strspn(e.text.text, "0123456789,-") == strlen(e.text.text))
                            strcat(ed->warp_input_buf, e.text.text);
                    }
                }
            }
        }


        // ================== Колесо мыши ==================
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my;
            get_logical_mouse(ed, &mx, &my);
            // Зум карты по Ctrl+колесо
            if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                    ed->zoom += e.wheel.y * 0.1f;
                    if (ed->zoom < 0.1f) ed->zoom = 0.1f;
                    if (ed->zoom > 2.0f) ed->zoom = 2.0f;
                }
            }
            // Скроллинг списка карт в правой панели
            if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                int max_visible = (WINDOW_H - 60 - 35 - 10) / 20;
                int total = ed->map_list.map_count;
                int max_scroll = (total > max_visible) ? (total - max_visible) : 0;
                ed->map_list.map_list_scroll -= e.wheel.y;
                if (ed->map_list.map_list_scroll < 0) ed->map_list.map_list_scroll = 0;
                if (ed->map_list.map_list_scroll > max_scroll) ed->map_list.map_list_scroll = max_scroll;
            }
            // Скроллинг списков левой панели (Roof Events, Tile Changes, Stairs)
            if (mx >= 0 && mx < LEFT_PANEL_W) {
                SDL_Point mouse_pt = {mx, my};
                // Roof Events (если развёрнуты и мышь над областью списка)
                if (!ed->left_panel_collapsed && SDL_PointInRect(&mouse_pt, &ed->roof_list_rect)) {
                    int max_visible = 10;
                    int max_scroll = (ed->roof_event_count > max_visible) ? ed->roof_event_count - max_visible : 0;
                    ed->roof_event_scroll -= e.wheel.y;
                    if (ed->roof_event_scroll < 0) ed->roof_event_scroll = 0;
                    if (ed->roof_event_scroll > max_scroll) ed->roof_event_scroll = max_scroll;
                }
                // Tile Changes (если развёрнуты и мышь над областью списка)
                else if (!ed->tc_section_collapsed && SDL_PointInRect(&mouse_pt, &ed->tc_list_rect)) {
                    int max_visible = 5;
                    int max_scroll = (ed->tile_change_count > max_visible) ? ed->tile_change_count - max_visible : 0;
                    ed->tile_change_scroll -= e.wheel.y;
                    if (ed->tile_change_scroll < 0) ed->tile_change_scroll = 0;
                    if (ed->tile_change_scroll > max_scroll) ed->tile_change_scroll = max_scroll;
                }
                // Stairs (если развёрнуты и мышь над областью списка)
                else if (!ed->stair_section_collapsed && SDL_PointInRect(&mouse_pt, &ed->stair_list_rect)) {
                    int max_visible = 5;
                    int max_scroll = (ed->stair_event_count > max_visible) ? ed->stair_event_count - max_visible : 0;
                    ed->stair_event_scroll -= e.wheel.y;
                    if (ed->stair_event_scroll < 0) ed->stair_event_scroll = 0;
                    if (ed->stair_event_scroll > max_scroll) ed->stair_event_scroll = max_scroll;
                }
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

            // Сброс Roof поля при клике вне
            if (ed->edit_field != -1) {
                if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                    // клик по карте – обработаем ниже
                } else if (!(mx >= 0 && mx < LEFT_PANEL_W)) {
                    ed->edit_field = -1;
                    ed->input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }

            // Сброс Tile Change поля при клике вне
            if (ed->tc_edit_field != -1) {
                if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                    // клик по карте – обработаем ниже
                } else if (!(mx >= 0 && mx < LEFT_PANEL_W)) {
                    ed->tc_edit_field = -1;
                    ed->tc_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }

            // Сброс Stair поля при клике вне
            if (ed->stair_edit_field != -1) {
                if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                    // клик по карте – обработаем ниже
                } else if (!(mx >= 0 && mx < LEFT_PANEL_W)) {
                    ed->stair_edit_field = -1;
                    ed->stair_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }

            // Сброс Warp поля при клике вне
            if (ed->warp_edit_field != -1) {
                if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                    // клик по карте – обработаем ниже
                } else if (!(mx >= 0 && mx < LEFT_PANEL_W)) {
                    ed->warp_edit_field = -1;
                    ed->warp_input_buf[0] = '\0';
                    SDL_StopTextInput();
                }
            }

            // --------------------- Левая панель ---------------------
            if (mx >= 0 && mx < LEFT_PANEL_W) {
                // --- Кнопка сворачивания Roof Events (всегда активна) ---
                if (my >= 10 && my < 34) {
                    if (mx >= LEFT_PANEL_W - 35 && mx < LEFT_PANEL_W - 9) {
                        ed->left_panel_collapsed = !ed->left_panel_collapsed;
                        return;
                    }
                    // Клик по заголовку Roof Events (не кнопка) – ничего не делаем
                    if (my < 34) return;
                }

                // --- Содержимое Roof Events (если развёрнуто) ---
                if (!ed->left_panel_collapsed) {
                    int y_off = 36; // 10 + 26
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
                    y_off += 30 + 5; // 71
                    int list_start_y = y_off;
                    int list_h = 200;
                    if (my >= list_start_y && my < list_start_y + list_h) {
                        int idx = ed->roof_event_scroll + (my - list_start_y) / 18;
                        if (idx >= 0 && idx < ed->roof_event_count) {
                            ed->selected_roof_event = idx;
                            ed->edit_field = -1;
                            ed->input_buf[0] = '\0';
                            return;
                        }
                    }
                    int edit_y = list_start_y + list_h + 5 + 5; // после списка
                    if (ed->selected_roof_event >= 0 && ed->selected_roof_event < ed->roof_event_count) {
                        check_roof_field_click(ed, 0, edit_y, mx, my);
                        check_roof_field_click(ed, 1, edit_y + 22, mx, my);
                        check_roof_field_click(ed, 2, edit_y + 44, mx, my);
                        check_roof_field_click(ed, 3, edit_y + 66, mx, my);
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
                }

                // --- Кнопка сворачивания Tile Changes (всегда активна) ---
                if (my >= ed->tc_section_y && my < ed->tc_section_y + 24) {
                    if (mx >= LEFT_PANEL_W - 35 && mx < LEFT_PANEL_W - 9) {
                        ed->tc_section_collapsed = !ed->tc_section_collapsed;
                        return;
                    }
                    return; // клик по заголовку – ничего
                }

                // --- Содержимое Tile Changes (если развёрнуто) ---
                if (!ed->tc_section_collapsed) {
                    int tc_y = ed->tc_section_y + 26;
                    SDL_Rect tc_add_btn = {10, tc_y, LEFT_PANEL_W-20, 24};
                    if (my >= tc_add_btn.y && my < tc_add_btn.y+tc_add_btn.h) {
                        if (ed->tile_change_count < MAX_TILE_CHANGES) {
                            TileChangeEvent *tc = &ed->tile_changes[ed->tile_change_count++];
                            tc->trigger_x = tc->trigger_y = -1;
                            tc->new_tile_id = 0;
                            tc->sample_x = tc->sample_y = -1;
                            ed->selected_tile_change = ed->tile_change_count - 1;
                            ed->tc_edit_field = -1;
                            ed->tc_input_buf[0] = '\0';
                        }
                        return;
                    }
                    tc_y += 30 + 5;
                    int tc_list_start_y = tc_y;
                    int tc_list_h = 100;

                    if (my >= tc_list_start_y && my < tc_list_start_y + tc_list_h) {
                        int idx = ed->tile_change_scroll + (my - tc_list_start_y) / 18;
                        if (idx >= 0 && idx < ed->tile_change_count) {
                            ed->selected_tile_change = idx;
                            ed->tc_edit_field = -1;
                            ed->tc_input_buf[0] = '\0';
                            return;
                        }
                    }
                    tc_y = tc_list_start_y + tc_list_h + 5 + 5;
                    if (ed->selected_tile_change >= 0 && ed->selected_tile_change < ed->tile_change_count) {
                        check_tc_click(ed, 0, tc_y, mx, my);
                        check_tc_click(ed, 1, tc_y + 22, mx, my);
                        SDL_Rect del_btn = {10, tc_y + 44, LEFT_PANEL_W-20, 24};
                        if (mx >= del_btn.x && mx < del_btn.x+del_btn.w && my >= del_btn.y && my < del_btn.y+del_btn.h) {
                            for (int i = ed->selected_tile_change; i < ed->tile_change_count-1; i++)
                                ed->tile_changes[i] = ed->tile_changes[i+1];
                            ed->tile_change_count--;
                            ed->selected_tile_change = -1;
                            ed->tc_edit_field = -1;
                            SDL_StopTextInput();
                            return;
                        }
                    }
                }

                // --- Кнопка сворачивания Stairs (всегда активна) ---
                if (my >= ed->stair_section_y && my < ed->stair_section_y + 24) {
                    if (mx >= LEFT_PANEL_W - 35 && mx < LEFT_PANEL_W - 9) {
                        ed->stair_section_collapsed = !ed->stair_section_collapsed;
                        return;
                    }
                    return; // клик по заголовку – ничего
                }

                // --- Содержимое Stairs (если развёрнуто) ---
                if (!ed->stair_section_collapsed) {
                    int stair_y = ed->stair_section_y + 26;
                    SDL_Rect stair_add_btn = {10, stair_y, LEFT_PANEL_W-20, 24};
                    if (my >= stair_add_btn.y && my < stair_add_btn.y+stair_add_btn.h) {
                        if (ed->stair_event_count < MAX_STAIRS) {
                            StairEvent *se = &ed->stair_events[ed->stair_event_count++];
                            se->start_x = se->start_y = 0;
                            se->end_x = 1; se->end_y = 1;
                            ed->selected_stair = ed->stair_event_count - 1;
                            ed->stair_edit_field = -1;
                            ed->stair_input_buf[0] = '\0';
                        }
                        return;
                    }
                    stair_y += 30 + 5;
                    int stair_list_start_y = stair_y;
                    int stair_list_h = 100;
                    if (my >= stair_list_start_y && my < stair_list_start_y + stair_list_h) {
                        int idx = ed->stair_event_scroll + (my - stair_list_start_y) / 18;
                        if (idx >= 0 && idx < ed->stair_event_count) {
                            ed->selected_stair = idx;
                            ed->stair_edit_field = -1;
                            ed->stair_input_buf[0] = '\0';
                            return;
                        }
                    }
                                        stair_y = stair_list_start_y + stair_list_h + 5 + 5;
                    if (ed->selected_stair >= 0 && ed->selected_stair < ed->stair_event_count) {
                        check_stair_click(ed, 0, stair_y, mx, my);
                        check_stair_click(ed, 1, stair_y + 22, mx, my);

                        // Переключатели направления лестницы (/ или \)
                        int dir_btn_y = stair_y + 44;
                        SDL_Rect slash_btn = {10, dir_btn_y, 50, 20};
                        SDL_Rect bslash_btn = {70, dir_btn_y, 50, 20};
                        StairEvent *se = &ed->stair_events[ed->selected_stair];
                        if (mx >= slash_btn.x && mx < slash_btn.x+slash_btn.w && my >= slash_btn.y && my < slash_btn.y+slash_btn.h) {
                        se->direction = 1;
                        return;
                        }
                        if (mx >= bslash_btn.x && mx < bslash_btn.x+bslash_btn.w && my >= bslash_btn.y && my < bslash_btn.y+bslash_btn.h) {
                        se->direction = 0;
                        return;
                        }

                        // Кнопка удаления (сдвинута на 22 пикселя вниз относительно старого положения)
                        int del_y = stair_y + 66;
                        SDL_Rect del_btn = {10, del_y, LEFT_PANEL_W-20, 24};
                        if (mx >= del_btn.x && mx < del_btn.x+del_btn.w && my >= del_btn.y && my < del_btn.y+del_btn.h) {
                            for (int i = ed->selected_stair; i < ed->stair_event_count-1; i++)
                                ed->stair_events[i] = ed->stair_events[i+1];
                            ed->stair_event_count--;
                            ed->selected_stair = -1;
                            ed->stair_edit_field = -1;
                            SDL_StopTextInput();
                            return;
                        }
                    }
                }

                // --- Кнопка сворачивания Warp Events (всегда активна) ---
                if (my >= ed->warp_section_y && my < ed->warp_section_y + 24) {
                    if (mx >= LEFT_PANEL_W - 35 && mx < LEFT_PANEL_W - 9) {
                        ed->warp_section_collapsed = !ed->warp_section_collapsed;
                        return;
                    }
                    return; // клик по заголовку – ничего
                }

                // --- Содержимое Warp Events (если развёрнуто) ---
                if (!ed->warp_section_collapsed) {
                    int warp_y = ed->warp_section_y + 26;
                    SDL_Rect warp_add_btn = {10, warp_y, LEFT_PANEL_W-20, 24};
                    if (my >= warp_add_btn.y && my < warp_add_btn.y+warp_add_btn.h) {
                        if (ed->warp_event_count < MAX_WARPS) {
                            WarpEvent *we = &ed->warp_events[ed->warp_event_count++];
                            we->trigger_x = we->trigger_y = -1;
                            we->target_map[0] = '\0';
                            we->target_x = we->target_y = 0;
                            we->facing = 2; // по умолчанию вниз
                            ed->selected_warp = ed->warp_event_count - 1;
                            ed->warp_edit_field = -1;
                            ed->warp_input_buf[0] = '\0';
                        }
                        return;
                    }
                    warp_y += 30 + 5;
                    int warp_list_start_y = warp_y;
                    int warp_list_h = 100;
                    if (my >= warp_list_start_y && my < warp_list_start_y + warp_list_h) {
                        int idx = ed->warp_event_scroll + (my - warp_list_start_y) / 18;
                        if (idx >= 0 && idx < ed->warp_event_count) {
                            ed->selected_warp = idx;
                            ed->warp_edit_field = -1;
                            ed->warp_input_buf[0] = '\0';
                            return;
                        }
                    }
                    warp_y = warp_list_start_y + warp_list_h + 5 + 5;
                    if (ed->selected_warp >= 0 && ed->selected_warp < ed->warp_event_count) {
                        check_warp_click(ed, 0, warp_y, mx, my);
                        check_warp_click(ed, 1, warp_y + 22, mx, my);
                        check_warp_click(ed, 2, warp_y + 44, mx, my);
                        check_warp_click(ed, 3, warp_y + 66, mx, my);
                        SDL_Rect del_btn = {10, warp_y + 88, LEFT_PANEL_W-20, 24};
                        if (mx >= del_btn.x && mx < del_btn.x+del_btn.w && my >= del_btn.y && my < del_btn.y+del_btn.h) {
                            for (int i = ed->selected_warp; i < ed->warp_event_count-1; i++)
                                ed->warp_events[i] = ed->warp_events[i+1];
                            ed->warp_event_count--;
                            ed->selected_warp = -1;
                            ed->warp_edit_field = -1;
                            SDL_StopTextInput();
                            return;
                        }
                    }
                }
                return;
            }

            // --------------------- Правая панель ---------------------
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

            // --------------------- Клик по карте ---------------------
            if (mx >= MAP_X && mx < MAP_X + MAP_VISIBLE_W && my >= MAP_Y && my < MAP_Y + MAP_VISIBLE_H) {
                Map *map = current_map(ed);
                if (map) {
                    float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
                    float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
                    int tx = (int)(world_x / TILE_SIZE);
                    int ty = (int)(world_y / TILE_SIZE);
                    if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                        // --- Roof Events ---
                        if (ed->selected_roof_event >= 0 && ed->roof_event_count > 0) {
                            RoofEvent *re = &ed->roof_events[ed->selected_roof_event];
                            if (ed->edit_field == -1 || ed->edit_field == 0) {
                                re->tile_id = map->tiles[tx * map->height + ty];
                                re->trigger_x = tx;
                                re->trigger_y = ty;
                                if (ed->edit_field == 0) {
                                    ed->edit_field = -1;
                                    ed->input_buf[0] = '\0';
                                    SDL_StopTextInput();
                                }
                            } else if (ed->edit_field == 1) {
                                re->start_x = tx; re->start_y = ty;
                                ed->edit_field = -1; ed->input_buf[0] = '\0'; SDL_StopTextInput();
                            } else if (ed->edit_field == 2) {
                                re->end_x = tx; re->end_y = ty;
                                ed->edit_field = -1; ed->input_buf[0] = '\0'; SDL_StopTextInput();
                            } else if (ed->edit_field == 3) {
                                re->exit_x = tx; re->exit_y = ty;
                                ed->edit_field = -1; ed->input_buf[0] = '\0'; SDL_StopTextInput();
                            }
                        }

                        // --- Tile Changes ---
                        if (ed->tc_edit_field != -1 && ed->selected_tile_change >= 0) {
                            TileChangeEvent *tc = &ed->tile_changes[ed->selected_tile_change];
                            if (ed->tc_edit_field == 0) {
                                tc->trigger_x = tx;
                                tc->trigger_y = ty;
                                ed->tc_edit_field = -1; ed->tc_input_buf[0] = '\0'; SDL_StopTextInput();
                            } else if (ed->tc_edit_field == 1) {
                                int tid = map->tiles[tx * map->height + ty];
                                tc->new_tile_id = tid;
                                tc->sample_x = tx;   // запоминаем координаты образца
                                tc->sample_y = ty;
                                ed->tc_edit_field = -1; ed->tc_input_buf[0] = '\0'; SDL_StopTextInput();
                            }
                        }

                        // --- Stairs ---
                        if (ed->stair_edit_field != -1 && ed->selected_stair >= 0) {
                            StairEvent *se = &ed->stair_events[ed->selected_stair];
                            if (ed->stair_edit_field == 0) {
                                se->start_x = tx; se->start_y = ty;
                                ed->stair_edit_field = -1; ed->stair_input_buf[0] = '\0'; SDL_StopTextInput();
                            } else if (ed->stair_edit_field == 1) {
                                se->end_x = tx; se->end_y = ty;
                                ed->stair_edit_field = -1; ed->stair_input_buf[0] = '\0'; SDL_StopTextInput();
                            }
                        }
                                                // --- Warps ---
                        if (ed->warp_edit_field != -1 && ed->selected_warp >= 0) {
                            WarpEvent *we = &ed->warp_events[ed->selected_warp];
                            if (ed->warp_edit_field == 0) {
                                we->trigger_x = tx; we->trigger_y = ty;
                                ed->warp_edit_field = -1; ed->warp_input_buf[0] = '\0'; SDL_StopTextInput();
                            } else if (ed->warp_edit_field == 2) {
                                we->target_x = tx; we->target_y = ty;
                                ed->warp_edit_field = -1; ed->warp_input_buf[0] = '\0'; SDL_StopTextInput();
                            }
                        }
                    }
                }
            }

            // --------------------- Скроллбары ---------------------
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