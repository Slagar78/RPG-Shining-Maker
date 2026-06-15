#ifndef EVENTS_H
#define EVENTS_H

#include "cJSON.h"

#define MAX_ROOF_EVENTS   128
#define MAX_TILE_CHANGES  64
#define MAX_STAIRS        64
#define MAX_WARPS         64

/* Структуры событий */
typedef struct {
    int start_x, start_y, end_x, end_y;
    int direction;          // 0 = '\', 1 = '/'
} StairEvent;

typedef struct {
    int trigger_x, trigger_y;
    char target_map[64];
    int target_x, target_y;
    int facing;             // 2=вниз, 4=влево, 6=вправо, 8=вверх
} WarpEvent;

typedef struct {
    int trigger_x, trigger_y;
    int new_tile_id;
    int sample_x, sample_y; // клетка-образец
    int close_x, close_y;   // клетка закрытия двери
} TileChangeEvent;

typedef struct {
    int tile_id;
    int trigger_x, trigger_y;
    int trigger2_x, trigger2_y;
    int start_x, start_y, end_x, end_y;
    int exit_x, exit_y;
    int exit2_x, exit2_y;
} RoofEvent;

/* Предварительное объявление, чтобы не тащить весь editor.h */
struct Editor;

/* Загрузка / сохранение событий карты */
void load_events_from_json(struct Editor *ed, const char *folder);
void save_events_to_json(struct Editor *ed, const char *folder);

#endif