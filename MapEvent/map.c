// map.c
#include "map.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (dest_size > 0) snprintf(dest, dest_size, "%s", src);
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
    for (char *p = out; *p; ++p) if (*p == '\\') *p = '/';
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
    if (!width_json || !height_json || !tileset_json) {
        cJSON_Delete(root); return false;
    }

    int w = width_json->valueint, h = height_json->valueint;
    if (w < MIN_MAP_SIZE || w > MAX_MAP_SIZE || h < MIN_MAP_SIZE || h > MAX_MAP_SIZE) {
        cJSON_Delete(root); return false;
    }

    map->width = w; map->height = h;
    get_relative_path(tileset_json->valuestring, map->tileset_path, sizeof(map->tileset_path));
    map->music_file[0] = '\0';
    map->music_volume = 0.8f;

    // Загрузка музыки, если есть
    cJSON *music_json = cJSON_GetObjectItem(root, "music");
    if (music_json && cJSON_IsObject(music_json)) {
        cJSON *file_json = cJSON_GetObjectItem(music_json, "file");
        cJSON *vol_json  = cJSON_GetObjectItem(music_json, "volume");
        if (file_json && cJSON_IsString(file_json))
            safe_strcpy(map->music_file, sizeof(map->music_file), file_json->valuestring);
        if (vol_json && cJSON_IsNumber(vol_json))
            map->music_volume = (float)vol_json->valuedouble;
    }

    // Первый слой
    cJSON *t_json = cJSON_GetObjectItem(root, "tiles");
    cJSON *r_json = cJSON_GetObjectItem(root, "rot");
    cJSON *mx_json = cJSON_GetObjectItem(root, "mirror_x");
    cJSON *my_json = cJSON_GetObjectItem(root, "mirror_y");
    if (!t_json || !r_json || !mx_json || !my_json) { cJSON_Delete(root); return false; }

    int sz = w * h;
    map->tiles = (int*)malloc(sz * sizeof(int));
    map->rot = (int*)malloc(sz * sizeof(int));
    map->mirror_x = (int*)malloc(sz * sizeof(int));
    map->mirror_y = (int*)malloc(sz * sizeof(int));
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
    map->tiles2 = (int*)malloc(sz * sizeof(int));
    map->rot2 = (int*)malloc(sz * sizeof(int));
    map->mirror_x2 = (int*)malloc(sz * sizeof(int));
    map->mirror_y2 = (int*)malloc(sz * sizeof(int));
    cJSON *t2_json = cJSON_GetObjectItem(root, "tiles2");
    if (t2_json) {
        cJSON *r2_json = cJSON_GetObjectItem(root, "rot2");
        cJSON *mx2_json = cJSON_GetObjectItem(root, "mirror_x2");
        cJSON *my2_json = cJSON_GetObjectItem(root, "mirror_y2");
        for (int x = 0; x < w; x++) {
            cJSON *col_t2  = cJSON_GetArrayItem(t2_json, x);
            cJSON *col_r2  = r2_json ? cJSON_GetArrayItem(r2_json, x) : NULL;
            cJSON *col_mx2 = mx2_json ? cJSON_GetArrayItem(mx2_json, x) : NULL;
            cJSON *col_my2 = my2_json ? cJSON_GetArrayItem(my2_json, x) : NULL;
            for (int y = 0; y < h; y++) {
                int idx = x * h + y;
                map->tiles2[idx]     = (col_t2 && cJSON_IsArray(col_t2) && cJSON_GetArrayItem(col_t2, y)) ? cJSON_GetArrayItem(col_t2, y)->valueint : -1;
                map->rot2[idx]       = (col_r2 && cJSON_IsArray(col_r2) && cJSON_GetArrayItem(col_r2, y)) ? cJSON_GetArrayItem(col_r2, y)->valueint : 0;
                map->mirror_x2[idx]  = (col_mx2 && cJSON_IsArray(col_mx2) && cJSON_GetArrayItem(col_mx2, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_mx2, y)) : false;
                map->mirror_y2[idx]  = (col_my2 && cJSON_IsArray(col_my2) && cJSON_GetArrayItem(col_my2, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_my2, y)) : false;
            }
        }
    } else {
        for (int i = 0; i < sz; i++) map->tiles2[i] = -1;
    }

    // Коллизия
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

void map_save_to_json(Map *map, const char *folder) {
    char dir[512];
    snprintf(dir, sizeof(dir), "../data/maps/%s", folder);
    CreateDirectoryA(dir, NULL);
    char filename[768];
    snprintf(filename, sizeof(filename), "%s/layout.json", dir);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "width", map->width);
    cJSON_AddNumberToObject(root, "height", map->height);
    cJSON_AddStringToObject(root, "tileset", map->tileset_path);

    // Музыка
    if (map->music_file[0]) {
        cJSON *music = cJSON_CreateObject();
        cJSON_AddStringToObject(music, "file", map->music_file);
        cJSON_AddNumberToObject(music, "volume", map->music_volume);
        cJSON_AddItemToObject(root, "music", music);
    }

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

    // Коллизия
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
}

void map_free(Map *map) {
    free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
    free(map->tiles2); free(map->rot2); free(map->mirror_x2); free(map->mirror_y2);
    free(map->cell_type);
}