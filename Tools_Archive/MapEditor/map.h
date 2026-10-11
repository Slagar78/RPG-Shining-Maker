// map.h
#ifndef MAP_STRUCT_H
#define MAP_STRUCT_H

typedef struct {
    char name[64];
    char folder[64];
    int width, height;
    int *tiles;
    int *rot;
    int *mirror_x;
    int *mirror_y;
    int *tiles2;
    int *rot2;
    int *mirror_x2;
    int *mirror_y2;
    int *cell_type;
    char tileset_path[256];
    char music_file[256];
    float music_volume;
    char areas_path[256];
} Map;

#endif// MAP_STRUCT_H