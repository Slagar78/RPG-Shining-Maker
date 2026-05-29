// map.h
#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stddef.h>

#define MIN_MAP_SIZE 2
#define MAX_MAP_SIZE 999

typedef struct {
    char name[64];
    char folder[64];
    int width, height;
    char tileset_path[256];
    char music_file[256];
    float music_volume;

    int *tiles;
    int *rot;
    int *mirror_x;
    int *mirror_y;

    int *tiles2;
    int *rot2;
    int *mirror_x2;
    int *mirror_y2;

    int *cell_type;
} Map;

bool map_load_from_json(Map *map, const char *filename);
void map_save_to_json(Map *map, const char *folder);
void map_free(Map *map);
void get_relative_path(const char *abs_path, char *out, size_t out_len);
void safe_strcpy(char *dest, size_t dest_size, const char *src);

#endif