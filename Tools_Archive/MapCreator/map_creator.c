/*
 * MapCreator — простой генератор тайлсетов (без поворотов).
 * Каждый уникальный тайл сохраняется “как есть” — без приведения к канонической форме.
 * Карта записывается без поворотов и отражений (rot=0, mirror_x/mirror_y = false).
 */

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../cJSON.h"

#define TILE_SIZE       48
#define TILE_PIXELS     (TILE_SIZE * TILE_SIZE)          // 2304
#define TILE_BYTES      (TILE_PIXELS * 4)                // 9216 (RGBA)
#define MAX_TILES       1024   // при необходимости увеличьте до 2048
#define TILESET_DIM     32     // 32×32 тайла
#define TILESET_WIDTH   (TILESET_DIM * TILE_SIZE)        // 1536
#define TILESET_HEIGHT  (TILESET_DIM * TILE_SIZE)        // 1536

typedef struct {
    uint32_t pixels[TILE_PIXELS];
} TileRGBA;

typedef struct {
    TileRGBA original;      // точная копия, как вырезан из карты
} UniqueTile;

static SDL_Window   *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static TTF_Font     *g_font = NULL;

static SDL_Surface  *g_tileset_surface = NULL;
static char g_status_message[1024] = "Load a PNG map to start.";
static bool g_ready_to_save = false;
static bool g_map_ready = false;

static char g_source_image_path[512] = "";
static char g_map_name[64] = "";
static UniqueTile *g_unique_tiles = NULL;
static int g_unique_count = 0;

static const char *g_tileset_folder = "../assets/tilesets/";

static SDL_Rect btn_load = { 50, 400, 140, 40 };
static SDL_Rect btn_save_tileset = { 220, 400, 140, 40 };
static SDL_Rect btn_save_map = { 390, 400, 140, 40 };
static SDL_Rect btn_open = { 560, 400, 140, 40 };

bool open_file_dialog(char *out_path, size_t out_len) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Files\0*.png\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) {
        strncpy(out_path, ofn.lpstrFile, out_len);
        out_path[out_len-1] = '\0';
        return true;
    }
    return false;
}

bool save_file_dialog(char *out_path, size_t out_len) {
    OPENFILENAMEA ofn;
    char szFile[260] = "tileset.png";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Files\0*.png\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) {
        strncpy(out_path, ofn.lpstrFile, out_len);
        out_path[out_len-1] = '\0';
        return true;
    }
    return false;
}

// ---------- Простое сравнение тайлов (без поворотов) ----------

int compare_tiles_rgba(const TileRGBA *a, const TileRGBA *b) {
    return memcmp(a->pixels, b->pixels, sizeof(a->pixels));
}

// ---------- Извлечение уникальных тайлов (простое, без ориентаций) ----------

int extract_unique_tiles_rgba(SDL_Surface *image, UniqueTile *list, int max_tiles, bool *limit_reached) {
    if (limit_reached) *limit_reached = false;
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA32, 0);
    if (!rgba) return 0;

    int tiles_x = rgba->w / TILE_SIZE;
    int tiles_y = rgba->h / TILE_SIZE;
    int unique_count = 0;

    for (int ty = 0; ty < tiles_y; ty++) {
        for (int tx = 0; tx < tiles_x; tx++) {
            TileRGBA tile;
            uint8_t *src_pixels = (uint8_t*)rgba->pixels;
            for (int y = 0; y < TILE_SIZE; y++) {
                int img_y = ty * TILE_SIZE + y;
                uint32_t *src_row = (uint32_t*)(src_pixels + img_y * rgba->pitch + tx * TILE_SIZE * 4);
                memcpy(&tile.pixels[y * TILE_SIZE], src_row, TILE_SIZE * sizeof(uint32_t));
            }

            // Проверяем, есть ли точно такой тайл уже в списке
            bool found = false;
            for (int u = 0; u < unique_count; u++) {
                if (compare_tiles_rgba(&tile, &list[u].original) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                if (unique_count < max_tiles) {
                    list[unique_count].original = tile;
                    unique_count++;
                } else {
                    if (limit_reached) *limit_reached = true;
                    ty = tiles_y; break;
                }
            }
        }
    }
    SDL_FreeSurface(rgba);
    return unique_count;
}

// ---------- Построение тайлсета (столбцами по 8) ----------

SDL_Surface* build_tileset_rgba(UniqueTile *list, int count) {
    SDL_Surface *ts = SDL_CreateRGBSurface(0, TILESET_WIDTH, TILESET_HEIGHT, 32,
                                           0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (!ts) return NULL;
    SDL_FillRect(ts, NULL, SDL_MapRGBA(ts->format, 255,255,255,255));

    int fill = (count > MAX_TILES) ? MAX_TILES : count;
    int palette_cols = 8;   // PALETTE_COLS редактора
    int strips = TILESET_DIM / palette_cols;   // 4

    int tile_idx = 0;
    for (int strip = 0; strip < strips && tile_idx < fill; strip++) {
        int start_col = strip * palette_cols;
        for (int row = 0; row < TILESET_DIM && tile_idx < fill; row++) {
            for (int c = 0; c < palette_cols && tile_idx < fill; c++) {
                int col = start_col + c;
                uint8_t *dst_base = (uint8_t*)ts->pixels + row * TILE_SIZE * ts->pitch + col * TILE_SIZE * 4;
                for (int y = 0; y < TILE_SIZE; y++) {
                    uint32_t *src_row = &list[tile_idx].original.pixels[y * TILE_SIZE];
                    uint32_t *dst_row = (uint32_t*)(dst_base + y * ts->pitch);
                    memcpy(dst_row, src_row, TILE_SIZE * sizeof(uint32_t));
                }
                tile_idx++;
            }
        }
    }
    return ts;
}

// ---------- Сохранение PNG (stb_image_write) ----------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool save_tileset_png(const char *filename, SDL_Surface *surface) {
    if (!surface) return false;
    int result = stbi_write_png(filename, surface->w, surface->h, 4,
                                surface->pixels, surface->pitch);
    return result != 0;
}

// ---------- Сохранение карты (JSON) без поворотов ----------

static void find_next_tileset_name(char *out, size_t out_len) {
    for (int i = 0; i < 1000; i++) {
        snprintf(out, out_len, "%stileset%03d.png", g_tileset_folder, i);
        FILE *f = fopen(out, "rb");
        if (!f) return;   // имя свободно
        fclose(f);
    }
    snprintf(out, out_len, "%s/tileset999.png", g_tileset_folder);
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

bool save_map_and_tileset(void) {
    if (!g_map_ready || !g_tileset_surface || !g_unique_tiles) return false;

    // 1. Сохраняем тайлсет под автоматическим именем
    char tileset_path[512];
    find_next_tileset_name(tileset_path, sizeof(tileset_path));
    if (!save_tileset_png(tileset_path, g_tileset_surface)) {
        snprintf(g_status_message, sizeof(g_status_message), "Failed to save tileset.");
        return false;
    }

    // Формируем относительный путь для JSON (без двойных слешей)
    char rel_path[256];
    const char *assets = strstr(tileset_path, "assets");
    if (assets) {
        strncpy(rel_path, assets, sizeof(rel_path) - 1);
        rel_path[sizeof(rel_path) - 1] = '\0';
    } else {
        snprintf(rel_path, sizeof(rel_path), "assets/tilesets/tileset000.png");
    }
    // Приводим все обратные слеши к прямым
    for (char *p = rel_path; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    // 2. Загружаем исходное изображение, чтобы построить карту
    SDL_Surface *img = IMG_Load(g_source_image_path);
    if (!img) {
        snprintf(g_status_message, sizeof(g_status_message), "Could not reload source image.");
        return false;
    }

    int tiles_x = img->w / TILE_SIZE;
    int tiles_y = img->h / TILE_SIZE;
    int total = tiles_x * tiles_y;
    int *tiles_arr = (int*)malloc(total * sizeof(int));
    int *rot_arr   = (int*)malloc(total * sizeof(int));
    int *mx_arr    = (int*)malloc(total * sizeof(int));
    int *my_arr    = (int*)malloc(total * sizeof(int));

    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img);
    uint8_t *pixels = (uint8_t*)rgba->pixels;

    for (int ty = 0; ty < tiles_y; ty++) {
        for (int tx = 0; tx < tiles_x; tx++) {
            TileRGBA tile;
            for (int y = 0; y < TILE_SIZE; y++) {
                int img_y = ty * TILE_SIZE + y;
                uint32_t *row = (uint32_t*)(pixels + img_y * rgba->pitch + tx * TILE_SIZE * 4);
                memcpy(&tile.pixels[y * TILE_SIZE], row, TILE_SIZE * sizeof(uint32_t));
            }

            // Ищем индекс этого точного тайла в массиве уникальных
            int idx = 0;
            for (int u = 0; u < g_unique_count; u++) {
                if (compare_tiles_rgba(&tile, &g_unique_tiles[u].original) == 0) {
                    idx = u;
                    break;
                }
            }

            int pos = tx * tiles_y + ty;
            tiles_arr[pos] = idx;
            rot_arr[pos]   = 0;
            mx_arr[pos]    = 0;
            my_arr[pos]    = 0;
        }
    }
    SDL_FreeSurface(rgba);

    // 3. Строим полноценный layout.json
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", g_map_name);
    cJSON_AddNumberToObject(root, "width", tiles_x);
    cJSON_AddNumberToObject(root, "height", tiles_y);
    cJSON_AddStringToObject(root, "tileset", rel_path);

    // Музыка – пока пустая, но структура нужна для совместимости
    cJSON *music = cJSON_CreateObject();
    cJSON_AddStringToObject(music, "file", "");
    cJSON_AddNumberToObject(music, "volume", 0.8);
    cJSON_AddItemToObject(root, "music", music);

    // Первый слой
    cJSON *t  = cJSON_AddArrayToObject(root, "tiles");
    cJSON *r  = cJSON_AddArrayToObject(root, "rot");
    cJSON *mxj = cJSON_AddArrayToObject(root, "mirror_x");
    cJSON *myj = cJSON_AddArrayToObject(root, "mirror_y");

    for (int x = 0; x < tiles_x; x++) {
        cJSON *col_t  = cJSON_CreateArray();
        cJSON *col_r  = cJSON_CreateArray();
        cJSON *col_mx = cJSON_CreateArray();
        cJSON *col_my = cJSON_CreateArray();
        for (int y = 0; y < tiles_y; y++) {
            int p = x * tiles_y + y;
            cJSON_AddItemToArray(col_t,  cJSON_CreateNumber(tiles_arr[p]));
            cJSON_AddItemToArray(col_r,  cJSON_CreateNumber(rot_arr[p]));
            cJSON_AddItemToArray(col_mx, cJSON_CreateBool(mx_arr[p]));
            cJSON_AddItemToArray(col_my, cJSON_CreateBool(my_arr[p]));
        }
        cJSON_AddItemToArray(t,  col_t);
        cJSON_AddItemToArray(r,  col_r);
        cJSON_AddItemToArray(mxj, col_mx);
        cJSON_AddItemToArray(myj, col_my);
    }

    // Второй слой – пустой (все -1)
    cJSON *t2  = cJSON_AddArrayToObject(root, "tiles2");
    cJSON *r2  = cJSON_AddArrayToObject(root, "rot2");
    cJSON *mx2 = cJSON_AddArrayToObject(root, "mirror_x2");
    cJSON *my2 = cJSON_AddArrayToObject(root, "mirror_y2");
    for (int x = 0; x < tiles_x; x++) {
        cJSON *col_t2  = cJSON_CreateArray();
        cJSON *col_r2  = cJSON_CreateArray();
        cJSON *col_mx2 = cJSON_CreateArray();
        cJSON *col_my2 = cJSON_CreateArray();
        for (int y = 0; y < tiles_y; y++) {
            cJSON_AddItemToArray(col_t2,  cJSON_CreateNumber(-1));
            cJSON_AddItemToArray(col_r2,  cJSON_CreateNumber(0));
            cJSON_AddItemToArray(col_mx2, cJSON_CreateBool(0));
            cJSON_AddItemToArray(col_my2, cJSON_CreateBool(0));
        }
        cJSON_AddItemToArray(t2,  col_t2);
        cJSON_AddItemToArray(r2,  col_r2);
        cJSON_AddItemToArray(mx2, col_mx2);
        cJSON_AddItemToArray(my2, col_my2);
    }

    // Коллизия – всё проходимо (0)
    cJSON *collision = cJSON_AddArrayToObject(root, "collision");
    for (int x = 0; x < tiles_x; x++) {
        cJSON *col = cJSON_CreateArray();
        for (int y = 0; y < tiles_y; y++) {
            cJSON_AddItemToArray(col, cJSON_CreateNumber(0));
        }
        cJSON_AddItemToArray(collision, col);
    }

    // 4. Создаём папку data/maps/<имя_карты>
    char map_folder[512];
    snprintf(map_folder, sizeof(map_folder), "../data/maps/%s", g_map_name);
    CreateDirectoryA(map_folder, NULL);

    // Сохраняем layout.json
    char json_path[768];
    snprintf(json_path, sizeof(json_path), "%s/layout.json", map_folder);
    char *json_str = cJSON_Print(root);
    FILE *f = fopen(json_path, "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
    }
    cJSON_Delete(root);
    free(json_str);

    // 5. Обновляем entries.json
    cJSON *entries = load_entries();                          // функция ниже
    add_or_update_entry(entries, g_map_name, g_map_name, "", 0.8); // пустая музыка
    save_entries(entries);                                    // функция ниже

    free(tiles_arr); free(rot_arr); free(mx_arr); free(my_arr);

    snprintf(g_status_message, sizeof(g_status_message),
             "Map saved: %s/layout.json, tileset: %s", g_map_name, tileset_path);
    return true;
}

// ---------- Отрисовка кнопок ----------

void draw_button_text(SDL_Renderer *ren, TTF_Font *font, SDL_Rect *btn, const char *text) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, white);
    if (s) {
        SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
        SDL_Rect dst = { btn->x + (btn->w - s->w)/2,
                         btn->y + (btn->h - s->h)/2,
                         s->w, s->h };
        SDL_RenderCopy(ren, t, NULL, &dst);
        SDL_DestroyTexture(t);
        SDL_FreeSurface(s);
    }
}

// ---------- Рендеринг ----------

void render() {
    SDL_SetRenderDrawColor(g_renderer, 40,40,40,255);
    SDL_RenderClear(g_renderer);

    SDL_SetRenderDrawColor(g_renderer, 100,100,200,255);
    SDL_RenderFillRect(g_renderer, &btn_load);

    SDL_SetRenderDrawColor(g_renderer, g_ready_to_save ? 100:60,
                           g_ready_to_save ? 200:60,
                           g_ready_to_save ? 100:60, 255);
    SDL_RenderFillRect(g_renderer, &btn_save_tileset);

    SDL_SetRenderDrawColor(g_renderer, g_map_ready ? 100:60,
                           g_map_ready ? 200:60,
                           g_map_ready ? 100:60, 255);
    SDL_RenderFillRect(g_renderer, &btn_save_map);

    SDL_SetRenderDrawColor(g_renderer, 140,140,140,255);
    SDL_RenderFillRect(g_renderer, &btn_open);

    draw_button_text(g_renderer, g_font, &btn_load, "Load PNG");
    draw_button_text(g_renderer, g_font, &btn_save_tileset, "Save tileset");
    draw_button_text(g_renderer, g_font, &btn_save_map, "Save Map");
    draw_button_text(g_renderer, g_font, &btn_open, "Open folder");

    if (g_status_message[0]) {
        SDL_Color msg_color = {220,220,220,255};
        SDL_Surface *msg = TTF_RenderUTF8_Blended(g_font, g_status_message, msg_color);
        if (msg) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, msg);
            SDL_Rect dst = { 50, 50, msg->w, msg->h };
            SDL_RenderCopy(g_renderer, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(msg);
        }
    }
    SDL_RenderPresent(g_renderer);
}

// ---------- Загрузка и обработка ----------

void process_image(const char *filename) {
    SDL_Surface *img = IMG_Load(filename);
    if (!img) {
        snprintf(g_status_message, sizeof(g_status_message), "Error loading image.");
        g_ready_to_save = false;
        g_map_ready = false;
        return;
    }
    if (img->w < 2*TILE_SIZE || img->h < 2*TILE_SIZE) {
        snprintf(g_status_message, sizeof(g_status_message), "Image too small (min 2x2 tiles).");
        SDL_FreeSurface(img);
        g_ready_to_save = false;
        g_map_ready = false;
        return;
    }

    UniqueTile *unique_tiles = (UniqueTile*)malloc(MAX_TILES * sizeof(UniqueTile));
    if (!unique_tiles) {
        snprintf(g_status_message, sizeof(g_status_message), "Memory allocation failed.");
        SDL_FreeSurface(img);
        g_ready_to_save = false;
        g_map_ready = false;
        return;
    }

    bool limit_reached = false;
    int unique_count = extract_unique_tiles_rgba(img, unique_tiles, MAX_TILES, &limit_reached);

    // Сохраняем исходное имя файла
    strncpy(g_source_image_path, filename, sizeof(g_source_image_path)-1);
    g_source_image_path[sizeof(g_source_image_path)-1] = '\0';
    const char *base = strrchr(filename, '\\');
    if (!base) base = strrchr(filename, '/');
    if (!base) base = filename; else base++;
    strncpy(g_map_name, base, sizeof(g_map_name)-1);
    g_map_name[sizeof(g_map_name)-1] = '\0';
    char *dot = strrchr(g_map_name, '.');
    if (dot) *dot = '\0';

    SDL_FreeSurface(img);

    if (unique_count == 0) {
        snprintf(g_status_message, sizeof(g_status_message), "No tiles found.");
        free(unique_tiles);
        g_ready_to_save = false;
        g_map_ready = false;
        return;
    }

    SDL_Surface *ts = build_tileset_rgba(unique_tiles, unique_count);
    if (!ts) {
        snprintf(g_status_message, sizeof(g_status_message), "Failed to create tileset.");
        free(unique_tiles);
        g_ready_to_save = false;
        g_map_ready = false;
        return;
    }

    if (g_tileset_surface) SDL_FreeSurface(g_tileset_surface);
    g_tileset_surface = ts;

    if (g_unique_tiles) free(g_unique_tiles);
    g_unique_tiles = unique_tiles;
    g_unique_count = unique_count;

    if (limit_reached) {
        snprintf(g_status_message, sizeof(g_status_message),
                 "More than %d unique tiles! Only %d saved.", MAX_TILES, MAX_TILES);
    } else {
        snprintf(g_status_message, sizeof(g_status_message),
                 "Tileset ready: %d unique tiles (max %d).", unique_count, MAX_TILES);
    }
    g_ready_to_save = true;
    g_map_ready = true;
}

// ---------- Главный цикл ----------

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    g_window = SDL_CreateWindow("Map Creator - Tileset Generator",
                                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                800, 480, SDL_WINDOW_SHOWN);
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    g_font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", 18);
    if (!g_font) g_font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    if (!g_font) { SDL_Log("No font!"); return 1; }

    bool running = true;
    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x, my = e.button.y;
                if (mx >= btn_load.x && mx < btn_load.x+btn_load.w &&
                    my >= btn_load.y && my < btn_load.y+btn_load.h) {
                    char filepath[260];
                    if (open_file_dialog(filepath, sizeof(filepath)))
                        process_image(filepath);
                }
                else if (mx >= btn_save_tileset.x && mx < btn_save_tileset.x+btn_save_tileset.w &&
                         my >= btn_save_tileset.y && my < btn_save_tileset.y+btn_save_tileset.h) {
                    if (g_ready_to_save && g_tileset_surface) {
                        char savepath[260];
                        if (save_file_dialog(savepath, sizeof(savepath))) {
                            if (save_tileset_png(savepath, g_tileset_surface))
                                snprintf(g_status_message, sizeof(g_status_message),
                                         "Saved: %s", savepath);
                            else
                                snprintf(g_status_message, sizeof(g_status_message),
                                         "Error saving PNG.");
                        }
                    }
                }
                else if (mx >= btn_save_map.x && mx < btn_save_map.x+btn_save_map.w &&
                         my >= btn_save_map.y && my < btn_save_map.y+btn_save_map.h) {
                    if (g_map_ready) {
                        save_map_and_tileset();
                    }
                }
                else if (mx >= btn_open.x && mx < btn_open.x+btn_open.w &&
                         my >= btn_open.y && my < btn_open.y+btn_open.h) {
                    ShellExecuteA(NULL, "open", g_tileset_folder, NULL, NULL, SW_SHOW);
                }
            }
        }
        render();
        SDL_Delay(16);
    }

    if (g_tileset_surface) SDL_FreeSurface(g_tileset_surface);
    if (g_unique_tiles) free(g_unique_tiles);
    TTF_CloseFont(g_font);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}