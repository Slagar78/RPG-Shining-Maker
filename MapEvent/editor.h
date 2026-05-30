// editor.h
#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "events.h"
#include "map.h"

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

typedef struct {
    Map *maps;
    int map_count;
    int current_map;
    int map_list_scroll;
} MapList;

typedef struct Editor {
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
    int selected_roof_event;
    int edit_field;
    char input_buf[32];

    bool left_panel_collapsed;

    bool show_all_roofs;
    bool show_all_tile_changes;
    bool show_all_stairs;
    bool show_all_warps;

    TileChangeEvent tile_changes[MAX_TILE_CHANGES];
    int tile_change_count;
    int selected_tile_change;
    int tc_edit_field;
    char tc_input_buf[32];
    bool tc_section_collapsed;
    int tc_section_y;

    int roof_event_scroll;
    int tile_change_scroll;
    SDL_Rect roof_list_rect;
    SDL_Rect tc_list_rect;

    StairEvent stair_events[MAX_STAIRS];
    int stair_event_count;
    int selected_stair;
    int stair_edit_field;
    char stair_input_buf[32];
    bool stair_section_collapsed;
    int stair_section_y;
    int stair_event_scroll;
    SDL_Rect stair_list_rect;

    WarpEvent warp_events[MAX_WARPS];
    int warp_event_count;
    int selected_warp;
    int warp_edit_field;            // 0=TriggerX, 1=TriggerY, 2=TargetMap, 3=TargetX, 4=TargetY, 5=Facing
    char warp_input_buf[64];        // для Target Map и совместимости
    bool warp_section_collapsed;
    int warp_section_y;
    int warp_event_scroll;
    SDL_Rect warp_list_rect;

    // Новые буферы для раздельного ввода координат и facing
    char warp_trigger_x_buf[12];     // макс 4 цифры
    char warp_trigger_y_buf[12];
    char warp_target_x_buf[12];
    char warp_target_y_buf[12];
    char warp_facing_buf[2];        // 1 цифра
} Editor;

void editor_init(Editor *ed);