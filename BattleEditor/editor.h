#pragma once
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "../cJSON.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define TILE_SIZE 48
#define MAP_TILE_SIZE 48

#define ALLY_COLS 4
#define ALLY_ROWS 4
#define ENEMY_COLS 4
#define ENEMY_ROWS 5
#define MAX_ALLIES (ALLY_COLS * ALLY_ROWS)
#define MAX_ENEMIES (ENEMY_COLS * ENEMY_ROWS)

#define ALLY_PANEL_X 10
#define ALLY_PANEL_Y 40
#define ENEMY_PANEL_X 10
#define ENEMY_PANEL_Y (ALLY_PANEL_Y + ALLY_ROWS * TILE_SIZE + 55)

#define SCROLLBAR_SIZE 16
#define VIEW_X (ALLY_PANEL_X + ALLY_COLS * TILE_SIZE + 30)
#define VIEW_Y 40
#define BATTLES_LIST_WIDTH 160
#define VIEW_W (WINDOW_WIDTH - VIEW_X - SCROLLBAR_SIZE - BATTLES_LIST_WIDTH)
#define VIEW_H (WINDOW_HEIGHT - VIEW_Y - SCROLLBAR_SIZE)

#define FALLBACK_MAP_W 12
#define FALLBACK_MAP_H 10

// --- инспектор (нижний левый угол) ---
#define INSPECTOR_W 220
#define INSPECTOR_H 60
#define INSPECTOR_X 10
#define INSPECTOR_Y (WINDOW_HEIGHT - INSPECTOR_H - 10)
// Кнопки переключения режима над картой
#define BTN_MAP_X       (VIEW_X + 10)
#define BTN_MAP_Y       5
#define BTN_MAP_W       60
#define BTN_MAP_H       24
#define BTN_BATTLE_X    (BTN_MAP_X + BTN_MAP_W + 10)
#define BTN_BATTLE_Y    5
#define BTN_BATTLE_W    60
#define BTN_BATTLE_H    24
#define BTN_TERRAIN_X   (BTN_BATTLE_X + BTN_BATTLE_W + 10)
#define BTN_TERRAIN_Y   5
#define BTN_TERRAIN_W   70
#define BTN_TERRAIN_H   24

#define BTN_SAVE_X      (BTN_TERRAIN_X + BTN_TERRAIN_W + 10)
#define BTN_SAVE_Y      5
#define BTN_SAVE_W      50
#define BTN_SAVE_H      24

typedef struct {
    int id;
    char name[64];
    char mapsprite[64];
    int x, y;
    SDL_Texture* tex;
    int used;
} UnitSlot;

typedef struct {
    char folder[64];
    char name[64];
    char map_id[64];
    char music[128];
    float music_volume;
    int battle_x;
    int battle_y;
    int battle_width;
    int battle_height;
} BattleEntry;

typedef struct {
    int w, h;
    int** tiles;
    int** rot;
    int** mirror_x;
    int** mirror_y;
    char tileset_path[256];
} MapLayout;

typedef struct {
    UnitSlot allies[MAX_ALLIES];
    UnitSlot enemies[MAX_ENEMIES];
    int selectedAllyId;
    int selectedEnemyId;
    int selectedSlot;
    int activePanel;
    cJSON* actorsRoot;
    cJSON* actorsArray;
    cJSON* enemiesRoot;
    cJSON* enemiesArray;
    int actorsCount;
    int enemiesCount;
    TTF_Font* font;
    SDL_Renderer* renderer;

    BattleEntry entries[64];
    int entryCount;
    int currentEntryIndex;
    MapLayout currentMap;

    SDL_Texture** tiles;
    int tile_count;

    float zoom;
    int camera_x, camera_y;
    int drawTileSize;

    int scrollbar_drag_h, scrollbar_drag_v;
    int scroll_drag_start_camera_x, scroll_drag_start_camera_y;
    int scroll_drag_mouse_x, scroll_drag_mouse_y;

    // инспектор
    int inspectorVisible;
    int inspectorPanel;      // 0 = ally, 1 = enemy
    int inspectorSlotIdx;
    Uint32 anim_counter;
    int viewMode;
    // Выделение зоны в глобальном режиме
    int selecting_zone;
    int sel_start_x, sel_start_y;
    int sel_current_x, sel_current_y;
    // hover и перетаскивание
    int hoveredAllyIdx;
    int hoveredEnemyIdx;
    int draggingUnit;         // 1 = перетаскиваем юнита
    int dragIsEnemy;
    int dragSlotIndex;
    int dragStartX, dragStartY;
    int battleListScroll;      // скролл списка битв    
    int terrainEditMode;      // 1 = режим редактирования местности
    int terrainPaintValue;    // выбранный тип: -1, 0 или 1
    int terrainPainting;      // зажата ли кнопка мыши для рисования
    int** terrain;            // [height][width] значения -1,0,1

} BattleEditor;

void init_editor(BattleEditor* ed, SDL_Renderer* ren);
void cleanup_editor(BattleEditor* ed);
void handle_input(BattleEditor* ed, SDL_Event* e);
void draw_ally_panel(BattleEditor* ed);
void draw_enemy_panel(BattleEditor* ed);
void draw_inspector(BattleEditor* ed);
void draw_map_area(BattleEditor* ed);
void switch_battle(BattleEditor* ed, int newIdx);
void set_unit_from_json(UnitSlot* unit, cJSON* array, int id);
void save_current_battle(BattleEditor* ed);