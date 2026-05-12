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
#include "clipboard.h"
#include "map.h"
#include "cJSON.h"

static SDL_Texture *transparent_bg_tex = NULL;   // шахматный фон

// ─── Геометрия окна ───────────────────────────
#define WINDOW_W 1280
#define WINDOW_H 720
#define TOOLBAR_H 34

#define LEFT_PANEL_W 300
#define RIGHT_PANEL_W 200
#define SCROLLBAR_SIZE 56   // 48 (тайл) + 8 отступ

#define MAP_X LEFT_PANEL_W
#define MAP_Y TOOLBAR_H
#define MAP_W (WINDOW_W - LEFT_PANEL_W - RIGHT_PANEL_W - SCROLLBAR_SIZE)
#define MAP_H (WINDOW_H - TOOLBAR_H - SCROLLBAR_SIZE)

#define TILE_SIZE 48
#define PALETTE_TILE_SIZE 32
#define PALETTE_COLS 8
#define PALETTE_START_X 10
#define PALETTE_START_Y 140

#define FONT_SIZE 16
#define MODE_A 0
#define MODE_C 1
#define MIN_MAP_SIZE 2
#define MAX_MAP_SIZE 999

#define RIGHT_ROTATE   1
#define RIGHT_FLIP_H   2
#define RIGHT_FLIP_V   3
#define RIGHT_DELETE   4

#define DIALOG_NONE          0
#define DIALOG_NEW_MAP       1
#define DIALOG_RENAME_MAP    2
#define DIALOG_RESIZE_MAP    3
#define DIALOG_CONFIRM_DEL   4
#define DIALOG_MUSIC_MAP     5

// ─── Структуры данных ─────────────────────────

typedef struct {
    Map *maps;
    int map_count;
    int current_map;
} MapList;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    SDL_Texture **tiles;
    int           tile_count;
    char          tileset_path[256];

    int          *tile_types;
    SDL_Texture  *type_icons[4];
    int           current_type;

    int palette_scroll;
    int selected_tile;
    int mode;
    bool tileset_loaded;

    Uint32 blink_time;
    bool   blink_visible;

    MapList map_list;
	int map_list_scroll;   // вертикальное смещение списка карт (в строках)

    float cam_x, cam_y;
    int   panning;
    Sint32 pan_start_x, pan_start_y;

    int right_click_mode;
    float zoom;
    int current_layer;
    bool show_layer1;
    bool show_layer2;
    SDL_Texture *transform_icons[4];

    bool dialog_active;
    int  dialog_type;
    char input_text[64];
    char input_text2[64];
    int dialog_active_field;
    Uint32 dialog_cursor_blink;

    bool save_blink_active;
    char music_fullpath[256];
    bool dialog_just_closed;
    Uint32 save_blink_time;

    bool grid_mode_active;
	// Выделение и буфер обмена
    bool selecting;
    int sel_start_x, sel_start_y;
    int sel_end_x, sel_end_y;
    bool has_clipboard;
    Clipboard clipboard;
    bool paste_just_performed;

    // scrollbars
    bool scrollbar_drag_h;
    bool scrollbar_drag_v;
    int  scroll_drag_start_x;
    int  scroll_drag_start_y;
    int  scroll_drag_mouse_offset_x;
    int  scroll_drag_mouse_offset_y;
} Editor;

// Прототипы
Map* current_map(Editor *ed);
void find_first_tileset_path(char *out, size_t out_len);
void find_first_sound_path(char *out, size_t out_len);
void get_relative_path(const char *abs_path, char *out, size_t out_len);

void safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (dest_size > 0) snprintf(dest, dest_size, "%s", src);
}

// Получение логических координат мыши (для высокого DPI)
void get_logical_mouse(Editor *ed, int *mx, int *my) {
    SDL_GetMouseState(mx, my);   // получаем реальные координаты окна

    int win_w, win_h;
    SDL_GetWindowSize(ed->window, &win_w, &win_h);

    // Если окно было масштабировано системой (DPI) — пересчитываем в логические координаты
    if (win_w != WINDOW_W || win_h != WINDOW_H) {
        *mx = (int)((float)*mx * WINDOW_W / win_w + 0.5f);
        *my = (int)((float)*my * WINDOW_H / win_h + 0.5f);
    }
}

void editor_init(Editor *ed) {
    memset(ed, 0, sizeof(Editor));
    ed->mode = MODE_A;
    ed->selected_tile = 0;
    ed->current_type = 0;
    ed->tileset_loaded = false;
    ed->blink_time = SDL_GetTicks();
    ed->blink_visible = true;
    ed->dialog_active = false;
    ed->dialog_type = DIALOG_NONE;
    ed->right_click_mode = 0;
    ed->cam_x = ed->cam_y = 0;
    ed->panning = 0;
    ed->save_blink_active = false;
    ed->music_fullpath[0] = '\0';
    ed->dialog_just_closed = false;
    ed->save_blink_time = 0;
    ed->dialog_active_field = 0;
    ed->dialog_cursor_blink = 0;
    ed->grid_mode_active = false;
	ed->selecting = false;
    ed->sel_start_x = ed->sel_start_y = -1;
    ed->sel_end_x = ed->sel_end_y = -1;
    ed->has_clipboard = false;
    clipboard_init(&ed->clipboard);
    ed->scrollbar_drag_h = false;
    ed->scrollbar_drag_v = false;
    for (int i = 0; i < 4; i++) ed->type_icons[i] = NULL;
    for (int i = 0; i < 4; i++) ed->transform_icons[i] = NULL;
    ed->map_list.maps = NULL;
    ed->map_list.map_count = 0;
    ed->map_list.current_map = 0;
	ed->map_list_scroll = 0;
    ed->current_layer = 0;
    ed->show_layer1 = true;
    ed->show_layer2 = true;
    ed->zoom = 1.0f;
    ed->paste_just_performed = false;
}

void free_tileset(Editor *ed) {
    if (ed->tiles) {
        for (int i = 0; i < ed->tile_count; i++) SDL_DestroyTexture(ed->tiles[i]);
        free(ed->tiles); ed->tiles = NULL;
    }
    if (ed->tile_types) { free(ed->tile_types); ed->tile_types = NULL; }
    ed->tile_count = 0;
    ed->tileset_loaded = false;
}

SDL_Texture* create_color_texture(SDL_Renderer *ren, Uint8 r, Uint8 g, Uint8 b, int w, int h) {
    SDL_Surface *surf = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
    SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, r, g, b, 255));
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void load_type_icons(Editor *ed) {
    for (int i = 0; i < 4; i++) {
        if (ed->type_icons[i]) SDL_DestroyTexture(ed->type_icons[i]);
        ed->type_icons[i] = NULL;
    }
    const char *filenames[4] = {
        "../assets/icons/passable.png",
        "../assets/icons/block.png",
        "../assets/icons/slow.png",
        "../assets/icons/under.png"
    };
    SDL_Color fallback[4] = {
        {0, 200, 0, 255}, {200, 0, 0, 255}, {200, 200, 0, 255}, {0, 0, 200, 255}
    };
    for (int i = 0; i < 4; i++) {
        SDL_Surface *surf = IMG_Load(filenames[i]);
        if (surf) {
            ed->type_icons[i] = SDL_CreateTextureFromSurface(ed->renderer, surf);
            SDL_FreeSurface(surf);
        } else {
            ed->type_icons[i] = create_color_texture(ed->renderer, fallback[i].r, fallback[i].g, fallback[i].b, 16, 16);
        }
    }
}

void load_transform_icons(Editor *ed) {
    const char *files[4] = {
        "../assets/icons/rotate.png", "../assets/icons/flip_h.png",
        "../assets/icons/flip_v.png", "../assets/icons/Delete_Tile.png"
    };
    Uint8 fr[4] = {200,100,100,200}, fg[4] = {100,200,100,80}, fb[4] = {100,100,200,80};
    for (int i = 0; i < 4; i++) {
        if (ed->transform_icons[i]) SDL_DestroyTexture(ed->transform_icons[i]);
        ed->transform_icons[i] = NULL;
        SDL_Surface *surf = IMG_Load(files[i]);
        if (surf) {
            ed->transform_icons[i] = SDL_CreateTextureFromSurface(ed->renderer, surf);
            SDL_FreeSurface(surf);
        } else {
            ed->transform_icons[i] = create_color_texture(ed->renderer, fr[i], fg[i], fb[i], 32, 32);
        }
    }
}

void save_tile_types_for_tileset(const Editor *ed, const char *tileset_path) {
    if (!ed->tileset_loaded || !ed->tile_types) return;

    char safe[256];
    safe_strcpy(safe, sizeof(safe), tileset_path);
    for (char *p = safe; *p; p++) {
        if (*p == '\\' || *p == '/' || *p == ':')
            *p = '_';
    }

    CreateDirectoryA("../data/tile_types", NULL);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "../data/tile_types/%s.json", safe);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ed->tile_count; i++) {
        cJSON_AddItemToArray(root, cJSON_CreateNumber(ed->tile_types[i]));
    }

    char *str = cJSON_Print(root);
    FILE *f = fopen(filepath, "w");
    if (f) {
        fputs(str, f);
        fclose(f);
    }
    cJSON_Delete(root);
    free(str);
}

void load_tile_types_for_tileset(Editor *ed, const char *tileset_path) {
    if (!ed->tileset_loaded) return;
    if (ed->tile_types) { free(ed->tile_types); ed->tile_types = NULL; }
    ed->tile_types = (int*)calloc(ed->tile_count, sizeof(int));
    char safe[256];
    safe_strcpy(safe, sizeof(safe), tileset_path);
    for (char *p = safe; *p; p++)
        if (*p == '\\' || *p == '/' || *p == ':') *p = '_';
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "../data/tile_types/%s.json", safe);
    FILE *f = fopen(filepath, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len+1);
    fread(data, 1, len, f); fclose(f); data[len] = '\0';
    cJSON *arr = cJSON_Parse(data); free(data);
    if (!arr) return;
    int size = cJSON_GetArraySize(arr);
    for (int i = 0; i < ed->tile_count && i < size; i++)
        ed->tile_types[i] = cJSON_GetArrayItem(arr, i)->valueint;
    cJSON_Delete(arr);
}

int load_tileset(Editor *ed, const char *path) {
    free_tileset(ed);
    char full_tileset[512];

    if (path[0] && path[1] == ':') {
        snprintf(full_tileset, sizeof(full_tileset), "%s", path);
    } else {
        snprintf(full_tileset, sizeof(full_tileset), "../%s", path);
    }
    SDL_Surface *surface = IMG_Load(full_tileset);
    if (!surface) return 0;

    int cols = surface->w / TILE_SIZE;
    int rows = surface->h / TILE_SIZE;
    int palette_cols = PALETTE_COLS;
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
    ed->palette_scroll = 0;
    ed->selected_tile = 0;
    load_tile_types_for_tileset(ed, path);
    return 1;
}

bool open_file_dialog(char *out_path, size_t out_len) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) {
        safe_strcpy(out_path, out_len, ofn.lpstrFile);
        return true;
    }
    return false;
}

bool map_load_from_json(Map *map, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long len = ftell(f);
    if (len <= 0) { fclose(f); return false; }
    fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len+1);
    if (!data) { fclose(f); return false; }
    fread(data, 1, len, f); data[len] = '\0'; fclose(f);

    cJSON *root = cJSON_Parse(data); free(data);
    if (!root) return false;

    cJSON *width_json = cJSON_GetObjectItem(root, "width");
    cJSON *height_json = cJSON_GetObjectItem(root, "height");
    cJSON *tileset_json = cJSON_GetObjectItem(root, "tileset");
    cJSON *t_json = cJSON_GetObjectItem(root, "tiles");
    cJSON *r_json = cJSON_GetObjectItem(root, "rot");
    cJSON *mx_json = cJSON_GetObjectItem(root, "mirror_x");
    cJSON *my_json = cJSON_GetObjectItem(root, "mirror_y");

    if (!width_json || !height_json || !tileset_json || !t_json || !r_json || !mx_json || !my_json) {
        cJSON_Delete(root); return false;
    }

    int w = width_json->valueint, h = height_json->valueint;
    if (w < MIN_MAP_SIZE || w > MAX_MAP_SIZE || h < MIN_MAP_SIZE || h > MAX_MAP_SIZE) {
        cJSON_Delete(root); return false;
    }
    int sz = w * h;
    if (sz <= 0) { cJSON_Delete(root); return false; }

    map->width = w; map->height = h;
    // name и folder будут установлены позже из entries (оставляем пустыми)
    map->name[0] = '\0';
    map->folder[0] = '\0';
    get_relative_path(tileset_json->valuestring, map->tileset_path, sizeof(map->tileset_path));

    // Музыка – не обязательна в layout.json, просто считываем если есть
    cJSON *music_json = cJSON_GetObjectItem(root, "music");
    if (music_json && cJSON_IsObject(music_json)) {
        cJSON *music_file_json = cJSON_GetObjectItem(music_json, "file");
        cJSON *music_vol_json  = cJSON_GetObjectItem(music_json, "volume");
        if (music_file_json && cJSON_IsString(music_file_json))
            safe_strcpy(map->music_file, sizeof(map->music_file), music_file_json->valuestring);
        else
            map->music_file[0] = '\0';
        if (music_vol_json && cJSON_IsNumber(music_vol_json))
            map->music_volume = (float)music_vol_json->valuedouble;
        else
            map->music_volume = 0.8f;
    } else {
        map->music_file[0] = '\0';
        map->music_volume = 0.8f;
    }

    // Первый слой
    map->tiles     = (int*)malloc(sz * sizeof(int));
    map->rot       = (int*)malloc(sz * sizeof(int));
    map->mirror_x  = (int*)malloc(sz * sizeof(int));
    map->mirror_y  = (int*)malloc(sz * sizeof(int));
    if (!map->tiles || !map->rot || !map->mirror_x || !map->mirror_y) {
        free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
        cJSON_Delete(root); return false;
    }

    for (int x = 0; x < w; x++) {
        cJSON *col_t  = cJSON_GetArrayItem(t_json, x);
        cJSON *col_r  = cJSON_GetArrayItem(r_json, x);
        cJSON *col_mx = cJSON_GetArrayItem(mx_json, x);
        cJSON *col_my = cJSON_GetArrayItem(my_json, x);

        for (int y = 0; y < h; y++) {
            int idx = x * h + y;
            map->tiles[idx]     = (col_t  && cJSON_IsArray(col_t)  && cJSON_GetArrayItem(col_t,  y)) ? cJSON_GetArrayItem(col_t,  y)->valueint : 0;
            map->rot[idx]       = (col_r  && cJSON_IsArray(col_r)  && cJSON_GetArrayItem(col_r,  y)) ? cJSON_GetArrayItem(col_r,  y)->valueint : 0;
            map->mirror_x[idx]  = (col_mx && cJSON_IsArray(col_mx) && cJSON_GetArrayItem(col_mx, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_mx, y)) : false;
            map->mirror_y[idx]  = (col_my && cJSON_IsArray(col_my) && cJSON_GetArrayItem(col_my, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_my, y)) : false;
        }
    }

    // Второй слой
    map->tiles2     = (int*)malloc(sz * sizeof(int));
    map->rot2       = (int*)malloc(sz * sizeof(int));
    map->mirror_x2  = (int*)malloc(sz * sizeof(int));
    map->mirror_y2  = (int*)malloc(sz * sizeof(int));
    if (!map->tiles2 || !map->rot2 || !map->mirror_x2 || !map->mirror_y2) {
        free(map->tiles2); free(map->rot2); free(map->mirror_x2); free(map->mirror_y2);
        free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
        cJSON_Delete(root); return false;
    }

    cJSON *t2_json = cJSON_GetObjectItem(root, "tiles2");
    cJSON *r2_json = cJSON_GetObjectItem(root, "rot2");
    cJSON *mx2_json = cJSON_GetObjectItem(root, "mirror_x2");
    cJSON *my2_json = cJSON_GetObjectItem(root, "mirror_y2");

    if (t2_json && r2_json && mx2_json && my2_json) {
        for (int x = 0; x < w; x++) {
            cJSON *col_t2  = cJSON_GetArrayItem(t2_json, x);
            cJSON *col_r2  = cJSON_GetArrayItem(r2_json, x);
            cJSON *col_mx2 = cJSON_GetArrayItem(mx2_json, x);
            cJSON *col_my2 = cJSON_GetArrayItem(my2_json, x);
            for (int y = 0; y < h; y++) {
                int idx = x * h + y;
                map->tiles2[idx]     = (col_t2  && cJSON_IsArray(col_t2)  && cJSON_GetArrayItem(col_t2,  y)) ? cJSON_GetArrayItem(col_t2,  y)->valueint : -1;
                map->rot2[idx]       = (col_r2  && cJSON_IsArray(col_r2)  && cJSON_GetArrayItem(col_r2,  y)) ? cJSON_GetArrayItem(col_r2,  y)->valueint : 0;
                map->mirror_x2[idx]  = (col_mx2 && cJSON_IsArray(col_mx2) && cJSON_GetArrayItem(col_mx2, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_mx2, y)) : false;
                map->mirror_y2[idx]  = (col_my2 && cJSON_IsArray(col_my2) && cJSON_GetArrayItem(col_my2, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_my2, y)) : false;
            }
        }
    } else {
        for (int i = 0; i < sz; i++) map->tiles2[i] = -1;
    }

    // collision
    cJSON *cell_json = cJSON_GetObjectItem(root, "collision");
    map->cell_type = (int*)malloc(sz * sizeof(int));
    if (cell_json && cJSON_IsArray(cell_json)) {
        for (int x = 0; x < w; x++) {
            cJSON *col_cell = cJSON_GetArrayItem(cell_json, x);
            for (int y = 0; y < h; y++) {
                int idx = x * h + y;
                map->cell_type[idx] = (col_cell && cJSON_IsArray(col_cell) && cJSON_GetArrayItem(col_cell, y))
                                      ? cJSON_GetArrayItem(col_cell, y)->valueint : 0;
            }
        }
    } else {
        for (int i = 0; i < sz; i++) map->cell_type[i] = 0;
    }

    cJSON_Delete(root);
    return true;
}

void map_init(Map *map, const char *name, int w, int h, const char *tileset) {
    safe_strcpy(map->name, sizeof(map->name), name);
    map->width = w;
    map->height = h;
    safe_strcpy(map->tileset_path, sizeof(map->tileset_path), tileset);
    map->music_file[0] = '\0';
    map->music_volume = 0.8f;

    int sz = w * h;
    map->tiles     = (int*)calloc(sz, sizeof(int));
    map->rot       = (int*)calloc(sz, sizeof(int));
    map->mirror_x  = (int*)calloc(sz, sizeof(int));
    map->mirror_y  = (int*)calloc(sz, sizeof(int));

    map->tiles2    = (int*)calloc(sz, sizeof(int));
    map->rot2      = (int*)calloc(sz, sizeof(int));
    map->mirror_x2 = (int*)calloc(sz, sizeof(int));
    map->mirror_y2 = (int*)calloc(sz, sizeof(int));

    for (int i = 0; i < sz; i++)
        map->tiles2[i] = -1;

    // NEW: cell_type
    map->cell_type = (int*)calloc(sz, sizeof(int));  // все 0 (passable)
}

void map_free(Map *map) {
    free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
    free(map->tiles2); free(map->rot2); free(map->mirror_x2); free(map->mirror_y2);
    free(map->cell_type);   // NEW
}

Map* current_map(Editor *ed) {
    if (ed->map_list.map_count == 0) return NULL;
    return &ed->map_list.maps[ed->map_list.current_map];
}

void find_first_tileset_path(char *out, size_t out_len) {
    out[0] = '\0';
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("../assets/tilesets/*.png", &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    snprintf(out, out_len, "../assets/tilesets/%s", findData.cFileName);
    FindClose(hFind);
}

void get_relative_path(const char *abs_path, char *out, size_t out_len) {
    const char *assets = strstr(abs_path, "assets");
    if (assets) {
        safe_strcpy(out, out_len, assets);
    } else {
        const char *name = strrchr(abs_path, '\\');
        if (!name) name = strrchr(abs_path, '/');
        if (name) name++; else name = abs_path;
        safe_strcpy(out, out_len, name);
    }
}

void find_first_sound_path(char *out, size_t out_len) {
    out[0] = '\0';
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("../assets/sounds/*.mp3", &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    snprintf(out, out_len, "../assets/sounds/%s", findData.cFileName);
    FindClose(hFind);
}

cJSON* load_entries(void) {
    const char *path = "../data/maps/entries.json";
    FILE *f = fopen(path, "r");
    if (!f) return cJSON_CreateArray();
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len + 1);
    if (!data) { fclose(f); return cJSON_CreateArray(); }
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);
    cJSON *root = cJSON_Parse(data);
    free(data);
    return root ? root : cJSON_CreateArray();
}

void save_entries(cJSON *entries) {
    const char *path = "../data/maps/entries.json";
    char *str = cJSON_Print(entries);
    FILE *f = fopen(path, "w");
    if (f) { fputs(str, f); fclose(f); }
    free(str);
    cJSON_Delete(entries);
}

cJSON* find_entry_by_folder(cJSON *entries, const char *folder) {
    int count = cJSON_GetArraySize(entries);
    for (int i = 0; i < count; i++) {
        cJSON *entry = cJSON_GetArrayItem(entries, i);
        cJSON *folder_json = cJSON_GetObjectItem(entry, "folder");
        if (folder_json && strcmp(folder_json->valuestring, folder) == 0)
            return entry;
    }
    return NULL;
}

void add_or_update_entry(cJSON *entries, const char *folder, const char *name,
                         const char *music_file, float volume) {
    cJSON *entry = find_entry_by_folder(entries, folder);
    if (entry) {
        cJSON_ReplaceItemInObject(entry, "name", cJSON_CreateString(name));
        cJSON_ReplaceItemInObject(entry, "music", cJSON_CreateString(music_file));
        cJSON_ReplaceItemInObject(entry, "music_volume", cJSON_CreateNumber(volume));
    } else {
        cJSON *new_entry = cJSON_CreateObject();
        cJSON_AddStringToObject(new_entry, "folder", folder);
        cJSON_AddStringToObject(new_entry, "name", name);
        cJSON_AddStringToObject(new_entry, "music", music_file);
        cJSON_AddNumberToObject(new_entry, "music_volume", volume);
        cJSON_AddItemToArray(entries, new_entry);
    }
}

void remove_entry(cJSON *entries, const char *folder) {
    int count = cJSON_GetArraySize(entries);
    for (int i = 0; i < count; i++) {
        cJSON *entry = cJSON_GetArrayItem(entries, i);
        cJSON *f = cJSON_GetObjectItem(entry, "folder");
        if (f && strcmp(f->valuestring, folder) == 0) {
            cJSON_DeleteItemFromArray(entries, i);
            break;
        }
    }
}

void map_save_to_json(const Map *map, const char *folder) {
    char dir[512];
    snprintf(dir, sizeof(dir), "../data/maps/%s", folder);
    CreateDirectoryA(dir, NULL);
    char filename[768];
    snprintf(filename, sizeof(filename), "%s/layout.json", dir);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "width", map->width);
    cJSON_AddNumberToObject(root, "height", map->height);
    cJSON_AddStringToObject(root, "tileset", map->tileset_path);

    // Первый слой
    cJSON *t  = cJSON_AddArrayToObject(root, "tiles");
    cJSON *r  = cJSON_AddArrayToObject(root, "rot");
    cJSON *mx = cJSON_AddArrayToObject(root, "mirror_x");
    cJSON *my = cJSON_AddArrayToObject(root, "mirror_y");
    for (int x = 0; x < map->width; x++) {
        cJSON *col_t = cJSON_CreateArray(), *col_r = cJSON_CreateArray();
        cJSON *col_mx = cJSON_CreateArray(), *col_my = cJSON_CreateArray();
        for (int y = 0; y < map->height; y++) {
            int idx = x * map->height + y;
            cJSON_AddItemToArray(col_t, cJSON_CreateNumber(map->tiles[idx]));
            cJSON_AddItemToArray(col_r, cJSON_CreateNumber(map->rot[idx]));
            cJSON_AddItemToArray(col_mx, cJSON_CreateBool(map->mirror_x[idx]));
            cJSON_AddItemToArray(col_my, cJSON_CreateBool(map->mirror_y[idx]));
        }
        cJSON_AddItemToArray(t, col_t); cJSON_AddItemToArray(r, col_r);
        cJSON_AddItemToArray(mx, col_mx); cJSON_AddItemToArray(my, col_my);
    }

    // Второй слой
    int has_l2 = 0;
    int total = map->width * map->height;
    for (int i = 0; i < total; i++) if (map->tiles2[i] != -1) { has_l2 = 1; break; }
    if (has_l2) {
        cJSON *t2  = cJSON_AddArrayToObject(root, "tiles2");
        cJSON *r2  = cJSON_AddArrayToObject(root, "rot2");
        cJSON *mx2 = cJSON_AddArrayToObject(root, "mirror_x2");
        cJSON *my2 = cJSON_AddArrayToObject(root, "mirror_y2");
        for (int x = 0; x < map->width; x++) {
            cJSON *col_t2 = cJSON_CreateArray(), *col_r2 = cJSON_CreateArray();
            cJSON *col_mx2 = cJSON_CreateArray(), *col_my2 = cJSON_CreateArray();
            for (int y = 0; y < map->height; y++) {
                int idx = x * map->height + y;
                cJSON_AddItemToArray(col_t2, cJSON_CreateNumber(map->tiles2[idx]));
                cJSON_AddItemToArray(col_r2, cJSON_CreateNumber(map->rot2[idx]));
                cJSON_AddItemToArray(col_mx2, cJSON_CreateBool(map->mirror_x2[idx]));
                cJSON_AddItemToArray(col_my2, cJSON_CreateBool(map->mirror_y2[idx]));
            }
            cJSON_AddItemToArray(t2, col_t2); cJSON_AddItemToArray(r2, col_r2);
            cJSON_AddItemToArray(mx2, col_mx2); cJSON_AddItemToArray(my2, col_my2);
        }
    }

    // collision
    cJSON *cell = cJSON_AddArrayToObject(root, "collision");
    for (int x = 0; x < map->width; x++) {
        cJSON *col = cJSON_CreateArray();
        for (int y = 0; y < map->height; y++)
            cJSON_AddItemToArray(col, cJSON_CreateNumber(map->cell_type[x * map->height + y]));
        cJSON_AddItemToArray(cell, col);
    }

    char *str = cJSON_Print(root);
    FILE *f = fopen(filename, "w");
    if (f) { fputs(str, f); fclose(f); }
    cJSON_Delete(root);
    free(str);

    // Обновляем entries.json
    cJSON *entries = load_entries();
    add_or_update_entry(entries, folder, map->name, map->music_file, map->music_volume);
    save_entries(entries);
}

void load_map_list(Editor *ed) {
    for (int i = 0; i < ed->map_list.map_count; i++) map_free(&ed->map_list.maps[i]);
    free(ed->map_list.maps);
    ed->map_list.maps = NULL;
    ed->map_list.map_count = 0;
    ed->map_list.current_map = 0;

    cJSON *entries = load_entries();
    int count = cJSON_GetArraySize(entries);

    for (int i = 0; i < count; i++) {
        cJSON *entry = cJSON_GetArrayItem(entries, i);
        cJSON *folder_json = cJSON_GetObjectItem(entry, "folder");
        cJSON *name_json   = cJSON_GetObjectItem(entry, "name");
        if (!folder_json || !name_json) continue;

        const char *folder = folder_json->valuestring;
        const char *name   = name_json->valuestring;

        cJSON *music_json = cJSON_GetObjectItem(entry, "music");
        cJSON *vol_json   = cJSON_GetObjectItem(entry, "music_volume");
        char music_file[256] = "";
        float volume = 0.8f;
        if (music_json && cJSON_IsString(music_json))
            safe_strcpy(music_file, sizeof(music_file), music_json->valuestring);
        if (vol_json && cJSON_IsNumber(vol_json))
            volume = (float)vol_json->valuedouble;

        char layout_path[512];
        snprintf(layout_path, sizeof(layout_path), "../data/maps/%s/layout.json", folder);
        Map temp;
        if (map_load_from_json(&temp, layout_path)) {
            safe_strcpy(temp.name, sizeof(temp.name), name);
            safe_strcpy(temp.folder, sizeof(temp.folder), folder);
            safe_strcpy(temp.music_file, sizeof(temp.music_file), music_file);
            temp.music_volume = volume;
            ed->map_list.map_count++;
            ed->map_list.maps = (Map*)realloc(ed->map_list.maps, ed->map_list.map_count * sizeof(Map));
            if (!ed->map_list.maps) exit(1);
            ed->map_list.maps[ed->map_list.map_count - 1] = temp;
        }
    }
    cJSON_Delete(entries);
}

void create_map(Editor *ed, const char *name, int w, int h, const char *tileset) {
    if (w < MIN_MAP_SIZE) w = MIN_MAP_SIZE;
    if (h < MIN_MAP_SIZE) h = MIN_MAP_SIZE;
    if (w > MAX_MAP_SIZE) w = MAX_MAP_SIZE;
    if (h > MAX_MAP_SIZE) h = MAX_MAP_SIZE;

    char fullname[64];
    safe_strcpy(fullname, sizeof(fullname), name);
    int counter = 0;
    while (1) {
        bool exists = false;
        for (int i = 0; i < ed->map_list.map_count; i++)
            if (strcmp(ed->map_list.maps[i].name, fullname) == 0) { exists = true; break; }
        if (!exists) break;
        counter++;
        snprintf(fullname, sizeof(fullname), "%s%d", name, counter);
    }

    Map new_map;
map_init(&new_map, fullname, w, h, tileset);
safe_strcpy(new_map.folder, sizeof(new_map.folder), fullname);

char first_sound[512];
find_first_sound_path(first_sound, sizeof(first_sound));
if (first_sound[0] != '\0')
    safe_strcpy(new_map.music_file, sizeof(new_map.music_file), first_sound);

map_save_to_json(&new_map, new_map.folder);

ed->map_list.map_count++;
ed->map_list.maps = (Map*)realloc(ed->map_list.maps, ed->map_list.map_count * sizeof(Map));
if (!ed->map_list.maps) exit(1);
ed->map_list.maps[ed->map_list.map_count - 1] = new_map;
ed->map_list.current_map = ed->map_list.map_count - 1;

load_tileset(ed, new_map.tileset_path);
}

void delete_current_map(Editor *ed) {
    if (ed->map_list.map_count <= 1) return;
    Map *map = &ed->map_list.maps[ed->map_list.current_map];

    // Удаляем layout.json и папку
    char path[512];
    snprintf(path, sizeof(path), "../data/maps/%s/layout.json", map->folder);
    remove(path);
    // areas.json тоже, если есть
    snprintf(path, sizeof(path), "../data/maps/%s/areas.json", map->folder);
    remove(path);
    snprintf(path, sizeof(path), "../data/maps/%s", map->folder);
    RemoveDirectoryA(path);

    // Обновляем entries.json
    cJSON *entries = load_entries();
    remove_entry(entries, map->folder);
    save_entries(entries);

    // Сдвигаем массив карт
    map_free(map);
    for (int i = ed->map_list.current_map; i < ed->map_list.map_count - 1; i++)
        ed->map_list.maps[i] = ed->map_list.maps[i+1];
    ed->map_list.map_count--;
    if (ed->map_list.current_map >= ed->map_list.map_count)
        ed->map_list.current_map = ed->map_list.map_count - 1;

    Map *cur = current_map(ed);
    if (cur) load_tileset(ed, cur->tileset_path);
}

void rename_current_map(Editor *ed, const char *new_name) {
    Map *map = current_map(ed);
    if (!map || strlen(new_name) == 0) return;

    char old_folder[64];
    safe_strcpy(old_folder, sizeof(old_folder), map->folder);

    // Новый folder = new_name (можно оставить как есть)
    char new_folder[64];
    safe_strcpy(new_folder, sizeof(new_folder), new_name);

    // Переименовываем папку
    char old_path[512], new_path[512];
    snprintf(old_path, sizeof(old_path), "../data/maps/%s", old_folder);
    snprintf(new_path, sizeof(new_path), "../data/maps/%s", new_folder);
    if (MoveFileA(old_path, new_path) == 0) return;  // ошибка

    // Обновляем данные карты
    safe_strcpy(map->folder, sizeof(map->folder), new_folder);
    safe_strcpy(map->name, sizeof(map->name), new_name);

    // Обновляем entries.json
    cJSON *entries = load_entries();
    remove_entry(entries, old_folder);
    add_or_update_entry(entries, new_folder, new_name, map->music_file, map->music_volume);
    save_entries(entries);

    // Сохраняем layout.json с новыми путями
    map_save_to_json(map, new_folder);
}

void resize_current_map(Editor *ed, int new_w, int new_h) {
    Map *map = current_map(ed);
    if (!map) return;

    new_w = (new_w < MIN_MAP_SIZE) ? MIN_MAP_SIZE : (new_w > MAX_MAP_SIZE) ? MAX_MAP_SIZE : new_w;
    new_h = (new_h < MIN_MAP_SIZE) ? MIN_MAP_SIZE : (new_h > MAX_MAP_SIZE) ? MAX_MAP_SIZE : new_h;

    Map bigger;
    map_init(&bigger, map->name, new_w, new_h, map->tileset_path);
    safe_strcpy(bigger.folder, sizeof(bigger.folder), map->folder);
    safe_strcpy(bigger.music_file, sizeof(bigger.music_file), map->music_file);
    bigger.music_volume = map->music_volume;

    for (int x = 0; x < map->width && x < new_w; x++) {
        for (int y = 0; y < map->height && y < new_h; y++) {
            int old_idx = x * map->height + y;
            int new_idx = x * new_h + y;
            bigger.tiles[new_idx]     = map->tiles[old_idx];
            bigger.rot[new_idx]       = map->rot[old_idx];
            bigger.mirror_x[new_idx]  = map->mirror_x[old_idx];
            bigger.mirror_y[new_idx]  = map->mirror_y[old_idx];
            bigger.tiles2[new_idx]    = map->tiles2[old_idx];
            bigger.rot2[new_idx]      = map->rot2[old_idx];
            bigger.mirror_x2[new_idx] = map->mirror_x2[old_idx];
            bigger.mirror_y2[new_idx] = map->mirror_y2[old_idx];
            bigger.cell_type[new_idx] = map->cell_type[old_idx];
        }
    }

    map_free(map);
    *map = bigger;
}

// ─── Отрисовка текста (вспомогательные) ─────
void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst = { cx - surf->w/2, cy - surf->h/2, surf->w, surf->h };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void draw_input_field(Editor *ed, SDL_Rect field, const char *text, bool active, bool cursor_visible) {
    SDL_SetRenderDrawColor(ed->renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(ed->renderer, &field);
    if (active) SDL_SetRenderDrawColor(ed->renderer, 0, 120, 215, 255);
    else SDL_SetRenderDrawColor(ed->renderer, 150, 150, 150, 255);
    SDL_RenderDrawRect(ed->renderer, &field);

    SDL_Surface *surf = TTF_RenderUTF8_Blended(ed->font, text, (SDL_Color){0,0,0,255});
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(ed->renderer, surf);
        SDL_Rect text_rect = { field.x + 4, field.y + (field.h - surf->h)/2, surf->w, surf->h };
        SDL_RenderCopy(ed->renderer, tex, NULL, &text_rect);
        if (active && cursor_visible) {
            int cur_x = text_rect.x + text_rect.w + 1;
            SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(ed->renderer, cur_x, field.y + 3, cur_x, field.y + field.h - 6);
        }
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    } else if (active && cursor_visible) {
        int cur_x = field.x + 4;
        SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
        SDL_RenderDrawLine(ed->renderer, cur_x, field.y + 3, cur_x, field.y + field.h - 6);
    }
}

void render_left_panel(Editor *ed) {
    SDL_Rect bg = { 0, 0, LEFT_PANEL_W, WINDOW_H };
    SDL_SetRenderDrawColor(ed->renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "TILESET", LEFT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    SDL_Rect load_btn = { 10, 35, LEFT_PANEL_W - 20, 26 };
    SDL_SetRenderDrawColor(ed->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(ed->renderer, &load_btn);
    draw_text_centered(ed->renderer, ed->font, "Load Tileset", load_btn.x + load_btn.w/2, load_btn.y + load_btn.h/2, (SDL_Color){255,255,255,255});

    SDL_Rect btnA = { 10, 70, (LEFT_PANEL_W - 30)/2, 24 };
    SDL_Rect btnC = { btnA.x + btnA.w + 10, 70, btnA.w, 24 };
    if (ed->mode == MODE_A) {
        SDL_SetRenderDrawColor(ed->renderer, 100, 160, 100, 255);
        SDL_RenderFillRect(ed->renderer, &btnA);
        SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
        SDL_RenderFillRect(ed->renderer, &btnC);
    } else {
        SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
        SDL_RenderFillRect(ed->renderer, &btnA);
        SDL_SetRenderDrawColor(ed->renderer, 100, 160, 100, 255);
        SDL_RenderFillRect(ed->renderer, &btnC);
    }
    draw_text_centered(ed->renderer, ed->font, "A", btnA.x + btnA.w/2, btnA.y + btnA.h/2, (SDL_Color){255,255,255,255});
    draw_text_centered(ed->renderer, ed->font, "C", btnC.x + btnC.w/2, btnC.y + btnC.h/2, (SDL_Color){255,255,255,255});

    if (ed->mode == MODE_C) {
        int icon_y = 100, icon_size = 24, spacing = 6;
        int total_w = 4 * icon_size + 3 * spacing;
        int start_x = (LEFT_PANEL_W - total_w) / 2;
        for (int t = 0; t < 4; t++) {
            SDL_Rect r = { start_x + t * (icon_size + spacing), icon_y, icon_size, icon_size };
            if (ed->type_icons[t]) SDL_RenderCopy(ed->renderer, ed->type_icons[t], NULL, &r);
            if (t == ed->current_type) {
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                SDL_RenderDrawRect(ed->renderer, &r);
            }
        }
    }

    SDL_SetRenderDrawColor(ed->renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(ed->renderer, 10, 130, LEFT_PANEL_W-10, 130);

    if (!ed->tileset_loaded) {
        draw_text_centered(ed->renderer, ed->font, "No tileset", LEFT_PANEL_W/2, 250, (SDL_Color){255,100,100,255});
        return;
    }

    int visible_rows = (WINDOW_H - PALETTE_START_Y) / (PALETTE_TILE_SIZE + 2);
    int total_rows = (ed->tile_count + PALETTE_COLS - 1) / PALETTE_COLS;

    for (int row = ed->palette_scroll; row < ed->palette_scroll + visible_rows && row < total_rows; row++) {
        for (int col = 0; col < PALETTE_COLS; col++) {
            int idx = row * PALETTE_COLS + col;
            if (idx >= ed->tile_count) break;

            SDL_Rect dst = {
                PALETTE_START_X + col * (PALETTE_TILE_SIZE + 2),
                PALETTE_START_Y + (row - ed->palette_scroll) * (PALETTE_TILE_SIZE + 2),
                PALETTE_TILE_SIZE, PALETTE_TILE_SIZE
            };
            SDL_RenderCopy(ed->renderer, ed->tiles[idx], NULL, &dst);

            SDL_SetRenderDrawColor(ed->renderer, 70, 70, 70, 255);
            SDL_Rect frame = { dst.x - 1, dst.y - 1, PALETTE_TILE_SIZE + 2, PALETTE_TILE_SIZE + 2 };
            SDL_RenderDrawRect(ed->renderer, &frame);

            if (ed->mode == MODE_C && ed->tile_types) {
                int type = ed->tile_types[idx];
                if (type >= 0 && type < 4 && ed->type_icons[type]) {
                    SDL_Rect icon_dst = { dst.x + PALETTE_TILE_SIZE - 16, dst.y + PALETTE_TILE_SIZE - 16, 16, 16 };
                    SDL_RenderCopy(ed->renderer, ed->type_icons[type], NULL, &icon_dst);
                }
            }

            if (ed->mode == MODE_A && idx == ed->selected_tile && ed->blink_visible) {
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                int thickness = 2;
                for (int t = 0; t < thickness; t++) {
                    SDL_Rect r = { frame.x - t, frame.y - t, frame.w + 2*t, frame.h + 2*t };
                    SDL_RenderDrawRect(ed->renderer, &r);
                }
            }
        }
    }
}

void render_toolbar(Editor *ed) {
    SDL_Rect bar = { MAP_X, 0, MAP_W, TOOLBAR_H };
    SDL_SetRenderDrawColor(ed->renderer, 70, 70, 70, 255);
    SDL_RenderFillRect(ed->renderer, &bar);

    struct { int x; int mode; int icon_index; } btns[] = {
        { MAP_X + 5,   RIGHT_ROTATE, 0 },
        { MAP_X + 45,  RIGHT_FLIP_H, 1 },
        { MAP_X + 85,  RIGHT_FLIP_V, 2 },
        { MAP_X + 125, RIGHT_DELETE, 3 }
    };
    int btn_w = 38, btn_h = TOOLBAR_H - 6;

    for (int i = 0; i < 4; i++) {
        SDL_Rect btn_rect = { btns[i].x, 3, btn_w, btn_h };
        bool active = (ed->right_click_mode == btns[i].mode);
        SDL_SetRenderDrawColor(ed->renderer, active ? 140 : 100, active ? 140 : 100, active ? 140 : 100, 255);
        SDL_RenderFillRect(ed->renderer, &btn_rect);

        if (ed->transform_icons[btns[i].icon_index]) {
            SDL_Rect icon_dst = { btn_rect.x + 3, btn_rect.y + 3, btn_rect.w - 6, btn_rect.h - 6 };
            SDL_RenderCopy(ed->renderer, ed->transform_icons[btns[i].icon_index], NULL, &icon_dst);
        } else {
            const char *labels[] = {"R", "FH", "FV", "Del"};
            draw_text_centered(ed->renderer, ed->font, labels[i], btn_rect.x + btn_rect.w/2, btn_rect.y + btn_rect.h/2, (SDL_Color){255,255,255,255});
        }
    }

    int vis_btn_w = 30, vis_btn_h = btn_h, vis_btn_y = 3;
    int vis_btn_x1 = MAP_X + 180, vis_btn_x2 = vis_btn_x1 + vis_btn_w + 4;
    SDL_Rect vis1 = { vis_btn_x1, vis_btn_y, vis_btn_w, vis_btn_h };
    SDL_Rect vis2 = { vis_btn_x2, vis_btn_y, vis_btn_w, vis_btn_h };

    int mx, my;
    get_logical_mouse(ed, &mx, &my);

    bool hover1 = (mx >= vis1.x && mx < vis1.x+vis1.w && my >= vis1.y && my < vis1.y+vis1.h);
    SDL_Color col1 = ed->show_layer1 ? (SDL_Color){100,255,100,255} : (SDL_Color){100,100,100,255};
    if (hover1) col1 = (SDL_Color){150,255,150,255};
    SDL_SetRenderDrawColor(ed->renderer, col1.r, col1.g, col1.b, 255);
    SDL_RenderFillRect(ed->renderer, &vis1);
    draw_text_centered(ed->renderer, ed->font, "L1", vis1.x + vis1.w/2, vis1.y + vis1.h/2, (SDL_Color){255,255,255,255});

    bool hover2 = (mx >= vis2.x && mx < vis2.x+vis2.w && my >= vis2.y && my < vis2.y+vis2.h);
    SDL_Color col2 = ed->show_layer2 ? (SDL_Color){100,255,100,255} : (SDL_Color){100,100,100,255};
    if (hover2) col2 = (SDL_Color){150,255,150,255};
    SDL_SetRenderDrawColor(ed->renderer, col2.r, col2.g, col2.b, 255);
    SDL_RenderFillRect(ed->renderer, &vis2);
    draw_text_centered(ed->renderer, ed->font, "L2", vis2.x + vis2.w/2, vis2.y + vis2.h/2, (SDL_Color){255,255,255,255});

    int active_btn_x = vis_btn_x2 + vis_btn_w + 8;
    SDL_Rect layer_btn = { active_btn_x, 3, btn_w, btn_h };
    bool layer_hover = (mx >= layer_btn.x && mx < layer_btn.x+btn_w && my >= layer_btn.y && my < layer_btn.y+btn_h);
    SDL_Color layer_color = layer_hover ? (SDL_Color){180,180,180,255} :
                           (ed->current_layer == 0 ? (SDL_Color){100,140,100,255} : (SDL_Color){140,100,100,255});
    SDL_SetRenderDrawColor(ed->renderer, layer_color.r, layer_color.g, layer_color.b, 255);
    SDL_RenderFillRect(ed->renderer, &layer_btn);
    char layer_label[2] = { '0' + ed->current_layer + 1, '\0' };
    draw_text_centered(ed->renderer, ed->font, layer_label, layer_btn.x + btn_w/2, layer_btn.y + btn_h/2, (SDL_Color){255,255,255,255});

    SDL_Color hint_color = {200, 200, 200, 255};
    int hint_x = active_btn_x + btn_w + 12, hint_y = layer_btn.y + (layer_btn.h - FONT_SIZE)/2;
    SDL_Surface* text_surf = TTF_RenderUTF8_Blended(ed->font, "Active Layer", hint_color);
    if (text_surf) {
        SDL_Texture* text_tex = SDL_CreateTextureFromSurface(ed->renderer, text_surf);
        SDL_Rect text_dst = { hint_x, hint_y, text_surf->w, text_surf->h };
        SDL_RenderCopy(ed->renderer, text_tex, NULL, &text_dst);
        SDL_FreeSurface(text_surf);
        SDL_DestroyTexture(text_tex);
    }
}

// Карта
void render_map(Editor *ed) {
    Map *map = current_map(ed);
    if (!map || !ed->tileset_loaded) return;

    float zoom = ed->zoom;
    float scaled_tile = TILE_SIZE * zoom;

    SDL_Rect map_area = { MAP_X, MAP_Y, MAP_W, MAP_H };
    SDL_RenderSetClipRect(ed->renderer, &map_area);
	
	// Шахматный фон (статичный)
if (transparent_bg_tex) {
    for (int y = MAP_Y; y < MAP_Y + MAP_H; y += TILE_SIZE) {
        for (int x = MAP_X; x < MAP_X + MAP_W; x += TILE_SIZE) {
                int draw_w = TILE_SIZE;
                int draw_h = TILE_SIZE;
            if (x + draw_w > MAP_X + MAP_W) draw_w = (MAP_X + MAP_W) - x;
            if (y + draw_h > MAP_Y + MAP_H) draw_h = (MAP_Y + MAP_H) - y;
                SDL_Rect src = {0, 0, draw_w, draw_h};
                SDL_Rect dst = {x, y, draw_w, draw_h};
                SDL_RenderCopy(ed->renderer, transparent_bg_tex, &src, &dst);
        }
    }
}

    int start_x = (int)(ed->cam_x / TILE_SIZE);
    int start_y = (int)(ed->cam_y / TILE_SIZE);
    int end_x = start_x + (int)(MAP_W / scaled_tile) + 1;
    int end_y = start_y + (int)(MAP_H / scaled_tile) + 1;
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

    // Второй слой
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

    // Сетка типов тайлов (Grid Mode)
    if (ed->grid_mode_active && map) {
        for (int x = start_x; x < end_x; x++) {
            for (int y = start_y; y < end_y; y++) {
                int idx = x * map->height + y;
                int tile_id = (ed->current_layer == 0) ? map->tiles[idx] : map->tiles2[idx];
                if (tile_id < 0 || tile_id >= ed->tile_count) continue;
                int type = ed->tile_types[tile_id];
                if (type >= 0 && type < 4 && ed->type_icons[type]) {
                    SDL_FRect dst = {
                        MAP_X + (x * TILE_SIZE - ed->cam_x) * zoom,
                        MAP_Y + (y * TILE_SIZE - ed->cam_y) * zoom,
                        scaled_tile, scaled_tile
                    };
                    SDL_SetTextureAlphaMod(ed->type_icons[type], 160);
                    SDL_RenderCopyF(ed->renderer, ed->type_icons[type], NULL, &dst);
                    SDL_SetTextureAlphaMod(ed->type_icons[type], 255);
                }
            }
        }
    }

    // Подсветка тайла под мышью
    int mx, my;
    get_logical_mouse(ed, &mx, &my);
    if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
        float world_x = (mx - MAP_X) / zoom + ed->cam_x;
        float world_y = (my - MAP_Y) / zoom + ed->cam_y;
        int tx = (int)(world_x / TILE_SIZE);
        int ty = (int)(world_y / TILE_SIZE);
        if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
            SDL_Rect hl = {
                (int)(MAP_X + (tx * TILE_SIZE - ed->cam_x) * zoom),
                (int)(MAP_Y + (ty * TILE_SIZE - ed->cam_y) * zoom),
                (int)scaled_tile, (int)scaled_tile
            };
            SDL_SetRenderDrawColor(ed->renderer, ed->current_layer == 0 ? 255 : 100,
                                   ed->current_layer == 0 ? 255 : 200,
                                   ed->current_layer == 0 ? 0 : 255, 255);
            SDL_RenderDrawRect(ed->renderer, &hl);
        }
    }

    // ─── Прямоугольник выделения ───
    if (ed->selecting) {
        int x1 = ed->sel_start_x, x2 = ed->sel_end_x;
        int y1 = ed->sel_start_y, y2 = ed->sel_end_y;
        if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
        if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
        SDL_Rect sel_rect = {
            (int)(MAP_X + (x1 * TILE_SIZE - ed->cam_x) * ed->zoom),
            (int)(MAP_Y + (y1 * TILE_SIZE - ed->cam_y) * ed->zoom),
            (int)((x2 - x1 + 1) * TILE_SIZE * ed->zoom),
            (int)((y2 - y1 + 1) * TILE_SIZE * ed->zoom)
        };
        SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
        SDL_RenderDrawRect(ed->renderer, &sel_rect);
    }

    // ─── Превью вставки ───
    if (ed->has_clipboard && !clipboard_is_empty(&ed->clipboard)) {
        int mx, my;
        get_logical_mouse(ed, &mx, &my);
        if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
            float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
            float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
            int tx = (int)(world_x / TILE_SIZE);
            int ty = (int)(world_y / TILE_SIZE);
            clipboard_draw_preview(&ed->clipboard, ed->renderer, ed->tiles, ed->tile_count,
                                   (int)ed->cam_x, (int)ed->cam_y, ed->zoom, tx, ty);
        }
    }

    // ─── ПОЛОСЫ ПРОКРУТКИ ───
    float map_pixel_w = map->width * TILE_SIZE;
    float map_pixel_h = map->height * TILE_SIZE;
    float view_w = MAP_W / zoom;
    float view_h = MAP_H / zoom;

    if (map_pixel_h > view_h) {
        SDL_Rect v_track = { MAP_X + MAP_W, MAP_Y, SCROLLBAR_SIZE, MAP_H };
        SDL_SetRenderDrawColor(ed->renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(ed->renderer, &v_track);
        float thumb_h = (view_h / map_pixel_h) * MAP_H;
        if (thumb_h < 8) thumb_h = 8;
        float max_cam_y = map_pixel_h - view_h;
        if (max_cam_y < 0) max_cam_y = 0;
        float thumb_y = (max_cam_y > 0) ? MAP_Y + (ed->cam_y / max_cam_y) * (MAP_H - thumb_h) : MAP_Y;
        SDL_Rect v_thumb = { MAP_X + MAP_W + 4, (int)thumb_y, SCROLLBAR_SIZE - 8, (int)thumb_h };
        SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
        SDL_RenderFillRect(ed->renderer, &v_thumb);
    }
    if (map_pixel_w > view_w) {
        SDL_Rect h_track = { MAP_X, MAP_Y + MAP_H, MAP_W, SCROLLBAR_SIZE };
        SDL_SetRenderDrawColor(ed->renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(ed->renderer, &h_track);
        float thumb_w = (view_w / map_pixel_w) * MAP_W;
        if (thumb_w < 8) thumb_w = 8;
        float max_cam_x = map_pixel_w - view_w;
        if (max_cam_x < 0) max_cam_x = 0;
        float thumb_x = (max_cam_x > 0) ? MAP_X + (ed->cam_x / max_cam_x) * (MAP_W - thumb_w) : MAP_X;
        SDL_Rect h_thumb = { (int)thumb_x, MAP_Y + MAP_H + 4, (int)thumb_w, SCROLLBAR_SIZE - 8 };
        SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
        SDL_RenderFillRect(ed->renderer, &h_thumb);
    }

    SDL_RenderSetClipRect(ed->renderer, NULL);
}

void render_right_panel(Editor *ed) {
    int pan_x = WINDOW_W - RIGHT_PANEL_W;
    int panel_x = pan_x + 5;
    int btn_w = RIGHT_PANEL_W - 10;

    // Фон
    SDL_Rect bg = { pan_x, 0, RIGHT_PANEL_W, WINDOW_H };
    SDL_SetRenderDrawColor(ed->renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "MAPS", pan_x + RIGHT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    int y_current = 35;
    int mx, my;
    get_logical_mouse(ed, &mx, &my);

    // ── Grid Mode ──
    SDL_Rect grid_btn = { panel_x, y_current, btn_w, 22 };
    bool hover_grid = (mx >= grid_btn.x && mx < grid_btn.x+grid_btn.w &&
                       my >= grid_btn.y && my < grid_btn.y+grid_btn.h);
    SDL_Color grid_bg = ed->grid_mode_active ? (SDL_Color){200, 50, 50, 255} :
                        (hover_grid ? (SDL_Color){140,140,140,255} : (SDL_Color){100,100,100,255});
    SDL_SetRenderDrawColor(ed->renderer, grid_bg.r, grid_bg.g, grid_bg.b, 255);
    SDL_RenderFillRect(ed->renderer, &grid_btn);
    draw_text_centered(ed->renderer, ed->font, "Grid Mode",
                       grid_btn.x + grid_btn.w/2, grid_btn.y + grid_btn.h/2,
                       (SDL_Color){255,255,255,255});

    y_current += 28;

    // ── Иконки типов ──
    int icon_size = 20, icon_spacing = 4;
    int total_icons_w = 4 * icon_size + 3 * icon_spacing;
    int start_icon_x = panel_x + (btn_w - total_icons_w) / 2;
    for (int t = 0; t < 4; t++) {
        SDL_Rect icon_rect = { start_icon_x + t * (icon_size + icon_spacing),
                               y_current, icon_size, icon_size };
        if (t == ed->current_type) {
            SDL_SetRenderDrawColor(ed->renderer, 255, 255, 100, 255);
            SDL_Rect sel = { icon_rect.x - 2, icon_rect.y - 2, icon_size+4, icon_size+4 };
            SDL_RenderFillRect(ed->renderer, &sel);
        }
        if (ed->type_icons[t])
            SDL_RenderCopy(ed->renderer, ed->type_icons[t], NULL, &icon_rect);
    }

    y_current += icon_size + 10;

    // ── Список карт ──
    int list_start_y = y_current;
    int buttons_y = WINDOW_H - 180;           // верхняя граница кнопок действий
    int list_max_h = buttons_y - list_start_y - 10;  // доступная высота для списка
    int line_h = 20;                          // высота строки
    int max_visible = list_max_h / line_h;    // сколько строк влезает

    // Обрезаем область списка
    SDL_Rect list_clip = { pan_x + 5, list_start_y, btn_w, list_max_h };
    SDL_RenderSetClipRect(ed->renderer, &list_clip);

    int total_rows = ed->map_list.map_count;
    int max_scroll = (total_rows > max_visible) ? (total_rows - max_visible) : 0;
    if (ed->map_list_scroll < 0) ed->map_list_scroll = 0;
    if (ed->map_list_scroll > max_scroll) ed->map_list_scroll = max_scroll;

    for (int i = ed->map_list_scroll; i < total_rows && i < ed->map_list_scroll + max_visible; i++) {
        int row_y = list_start_y + (i - ed->map_list_scroll) * line_h;
        SDL_Color col = (i == ed->map_list.current_map) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,255,255};
        draw_text_centered(ed->renderer, ed->font, ed->map_list.maps[i].name,
                          pan_x + RIGHT_PANEL_W/2, row_y + line_h/2, col);
    }

    // ── Скроллбар списка ──
    if (total_rows > max_visible) {
        int bar_x = pan_x + RIGHT_PANEL_W - 10;
        int bar_w = 6;
        SDL_Rect track = { bar_x, list_start_y, bar_w, list_max_h };
        SDL_SetRenderDrawColor(ed->renderer, 90, 90, 90, 255);
        SDL_RenderFillRect(ed->renderer, &track);

        float visible_ratio = (float)max_visible / total_rows;
        int thumb_h = (int)(list_max_h * visible_ratio);
        if (thumb_h < 12) thumb_h = 12;
        int thumb_y = list_start_y + (int)((list_max_h - thumb_h) * ((float)ed->map_list_scroll / max_scroll));
        SDL_Rect thumb = { bar_x, thumb_y, bar_w, thumb_h };
        SDL_SetRenderDrawColor(ed->renderer, 180, 180, 180, 255);
        SDL_RenderFillRect(ed->renderer, &thumb);
    }

    SDL_RenderSetClipRect(ed->renderer, NULL);   // снимаем обрезку

    // ── Подсказка по буферу обмена ──
    SDL_Color help_color = {200, 200, 200, 255};
    int help_y = buttons_y - 10;
    draw_text_centered(ed->renderer, ed->font, "Alt+LMB: Select area", pan_x + RIGHT_PANEL_W/2, help_y - 18, help_color);
    draw_text_centered(ed->renderer, ed->font, "LMB: Paste  |  Esc: Cancel", pan_x + RIGHT_PANEL_W/2, help_y, help_color);

    // ── Кнопки действий ──
    int btn_x = pan_x + 5;
    int y = buttons_y;
    const char *names[] = {"New Map", "Save Map", "Delete Map", "Rename Map", "Resize Map", "Set Music"};
    for (int i = 0; i < 6; i++) {
        SDL_Rect btn = { btn_x, y + i*30, btn_w, 24 };
        bool hover = (mx >= btn.x && mx < btn.x+btn.w && my >= btn.y && my < btn.y+btn.h);
        SDL_Color bg_col = hover ? (SDL_Color){110,110,110,255} : (SDL_Color){90,90,90,255};
        if (i == 1 && ed->save_blink_active) bg_col = (SDL_Color){220, 80, 80, 255};
        SDL_SetRenderDrawColor(ed->renderer, bg_col.r, bg_col.g, bg_col.b, 255);
        SDL_RenderFillRect(ed->renderer, &btn);
        draw_text_centered(ed->renderer, ed->font, names[i], btn.x + btn.w/2, btn.y + btn.h/2, (SDL_Color){255,255,255,255});
    }
}

void open_dialog(Editor *ed, int type) {
    ed->dialog_active = true;
    ed->dialog_type = type;
    ed->dialog_active_field = 0;
    ed->dialog_cursor_blink = SDL_GetTicks();
    SDL_StartTextInput();

    switch (type) {
        case DIALOG_MUSIC_MAP: {
            Map *cur = current_map(ed);
            if (cur) {
                safe_strcpy(ed->music_fullpath, sizeof(ed->music_fullpath), cur->music_file);
                const char *full = ed->music_fullpath;
                const char *name = strrchr(full, '/');
                if (!name) name = strrchr(full, '\\');
                if (name) name++; else name = full;
                char display_name[64];
                size_t len = strlen(name);
                if (len > 12) {
                    memcpy(display_name, name, 12);
                    memcpy(display_name + 12, "...", 3);
                    display_name[15] = '\0';
                } else {
                    strcpy(display_name, name);
                }
                snprintf(ed->input_text, sizeof(ed->input_text), "%s", display_name);
                snprintf(ed->input_text2, sizeof(ed->input_text2), "%.2f", cur->music_volume);
            } else {
                ed->music_fullpath[0] = '\0';
                ed->input_text[0] = '\0';
                strcpy(ed->input_text2, "0.80");
            }
            break;
        }
        default: break;
    }
}

void close_dialog(Editor *ed) {
    ed->dialog_active = false;
    ed->dialog_type = DIALOG_NONE;
    ed->dialog_active_field = 0;
    ed->dialog_just_closed = true;
    memset(ed->input_text, 0, sizeof(ed->input_text));
    memset(ed->input_text2, 0, sizeof(ed->input_text2));
    SDL_StopTextInput();
}

void draw_dialog(Editor *ed) {
    if (!ed->dialog_active) return;

    SDL_SetRenderDrawBlendMode(ed->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 180);
    SDL_Rect full = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(ed->renderer, &full);
    SDL_SetRenderDrawBlendMode(ed->renderer, SDL_BLENDMODE_NONE);

    SDL_Rect dlg = { WINDOW_W/2 - 180, WINDOW_H/2 - 110, 360, 220 };
    SDL_SetRenderDrawColor(ed->renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(ed->renderer, &dlg);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(ed->renderer, &dlg);

    SDL_Color black = {0,0,0,255}, white = {255,255,255,255};
    int y_text = dlg.y + 20;
    SDL_Rect field1, field2;
    bool show_cursor = (SDL_GetTicks() - ed->dialog_cursor_blink) < 400;

    switch (ed->dialog_type) {
        case DIALOG_CONFIRM_DEL:
            draw_text_centered(ed->renderer, ed->font, "Delete this map?", dlg.x + 180, y_text, black);
            draw_text_centered(ed->renderer, ed->font, ed->input_text, dlg.x + 180, y_text + 30, black);
            break;
        case DIALOG_NEW_MAP:
            draw_text_centered(ed->renderer, ed->font, "Name:", dlg.x + 50, y_text + 10, black);
            field1 = (SDL_Rect){ dlg.x + 110, y_text - 2, 200, 24 };
            draw_input_field(ed, field1, ed->input_text, ed->dialog_active_field == 0, show_cursor);
            draw_text_centered(ed->renderer, ed->font, "Size (WxH):", dlg.x + 50, y_text + 45, black);
            field2 = (SDL_Rect){ dlg.x + 110, y_text + 33, 200, 24 };
            draw_input_field(ed, field2, ed->input_text2, ed->dialog_active_field == 1, show_cursor);
            break;
        case DIALOG_RENAME_MAP:
            draw_text_centered(ed->renderer, ed->font, "New name:", dlg.x + 50, y_text + 15, black);
            field1 = (SDL_Rect){ dlg.x + 130, y_text + 3, 180, 24 };
            draw_input_field(ed, field1, ed->input_text, true, show_cursor);
            break;
        case DIALOG_RESIZE_MAP:
            draw_text_centered(ed->renderer, ed->font, "Width:", dlg.x + 50, y_text + 10, black);
            field1 = (SDL_Rect){ dlg.x + 110, y_text - 2, 180, 24 };
            draw_input_field(ed, field1, ed->input_text, ed->dialog_active_field == 0, show_cursor);
            draw_text_centered(ed->renderer, ed->font, "Height:", dlg.x + 50, y_text + 45, black);
            field2 = (SDL_Rect){ dlg.x + 110, y_text + 33, 180, 24 };
            draw_input_field(ed, field2, ed->input_text2, ed->dialog_active_field == 1, show_cursor);
            break;
        case DIALOG_MUSIC_MAP:
            draw_text_centered(ed->renderer, ed->font, "Music file:", dlg.x + 50, y_text + 5, black);
            field1 = (SDL_Rect){ dlg.x + 120, y_text - 2, 150, 24 };
            draw_input_field(ed, field1, ed->input_text, ed->dialog_active_field == 0, show_cursor);
            { SDL_Rect browse_btn = { dlg.x + 280, y_text - 2, 65, 24 };
            SDL_SetRenderDrawColor(ed->renderer, 150, 150, 150, 255);
            SDL_RenderFillRect(ed->renderer, &browse_btn);
            draw_text_centered(ed->renderer, ed->font, "...", browse_btn.x + browse_btn.w/2, browse_btn.y + browse_btn.h/2, black); }
            draw_text_centered(ed->renderer, ed->font, "Volume (0-1):", dlg.x + 50, y_text + 40, black);
            field2 = (SDL_Rect){ dlg.x + 140, y_text + 28, 80, 24 };
            draw_input_field(ed, field2, ed->input_text2, ed->dialog_active_field == 1, show_cursor);
            break;
    }

    SDL_Rect ok = { dlg.x + 30, dlg.y + 180, 110, 28 };
    SDL_Rect cancel = { dlg.x + 200, dlg.y + 180, 110, 28 };
    SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
    SDL_RenderFillRect(ed->renderer, &ok);
    SDL_RenderFillRect(ed->renderer, &cancel);
    const char *ok_text = (ed->dialog_type == DIALOG_CONFIRM_DEL) ? "Yes" : "OK";
    draw_text_centered(ed->renderer, ed->font, ok_text, ok.x + ok.w/2, ok.y + ok.h/2, white);
    draw_text_centered(ed->renderer, ed->font, "Cancel", cancel.x + cancel.w/2, cancel.y + cancel.h/2, white);
}

void handle_dialog_click(Editor *ed, int mx, int my) {
    SDL_Rect dlg = { WINDOW_W/2 - 180, WINDOW_H/2 - 110, 360, 220 };
    SDL_Rect ok = { dlg.x + 30, dlg.y + 180, 110, 28 };
    SDL_Rect cancel = { dlg.x + 200, dlg.y + 180, 110, 28 };

    if (ed->dialog_type == DIALOG_MUSIC_MAP) {
        SDL_Rect browse_btn = { dlg.x + 280, dlg.y + 18, 65, 24 };
        if (mx >= browse_btn.x && mx < browse_btn.x+browse_btn.w &&
            my >= browse_btn.y && my < browse_btn.y+browse_btn.h)
        {
            char path[256];
            if (open_file_dialog(path, sizeof(path))) {
                char rel_music[256];
                get_relative_path(path, rel_music, sizeof(rel_music));
                safe_strcpy(ed->music_fullpath, sizeof(ed->music_fullpath), rel_music);
                const char *bname = strrchr(rel_music, '/');
                if (!bname) bname = strrchr(rel_music, '\\');
                if (bname) bname++; else bname = rel_music;
                size_t blen = strlen(bname);
                if (blen > 12) {
                    memcpy(ed->input_text, bname, 12);
                    memcpy(ed->input_text + 12, "...", 3);
                    ed->input_text[15] = '\0';
                } else snprintf(ed->input_text, sizeof(ed->input_text), "%s", bname);
                ed->dialog_active_field = 0;
            }
            return;
        }
    }

    if (mx >= ok.x && mx < ok.x+ok.w && my >= ok.y && my < ok.y+ok.h) {
        switch (ed->dialog_type) {
            case DIALOG_CONFIRM_DEL: delete_current_map(ed); break;
            case DIALOG_NEW_MAP: {
                int w = 20, h = 15;
                sscanf(ed->input_text2, "%dx%d", &w, &h);
                char first_ts[512];
                find_first_tileset_path(first_ts, sizeof(first_ts));
                create_map(ed, ed->input_text, w, h, first_ts);
                break;
            }
            case DIALOG_RENAME_MAP: rename_current_map(ed, ed->input_text); break;
            case DIALOG_RESIZE_MAP: {
                int w = atoi(ed->input_text), h = atoi(ed->input_text2);
                resize_current_map(ed, w, h);
                break;
            }
            case DIALOG_MUSIC_MAP: {
               Map *cur = current_map(ed);
               if (cur) {
               safe_strcpy(cur->music_file, sizeof(cur->music_file), ed->music_fullpath);
               cur->music_volume = (float)atof(ed->input_text2);
               map_save_to_json(cur, cur->folder);   // ← теперь через folder
               }
            break;
            }
        }
        close_dialog(ed);
    } else if (mx >= cancel.x && mx < cancel.x+cancel.w && my >= cancel.y && my < cancel.y+cancel.h)
        close_dialog(ed);
}

void handle_input(Editor *ed, bool *running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *running = false; return; }

        // ===== ДИАЛОГИ =====
        if (ed->dialog_active) {
            if (SDL_GetTicks() - ed->dialog_cursor_blink > 530)
                ed->dialog_cursor_blink = SDL_GetTicks();

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_RETURN: handle_dialog_click(ed, WINDOW_W/2, WINDOW_H/2); break;
                    case SDLK_ESCAPE: close_dialog(ed); break;
                    case SDLK_TAB:
                        if (ed->dialog_type == DIALOG_NEW_MAP || ed->dialog_type == DIALOG_RESIZE_MAP ||
                            ed->dialog_type == DIALOG_MUSIC_MAP)
                            ed->dialog_active_field = !ed->dialog_active_field;
                        break;
                    case SDLK_BACKSPACE: {
                        if (ed->dialog_type == DIALOG_MUSIC_MAP && ed->dialog_active_field == 0) break;
                        char *str = (ed->dialog_active_field == 0) ? ed->input_text : ed->input_text2;
                        if (str && strlen(str) > 0) str[strlen(str)-1] = '\0';
                        break;
                    }
                }
            } else if (e.type == SDL_TEXTINPUT) {
                if (ed->dialog_type == DIALOG_MUSIC_MAP && ed->dialog_active_field == 0) break;
                char *dest = (ed->dialog_active_field == 0) ? ed->input_text : ed->input_text2;
                int max_len = (ed->dialog_active_field == 1) ? 15 : 63;
                if (dest && strlen(dest) < max_len) strcat(dest, e.text.text);
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_Rect dlg = { WINDOW_W/2 - 180, WINDOW_H/2 - 110, 360, 220 };
                if (ed->dialog_type == DIALOG_NEW_MAP || ed->dialog_type == DIALOG_RESIZE_MAP ||
                    ed->dialog_type == DIALOG_MUSIC_MAP) {
                    int y_base = dlg.y + 20;
                    SDL_Rect f1, f2;
                    if (ed->dialog_type == DIALOG_NEW_MAP) {
                        f1 = (SDL_Rect){ dlg.x + 110, y_base - 2, 200, 24 };
                        f2 = (SDL_Rect){ dlg.x + 110, y_base + 33, 200, 24 };
                    } else if (ed->dialog_type == DIALOG_RESIZE_MAP) {
                        f1 = (SDL_Rect){ dlg.x + 110, y_base - 2, 180, 24 };
                        f2 = (SDL_Rect){ dlg.x + 110, y_base + 33, 180, 24 };
                    } else {
                        f1 = (SDL_Rect){ dlg.x + 120, y_base - 2, 150, 24 };
                        f2 = (SDL_Rect){ dlg.x + 140, y_base + 28, 80, 24 };
                    }
                    if (e.button.x >= f1.x && e.button.x < f1.x+f1.w &&
                        e.button.y >= f1.y && e.button.y < f1.y+f1.h)
                        ed->dialog_active_field = 0;
                    else if (e.button.x >= f2.x && e.button.x < f2.x+f2.w &&
                             e.button.y >= f2.y && e.button.y < f2.y+f2.h)
                        ed->dialog_active_field = 1;
                    else handle_dialog_click(ed, e.button.x, e.button.y);
                } else handle_dialog_click(ed, e.button.x, e.button.y);
            }
            continue;
        }

        // --- Esc для отмены буфера/выделения ---
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            if (ed->has_clipboard) {
                ed->has_clipboard = false;
            } else if (ed->selecting) {
                ed->selecting = false;
            }
        }

        // Переключение активного слоя по Tab
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_TAB) {
            ed->current_layer = !ed->current_layer;
        }

        // ========== РЕЖИМ РЕДАКТОРА ==========
        // Колесо мыши
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my;
            get_logical_mouse(ed, &mx, &my);
            if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y && ed->tileset_loaded) {
                ed->palette_scroll -= e.wheel.y;
                int max_scroll = ((ed->tile_count + PALETTE_COLS - 1) / PALETTE_COLS) -
                                 ((WINDOW_H - PALETTE_START_Y) / (PALETTE_TILE_SIZE + 2));
                if (ed->palette_scroll < 0) ed->palette_scroll = 0;
                if (max_scroll > 0 && ed->palette_scroll > max_scroll) ed->palette_scroll = max_scroll;
            }
            else if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                int icon_size = 20;
                int list_start_y = 35 + 28 + (icon_size + 10);
                int buttons_y = WINDOW_H - 180;
                int list_max_h = buttons_y - list_start_y - 10;
                int line_h = 20;
                int max_visible = list_max_h / line_h;
                int total_rows = ed->map_list.map_count;
                int max_scroll = (total_rows > max_visible) ? (total_rows - max_visible) : 0;
                ed->map_list_scroll -= e.wheel.y;
                if (ed->map_list_scroll < 0) ed->map_list_scroll = 0;
                if (ed->map_list_scroll > max_scroll) ed->map_list_scroll = max_scroll;
            }
            else if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                    ed->zoom += e.wheel.y * 0.1f;
                    if (ed->zoom < 0.1f) ed->zoom = 0.1f;
                    if (ed->zoom > 2.0f) ed->zoom = 2.0f;
                }
            }
        }

        // Правая кнопка – панорамирование (с Ctrl)
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
            if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                int mx, my;
                get_logical_mouse(ed, &mx, &my);
                ed->panning = 1;
                ed->pan_start_x = mx;
                ed->pan_start_y = my;
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) ed->panning = 0;

        // Отпускание левой кнопки – завершение выделения и копирование в буфер
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            ed->dialog_just_closed = false;
            ed->scrollbar_drag_h = false;
            ed->scrollbar_drag_v = false;
            ed->paste_just_performed = false;
            if (ed->selecting) {
                ed->selecting = false;
                clipboard_copy(&ed->clipboard, current_map(ed),
                               ed->sel_start_x, ed->sel_start_y,
                               ed->sel_end_x, ed->sel_end_y, ed->current_layer);
                ed->has_clipboard = !clipboard_is_empty(&ed->clipboard);
            }
        }

        // Инструменты правой кнопки по карте
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT &&
            !(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) &&
            ed->right_click_mode != 0 && !ed->grid_mode_active)
        {
            int mx = e.button.x, my = e.button.y;
            if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
                float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
                float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
                int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
                Map *map = current_map(ed);
                if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                    int idx = tx * map->height + ty;
                    int *rot = (ed->current_layer == 0) ? map->rot : map->rot2;
                    int *mirror_x = (ed->current_layer == 0) ? map->mirror_x : map->mirror_x2;
                    int *mirror_y = (ed->current_layer == 0) ? map->mirror_y : map->mirror_y2;
                    switch (ed->right_click_mode) {
                        case RIGHT_ROTATE: rot[idx] = (rot[idx] + 1) % 4; break;
                        case RIGHT_FLIP_H: mirror_x[idx] = !mirror_x[idx]; break;
                        case RIGHT_FLIP_V: mirror_y[idx] = !mirror_y[idx]; break;
                        case RIGHT_DELETE:
                            if (ed->current_layer == 0) map->tiles[idx] = -1;
                            else map->tiles2[idx] = -1;
                            break;
                    }
                }
            }
        }

        // Левая кнопка
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx, my;
            get_logical_mouse(ed, &mx, &my);

            // --- Alt+ЛКМ (выделение) – ПРОВЕРЯЕТСЯ ПЕРВЫМ ---
            if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
                if (SDL_GetModState() & KMOD_ALT) {
                    Map *map = current_map(ed);
                    if (map) {
                        float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
                        float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
                        int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
                        if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                            ed->selecting = true;
                            ed->sel_start_x = tx;
                            ed->sel_end_x   = tx;
                            ed->sel_start_y = ty;
                            ed->sel_end_y   = ty;
                            return;
                        }
                    }
                }
            }

            // --- Вставка буфера (без Alt) ---
            if (!ed->selecting && ed->has_clipboard &&
            !(SDL_GetModState() & KMOD_ALT))
            {
                if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
                    float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
                    float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
                    int tx = (int)(world_x / TILE_SIZE);
                    int ty = (int)(world_y / TILE_SIZE);
                    clipboard_paste(&ed->clipboard, current_map(ed), tx, ty, ed->current_layer);
                    ed->paste_just_performed = true;
                }
                return;
            }

            // --- Левая панель ---
            if (mx < LEFT_PANEL_W) {
                if (my >= 35 && my < 61) {
                    char path[256];
                    if (open_file_dialog(path, sizeof(path)) && load_tileset(ed, path)) {
                        Map *cur = current_map(ed);
                        if (cur) get_relative_path(path, cur->tileset_path, sizeof(cur->tileset_path));
                    }
                }
                else if (my >= 70 && my < 94) {
                    int half = (LEFT_PANEL_W - 30) / 2;
                    if (mx >= 10 && mx < 10+half) ed->mode = MODE_A;
                    else if (mx >= 10+half+10 && mx < 10+half+10+half) ed->mode = MODE_C;
                }
                else if (ed->mode == MODE_C && my >= 100 && my < 124) {
                    int icon_y = 100, sz = 24, sp = 6;
                    int tw = 4*sz + 3*sp;
                    int sx = (LEFT_PANEL_W - tw) / 2;
                    for (int t = 0; t < 4; t++) {
                        SDL_Rect r = { sx + t*(sz+sp), icon_y, sz, sz };
                        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) { ed->current_type = t; break; }
                    }
                }
                else if (ed->tileset_loaded && my >= PALETTE_START_Y && ed->mode == MODE_A) {
                    int rx = mx - PALETTE_START_X, ry = my - PALETTE_START_Y;
                    int step = PALETTE_TILE_SIZE + 2;
                    if (rx >= 0 && ry >= 0) {
                        int col = rx / step, row = ry / step + ed->palette_scroll;
                        if (col < PALETTE_COLS && (rx % step) < PALETTE_TILE_SIZE) {
                            int idx = row * PALETTE_COLS + col;
                            if (idx >= 0 && idx < ed->tile_count) ed->selected_tile = idx;
                        }
                    }
                }
            }
            // --- Тулбар ---
            else if (my < TOOLBAR_H && mx >= MAP_X && mx < MAP_X + MAP_W) {
                int vis_btn_w = 30, vis_btn_x1 = MAP_X + 180, vis_btn_x2 = vis_btn_x1 + vis_btn_w + 4;
                int active_btn_x = vis_btn_x2 + vis_btn_w + 8, btn_w = 38;
                if (mx >= MAP_X+5 && mx < MAP_X+43) ed->right_click_mode = RIGHT_ROTATE;
                else if (mx >= MAP_X+45 && mx < MAP_X+83) ed->right_click_mode = RIGHT_FLIP_H;
                else if (mx >= MAP_X+85 && mx < MAP_X+123) ed->right_click_mode = RIGHT_FLIP_V;
                else if (mx >= MAP_X+125 && mx < MAP_X+163) ed->right_click_mode = RIGHT_DELETE;
                else if (mx >= vis_btn_x1 && mx < vis_btn_x1+vis_btn_w) ed->show_layer1 = !ed->show_layer1;
                else if (mx >= vis_btn_x2 && mx < vis_btn_x2+vis_btn_w) ed->show_layer2 = !ed->show_layer2;
                else if (mx >= active_btn_x && mx < active_btn_x+btn_w) ed->current_layer = !ed->current_layer;
                else ed->right_click_mode = 0;
            }
            // --- Карта (рисование) ---
            else if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
                float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
                float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
                int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
                Map *map = current_map(ed);
                if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                    int idx = tx * map->height + ty;
                    if (ed->grid_mode_active) {
                        int tile_id = (ed->current_layer == 0) ? map->tiles[idx] : map->tiles2[idx];
                        if (tile_id >= 0 && tile_id < ed->tile_count) ed->tile_types[tile_id] = ed->current_type;
                    } else if (ed->mode == MODE_A) {
                        if (ed->current_layer == 0) {
                            map->tiles[idx] = ed->selected_tile; map->rot[idx] = 0;
                            map->mirror_x[idx] = 0; map->mirror_y[idx] = 0;
                        } else {
                            map->tiles2[idx] = ed->selected_tile; map->rot2[idx] = 0;
                            map->mirror_x2[idx] = 0; map->mirror_y2[idx] = 0;
                        }
                    } else if (ed->mode == MODE_C) {
                        int tile_id = (ed->current_layer == 0) ? map->tiles[idx] : map->tiles2[idx];
                        if (tile_id >= 0 && tile_id < ed->tile_count) ed->tile_types[tile_id] = ed->current_type;
                    }
                }
            }
            // --- Правая панель ---
            else if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                int panel_x = WINDOW_W - RIGHT_PANEL_W + 5, btn_w = RIGHT_PANEL_W - 10;
                if (my >= 35 && my < 57 && mx >= panel_x && mx < panel_x+btn_w) { ed->grid_mode_active = !ed->grid_mode_active; return; }
                if (my >= 63 && my < 83) {
                    int icon_size = 20, icon_spacing = 4, total_icons_w = 4*icon_size + 3*icon_spacing;
                    int start_icon_x = panel_x + (btn_w - total_icons_w)/2;
                    for (int t = 0; t < 4; t++) {
                        SDL_Rect r = { start_icon_x + t*(icon_size+icon_spacing), 63, icon_size, icon_size };
                        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) { ed->current_type = t; return; }
                    }
                }
                int icon_size = 20;
                int list_start_y = 35 + 28 + (icon_size + 10);
                int line_h = 20;
                int max_visible = (WINDOW_H - 180 - list_start_y - 10) / line_h;
                int start_idx = ed->map_list_scroll;
                int end_idx = (ed->map_list_scroll + max_visible < ed->map_list.map_count) ? 
                              (ed->map_list_scroll + max_visible) : ed->map_list.map_count;
                for (int i = start_idx; i < end_idx; i++) {
                    int row_y = list_start_y + (i - ed->map_list_scroll) * line_h;
                    if (my >= row_y - 10 && my < row_y + 10) {
                        ed->map_list.current_map = i;
                        Map *m = &ed->map_list.maps[i];
                        load_tileset(ed, m->tileset_path);
                        return;
                    }
                }
                int y = WINDOW_H - 180;
                if (my >= y && my < y+24) { open_dialog(ed, DIALOG_NEW_MAP); strcpy(ed->input_text, "map00"); strcpy(ed->input_text2, "20x15"); }
                else if (my >= y+30 && my < y+54) {
                    Map *cur = current_map(ed);
                    if (cur) {
                        map_save_to_json(cur, cur->folder);
                        save_tile_types_for_tileset(ed, cur->tileset_path);
                        ed->save_blink_active = true;
                        ed->save_blink_time = SDL_GetTicks();
                    }
                }
                else if (my >= y+60 && my < y+84) {
                    if (ed->map_list.map_count > 1) {
                        safe_strcpy(ed->input_text, sizeof(ed->input_text), current_map(ed)->name);
                        open_dialog(ed, DIALOG_CONFIRM_DEL);
                    }
                }
                else if (my >= y+90 && my < y+114) {
                    safe_strcpy(ed->input_text, sizeof(ed->input_text), current_map(ed) ? current_map(ed)->name : "");
                    open_dialog(ed, DIALOG_RENAME_MAP);
                }
                else if (my >= y+120 && my < y+144) {
                    if (current_map(ed)) {
                        snprintf(ed->input_text, sizeof(ed->input_text), "%d", current_map(ed)->width);
                        snprintf(ed->input_text2, sizeof(ed->input_text2), "%d", current_map(ed)->height);
                    }
                    open_dialog(ed, DIALOG_RESIZE_MAP);
                }
                else if (my >= y+150 && my < y+174) open_dialog(ed, DIALOG_MUSIC_MAP);
            }
            // --- Скроллбары карты ---
            else if (mx >= MAP_X + MAP_W && mx < MAP_X + MAP_W + SCROLLBAR_SIZE &&
                     my >= MAP_Y && my < MAP_Y + MAP_H) {
                Map *map = current_map(ed);
                if (map) {
                    float view_h = MAP_H / ed->zoom;
                    float max_cam_y = map->height * TILE_SIZE - view_h;
                    if (max_cam_y > 0) {
                        float thumb_h = (view_h / (map->height * TILE_SIZE)) * MAP_H;
                        if (thumb_h < 8) thumb_h = 8;
                        float thumb_y = MAP_Y + (ed->cam_y / max_cam_y) * (MAP_H - thumb_h);
                        if (my >= thumb_y && my <= thumb_y + thumb_h) {
                            ed->scrollbar_drag_v = true;
                            ed->scroll_drag_mouse_offset_y = my - (int)thumb_y;
                        } else {
                            float target_y = ((my - MAP_Y) / (float)MAP_H) * max_cam_y;
                            ed->cam_y = target_y;
                            if (ed->cam_y > max_cam_y) ed->cam_y = max_cam_y;
                            if (ed->cam_y < 0) ed->cam_y = 0;
                        }
                    }
                }
            }
            else if (mx >= MAP_X && mx < MAP_X + MAP_W &&
                     my >= MAP_Y + MAP_H && my < MAP_Y + MAP_H + SCROLLBAR_SIZE) {
                Map *map = current_map(ed);
                if (map) {
                    float view_w = MAP_W / ed->zoom;
                    float max_cam_x = map->width * TILE_SIZE - view_w;
                    if (max_cam_x > 0) {
                        float thumb_w = (view_w / (map->width * TILE_SIZE)) * MAP_W;
                        if (thumb_w < 8) thumb_w = 8;
                        float thumb_x = MAP_X + (ed->cam_x / max_cam_x) * (MAP_W - thumb_w);
                        if (mx >= thumb_x && mx <= thumb_x + thumb_w) {
                            ed->scrollbar_drag_h = true;
                            ed->scroll_drag_mouse_offset_x = mx - (int)thumb_x;
                        } else {
                            float target_x = ((mx - MAP_X) / (float)MAP_W) * max_cam_x;
                            ed->cam_x = target_x;
                            if (ed->cam_x > max_cam_x) ed->cam_x = max_cam_x;
                            if (ed->cam_x < 0) ed->cam_x = 0;
                        }
                    }
                }
            }
        }
    }   // КОНЕЦ while (SDL_PollEvent)

    // ======= НЕПРЕРЫВНЫЕ ДЕЙСТВИЯ (состояние мыши/клавиш) ======= 
    // Перетаскивание скроллбаров
    if (ed->scrollbar_drag_v) {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        Map *map = current_map(ed);
        if (map) {
            float view_h = MAP_H / ed->zoom;
            float max_cam_y = map->height * TILE_SIZE - view_h;
            if (max_cam_y > 0) {
                float thumb_h = (view_h / (map->height * TILE_SIZE)) * MAP_H;
                if (thumb_h < 8) thumb_h = 8;
                float range = MAP_H - thumb_h;
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
            float view_w = MAP_W / ed->zoom;
            float max_cam_x = map->width * TILE_SIZE - view_w;
            if (max_cam_x > 0) {
                float thumb_w = (view_w / (map->width * TILE_SIZE)) * MAP_W;
                if (thumb_w < 8) thumb_w = 8;
                float range = MAP_W - thumb_w;
                float local_x = mx - MAP_X - ed->scroll_drag_mouse_offset_x;
                if (local_x < 0) local_x = 0;
                if (local_x > range) local_x = range;
                ed->cam_x = (local_x / range) * max_cam_x;
            }
        }
    }

    // Панорамирование
    if (ed->panning && !ed->dialog_active) {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        float dx = (mx - ed->pan_start_x) / ed->zoom;
        float dy = (my - ed->pan_start_y) / ed->zoom;
        ed->cam_x -= dx; ed->cam_y -= dy;
        ed->pan_start_x = mx; ed->pan_start_y = my;
        Map *map = current_map(ed);
        if (map) {
            float max_x = map->width * TILE_SIZE - MAP_W / ed->zoom;
            float max_y = map->height * TILE_SIZE - MAP_H / ed->zoom;
            if (max_x < 0) max_x = 0;
            if (max_y < 0) max_y = 0;
            if (ed->cam_x < 0) ed->cam_x = 0;
            if (ed->cam_x > max_x) ed->cam_x = max_x;
            if (ed->cam_y < 0) ed->cam_y = 0;
            if (ed->cam_y > max_y) ed->cam_y = max_y;
        }
    }

    Uint32 mouse_state = SDL_GetMouseState(NULL, NULL);

    // Обновление прямоугольника выделения при зажатом Alt+ЛКМ
    if (ed->selecting && (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT))) {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
            float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
            float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
            int tx = (int)(world_x / TILE_SIZE);
            int ty = (int)(world_y / TILE_SIZE);
            ed->sel_end_x = tx;
            ed->sel_end_y = ty;
        }
    }

    // Непрерывное рисование левой кнопкой
    if (!ed->dialog_active && (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) && !ed->panning && !ed->dialog_just_closed && !ed->selecting && !ed->paste_just_performed) {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
            float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
            float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
            int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
            Map *map = current_map(ed);
            if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                int idx = tx * map->height + ty;
                if (ed->grid_mode_active) {
                    int tile_id = (ed->current_layer == 0) ? map->tiles[idx] : map->tiles2[idx];
                    if (tile_id >= 0 && tile_id < ed->tile_count) ed->tile_types[tile_id] = ed->current_type;
                } else if (ed->mode == MODE_A) {
                    if (ed->current_layer == 0) {
                        map->tiles[idx] = ed->selected_tile; map->rot[idx] = 0;
                        map->mirror_x[idx] = 0; map->mirror_y[idx] = 0;
                    } else {
                        map->tiles2[idx] = ed->selected_tile; map->rot2[idx] = 0;
                        map->mirror_x2[idx] = 0; map->mirror_y2[idx] = 0;
                    }
                }
            }
        }
    }

    // Непрерывное удаление правой кнопкой
    if (!ed->dialog_active && !ed->grid_mode_active && ed->right_click_mode == RIGHT_DELETE &&
        (mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) && !ed->panning)
    {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
            float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
            float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
            int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
            Map *map = current_map(ed);
            if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                int idx = tx * map->height + ty;
                if (ed->current_layer == 0) map->tiles[idx] = -1;
                else map->tiles2[idx] = -1;
            }
        }
    }

    // Непрерывное редактирование tile_types в палитре (MODE_C)
    if (!ed->dialog_active && ed->mode == MODE_C && ed->tileset_loaded && (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT))) {
        int mx, my; get_logical_mouse(ed, &mx, &my);
        if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y) {
            int rx = mx - PALETTE_START_X, ry = my - PALETTE_START_Y;
            int step = PALETTE_TILE_SIZE + 2;
            if (rx >= 0 && ry >= 0) {
                int col = rx / step, row = ry / step + ed->palette_scroll;
                if (col < PALETTE_COLS && (rx % step) < PALETTE_TILE_SIZE) {
                    int idx = row * PALETTE_COLS + col;
                    if (idx >= 0 && idx < ed->tile_count) ed->tile_types[idx] = ed->current_type;
                }
            }
        }
    }
}

void load_transparent_bg(SDL_Renderer *renderer) {
    if (transparent_bg_tex) return;  // уже загружена
    SDL_Surface *surf = IMG_Load("../assets/icons/transparent.png");
    if (surf) {
        transparent_bg_tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
    } else {
        fprintf(stderr, "Failed to load transparent.png: %s\n", IMG_GetError());
    }
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    Editor ed;
    editor_init(&ed);

// === СОЗДАНИЕ ОКНА ===
RECT workArea;
SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
int screenW = workArea.right - workArea.left;
int screenH = workArea.bottom - workArea.top;

int winW = WINDOW_W;
int winH = WINDOW_H;

if (screenW < WINDOW_W || screenH < WINDOW_H) {
    float scale = fminf((float)screenW / WINDOW_W, (float)screenH / WINDOW_H) * 0.92f;
    winW = (int)(WINDOW_W * scale);
    winH = (int)(WINDOW_H * scale);
}

ed.window = SDL_CreateWindow("Map Editor C SDL2",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             winW, winH,
                             SDL_WINDOW_SHOWN | 
                             SDL_WINDOW_RESIZABLE | 
                             SDL_WINDOW_ALLOW_HIGHDPI);

ed.renderer = SDL_CreateRenderer(ed.window, -1,
                                 SDL_RENDERER_ACCELERATED | 
                                 SDL_RENDERER_PRESENTVSYNC);

SDL_RenderSetLogicalSize(ed.renderer, WINDOW_W, WINDOW_H);

load_transparent_bg(ed.renderer);

    ed.font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", FONT_SIZE);
    if (!ed.font) ed.font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", FONT_SIZE);
    if (!ed.font) { printf("No font!\n"); return 1; }

    load_type_icons(&ed);
    load_transform_icons(&ed);

    CreateDirectoryA("../data/maps", NULL);
    load_map_list(&ed);

    if (ed.map_list.map_count == 0) {
        char first_ts[512];
        find_first_tileset_path(first_ts, sizeof(first_ts));
        create_map(&ed, "map00", 20, 15, first_ts);
    } else {
        Map *cur = current_map(&ed);
        if (cur) load_tileset(&ed, cur->tileset_path);
    }

    Uint32 last_blink = SDL_GetTicks();
    bool running = true;
    while (running) {
        handle_input(&ed, &running);

        Uint32 now = SDL_GetTicks();
        if (now - last_blink >= 500) { ed.blink_visible = !ed.blink_visible; last_blink = now; }
        if (ed.save_blink_active && now - ed.save_blink_time >= 150) ed.save_blink_active = false;

        SDL_SetRenderDrawColor(ed.renderer, 30, 30, 30, 255);
        SDL_RenderClear(ed.renderer);

        render_left_panel(&ed);
        render_toolbar(&ed);
        render_map(&ed);
        render_right_panel(&ed);
        draw_dialog(&ed);

        SDL_RenderPresent(ed.renderer);
        SDL_Delay(16);
    }

    free_tileset(&ed);
	if (transparent_bg_tex) SDL_DestroyTexture(transparent_bg_tex);
    TTF_CloseFont(ed.font);
    for (int i = 0; i < 4; i++) if (ed.type_icons[i]) SDL_DestroyTexture(ed.type_icons[i]);
    for (int i = 0; i < 4; i++) if (ed.transform_icons[i]) SDL_DestroyTexture(ed.transform_icons[i]);
    for (int i = 0; i < ed.map_list.map_count; i++) map_free(&ed.map_list.maps[i]);
    free(ed.map_list.maps);

    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}