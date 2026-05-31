#include "editor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PALETTE_COLS 8

static void free_map_layout(MapLayout* map);
void recalc_tile_size(BattleEditor* ed);
void draw_map_area(BattleEditor* ed);
void switch_battle(BattleEditor* ed, int newIdx);
void cleanup_editor(BattleEditor* ed);

static const char* json_string(cJSON* item, const char* key) {
    cJSON* field = cJSON_GetObjectItem(item, key);
    return (field && cJSON_IsString(field)) ? field->valuestring : "";
}
static int json_int(cJSON* item, const char* key, int def) {
    cJSON* field = cJSON_GetObjectItem(item, key);
    return (field && cJSON_IsNumber(field)) ? field->valueint : def;
}

static int load_json_file(const char* path, cJSON** outRoot, cJSON** outArray, const char* arrayKey) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(len+1); fread(buf,1,len,f); buf[len]='\0'; fclose(f);
    cJSON* root = cJSON_Parse(buf); free(buf);
    if (!root) return 0;
    if (arrayKey && arrayKey[0] != '\0') {
        cJSON* arr = cJSON_GetObjectItem(root, arrayKey);
        if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(root); return 0; }
        if (outArray) *outArray = arr;
    } else { if (outArray) *outArray = NULL; }
    if (outRoot) *outRoot = root;
    return 1;
}

static void load_json_data(BattleEditor* ed) {
    if (load_json_file("../data/actors/actors.json", &ed->actorsRoot, &ed->actorsArray, "actors"))
        ed->actorsCount = cJSON_GetArraySize(ed->actorsArray);
    else { ed->actorsCount = 0; ed->actorsArray = NULL; }
    if (load_json_file("../data/enemies/enemies.json", &ed->enemiesRoot, &ed->enemiesArray, "enemies"))
        ed->enemiesCount = cJSON_GetArraySize(ed->enemiesArray);
    else { ed->enemiesCount = 0; ed->enemiesArray = NULL; }
}

static int load_battle_entries(BattleEditor* ed) {
    cJSON* root = NULL;
    if (!load_json_file("../data/battles/entries.json", &root, NULL, "")) { ed->entryCount = 0; return 0; }
    if (!cJSON_IsArray(root)) { cJSON_Delete(root); ed->entryCount = 0; return 0; }
    int count = cJSON_GetArraySize(root); if (count > 64) count = 64;
    ed->entryCount = count;
    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i); if (!item) continue;
        BattleEntry* be = &ed->entries[i];
        strcpy(be->folder, json_string(item, "folder"));
        strcpy(be->name,   json_string(item, "name"));
        strcpy(be->map_id, json_string(item, "map_id"));
        strcpy(be->music,  json_string(item, "music"));
        cJSON* vol = cJSON_GetObjectItem(item, "music_volume");
        be->music_volume = (vol && cJSON_IsNumber(vol)) ? (float)vol->valuedouble : 1.0f;

        // *** новые поля боевой зоны ***
        be->battle_x      = json_int(item, "battle_x", 0);
        be->battle_y      = json_int(item, "battle_y", 0);
        be->battle_width  = json_int(item, "battle_width", 0);
        be->battle_height = json_int(item, "battle_height", 0);
    }
    cJSON_Delete(root);
    return 1;
}

static void free_map_layout(MapLayout* map) {
    if (map->tiles) {
        for (int y = 0; y < map->h; y++) {
            free(map->tiles[y]);
            free(map->rot[y]);
            free(map->mirror_x[y]);
            free(map->mirror_y[y]);
        }
        free(map->tiles);
        free(map->rot);
        free(map->mirror_x);
        free(map->mirror_y);
        map->tiles = NULL;
        map->rot = NULL;
        map->mirror_x = NULL;
        map->mirror_y = NULL;
    }
}

static int load_map_layout(const char* map_id, MapLayout* map) {
    char path[256];
    snprintf(path, sizeof(path), "../data/maps/%s/layout.json", map_id);
    cJSON* root = NULL;
    if (!load_json_file(path, &root, NULL, "")) return 0;

    map->w = json_int(root, "width", 10);
    map->h = json_int(root, "height", 10);

    cJSON* tileset = cJSON_GetObjectItem(root, "tileset");
    if (tileset && cJSON_IsString(tileset))
        strcpy(map->tileset_path, tileset->valuestring);
    else
        map->tileset_path[0] = '\0';

    map->tiles = malloc(map->h * sizeof(int*));
    map->rot = malloc(map->h * sizeof(int*));
    map->mirror_x = malloc(map->h * sizeof(int*));
    map->mirror_y = malloc(map->h * sizeof(int*));

    if (!map->tiles || !map->rot || !map->mirror_x || !map->mirror_y) {
        cJSON_Delete(root);
        free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
        return 0;
    }

    for (int y = 0; y < map->h; y++) {
        map->tiles[y] = malloc(map->w * sizeof(int));
        map->rot[y] = malloc(map->w * sizeof(int));
        map->mirror_x[y] = malloc(map->w * sizeof(int));
        map->mirror_y[y] = malloc(map->w * sizeof(int));

        if (!map->tiles[y] || !map->rot[y] || !map->mirror_x[y] || !map->mirror_y[y]) {
            for (int j = 0; j < y; j++) {
                free(map->tiles[j]); free(map->rot[j]);
                free(map->mirror_x[j]); free(map->mirror_y[j]);
            }
            free(map->tiles); free(map->rot);
            free(map->mirror_x); free(map->mirror_y);
            map->tiles = NULL; map->rot = NULL;
            map->mirror_x = NULL; map->mirror_y = NULL;
            cJSON_Delete(root);
            return 0;
        }
    }

    cJSON* tilesArray = cJSON_GetObjectItem(root, "tiles");
    cJSON* rotArray   = cJSON_GetObjectItem(root, "rot");
    cJSON* mxArray    = cJSON_GetObjectItem(root, "mirror_x");
    cJSON* myArray    = cJSON_GetObjectItem(root, "mirror_y");

    for (int x = 0; x < map->w; x++) {
        cJSON* colT  = tilesArray ? cJSON_GetArrayItem(tilesArray, x) : NULL;
        cJSON* colR  = rotArray   ? cJSON_GetArrayItem(rotArray, x)   : NULL;
        cJSON* colMX = mxArray    ? cJSON_GetArrayItem(mxArray, x)    : NULL;
        cJSON* colMY = myArray    ? cJSON_GetArrayItem(myArray, x)    : NULL;

        for (int y = 0; y < map->h; y++) {
            map->tiles[y][x]     = (colT  && y < cJSON_GetArraySize(colT))  ? cJSON_GetArrayItem(colT, y)->valueint  : 0;
            map->rot[y][x]       = (colR  && y < cJSON_GetArraySize(colR))  ? cJSON_GetArrayItem(colR, y)->valueint  : 0;
            map->mirror_x[y][x]  = (colMX && y < cJSON_GetArraySize(colMX)) ? cJSON_IsTrue(cJSON_GetArrayItem(colMX, y)) : 0;
            map->mirror_y[y][x]  = (colMY && y < cJSON_GetArraySize(colMY)) ? cJSON_IsTrue(cJSON_GetArrayItem(colMY, y)) : 0;
        }
    }

    cJSON_Delete(root);
    return 1;
}

static void load_spriteset(BattleEditor* ed, const char* folder) {
    char path[256]; snprintf(path, sizeof(path), "../data/battles/%s/spriteset.json", folder);
    cJSON* root = NULL; if (!load_json_file(path, &root, NULL, "")) return;
    for (int i = 0; i < MAX_ALLIES; i++) { if (ed->allies[i].tex) SDL_DestroyTexture(ed->allies[i].tex); memset(&ed->allies[i], 0, sizeof(UnitSlot)); }
    for (int i = 0; i < MAX_ENEMIES; i++) { if (ed->enemies[i].tex) SDL_DestroyTexture(ed->enemies[i].tex); memset(&ed->enemies[i], 0, sizeof(UnitSlot)); }
    cJSON* allies = cJSON_GetObjectItem(root, "allies");
    if (allies && cJSON_IsArray(allies)) {
        int n = cJSON_GetArraySize(allies); if (n > MAX_ALLIES) n = MAX_ALLIES;
        for (int i = 0; i < n; i++) {
            cJSON* item = cJSON_GetArrayItem(allies, i); if (!item) continue;
            int id = json_int(item, "actor_id", -1);
            if (id < 0 || id >= ed->actorsCount) continue;
            cJSON* actor = cJSON_GetArrayItem(ed->actorsArray, id); if (!actor) continue;
            ed->allies[i].id = id;
            strcpy(ed->allies[i].name, json_string(actor, "name"));
            strcpy(ed->allies[i].mapsprite, json_string(actor, "mapsprite"));
            ed->allies[i].x = json_int(item, "x", 0);
            ed->allies[i].y = json_int(item, "y", 0);
            ed->allies[i].used = 1; ed->allies[i].tex = NULL;
        }
    }
    cJSON* enemies = cJSON_GetObjectItem(root, "enemies");
    if (enemies && cJSON_IsArray(enemies)) {
        int n = cJSON_GetArraySize(enemies); if (n > MAX_ENEMIES) n = MAX_ENEMIES;
        for (int i = 0; i < n; i++) {
            cJSON* item = cJSON_GetArrayItem(enemies, i); if (!item) continue;
            int id = json_int(item, "enemy_id", -1);
            if (id < 0 || id >= ed->enemiesCount) continue;
            cJSON* enemy = cJSON_GetArrayItem(ed->enemiesArray, id); if (!enemy) continue;
            ed->enemies[i].id = id;
            strcpy(ed->enemies[i].name, json_string(enemy, "name"));
            strcpy(ed->enemies[i].mapsprite, json_string(enemy, "mapsprite"));
            ed->enemies[i].x = json_int(item, "x", 0);
            ed->enemies[i].y = json_int(item, "y", 0);
            ed->enemies[i].used = 1; ed->enemies[i].tex = NULL;
        }
    }
    cJSON_Delete(root);
}

static void load_terrain(BattleEditor* ed, const char* folder) {
    // Освободим старый, если есть
    if (ed->terrain) {
        for (int y = 0; y < ed->currentMap.h; y++) free(ed->terrain[y]);
        free(ed->terrain);
        ed->terrain = NULL;
    }
    int w = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
    int h = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;
    ed->terrain = (int**)malloc(h * sizeof(int*));
    for (int y = 0; y < h; y++) {
        ed->terrain[y] = (int*)malloc(w * sizeof(int));
        for (int x = 0; x < w; x++) ed->terrain[y][x] = 1; // по умолчанию проходимо
    }
    char path[256];
    snprintf(path, sizeof(path), "../data/battles/%s/terrain.json", folder);
    cJSON* root = NULL;
    if (!load_json_file(path, &root, NULL, "")) return;
    if (!cJSON_IsArray(root)) { cJSON_Delete(root); return; }
    int rows = cJSON_GetArraySize(root);
    for (int y = 0; y < rows && y < h; y++) {
        cJSON* row = cJSON_GetArrayItem(root, y);
        if (!cJSON_IsArray(row)) continue;
        int cols = cJSON_GetArraySize(row);
        for (int x = 0; x < cols && x < w; x++) {
            cJSON* val = cJSON_GetArrayItem(row, x);
            if (cJSON_IsNumber(val)) ed->terrain[y][x] = val->valueint;
        }
    }
    cJSON_Delete(root);
}

static int load_tileset(BattleEditor* ed) { 
    if (ed->tiles) {
        for (int i = 0; i < ed->tile_count; i++) SDL_DestroyTexture(ed->tiles[i]);
        free(ed->tiles);
        ed->tiles = NULL;
        ed->tile_count = 0;
    }
    if (!ed->currentMap.tileset_path[0]) return 0;

    char full[512];
    snprintf(full, sizeof(full), "../%s", ed->currentMap.tileset_path);
    SDL_Surface* surface = IMG_Load(full);
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
                SDL_Surface* tile_surf = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
                SDL_BlitSurface(surface, &src, tile_surf, NULL);
                ed->tiles[idx++] = SDL_CreateTextureFromSurface(ed->renderer, tile_surf);
                SDL_FreeSurface(tile_surf);
            }
        }
    }
    SDL_FreeSurface(surface);
    return 1;
}


SDL_Texture* load_mapsprite(SDL_Renderer* ren, const char* name, int isEnemy) {
    char path[256]; snprintf(path, sizeof(path), isEnemy ? "../assets/mapsprites_enemy/%s.png" : "../assets/mapsprites/%s.png", name);
    SDL_Surface* s = IMG_Load(path); if (!s) return NULL;
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_FreeSurface(s); SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
    return t;
}

void init_editor(BattleEditor* ed, SDL_Renderer* ren) {
    memset(ed, 0, sizeof(BattleEditor));
    ed->renderer = ren;
    ed->font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 12);
    if (!ed->font) ed->font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", 12);
    if (!ed->font) { printf("No font\n"); exit(1); }
    ed->zoom = 1.0f;
    ed->inspectorVisible = 0;  // +++ изначально скрыт
    ed->anim_counter = 0;
    ed->viewMode = 1;
    ed->selecting_zone = 0;
    ed->hoveredAllyIdx = -1;
    ed->hoveredEnemyIdx = -1;
    ed->draggingUnit = 0;
    ed->terrainEditMode = 0;
    ed->terrainPaintValue = 1;
    ed->terrainPainting = 0;
    ed->battleListScroll = 0; 
    ed->saveFlashTimer = 0.0f;       
    ed->terrain = NULL;
    load_json_data(ed);
    if (ed->actorsCount > 0) ed->selectedAllyId = 0;
    if (ed->enemiesCount > 0) ed->selectedEnemyId = 0;
    if (load_battle_entries(ed) && ed->entryCount > 0) {
        ed->currentEntryIndex = 0;
        BattleEntry* entry = &ed->entries[0];
        if (load_map_layout(entry->map_id, &ed->currentMap)) {
            load_tileset(ed);
        } else { ed->currentMap.w = 0; ed->currentMap.h = 0; }
        load_spriteset(ed, entry->folder);
        load_terrain(ed, entry->folder);
    } else { ed->currentMap.w = 0; ed->currentMap.h = 0; }
    recalc_tile_size(ed);
}

static int get_anim_frame(BattleEditor* ed) {
    return (ed->anim_counter / 12) % 2;  // скорость: смена кадра
}

static void draw_slot_sprite(SDL_Renderer* r, TTF_Font* font, BattleEditor* ed, int x, int y, UnitSlot* sl, int sel, int isEnemy, int showNumber) {
    SDL_Rect rect = {x, y, TILE_SIZE, TILE_SIZE};
    SDL_SetRenderDrawColor(r, 60,60,60,255); SDL_RenderFillRect(r, &rect);
    if (sl->used) {
        if (!sl->tex) {
            sl->tex = load_mapsprite(r, sl->mapsprite, isEnemy);
        }
        if (sl->tex) {
            int frame = get_anim_frame(ed);
            SDL_Rect src = {frame * TILE_SIZE, 2 * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            SDL_RenderCopy(r, sl->tex, &src, &rect);
        }
    } else { SDL_SetRenderDrawColor(r, 80,80,80,255); SDL_RenderDrawRect(r, &rect); }
    if (sel) {
        SDL_SetRenderDrawColor(r, 255,255,0,255); SDL_RenderDrawRect(r, &rect);
        SDL_Rect outer = {x-2, y-2, TILE_SIZE+4, TILE_SIZE+4}; SDL_RenderDrawRect(r, &outer);
    }
    if (sl->used) {
        char buf[16];
        if (showNumber >= 0) {
            snprintf(buf, sizeof(buf), "%d", showNumber);
        } else {
            snprintf(buf, sizeof(buf), "%d", sl->id);
        }
        SDL_Surface* s = TTF_RenderText_Solid(font, buf, (SDL_Color){255,255,255});
        if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
            SDL_Rect d = {x+2, y+2, s->w, s->h}; SDL_RenderCopy(r, t, NULL, &d);
            SDL_FreeSurface(s); SDL_DestroyTexture(t); }
    }
}

void update_editor(BattleEditor* ed) {
    if (ed->saveFlashTimer > 0.0f) {
        ed->saveFlashTimer -= 0.016f;   // примерно 1 кадр при 60 FPS
        if (ed->saveFlashTimer < 0.0f) ed->saveFlashTimer = 0.0f;
    }
}

void draw_ally_panel(BattleEditor* ed) {
    int sx = ALLY_PANEL_X, sy = ALLY_PANEL_Y;
    SDL_Rect panel = {sx-5, sy-5, ALLY_COLS*TILE_SIZE+10, ALLY_ROWS*TILE_SIZE+55};
    SDL_SetRenderDrawColor(ed->renderer, 40,40,80,255); SDL_RenderFillRect(ed->renderer, &panel);
    SDL_SetRenderDrawColor(ed->renderer, 200,200,255,255); SDL_RenderDrawRect(ed->renderer, &panel);
    SDL_Color w = {255,255,255}; char buf[64];
    snprintf(buf, sizeof(buf), "Allies (sel ID: %d)", ed->selectedAllyId);
    SDL_Surface* ts = TTF_RenderText_Solid(ed->font, buf, w);

    if(ts){ SDL_Texture* tt=SDL_CreateTextureFromSurface(ed->renderer,ts);
        SDL_Rect d={sx+10,sy-25,ts->w,ts->h}; SDL_RenderCopy(ed->renderer,tt,NULL,&d);
        SDL_FreeSurface(ts); SDL_DestroyTexture(tt); }
    for(int row=0;row<ALLY_ROWS;row++) for(int col=0;col<ALLY_COLS;col++){
        int idx=row*ALLY_COLS+col;
        draw_slot_sprite(ed->renderer, ed->font, ed, sx+col*TILE_SIZE, sy+row*TILE_SIZE,
                  &ed->allies[idx], (ed->activePanel==0 && ed->selectedSlot==idx), 0, -1);

        if (ed->hoveredAllyIdx == idx) {
            SDL_Rect hoverRect = {sx+col*TILE_SIZE, sy+row*TILE_SIZE, TILE_SIZE, TILE_SIZE};
            SDL_SetRenderDrawColor(ed->renderer, 255, 255, 255, 200);
            SDL_RenderDrawRect(ed->renderer, &hoverRect);
        }
    }
    SDL_Rect add={sx, sy+ALLY_ROWS*TILE_SIZE+5,50,20}, del={sx+55, sy+ALLY_ROWS*TILE_SIZE+5,50,20};
    SDL_SetRenderDrawColor(ed->renderer,100,200,100,255); SDL_RenderFillRect(ed->renderer,&add);
    SDL_SetRenderDrawColor(ed->renderer,200,80,80,255); SDL_RenderFillRect(ed->renderer,&del);
    SDL_Color b = {0,0,0};
    ts=TTF_RenderText_Solid(ed->font,"Add",b); if(ts){SDL_Texture* tt=SDL_CreateTextureFromSurface(ed->renderer,ts);
        SDL_Rect d={add.x+8,add.y+2,ts->w,ts->h}; SDL_RenderCopy(ed->renderer,tt,NULL,&d); SDL_FreeSurface(ts); SDL_DestroyTexture(tt);}
    ts=TTF_RenderText_Solid(ed->font,"Del",b); if(ts){SDL_Texture* tt=SDL_CreateTextureFromSurface(ed->renderer,ts);
        SDL_Rect d={del.x+8,del.y+2,ts->w,ts->h}; SDL_RenderCopy(ed->renderer,tt,NULL,&d); SDL_FreeSurface(ts); SDL_DestroyTexture(tt);}
}

void draw_enemy_panel(BattleEditor* ed) {
    int sx = ENEMY_PANEL_X, sy = ENEMY_PANEL_Y;
    int pw = ENEMY_COLS*TILE_SIZE+10, ph = ENEMY_ROWS*TILE_SIZE+55;
    SDL_Rect panel = {sx-5, sy-5, pw, ph};
    SDL_SetRenderDrawColor(ed->renderer, 80,40,40,255); SDL_RenderFillRect(ed->renderer, &panel);
    SDL_SetRenderDrawColor(ed->renderer, 255,200,200,255); SDL_RenderDrawRect(ed->renderer, &panel);
    SDL_Color w = {255,255,255}; char buf[64];
    snprintf(buf, sizeof(buf), "Enemies (sel ID: %d)", ed->selectedEnemyId);
    SDL_Surface* ts = TTF_RenderText_Solid(ed->font, buf, w);
    if(ts){ SDL_Texture* tt=SDL_CreateTextureFromSurface(ed->renderer,ts);
        SDL_Rect d={sx+10,sy-25,ts->w,ts->h}; SDL_RenderCopy(ed->renderer,tt,NULL,&d);
        SDL_FreeSurface(ts); SDL_DestroyTexture(tt); }
        for(int row=0;row<ENEMY_ROWS;row++) for(int col=0;col<ENEMY_COLS;col++){
        int idx=row*ENEMY_COLS+col;
        draw_slot_sprite(ed->renderer, ed->font, ed, sx+col*TILE_SIZE, sy+row*TILE_SIZE,
                  &ed->enemies[idx], (ed->activePanel==1 && ed->selectedSlot==idx), 1, idx+1);
        if (ed->hoveredEnemyIdx == idx) {
            SDL_Rect hoverRect = {sx+col*TILE_SIZE, sy+row*TILE_SIZE, TILE_SIZE, TILE_SIZE};
            SDL_SetRenderDrawColor(ed->renderer, 255, 255, 255, 200);
            SDL_RenderDrawRect(ed->renderer, &hoverRect);
        }
    }
    SDL_Rect add={sx, sy+ENEMY_ROWS*TILE_SIZE+5,50,20}, del={sx+55, sy+ENEMY_ROWS*TILE_SIZE+5,50,20};
    SDL_SetRenderDrawColor(ed->renderer,100,200,100,255); SDL_RenderFillRect(ed->renderer,&add);
    SDL_SetRenderDrawColor(ed->renderer,200,80,80,255); SDL_RenderFillRect(ed->renderer,&del);
    SDL_Color b = {0,0,0};
    ts=TTF_RenderText_Solid(ed->font,"Add",b); if(ts){SDL_Texture* tt=SDL_CreateTextureFromSurface(ed->renderer,ts);
        SDL_Rect d={add.x+8,add.y+2,ts->w,ts->h}; SDL_RenderCopy(ed->renderer,tt,NULL,&d); SDL_FreeSurface(ts); SDL_DestroyTexture(tt);}
    ts=TTF_RenderText_Solid(ed->font,"Del",b); if(ts){SDL_Texture* tt=SDL_CreateTextureFromSurface(ed->renderer,ts);
        SDL_Rect d={del.x+8,del.y+2,ts->w,ts->h}; SDL_RenderCopy(ed->renderer,tt,NULL,&d); SDL_FreeSurface(ts); SDL_DestroyTexture(tt);}
}

// +++ отрисовка инспектора
void draw_inspector(BattleEditor* ed) {
    if (!ed->inspectorVisible) return;
    SDL_Rect bg = {INSPECTOR_X, INSPECTOR_Y, INSPECTOR_W, INSPECTOR_H};
    SDL_SetRenderDrawColor(ed->renderer, 30, 30, 30, 240);
    SDL_RenderFillRect(ed->renderer, &bg);
    SDL_SetRenderDrawColor(ed->renderer, 180, 180, 200, 255);
    SDL_RenderDrawRect(ed->renderer, &bg);

    UnitSlot* slot = ed->inspectorPanel == 0 ? &ed->allies[ed->inspectorSlotIdx] : &ed->enemies[ed->inspectorSlotIdx];
    char line[128];
    snprintf(line, sizeof(line), "ID: %d   %s", slot->id, slot->name);
    SDL_Surface* ts = TTF_RenderText_Solid(ed->font, line, (SDL_Color){255,255,255});
    if (ts) {
        SDL_Texture* tt = SDL_CreateTextureFromSurface(ed->renderer, ts);
        SDL_Rect dst = {INSPECTOR_X + 50, INSPECTOR_Y + 10, ts->w, ts->h};
        SDL_RenderCopy(ed->renderer, tt, NULL, &dst);
        SDL_FreeSurface(ts);
        SDL_DestroyTexture(tt);
    }

    // кнопки
    SDL_Rect btnLeft  = {INSPECTOR_X + 10, INSPECTOR_Y + 15, 30, 30};
    SDL_Rect btnRight = {INSPECTOR_X + INSPECTOR_W - 40, INSPECTOR_Y + 15, 30, 30};
    SDL_SetRenderDrawColor(ed->renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(ed->renderer, &btnLeft);
    SDL_RenderFillRect(ed->renderer, &btnRight);
    SDL_SetRenderDrawColor(ed->renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(ed->renderer, &btnLeft);
    SDL_RenderDrawRect(ed->renderer, &btnRight);

    // стрелки текстом
    SDL_Surface* leftS = TTF_RenderText_Solid(ed->font, "<", (SDL_Color){255,255,255});
    SDL_Surface* rightS = TTF_RenderText_Solid(ed->font, ">", (SDL_Color){255,255,255});
    if (leftS) {
        SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, leftS);
        SDL_Rect d = {btnLeft.x+10, btnLeft.y+5, leftS->w, leftS->h};
        SDL_RenderCopy(ed->renderer, t, NULL, &d);
        SDL_FreeSurface(leftS); SDL_DestroyTexture(t);
    }
    if (rightS) {
        SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, rightS);
        SDL_Rect d = {btnRight.x+10, btnRight.y+5, rightS->w, rightS->h};
        SDL_RenderCopy(ed->renderer, t, NULL, &d);
        SDL_FreeSurface(rightS); SDL_DestroyTexture(t);
    }
}

void recalc_tile_size(BattleEditor* ed) {
    int mw = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
    int mh = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;
    float base = (float)VIEW_W / mw < (float)VIEW_H / mh ? (float)VIEW_W / mw : (float)VIEW_H / mh;
    ed->drawTileSize = (int)(base * ed->zoom);
    if (ed->drawTileSize < 4) ed->drawTileSize = 4;
}


void draw_map_area(BattleEditor* ed) {
    int ts = ed->drawTileSize;
    int mx = VIEW_X, my = VIEW_Y;
    int mw = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
    int mh = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;
    int** tiles = (ed->currentMap.w > 0 && ed->currentMap.tiles) ? ed->currentMap.tiles : NULL;

    BattleEntry* entry = NULL;
    if (ed->entryCount > 0 && ed->currentEntryIndex >= 0 && ed->currentEntryIndex < ed->entryCount)
        entry = &ed->entries[ed->currentEntryIndex];

    // ======= Кнопки Map / Battle / Terrain с подсветкой активной =======
    SDL_Rect btnMap     = {BTN_MAP_X, BTN_MAP_Y, BTN_MAP_W, BTN_MAP_H};
    SDL_Rect btnBattle  = {BTN_BATTLE_X, BTN_BATTLE_Y, BTN_BATTLE_W, BTN_BATTLE_H};
    SDL_Rect btnTerrain = {BTN_TERRAIN_X, BTN_TERRAIN_Y, BTN_TERRAIN_W, BTN_TERRAIN_H};

    SDL_Color activeColor        = {140, 140, 200, 255};
    SDL_Color inactiveColor      = {60, 60, 60, 255};
    SDL_Color activeTerrainColor = {140, 200, 140, 255};
    SDL_Color borderActive       = {255, 255, 255, 255};
    SDL_Color borderInactive     = {150, 150, 150, 255};

    int currentMode = ed->terrainEditMode ? 2 : ed->viewMode;

    // Map
    SDL_Color fill = (currentMode == 0) ? activeColor : inactiveColor;
    SDL_SetRenderDrawColor(ed->renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(ed->renderer, &btnMap);
    SDL_Color border = (currentMode == 0) ? borderActive : borderInactive;
    SDL_SetRenderDrawColor(ed->renderer, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(ed->renderer, &btnMap);

    // Battle
    fill = (currentMode == 1) ? activeColor : inactiveColor;
    SDL_SetRenderDrawColor(ed->renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(ed->renderer, &btnBattle);
    border = (currentMode == 1) ? borderActive : borderInactive;
    SDL_SetRenderDrawColor(ed->renderer, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(ed->renderer, &btnBattle);

    // Terrain
    fill = (currentMode == 2) ? activeTerrainColor : inactiveColor;
    SDL_SetRenderDrawColor(ed->renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(ed->renderer, &btnTerrain);
    border = (currentMode == 2) ? borderActive : borderInactive;
    SDL_SetRenderDrawColor(ed->renderer, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(ed->renderer, &btnTerrain);

    // Текст на кнопках
    SDL_Color btnTextCol = {255,255,255};
    SDL_Surface* btnSurf = TTF_RenderText_Solid(ed->font, "Map", btnTextCol);
    if(btnSurf) { SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, btnSurf);
        SDL_Rect d = {btnMap.x+10, btnMap.y+4, btnSurf->w, btnSurf->h}; SDL_RenderCopy(ed->renderer, t, NULL, &d);
        SDL_FreeSurface(btnSurf); SDL_DestroyTexture(t); }
    btnSurf = TTF_RenderText_Solid(ed->font, "Battle", btnTextCol);
    if(btnSurf) { SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, btnSurf);
        SDL_Rect d = {btnBattle.x+4, btnBattle.y+4, btnSurf->w, btnSurf->h}; SDL_RenderCopy(ed->renderer, t, NULL, &d);
        SDL_FreeSurface(btnSurf); SDL_DestroyTexture(t); }

    btnSurf = TTF_RenderText_Solid(ed->font, "Terrain", btnTextCol);
    if(btnSurf) { SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, btnSurf);
        SDL_Rect d = {btnTerrain.x+6, btnTerrain.y+4, btnSurf->w, btnSurf->h};
        SDL_RenderCopy(ed->renderer, t, NULL, &d);
        SDL_FreeSurface(btnSurf); SDL_DestroyTexture(t); }

    // Save button

    {
        SDL_Rect btnSave = {BTN_SAVE_X, BTN_SAVE_Y, BTN_SAVE_W, BTN_SAVE_H};
        if (ed->saveFlashTimer > 0.0f) {
            SDL_SetRenderDrawColor(ed->renderer, 0, 200, 0, 255);  // зелёный
        } else {
            SDL_SetRenderDrawColor(ed->renderer, 200, 140, 80, 255); // обычный
        }
        SDL_RenderFillRect(ed->renderer, &btnSave);
        SDL_SetRenderDrawColor(ed->renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(ed->renderer, &btnSave);
        SDL_Surface* saveSurf = TTF_RenderText_Solid(ed->font, "Save", btnTextCol);
        if(saveSurf) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, saveSurf);
            SDL_Rect d = {btnSave.x+6, btnSave.y+4, saveSurf->w, saveSurf->h};
            SDL_RenderCopy(ed->renderer, t, NULL, &d);
            SDL_FreeSurface(saveSurf);
            SDL_DestroyTexture(t);
        }
    }

    if (ed->terrainEditMode) {
    // Селекторы теперь после кнопки Save
    int selStartX = BTN_ZOOM_OUT_X + BTN_ZOOM_OUT_W + 10;
        SDL_Rect tBtnBlocked = {selStartX, BTN_TERRAIN_Y, 24, 24};
        SDL_Rect tBtnFly     = {selStartX + 24 + 5, BTN_TERRAIN_Y, 24, 24};
        SDL_Rect tBtnWalk    = {selStartX + 24 + 5 + 24 + 5, BTN_TERRAIN_Y, 24, 24};

        SDL_Color cols[3] = {
            {255, 80, 80, 255},   // -1 Blocked
            {255, 255, 80, 255},  //  0 Fly only
            {80, 255, 80, 255}    //  1 Walkable
        };

        // Рисуем квадраты
        SDL_SetRenderDrawColor(ed->renderer, cols[0].r, cols[0].g, cols[0].b, 255);
        SDL_RenderFillRect(ed->renderer, &tBtnBlocked);
        SDL_SetRenderDrawColor(ed->renderer, cols[1].r, cols[1].g, cols[1].b, 255);
        SDL_RenderFillRect(ed->renderer, &tBtnFly);
        SDL_SetRenderDrawColor(ed->renderer, cols[2].r, cols[2].g, cols[2].b, 255);
        SDL_RenderFillRect(ed->renderer, &tBtnWalk);

        // Рамка вокруг активного
        int val = ed->terrainPaintValue;
        if (val == -1) {
            SDL_SetRenderDrawColor(ed->renderer, 255,255,255,255);
            SDL_RenderDrawRect(ed->renderer, &tBtnBlocked);
            SDL_Rect r = {tBtnBlocked.x-2, tBtnBlocked.y-2, 28, 28};
            SDL_RenderDrawRect(ed->renderer, &r);
        } else if (val == 0) {
            SDL_SetRenderDrawColor(ed->renderer, 255,255,255,255);
            SDL_RenderDrawRect(ed->renderer, &tBtnFly);
            SDL_Rect r = {tBtnFly.x-2, tBtnFly.y-2, 28, 28};
            SDL_RenderDrawRect(ed->renderer, &r);
        } else {
            SDL_SetRenderDrawColor(ed->renderer, 255,255,255,255);
            SDL_RenderDrawRect(ed->renderer, &tBtnWalk);
            SDL_Rect r = {tBtnWalk.x-2, tBtnWalk.y-2, 28, 28};
            SDL_RenderDrawRect(ed->renderer, &r);
        }

        // Буквы B / F / W
        SDL_Color white = {255,255,255};
        SDL_Surface* s = TTF_RenderText_Solid(ed->font, "B", white);
        if(s) { SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, s);
            SDL_Rect d = {tBtnBlocked.x+6, tBtnBlocked.y+4, s->w, s->h}; SDL_RenderCopy(ed->renderer, t, NULL, &d); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
        s = TTF_RenderText_Solid(ed->font, "F", white);
        if(s) { SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, s);
            SDL_Rect d = {tBtnFly.x+6, tBtnFly.y+4, s->w, s->h}; SDL_RenderCopy(ed->renderer, t, NULL, &d); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
        s = TTF_RenderText_Solid(ed->font, "W", white);
        if(s) { SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, s);
            SDL_Rect d = {tBtnWalk.x+6, tBtnWalk.y+4, s->w, s->h}; SDL_RenderCopy(ed->renderer, t, NULL, &d); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
    }

    // === Отображение координат курсора и юнита под ним ===
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    if (mouseX >= VIEW_X && mouseX < VIEW_X + VIEW_W && mouseY >= VIEW_Y && mouseY < VIEW_Y + VIEW_H) {
        int tx = ed->camera_x + (mouseX - VIEW_X) / ts;
        int ty = ed->camera_y + (mouseY - VIEW_Y) / ts;
        char infoText[256];
        if (ed->viewMode == 1)
            snprintf(infoText, sizeof(infoText), "Cursor: %d, %d", tx, ty);
        else
            snprintf(infoText, sizeof(infoText), "Cursor (map): %d, %d", tx, ty);

        // Вычисляем info_x перед первым использованием
        int info_x;
        if (ed->terrainEditMode) {
            // После зум-кнопок + селекторов (3 шт. по 24, 2 промежутка по 5) + отступ
            info_x = BTN_ZOOM_OUT_X + BTN_ZOOM_OUT_W + 10 + 24*3 + 5*2 + 15;
        } else {
            // Правее зум-кнопок
            info_x = BTN_ZOOM_OUT_X + BTN_ZOOM_OUT_W + 15;
        }

        SDL_Surface* s = TTF_RenderText_Solid(ed->font, infoText, (SDL_Color){255,255,0});
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, s);
            SDL_Rect d = { info_x, 5, s->w, s->h };
            SDL_RenderCopy(ed->renderer, t, NULL, &d);
            SDL_FreeSurface(s); SDL_DestroyTexture(t);
        }

        // Информация о юните или местности под мышью
        if (ed->terrainEditMode) {
            int tx = ed->camera_x + (mouseX - VIEW_X) / ts;
            int ty = ed->camera_y + (mouseY - VIEW_Y) / ts;
            int bx = (ed->viewMode == 1 && entry) ? entry->battle_x : 0;
            int by = (ed->viewMode == 1 && entry) ? entry->battle_y : 0;
            int map_x = tx + bx;
            int map_y = ty + by;
            if (map_x >= 0 && map_x < ed->currentMap.w && map_y >= 0 && map_y < ed->currentMap.h) {
                int t = ed->terrain[map_y][map_x];
                const char* terrainName = (t == -1) ? "Blocked" : ((t == 0) ? "Fly only" : "Walkable");
                snprintf(infoText, sizeof(infoText), "Terrain: %s (%d)", terrainName, t);
            } else {
                infoText[0] = '\0';
            }
        } else if (ed->hoveredAllyIdx >= 0) {
            UnitSlot* un = &ed->allies[ed->hoveredAllyIdx];
            snprintf(infoText, sizeof(infoText), "Ally: %s (ID:%d)  %d,%d", un->name, un->id, un->x, un->y);
        } else if (ed->hoveredEnemyIdx >= 0) {
            UnitSlot* un = &ed->enemies[ed->hoveredEnemyIdx];
            snprintf(infoText, sizeof(infoText), "Enemy: %s (ID:%d)  %d,%d", un->name, un->id, un->x, un->y);
        } else {
            infoText[0] = '\0';
        }
        if (infoText[0]) {
            s = TTF_RenderText_Solid(ed->font, infoText, (SDL_Color){255,255,0});
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, s);
                SDL_Rect d = { info_x, 30, s->w, s->h };
                SDL_RenderCopy(ed->renderer, t, NULL, &d);
                SDL_FreeSurface(s); SDL_DestroyTexture(t);
            }
        }
    }

    // Кнопки Zoom In / Zoom Out
    {
        SDL_Rect btnZoomIn  = {BTN_ZOOM_IN_X, BTN_ZOOM_IN_Y, BTN_ZOOM_IN_W, BTN_ZOOM_IN_H};
        SDL_Rect btnZoomOut = {BTN_ZOOM_OUT_X, BTN_ZOOM_OUT_Y, BTN_ZOOM_OUT_W, BTN_ZOOM_OUT_H};

        SDL_SetRenderDrawColor(ed->renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(ed->renderer, &btnZoomIn);
        SDL_RenderFillRect(ed->renderer, &btnZoomOut);
        SDL_SetRenderDrawColor(ed->renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(ed->renderer, &btnZoomIn);
        SDL_RenderDrawRect(ed->renderer, &btnZoomOut);

        SDL_Surface* sIn  = TTF_RenderText_Solid(ed->font, "+", (SDL_Color){255,255,255});
        SDL_Surface* sOut = TTF_RenderText_Solid(ed->font, "-", (SDL_Color){255,255,255});
        if (sIn) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, sIn);
            SDL_Rect d = {btnZoomIn.x + 8, btnZoomIn.y + 4, sIn->w, sIn->h};
            SDL_RenderCopy(ed->renderer, t, NULL, &d);
            SDL_FreeSurface(sIn);
            SDL_DestroyTexture(t);
        }
        if (sOut) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(ed->renderer, sOut);
            SDL_Rect d = {btnZoomOut.x + 8, btnZoomOut.y + 4, sOut->w, sOut->h};
            SDL_RenderCopy(ed->renderer, t, NULL, &d);
            SDL_FreeSurface(sOut);
            SDL_DestroyTexture(t);
        }
    }

    // ========== БОЕВОЙ РЕЖИМ ==========
    if (ed->viewMode == 1) {
        int bw = entry ? entry->battle_width : mw;
        int bh = entry ? entry->battle_height : mh;
        if (bw <= 0) bw = mw;
        if (bh <= 0) bh = mh;

        int visW = VIEW_W / ts; if (visW > bw) visW = bw;
        int visH = VIEW_H / ts; if (visH > bh) visH = bh;
        if (ed->camera_x > bw - visW) ed->camera_x = bw - visW;
        if (ed->camera_x < 0) ed->camera_x = 0;
        if (ed->camera_y > bh - visH) ed->camera_y = bh - visH;
        if (ed->camera_y < 0) ed->camera_y = 0;

        int bx = entry ? entry->battle_x : 0;
        int by = entry ? entry->battle_y : 0;

        // Тайлы зоны
        for (int y = 0; y < visH && (y + ed->camera_y) < bh; y++) {
            for (int x = 0; x < visW && (x + ed->camera_x) < bw; x++) {
                int tile_id = tiles ? tiles[y + ed->camera_y + by][x + ed->camera_x + bx] : 0;
                if (tile_id < 0) continue;
                SDL_Rect dest = {mx + x * ts, my + y * ts, ts, ts};
                if (ed->tiles && tile_id >= 0 && tile_id < ed->tile_count && ed->tiles[tile_id]) {
                    int rot = ed->currentMap.rot[y + ed->camera_y + by][x + ed->camera_x + bx];
                    int mirrorX = ed->currentMap.mirror_x[y + ed->camera_y + by][x + ed->camera_x + bx];
                    int mirrorY = ed->currentMap.mirror_y[y + ed->camera_y + by][x + ed->camera_x + bx];
                    double angle = rot * 90.0;
                    SDL_RendererFlip flip = SDL_FLIP_NONE;
                    if (mirrorX) flip |= SDL_FLIP_HORIZONTAL;
                    if (mirrorY) flip |= SDL_FLIP_VERTICAL;
                    SDL_Point center = { ts/2, ts/2 };
                    SDL_RenderCopyEx(ed->renderer, ed->tiles[tile_id], NULL, &dest, angle, &center, flip);
                } else {
                    SDL_SetRenderDrawColor(ed->renderer, (tile_id*37+50)%256, (tile_id*71+80)%256, (tile_id*13+120)%256, 255);
                    SDL_RenderFillRect(ed->renderer, &dest);
                }
            }
        }

        // Сетка
        SDL_SetRenderDrawColor(ed->renderer, 80,80,80,40);
        for (int i = 0; i <= visW && (i + ed->camera_x) <= bw; i++)
            SDL_RenderDrawLine(ed->renderer, mx + i * ts, my, mx + i * ts, my + visH * ts);
        for (int i = 0; i <= visH && (i + ed->camera_y) <= bh; i++)
            SDL_RenderDrawLine(ed->renderer, mx, my + i * ts, mx + visW * ts, my + i * ts);

        // ====== Местность или юниты ======
        if (ed->terrainEditMode) {
            for (int y = 0; y < visH && (y + ed->camera_y) < bh; y++) {
                for (int x = 0; x < visW && (x + ed->camera_x) < bw; x++) {
                    int tx = x + ed->camera_x + bx;
                    int ty = y + ed->camera_y + by;
                    int t = ed->terrain[ty][tx];
                    SDL_Color col;
                    if (t == -1)      col = (SDL_Color){255, 80, 80, 255};   // красный
                    else if (t == 0)  col = (SDL_Color){255, 255, 80, 255};  // желтый
                    else              col = (SDL_Color){80, 255, 80, 255};    // зеленый
                    int indicatorSize = ts / 4;
                    if (indicatorSize < 4) indicatorSize = 4;
                    SDL_Rect marker = {mx + x * ts + ts - indicatorSize, my + y * ts + ts - indicatorSize, indicatorSize, indicatorSize};
                    SDL_SetRenderDrawColor(ed->renderer, col.r, col.g, col.b, col.a);
                    SDL_RenderFillRect(ed->renderer, &marker);
                }
            }
        }

        else {
            for (int i = 0; i < MAX_ALLIES; i++) {
                if (!ed->allies[i].used) continue;
                int ux = ed->allies[i].x - ed->camera_x, uy = ed->allies[i].y - ed->camera_y;
                if (ux >= 0 && ux < visW && uy >= 0 && uy < visH) {
                    if (!ed->allies[i].tex) ed->allies[i].tex = load_mapsprite(ed->renderer, ed->allies[i].mapsprite, 0);
                    SDL_Rect dst = {mx + ux * ts, my + uy * ts, ts, ts};
                    int frame = get_anim_frame(ed);
                    SDL_Rect src = {frame * TILE_SIZE, 2 * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                    if (ed->allies[i].tex) {
                        SDL_RenderCopy(ed->renderer, ed->allies[i].tex, &src, &dst);
                        if (ed->hoveredAllyIdx == i) {
                            SDL_Rect hoverRect = {mx + ux * ts - 2, my + uy * ts - 2, ts+4, ts+4};
                            SDL_SetRenderDrawColor(ed->renderer, 255,255,255,200);
                            SDL_RenderDrawRect(ed->renderer, &hoverRect);
                        }
                    } else { SDL_SetRenderDrawColor(ed->renderer,0,255,0,255); SDL_RenderFillRect(ed->renderer,&dst); }
        // Рамка выделения выбранного в панели союзника
        if (ed->activePanel == 0 && ed->selectedSlot == i && !ed->terrainEditMode) {
            SDL_Rect selRect = {mx + ux * ts - 2, my + uy * ts - 2, ts+4, ts+4};
            SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
            SDL_RenderDrawRect(ed->renderer, &selRect);
        }
    }
}
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (!ed->enemies[i].used) continue;
                int ux = ed->enemies[i].x - ed->camera_x, uy = ed->enemies[i].y - ed->camera_y;
                if (ux >= 0 && ux < visW && uy >= 0 && uy < visH) {
                    if (!ed->enemies[i].tex) ed->enemies[i].tex = load_mapsprite(ed->renderer, ed->enemies[i].mapsprite, 1);
                    SDL_Rect dst = {mx + ux * ts, my + uy * ts, ts, ts};
                    int frame = get_anim_frame(ed);
                    SDL_Rect src = {frame * TILE_SIZE, 2 * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                    if (ed->enemies[i].tex) {
                        SDL_RenderCopy(ed->renderer, ed->enemies[i].tex, &src, &dst);
                        if (ed->hoveredEnemyIdx == i) {
                            SDL_Rect hoverRect = {mx + ux * ts - 2, my + uy * ts - 2, ts+4, ts+4};
                            SDL_SetRenderDrawColor(ed->renderer, 255,255,255,200);
                            SDL_RenderDrawRect(ed->renderer, &hoverRect);
                        }
                    } else { SDL_SetRenderDrawColor(ed->renderer,255,0,0,255); SDL_RenderFillRect(ed->renderer,&dst); }
                // Рамка выделения выбранного в панели врага
                if (ed->activePanel == 1 && ed->selectedSlot == i && !ed->terrainEditMode) {
                    SDL_Rect selRect = {mx + ux * ts - 2, my + uy * ts - 2, ts+4, ts+4};
                    SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                    SDL_RenderDrawRect(ed->renderer, &selRect);
                    }
                }
            }
        }

        // Полосы прокрутки
        if (bw * ts > VIEW_W) {
            int track_x = mx, track_y = my + VIEW_H;
            int track_w = VIEW_W, track_h = SCROLLBAR_SIZE;
            SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
            SDL_RenderFillRect(ed->renderer, &(SDL_Rect){track_x, track_y, track_w, track_h});
            float thumb_w = (float)VIEW_W / (bw * ts) * VIEW_W;
            if (thumb_w < 8) thumb_w = 8;
            float max_cam_x = bw - visW;
            float thumb_x = max_cam_x > 0 ? track_x + (float)ed->camera_x / max_cam_x * (VIEW_W - thumb_w) : track_x;
            SDL_Rect thumb = { (int)thumb_x, track_y+2, (int)thumb_w, SCROLLBAR_SIZE-4 };
            SDL_SetRenderDrawColor(ed->renderer, 200,200,200,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }
        if (bh * ts > VIEW_H) {
            int track_x = mx + VIEW_W, track_y = my;
            int track_w = SCROLLBAR_SIZE, track_h = VIEW_H;
            SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
            SDL_RenderFillRect(ed->renderer, &(SDL_Rect){track_x, track_y, track_w, track_h});
            float thumb_h = (float)VIEW_H / (bh * ts) * VIEW_H;
            if (thumb_h < 8) thumb_h = 8;
            float max_cam_y = bh - visH;
            float thumb_y = max_cam_y > 0 ? track_y + (float)ed->camera_y / max_cam_y * (VIEW_H - thumb_h) : track_y;
            SDL_Rect thumb = { track_x+2, (int)thumb_y, SCROLLBAR_SIZE-4, (int)thumb_h };
            SDL_SetRenderDrawColor(ed->renderer, 200,200,200,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }

    } else {
        // ========== ГЛОБАЛЬНЫЙ РЕЖИМ ==========
        int visW = VIEW_W / ts; if (visW > mw) visW = mw;
        int visH = VIEW_H / ts; if (visH > mh) visH = mh;
        if (ed->camera_x > mw - visW) ed->camera_x = mw - visW;
        if (ed->camera_x < 0) ed->camera_x = 0;
        if (ed->camera_y > mh - visH) ed->camera_y = mh - visH;
        if (ed->camera_y < 0) ed->camera_y = 0;

        // Тайлы всей карты
        for (int y = 0; y < visH && (y + ed->camera_y) < mh; y++) {
            for (int x = 0; x < visW && (x + ed->camera_x) < mw; x++) {
                int tile_id = tiles ? tiles[y + ed->camera_y][x + ed->camera_x] : 0;
                if (tile_id < 0) continue;
                SDL_Rect dest = {mx + x * ts, my + y * ts, ts, ts};
                if (ed->tiles && tile_id >= 0 && tile_id < ed->tile_count && ed->tiles[tile_id]) {
                    int rot = ed->currentMap.rot[y + ed->camera_y][x + ed->camera_x];
                    int mirrorX = ed->currentMap.mirror_x[y + ed->camera_y][x + ed->camera_x];
                    int mirrorY = ed->currentMap.mirror_y[y + ed->camera_y][x + ed->camera_x];
                    double angle = rot * 90.0;
                    SDL_RendererFlip flip = SDL_FLIP_NONE;
                    if (mirrorX) flip |= SDL_FLIP_HORIZONTAL;
                    if (mirrorY) flip |= SDL_FLIP_VERTICAL;
                    SDL_Point center = { ts/2, ts/2 };
                    SDL_RenderCopyEx(ed->renderer, ed->tiles[tile_id], NULL, &dest, angle, &center, flip);
                } else {
                    SDL_SetRenderDrawColor(ed->renderer, (tile_id*37+50)%256, (tile_id*71+80)%256, (tile_id*13+120)%256, 255);
                    SDL_RenderFillRect(ed->renderer, &dest);
                }
            }
        }

        // Сетка
        SDL_SetRenderDrawColor(ed->renderer, 80,80,80,40);
        for (int i = 0; i <= visW && (i + ed->camera_x) <= mw; i++)
            SDL_RenderDrawLine(ed->renderer, mx + i * ts, my, mx + i * ts, my + visH * ts);
        for (int i = 0; i <= visH && (i + ed->camera_y) <= mh; i++)
            SDL_RenderDrawLine(ed->renderer, mx, my + i * ts, mx + visW * ts, my + i * ts);

        // Местность или юпиты
        int bx = entry ? entry->battle_x : 0;
        int by = entry ? entry->battle_y : 0;

        if (ed->terrainEditMode) {
            for (int y = 0; y < visH && (y + ed->camera_y) < mh; y++) {
                for (int x = 0; x < visW && (x + ed->camera_x) < mw; x++) {
                    int tx = x + ed->camera_x;
                    int ty = y + ed->camera_y;
                    int t = ed->terrain[ty][tx];
                    SDL_Color col;
                    if (t == -1)      col = (SDL_Color){255, 80, 80, 255};
                    else if (t == 0)  col = (SDL_Color){255, 255, 80, 255};
                    else              col = (SDL_Color){80, 255, 80, 255};
                    int indicatorSize = ts / 4;
                    if (indicatorSize < 4) indicatorSize = 4;
                    SDL_Rect marker = {mx + x * ts + ts - indicatorSize, my + y * ts + ts - indicatorSize, indicatorSize, indicatorSize};
                    SDL_SetRenderDrawColor(ed->renderer, col.r, col.g, col.b, col.a);
                    SDL_RenderFillRect(ed->renderer, &marker);
                }
            }
        }

      else {
            for (int i = 0; i < MAX_ALLIES; i++) {
                if (!ed->allies[i].used) continue;
                int gx = ed->allies[i].x + bx;
                int gy = ed->allies[i].y + by;
                int ux = gx - ed->camera_x, uy = gy - ed->camera_y;
                if (ux >= 0 && ux < visW && uy >= 0 && uy < visH) {
                    if (!ed->allies[i].tex) ed->allies[i].tex = load_mapsprite(ed->renderer, ed->allies[i].mapsprite, 0);
                    SDL_Rect dst = {mx + ux * ts, my + uy * ts, ts, ts};
                    int frame = get_anim_frame(ed);
                    SDL_Rect src = {frame * TILE_SIZE, 2 * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                    if (ed->allies[i].tex) {
                        SDL_RenderCopy(ed->renderer, ed->allies[i].tex, &src, &dst);
                        if (ed->hoveredAllyIdx == i) {
                            SDL_Rect hoverRect = {mx + ux * ts, my + uy * ts, ts, ts};
                            SDL_SetRenderDrawColor(ed->renderer, 255,255,255,200);
                            SDL_RenderDrawRect(ed->renderer, &hoverRect);
                    }
                } else { SDL_SetRenderDrawColor(ed->renderer,0,255,0,255); SDL_RenderFillRect(ed->renderer,&dst); }
                    if (ed->activePanel == 0 && ed->selectedSlot == i && !ed->terrainEditMode) {
                    SDL_Rect selRect = {mx + ux * ts - 2, my + uy * ts - 2, ts+4, ts+4};
                    SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                    SDL_RenderDrawRect(ed->renderer, &selRect);
                    }
                }
            }
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (!ed->enemies[i].used) continue;
                int gx = ed->enemies[i].x + bx;
                int gy = ed->enemies[i].y + by;
                int ux = gx - ed->camera_x, uy = gy - ed->camera_y;
                if (ux >= 0 && ux < visW && uy >= 0 && uy < visH) {
                    if (!ed->enemies[i].tex) ed->enemies[i].tex = load_mapsprite(ed->renderer, ed->enemies[i].mapsprite, 1);
                    SDL_Rect dst = {mx + ux * ts, my + uy * ts, ts, ts};
                    int frame = get_anim_frame(ed);
                    SDL_Rect src = {frame * TILE_SIZE, 2 * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                    if (ed->enemies[i].tex) {
                        SDL_RenderCopy(ed->renderer, ed->enemies[i].tex, &src, &dst);
                        if (ed->hoveredEnemyIdx == i) {
                            SDL_Rect hoverRect = {mx + ux * ts, my + uy * ts, ts, ts};
                            SDL_SetRenderDrawColor(ed->renderer, 255,255,255,200);
                            SDL_RenderDrawRect(ed->renderer, &hoverRect);
                    }
                } else { SDL_SetRenderDrawColor(ed->renderer,255,0,0,255); SDL_RenderFillRect(ed->renderer,&dst); }
                    if (ed->activePanel == 1 && ed->selectedSlot == i && !ed->terrainEditMode) {
                    SDL_Rect selRect = {mx + ux * ts - 2, my + uy * ts - 2, ts+4, ts+4};
                    SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                    SDL_RenderDrawRect(ed->renderer, &selRect);
                    }
                }
            }
        }

        // Красный прямоугольник текущей боевой зоны
        if (entry && entry->battle_width > 0 && entry->battle_height > 0) {
            SDL_Rect zoneRect = {
                mx + (entry->battle_x - ed->camera_x) * ts,
                my + (entry->battle_y - ed->camera_y) * ts,
                entry->battle_width * ts,
                entry->battle_height * ts
            };
            SDL_SetRenderDrawColor(ed->renderer, 255, 0, 0, 150);
            SDL_RenderDrawRect(ed->renderer, &zoneRect);
        }

        // Полосы прокрутки
        if (mw * ts > VIEW_W) {
            int track_x = mx, track_y = my + VIEW_H;
            int track_w = VIEW_W, track_h = SCROLLBAR_SIZE;
            SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
            SDL_RenderFillRect(ed->renderer, &(SDL_Rect){track_x, track_y, track_w, track_h});
            float thumb_w = (float)VIEW_W / (mw * ts) * VIEW_W;
            if (thumb_w < 8) thumb_w = 8;
            float max_cam_x = mw - visW;
            float thumb_x = max_cam_x > 0 ? track_x + (float)ed->camera_x / max_cam_x * (VIEW_W - thumb_w) : track_x;
            SDL_Rect thumb = { (int)thumb_x, track_y+2, (int)thumb_w, SCROLLBAR_SIZE-4 };
            SDL_SetRenderDrawColor(ed->renderer, 200,200,200,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }
        if (mh * ts > VIEW_H) {
            int track_x = mx + VIEW_W, track_y = my;
            int track_w = SCROLLBAR_SIZE, track_h = VIEW_H;
            SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
            SDL_RenderFillRect(ed->renderer, &(SDL_Rect){track_x, track_y, track_w, track_h});
            float thumb_h = (float)VIEW_H / (mh * ts) * VIEW_H;
            if (thumb_h < 8) thumb_h = 8;
            float max_cam_y = mh - visH;
            float thumb_y = max_cam_y > 0 ? track_y + (float)ed->camera_y / max_cam_y * (VIEW_H - thumb_h) : track_y;
            SDL_Rect thumb = { track_x+2, (int)thumb_y, SCROLLBAR_SIZE-4, (int)thumb_h };
            SDL_SetRenderDrawColor(ed->renderer, 200,200,200,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
        }
    }

    // ---- Режим и информация о зоне ----
    char mode_text[64];
    snprintf(mode_text, sizeof(mode_text), ed->viewMode ? "BATTLE ZONE" : "GLOBAL MAP");
    SDL_Surface* text_surf = TTF_RenderText_Solid(ed->font, mode_text, (SDL_Color){255,255,0});
    if (text_surf) {
        SDL_Texture* text_tex = SDL_CreateTextureFromSurface(ed->renderer, text_surf);
        SDL_Rect d = {VIEW_X + 10, VIEW_Y + VIEW_H - 40, text_surf->w, text_surf->h};
        SDL_RenderCopy(ed->renderer, text_tex, NULL, &d);
        SDL_FreeSurface(text_surf);
        SDL_DestroyTexture(text_tex);
    }

    if (entry) {
        char zone_info[128];
        snprintf(zone_info, sizeof(zone_info), "Zone: %d,%d  %dx%d",
                 entry->battle_x, entry->battle_y,
                 entry->battle_width, entry->battle_height);
        text_surf = TTF_RenderText_Solid(ed->font, zone_info, (SDL_Color){255,255,0});
        if (text_surf) {
            SDL_Texture* text_tex = SDL_CreateTextureFromSurface(ed->renderer, text_surf);
            SDL_Rect d = {VIEW_X + 10, VIEW_Y + VIEW_H - 20, text_surf->w, text_surf->h};
            SDL_RenderCopy(ed->renderer, text_tex, NULL, &d);
            SDL_FreeSurface(text_surf);
            SDL_DestroyTexture(text_tex);
        }

            // ==== Панель списка битв (справа) ====
    {
        int list_x = VIEW_X + VIEW_W + SCROLLBAR_SIZE;
        int list_y = VIEW_Y;
        int list_w = BATTLES_LIST_WIDTH;
        int list_h = VIEW_H;

        SDL_Rect list_bg = {list_x, list_y, list_w, list_h};
        SDL_SetRenderDrawColor(ed->renderer, 30, 30, 60, 255);
        SDL_RenderFillRect(ed->renderer, &list_bg);
        SDL_SetRenderDrawColor(ed->renderer, 100, 100, 150, 255);
        SDL_RenderDrawRect(ed->renderer, &list_bg);

        int y_offset = 20;
        int line_height = 18;
        int max_visible = (list_h - 30) / line_height;
        if (max_visible < 1) max_visible = 1;

        int start_idx = ed->battleListScroll;
        if (start_idx > ed->entryCount - max_visible) start_idx = ed->entryCount - max_visible;
        if (start_idx < 0) start_idx = 0;

        for (int i = 0; i < max_visible; i++) {
            int idx = start_idx + i;
            if (idx >= ed->entryCount) break;
            BattleEntry* be = &ed->entries[idx];
            SDL_Color color = (idx == ed->currentEntryIndex) ? (SDL_Color){255,255,0} : (SDL_Color){200,200,200};
            SDL_Surface* s = TTF_RenderText_Solid(ed->font, be->name[0] ? be->name : "Unnamed", color);
            if (s) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(ed->renderer, s);
                SDL_Rect d = {list_x + 5, list_y + y_offset + i*line_height, s->w, s->h};
                SDL_RenderCopy(ed->renderer, tex, NULL, &d);
                SDL_FreeSurface(s);
                SDL_DestroyTexture(tex);
            }
        }

        // Вертикальный скроллбар
        if (ed->entryCount > max_visible) {
            int sb_x = list_x + list_w - 12;
            int sb_y = list_y + y_offset;
            int sb_h = max_visible * line_height;
            SDL_Rect track = {sb_x, sb_y, 10, sb_h};
            SDL_SetRenderDrawColor(ed->renderer, 60,60,60,255);
            SDL_RenderFillRect(ed->renderer, &track);
            float thumb_h = (float)max_visible / ed->entryCount * sb_h;
            if (thumb_h < 6) thumb_h = 6;
            float max_scroll = ed->entryCount - max_visible;
            float thumb_y = max_scroll > 0 ? sb_y + (float)ed->battleListScroll / max_scroll * (sb_h - thumb_h) : sb_y;
            SDL_Rect thumb = {sb_x, (int)thumb_y, 10, (int)thumb_h};
            SDL_SetRenderDrawColor(ed->renderer, 150,150,150,255);
            SDL_RenderFillRect(ed->renderer, &thumb);
            }
        }

    }
}

void handle_input(BattleEditor* ed, SDL_Event* e) {
    
    int raw_mx, raw_my;
    SDL_GetMouseState(&raw_mx, &raw_my);
    int win_w, win_h;
    SDL_GetWindowSize(SDL_RenderGetWindow(ed->renderer), &win_w, &win_h);
    int mx = (int)((float)raw_mx * WINDOW_WIDTH / win_w);
    int my = (int)((float)raw_my * WINDOW_HEIGHT / win_h);

    int list_x = VIEW_X + VIEW_W + SCROLLBAR_SIZE;
    int list_y = VIEW_Y;
    int list_w = BATTLES_LIST_WIDTH;
    int list_h = VIEW_H;    

        // === Определение юнита под мышью (hover) ===
    {
        ed->hoveredAllyIdx = -1;
        ed->hoveredEnemyIdx = -1;
        if (!ed->draggingUnit && !ed->scrollbar_drag_h && !ed->scrollbar_drag_v) {
            int ts = ed->drawTileSize;
            int bx = (ed->entryCount > 0) ? ed->entries[ed->currentEntryIndex].battle_x : 0;
            int by = (ed->entryCount > 0) ? ed->entries[ed->currentEntryIndex].battle_y : 0;
            if (mx >= VIEW_X && mx < VIEW_X + VIEW_W && my >= VIEW_Y && my < VIEW_Y + VIEW_H) {
                int tx = ed->camera_x + (mx - VIEW_X) / ts;
                int ty = ed->camera_y + (my - VIEW_Y) / ts;
                // ищем союзника в этой клетке
                for (int i = 0; i < MAX_ALLIES; i++) {
                    if (ed->allies[i].used) {
                        int ux = ed->allies[i].x, uy = ed->allies[i].y;
                        if (ed->viewMode == 0) { ux += bx; uy += by; }
                        if (ux == tx && uy == ty) {
                            ed->hoveredAllyIdx = i;
                            break;
                        }
                    }
                }
                if (ed->hoveredAllyIdx < 0) {
                    for (int i = 0; i < MAX_ENEMIES; i++) {
                        if (ed->enemies[i].used) {
                            int ux = ed->enemies[i].x, uy = ed->enemies[i].y;
                            if (ed->viewMode == 0) { ux += bx; uy += by; }
                            if (ux == tx && uy == ty) {
                                ed->hoveredEnemyIdx = i;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // === Перетаскивание юнита ===
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT && !ed->draggingUnit) {
        if (!ed->selecting_zone && !ed->scrollbar_drag_h && !ed->scrollbar_drag_v && !ed->inspectorVisible) {
            if (ed->hoveredAllyIdx >= 0 || ed->hoveredEnemyIdx >= 0) {
                ed->draggingUnit = 1;
                ed->selecting_zone = 0;
                ed->dragIsEnemy = (ed->hoveredEnemyIdx >= 0);
                ed->dragSlotIndex = ed->dragIsEnemy ? ed->hoveredEnemyIdx : ed->hoveredAllyIdx;
                UnitSlot* u = ed->dragIsEnemy ? &ed->enemies[ed->dragSlotIndex] : &ed->allies[ed->dragSlotIndex];
                ed->dragStartX = u->x;
                ed->dragStartY = u->y;
                return;
            }
        }
    }

    if (e->type == SDL_MOUSEMOTION && ed->draggingUnit) {
        int ts = ed->drawTileSize;
        int bx = (ed->entryCount > 0) ? ed->entries[ed->currentEntryIndex].battle_x : 0;
        int by = (ed->entryCount > 0) ? ed->entries[ed->currentEntryIndex].battle_y : 0;
        int tx = ed->camera_x + (mx - VIEW_X) / ts;
        int ty = ed->camera_y + (my - VIEW_Y) / ts;

        if (ed->viewMode == 1) {
            int bw = ed->entries[ed->currentEntryIndex].battle_width;
            int bh = ed->entries[ed->currentEntryIndex].battle_height;
            int mw_fallback = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
            int mh_fallback = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;
            if (bw <= 0) bw = mw_fallback;
            if (bh <= 0) bh = mh_fallback;
            if (tx < 0) tx = 0;
            if (ty < 0) ty = 0;
            if (tx >= bw) tx = bw - 1;
            if (ty >= bh) ty = bh - 1;
        } else {
            int mw = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
            int mh = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;
            if (tx < 0) tx = 0;
            if (ty < 0) ty = 0;
            if (tx >= mw) tx = mw - 1;
            if (ty >= mh) ty = mh - 1;
            tx -= bx;
            ty -= by;
        }
        UnitSlot* u = ed->dragIsEnemy ? &ed->enemies[ed->dragSlotIndex] : &ed->allies[ed->dragSlotIndex];
        u->x = tx;
        u->y = ty;
        return;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT && ed->draggingUnit) {
        ed->draggingUnit = 0;
        return;
    }

    // --- Кнопки переключения режима ---
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        // Кнопка MAP
        if (mx >= BTN_MAP_X && mx < BTN_MAP_X + BTN_MAP_W &&
            my >= BTN_MAP_Y && my < BTN_MAP_Y + BTN_MAP_H) {
            ed->viewMode = 0;
            ed->camera_x = ed->camera_y = 0;
            recalc_tile_size(ed);
            return;
        }

        // Кнопка SAVE
        if (mx >= BTN_SAVE_X && mx < BTN_SAVE_X + BTN_SAVE_W &&
            my >= BTN_SAVE_Y && my < BTN_SAVE_Y + BTN_SAVE_H) {
            ed->saveFlashTimer = 0.3f;
            save_current_battle(ed);
            return;
        }

        // Кнопка BATTLE
        if (mx >= BTN_BATTLE_X && mx < BTN_BATTLE_X + BTN_BATTLE_W &&
            my >= BTN_BATTLE_Y && my < BTN_BATTLE_Y + BTN_BATTLE_H) {
            ed->viewMode = 1;
            ed->camera_x = ed->camera_y = 0;
            recalc_tile_size(ed);
            return;
        }
        // Кнопка TERRAIN
        if (mx >= BTN_TERRAIN_X && mx < BTN_TERRAIN_X + BTN_TERRAIN_W &&
            my >= BTN_TERRAIN_Y && my < BTN_TERRAIN_Y + BTN_TERRAIN_H) {
            ed->terrainEditMode = !ed->terrainEditMode;
            ed->selecting_zone = 0;
            ed->draggingUnit = 0;
            return;
        }

        // Кнопка Zoom In
        if (mx >= BTN_ZOOM_IN_X && mx < BTN_ZOOM_IN_X + BTN_ZOOM_IN_W &&
            my >= BTN_ZOOM_IN_Y && my < BTN_ZOOM_IN_Y + BTN_ZOOM_IN_H) {
            float zoomLevels[] = {0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
            int idx = -1;
            // ищем текущий уровень зума
            for (int i = 0; i < 6; i++) {
                if (fabsf(ed->zoom - zoomLevels[i]) < 0.01f) { idx = i; break; }
            }
            if (idx == -1) {
                // если точного совпадения нет, округляем вверх
                for (int i = 0; i < 6; i++) {
                    if (ed->zoom < zoomLevels[i]) { idx = i - 1; break; }
                }
                if (idx == -1) idx = 5; // больше всех — максимум
            }
            if (idx < 5) {
                ed->zoom = zoomLevels[idx + 1];
                recalc_tile_size(ed);
            }
            return;
        }

        // Кнопка Zoom Out
        if (mx >= BTN_ZOOM_OUT_X && mx < BTN_ZOOM_OUT_X + BTN_ZOOM_OUT_W &&
            my >= BTN_ZOOM_OUT_Y && my < BTN_ZOOM_OUT_Y + BTN_ZOOM_OUT_H) {
            float zoomLevels[] = {0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
            int idx = -1;
            for (int i = 0; i < 6; i++) {
                if (fabsf(ed->zoom - zoomLevels[i]) < 0.01f) { idx = i; break; }
            }
            if (idx == -1) {
                for (int i = 0; i < 6; i++) {
                    if (ed->zoom > zoomLevels[i]) { idx = i; }
                }
                if (idx == -1) idx = 0; // меньше всех — минимум
            }
            if (idx > 0) {
                ed->zoom = zoomLevels[idx - 1];
                recalc_tile_size(ed);
            }
            return;
        }
    }   // ← конец блока


    // --- Выделение зоны в глобальном режиме ---
    if (ed->viewMode == 0 && !ed->scrollbar_drag_h && !ed->scrollbar_drag_v) {
        int ts = ed->drawTileSize;
        int mw = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
        int mh = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;

        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
            if (mx >= VIEW_X && mx < VIEW_X + VIEW_W && my >= VIEW_Y && my < VIEW_Y + VIEW_H) {
                ed->selecting_zone = 1;
                int tx = ed->camera_x + (mx - VIEW_X) / ts;
                int ty = ed->camera_y + (my - VIEW_Y) / ts;
                if (tx < 0) tx = 0;
                if (ty < 0) ty = 0;
                if (tx >= mw) tx = mw - 1;
                if (ty >= mh) ty = mh - 1;
                ed->sel_start_x = tx;
                ed->sel_start_y = ty;
                ed->sel_current_x = tx;
                ed->sel_current_y = ty;
                return;
            }
        }

        if (e->type == SDL_MOUSEMOTION && ed->selecting_zone) {
            int tx = ed->camera_x + (mx - VIEW_X) / ts;
            int ty = ed->camera_y + (my - VIEW_Y) / ts;
            if (tx < 0) tx = 0;
            if (ty < 0) ty = 0;
            if (tx >= mw) tx = mw - 1;
            if (ty >= mh) ty = mh - 1;
            ed->sel_current_x = tx;
            ed->sel_current_y = ty;
            return;
        }

        if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT && ed->selecting_zone) {
            ed->selecting_zone = 0;
            int x1 = ed->sel_start_x < ed->sel_current_x ? ed->sel_start_x : ed->sel_current_x;
            int y1 = ed->sel_start_y < ed->sel_current_y ? ed->sel_start_y : ed->sel_current_y;
            int x2 = ed->sel_start_x > ed->sel_current_x ? ed->sel_start_x : ed->sel_current_x;
            int y2 = ed->sel_start_y > ed->sel_current_y ? ed->sel_start_y : ed->sel_current_y;
            int w = x2 - x1 + 1;
            int h = y2 - y1 + 1;
            if (w > 0 && h > 0 && ed->entryCount > 0) {
                BattleEntry* entry = &ed->entries[ed->currentEntryIndex];
                entry->battle_x = x1;
                entry->battle_y = y1;
                entry->battle_width = w;
                entry->battle_height = h;
                printf("Zone set: %d,%d %dx%d\n", x1, y1, w, h);
            }
            return;
        }
    }

    // ===== Селектор типа местности (кнопки) =====
    if (ed->terrainEditMode && e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int selStartX = BTN_ZOOM_OUT_X + BTN_ZOOM_OUT_W + 10;
        SDL_Rect tBtnBlocked = {selStartX, BTN_TERRAIN_Y, 24, 24};
        SDL_Rect tBtnFly     = {selStartX + 24 + 5, BTN_TERRAIN_Y, 24, 24};
        SDL_Rect tBtnWalk    = {selStartX + 24 + 5 + 24 + 5, BTN_TERRAIN_Y, 24, 24};

        if (mx >= tBtnBlocked.x && mx < tBtnBlocked.x + tBtnBlocked.w && my >= tBtnBlocked.y && my < tBtnBlocked.y + tBtnBlocked.h) {
            ed->terrainPaintValue = -1;
            return;
        }
        if (mx >= tBtnFly.x && mx < tBtnFly.x + tBtnFly.w && my >= tBtnFly.y && my < tBtnFly.y + tBtnFly.h) {
            ed->terrainPaintValue = 0;
            return;
        }
        if (mx >= tBtnWalk.x && mx < tBtnWalk.x + tBtnWalk.w && my >= tBtnWalk.y && my < tBtnWalk.y + tBtnWalk.h) {
            ed->terrainPaintValue = 1;
            return;
        }
    }

    // ===== Рисование местности зажатой кнопкой =====
    if (ed->terrainEditMode) {
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
            if (mx >= VIEW_X && mx < VIEW_X + VIEW_W && my >= VIEW_Y && my < VIEW_Y + VIEW_H) {
                ed->terrainPainting = 1;
                e->type = SDL_MOUSEMOTION; // хитрый трюк, чтобы сразу закрасить первую клетку
            }
        }
        if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
            ed->terrainPainting = 0;
            return;
        }

        if (ed->terrainPainting && e->type == SDL_MOUSEMOTION) {
            int ts = ed->drawTileSize;
            int tx = ed->camera_x + (mx - VIEW_X) / ts;
            int ty = ed->camera_y + (my - VIEW_Y) / ts;

            int limit_w, limit_h, offset_x = 0, offset_y = 0;
            if (ed->viewMode == 1) {
                BattleEntry* entry = &ed->entries[ed->currentEntryIndex];
                limit_w = entry->battle_width > 0 ? entry->battle_width : (ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W);
                limit_h = entry->battle_height > 0 ? entry->battle_height : (ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H);
                offset_x = entry->battle_x;
                offset_y = entry->battle_y;
            } else {
                limit_w = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
                limit_h = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;
            }

            if (tx >= 0 && tx < limit_w && ty >= 0 && ty < limit_h) {
                int map_x = tx + offset_x;
                int map_y = ty + offset_y;
                if (map_x >= 0 && map_x < ed->currentMap.w && map_y >= 0 && map_y < ed->currentMap.h) {
                    ed->terrain[map_y][map_x] = ed->terrainPaintValue;
                }
            }
        }
    }

    // +++ сначала обработка инспектора, если видим
    if (ed->inspectorVisible && e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (mx >= INSPECTOR_X && mx < INSPECTOR_X + INSPECTOR_W &&
            my >= INSPECTOR_Y && my < INSPECTOR_Y + INSPECTOR_H) {
            SDL_Rect btnLeft  = {INSPECTOR_X + 10, INSPECTOR_Y + 15, 30, 30};
            SDL_Rect btnRight = {INSPECTOR_X + INSPECTOR_W - 40, INSPECTOR_Y + 15, 30, 30};

            if (mx >= btnLeft.x && mx < btnLeft.x + btnLeft.w && my >= btnLeft.y && my < btnLeft.y + btnLeft.h) {
                // стрелка влево
                int panel = ed->inspectorPanel;
                UnitSlot* slot = (panel == 0) ? &ed->allies[ed->inspectorSlotIdx] : &ed->enemies[ed->inspectorSlotIdx];
                int count = (panel == 0) ? ed->actorsCount : ed->enemiesCount;
                cJSON* array = (panel == 0) ? ed->actorsArray : ed->enemiesArray;

                int newId = slot->id;
                int attempts = 0;
                do {
                    newId = newId - 1;
                    if (newId < 0) newId = count - 1;
                    attempts++;
                    if (attempts > count) break; // прошли полный круг – нет доступных
                    if (panel == 0) {
                        // проверка уникальности для союзников
                        int duplicate = 0;
                        for (int j = 0; j < MAX_ALLIES; j++) {
                            if (ed->allies[j].used && ed->allies[j].id == newId && j != ed->inspectorSlotIdx) {
                                duplicate = 1;
                                break;
                            }
                        }
                        if (!duplicate) break; // нашли свободный ID
                    } else {
                        break; // для врагов можно любой
                    }
                } while (1);

                set_unit_from_json(slot, array, newId);
                if (slot->tex) { SDL_DestroyTexture(slot->tex); slot->tex = NULL; }
            } else if (mx >= btnRight.x && mx < btnRight.x + btnRight.w && my >= btnRight.y && my < btnRight.y + btnRight.h) {
                // стрелка вправо
                int panel = ed->inspectorPanel;
                UnitSlot* slot = (panel == 0) ? &ed->allies[ed->inspectorSlotIdx] : &ed->enemies[ed->inspectorSlotIdx];
                int count = (panel == 0) ? ed->actorsCount : ed->enemiesCount;
                cJSON* array = (panel == 0) ? ed->actorsArray : ed->enemiesArray;

                int newId = slot->id;
                int attempts = 0;
                do {
                    newId = newId + 1;
                    if (newId >= count) newId = 0;
                    attempts++;
                    if (attempts > count) break;
                    if (panel == 0) {
                        int duplicate = 0;
                        for (int j = 0; j < MAX_ALLIES; j++) {
                            if (ed->allies[j].used && ed->allies[j].id == newId && j != ed->inspectorSlotIdx) {
                                duplicate = 1;
                                break;
                            }
                        }
                        if (!duplicate) break;
                    } else {
                        break;
                    }
                } while (1);

                set_unit_from_json(slot, array, newId);
                if (slot->tex) { SDL_DestroyTexture(slot->tex); slot->tex = NULL; }
            }
            return;
        } else {
            ed->inspectorVisible = 0;
        }
    }

    // ==== Клик по списку битв ====
    {
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT &&
            mx >= list_x && mx < list_x + list_w && my >= list_y && my < list_y + list_h) {
            int rel_y = my - list_y;
            int line_height = 18;
            int y_offset = 20;
            int clicked_line = (rel_y - y_offset) / line_height;

            if (clicked_line >= 0) {
                int idx = ed->battleListScroll + clicked_line;
                if (idx >= 0 && idx < ed->entryCount && idx != ed->currentEntryIndex) {
                    switch_battle(ed, idx);
                }
            }
            return;
        }
    }

    // Прокрутка списка битв колёсиком
    if (e->type == SDL_MOUSEWHEEL && mx >= list_x && mx < list_x + list_w && my >= list_y && my < list_y + list_h) {
        int max_visible = (list_h - 30) / 18;
        ed->battleListScroll -= e->wheel.y * 2;
        if (ed->battleListScroll < 0) ed->battleListScroll = 0;
        if (ed->battleListScroll > ed->entryCount - max_visible) ed->battleListScroll = ed->entryCount - max_visible;
        return;
    }

    // остальная обработка мыши (полосы прокрутки) и клавиатуры как раньше...
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int ts = ed->drawTileSize;
        int mw = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
        int mh = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;
        int visW = VIEW_W / ts; if (visW > mw) visW = mw;
        int visH = VIEW_H / ts; if (visH > mh) visH = mh;
        if (mw * ts > VIEW_W && my >= VIEW_Y + VIEW_H && my < VIEW_Y + VIEW_H + SCROLLBAR_SIZE && mx >= VIEW_X && mx < VIEW_X + VIEW_W) {
            ed->scrollbar_drag_h = 1;
            ed->scroll_drag_mouse_x = mx;
            ed->scroll_drag_start_camera_x = ed->camera_x;
            return;
        }
        if (mh * ts > VIEW_H && mx >= VIEW_X + VIEW_W && mx < VIEW_X + VIEW_W + SCROLLBAR_SIZE && my >= VIEW_Y && my < VIEW_Y + VIEW_H) {
            ed->scrollbar_drag_v = 1;
            ed->scroll_drag_mouse_y = my;
            ed->scroll_drag_start_camera_y = ed->camera_y;
            return;
        }

        // +++ Проверка клика по слотам панелей
        // Проверим ally_panel
        if (mx >= ALLY_PANEL_X && mx < ALLY_PANEL_X + ALLY_COLS * TILE_SIZE &&
            my >= ALLY_PANEL_Y && my < ALLY_PANEL_Y + ALLY_ROWS * TILE_SIZE) {
            int col = (mx - ALLY_PANEL_X) / TILE_SIZE;
            int row = (my - ALLY_PANEL_Y) / TILE_SIZE;
            int idx = row * ALLY_COLS + col;
            if (ed->allies[idx].used) {
                ed->inspectorVisible = 1;
                ed->inspectorPanel = 0;
                ed->inspectorSlotIdx = idx;
                ed->activePanel = 0;
                ed->selectedSlot = idx;
            }
            return;
        }
        // enemy_panel
        if (mx >= ENEMY_PANEL_X && mx < ENEMY_PANEL_X + ENEMY_COLS * TILE_SIZE &&
            my >= ENEMY_PANEL_Y && my < ENEMY_PANEL_Y + ENEMY_ROWS * TILE_SIZE) {
            int col = (mx - ENEMY_PANEL_X) / TILE_SIZE;
            int row = (my - ENEMY_PANEL_Y) / TILE_SIZE;
            int idx = row * ENEMY_COLS + col;
            if (ed->enemies[idx].used) {
                ed->inspectorVisible = 1;
                ed->inspectorPanel = 1;
                ed->inspectorSlotIdx = idx;
                ed->activePanel = 1;
                ed->selectedSlot = idx;
            }
            return;
        }

        // Кнопки Add/Del для врагов
        {
            SDL_Rect addBtn = {ENEMY_PANEL_X, ENEMY_PANEL_Y + ENEMY_ROWS * TILE_SIZE + 5, 50, 20};
            SDL_Rect delBtn = {ENEMY_PANEL_X + 55, ENEMY_PANEL_Y + ENEMY_ROWS * TILE_SIZE + 5, 50, 20};

            if (mx >= addBtn.x && mx < addBtn.x + addBtn.w && my >= addBtn.y && my < addBtn.y + addBtn.h) {
                // Добавить врага
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (!ed->enemies[i].used) {
                        set_unit_from_json(&ed->enemies[i], ed->enemiesArray, ed->selectedEnemyId);
                        ed->enemies[i].x = 0;
                        ed->enemies[i].y = 0;
                        ed->selectedEnemyId = (ed->selectedEnemyId + 1) % ed->enemiesCount;
                        ed->activePanel = 1;
                        ed->selectedSlot = i;
                        ed->inspectorVisible = 1;
                        ed->inspectorPanel = 1;
                        ed->inspectorSlotIdx = i;
                        return;
                    }
                }
                return;
            }
            if (mx >= delBtn.x && mx < delBtn.x + delBtn.w && my >= delBtn.y && my < delBtn.y + delBtn.h) {
                // Удалить выбранного врага
                if (ed->activePanel == 1 && ed->selectedSlot >= 0 && ed->selectedSlot < MAX_ENEMIES
                    && ed->enemies[ed->selectedSlot].used) {
                    if (ed->enemies[ed->selectedSlot].tex)
                        SDL_DestroyTexture(ed->enemies[ed->selectedSlot].tex);
                    memset(&ed->enemies[ed->selectedSlot], 0, sizeof(UnitSlot));
                    if (ed->inspectorVisible && ed->inspectorPanel == 1
                        && ed->inspectorSlotIdx == ed->selectedSlot)
                        ed->inspectorVisible = 0;
                }
                return;
            }
        }

        // Кнопки Add/Del для союзников (с проверкой уникальности)
        {
            SDL_Rect addBtn = {ALLY_PANEL_X, ALLY_PANEL_Y + ALLY_ROWS * TILE_SIZE + 5, 50, 20};
            SDL_Rect delBtn = {ALLY_PANEL_X + 55, ALLY_PANEL_Y + ALLY_ROWS * TILE_SIZE + 5, 50, 20};

            if (mx >= addBtn.x && mx < addBtn.x + addBtn.w && my >= addBtn.y && my < addBtn.y + addBtn.h) {
                // Найти первый свободный слот
                int slot = -1;
                for (int i = 0; i < MAX_ALLIES; i++) {
                    if (!ed->allies[i].used) { slot = i; break; }
                }
                if (slot == -1) return;   // нет свободных слотов

                // Проверка уникальности: найти actor_id, которого ещё нет среди союзников
                int startId = ed->selectedAllyId;
                int newId = -1;
                for (int attempt = 0; attempt < ed->actorsCount; attempt++) {
                    int testId = (startId + attempt) % ed->actorsCount;
                    // проверим, используется ли уже testId
                    int duplicate = 0;
                    for (int j = 0; j < MAX_ALLIES; j++) {
                        if (ed->allies[j].used && ed->allies[j].id == testId) {
                            duplicate = 1;
                            break;
                        }
                    }
                    if (!duplicate) {
                        newId = testId;
                        break;
                    }
                }
                if (newId == -1) return;   // все актёры уже расставлены

                // Создаём союзника
                set_unit_from_json(&ed->allies[slot], ed->actorsArray, newId);
                ed->allies[slot].x = 0;
                ed->allies[slot].y = 0;
                ed->selectedAllyId = (newId + 1) % ed->actorsCount;   // для удобства следующий Add предложит другого
                ed->activePanel = 0;
                ed->selectedSlot = slot;
                ed->inspectorVisible = 1;
                ed->inspectorPanel = 0;
                ed->inspectorSlotIdx = slot;
                return;
            }

            if (mx >= delBtn.x && mx < delBtn.x + delBtn.w && my >= delBtn.y && my < delBtn.y + delBtn.h) {
                // Удалить выбранного союзника
                if (ed->activePanel == 0 && ed->selectedSlot >= 0 && ed->selectedSlot < MAX_ALLIES
                    && ed->allies[ed->selectedSlot].used) {
                    if (ed->allies[ed->selectedSlot].tex)
                        SDL_DestroyTexture(ed->allies[ed->selectedSlot].tex);
                    memset(&ed->allies[ed->selectedSlot], 0, sizeof(UnitSlot));
                    if (ed->inspectorVisible && ed->inspectorPanel == 0
                        && ed->inspectorSlotIdx == ed->selectedSlot)
                        ed->inspectorVisible = 0;
                }
                return;
            }
        }
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        ed->scrollbar_drag_h = 0;
        ed->scrollbar_drag_v = 0;
    }
    if (e->type == SDL_MOUSEMOTION) {
        if (ed->scrollbar_drag_h) {
            int ts = ed->drawTileSize;
            int mw = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
            int visW = VIEW_W / ts; if (visW > mw) visW = mw;
            float dx = (float)(mx - ed->scroll_drag_mouse_x) / ts;
            ed->camera_x = ed->scroll_drag_start_camera_x + (int)dx;
            if (ed->camera_x < 0) ed->camera_x = 0;
            if (ed->camera_x > mw - visW) ed->camera_x = mw - visW;
        }
        if (ed->scrollbar_drag_v) {
            int ts = ed->drawTileSize;
            int mh = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;
            int visH = VIEW_H / ts; if (visH > mh) visH = mh;
            float dy = (float)(my - ed->scroll_drag_mouse_y) / ts;
            ed->camera_y = ed->scroll_drag_start_camera_y + (int)dy;
            if (ed->camera_y < 0) ed->camera_y = 0;
            if (ed->camera_y > mh - visH) ed->camera_y = mh - visH;
        }
    }

    if (e->type != SDL_KEYDOWN) return;


    if (e->key.keysym.mod & KMOD_SHIFT) {
        int step = (int)(48 / ed->zoom); if (step < 1) step = 1;
        switch(e->key.keysym.sym) {
            case SDLK_LEFT:  ed->camera_x -= step; if(ed->camera_x<0)ed->camera_x=0; break;
            case SDLK_RIGHT: ed->camera_x += step; break;
            case SDLK_UP:    ed->camera_y -= step; if(ed->camera_y<0)ed->camera_y=0; break;
            case SDLK_DOWN:  ed->camera_y += step; break;
        }
        return;
    }

    int cols = ed->activePanel==0 ? ALLY_COLS : ENEMY_COLS;
    int rows = ed->activePanel==0 ? ALLY_ROWS : ENEMY_ROWS;
    UnitSlot* slots = ed->activePanel==0 ? ed->allies : ed->enemies;
    int* selId = ed->activePanel==0 ? &ed->selectedAllyId : &ed->selectedEnemyId;
    int count = ed->activePanel==0 ? ed->actorsCount : ed->enemiesCount;
    cJSON* array = ed->activePanel==0 ? ed->actorsArray : ed->enemiesArray;

    switch(e->key.keysym.sym) {
        case SDLK_UP:    if(ed->selectedSlot >= cols) ed->selectedSlot -= cols; break;
        case SDLK_DOWN:  if(ed->selectedSlot/cols < rows-1) ed->selectedSlot += cols; break;
        case SDLK_LEFT:
            if(slots[ed->selectedSlot].used) {
                int n = slots[ed->selectedSlot].id - 1; if(n<0) n=count-1;
                set_unit_from_json(&slots[ed->selectedSlot], array, n);
                if(slots[ed->selectedSlot].tex) { SDL_DestroyTexture(slots[ed->selectedSlot].tex); slots[ed->selectedSlot].tex=NULL; }
            } else { if(ed->selectedSlot%cols > 0) ed->selectedSlot--; }
            break;
        case SDLK_RIGHT:
            if(slots[ed->selectedSlot].used) {
                int n = slots[ed->selectedSlot].id + 1; if(n>=count) n=0;
                set_unit_from_json(&slots[ed->selectedSlot], array, n);
                if(slots[ed->selectedSlot].tex) { SDL_DestroyTexture(slots[ed->selectedSlot].tex); slots[ed->selectedSlot].tex=NULL; }
            } else { if(ed->selectedSlot%cols < cols-1) ed->selectedSlot++; }
            break;

        case SDLK_SPACE:
            ed->viewMode = !ed->viewMode;
            if (ed->viewMode) {
                ed->camera_x = 0;
                ed->camera_y = 0;
            }
            recalc_tile_size(ed);
            break;   
        case SDLK_TAB:   ed->activePanel = !ed->activePanel; ed->selectedSlot=0; break;
        case SDLK_a:
            if (!slots[ed->selectedSlot].used && count > 0) {
                if (ed->activePanel == 0) {   // союзники – проверка уникальности
                    int startId = *selId;
                    int newId = -1;
                    for (int attempt = 0; attempt < ed->actorsCount; attempt++) {
                        int testId = (startId + attempt) % ed->actorsCount;
                        int duplicate = 0;
                        for (int j = 0; j < MAX_ALLIES; j++) {
                            if (ed->allies[j].used && ed->allies[j].id == testId) {
                                duplicate = 1;
                                break;
                            }
                        }
                        if (!duplicate) {
                            newId = testId;
                            break;
                        }
                    }
                    if (newId == -1) break;
                    set_unit_from_json(&slots[ed->selectedSlot], array, newId);
                    *selId = (newId + 1) % count;
                } else {   // враги
                    set_unit_from_json(&slots[ed->selectedSlot], array, *selId);
                    *selId = (*selId + 1) % count;
                }
                if (slots[ed->selectedSlot].tex) { SDL_DestroyTexture(slots[ed->selectedSlot].tex); slots[ed->selectedSlot].tex = NULL; }
            }
            break;
        case SDLK_d:
            if(slots[ed->selectedSlot].used) { if(slots[ed->selectedSlot].tex) SDL_DestroyTexture(slots[ed->selectedSlot].tex); memset(&slots[ed->selectedSlot],0,sizeof(UnitSlot)); }
            if (ed->inspectorVisible && ed->inspectorPanel == ed->activePanel && ed->inspectorSlotIdx == ed->selectedSlot)
                ed->inspectorVisible = 0;
            break;
        case SDLK_q: if(count>0) *selId = (*selId-1+count) % count; break;
        case SDLK_e: if(count>0) *selId = (*selId+1) % count; break;
        case SDLK_LEFTBRACKET:  if(ed->entryCount>1) switch_battle(ed, (ed->currentEntryIndex-1+ed->entryCount)%ed->entryCount); break;
        case SDLK_RIGHTBRACKET: if(ed->entryCount>1) switch_battle(ed, (ed->currentEntryIndex+1)%ed->entryCount); break;
    }
}

void cleanup_editor(BattleEditor* ed) {
    // Уничтожаем текстуры союзников и врагов
    for (int i = 0; i < MAX_ALLIES; i++) {
        if (ed->allies[i].tex) SDL_DestroyTexture(ed->allies[i].tex);
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (ed->enemies[i].tex) SDL_DestroyTexture(ed->enemies[i].tex);
    }

    // Освобождаем JSON-корни, если они были загружены
    if (ed->actorsRoot) cJSON_Delete(ed->actorsRoot);
    if (ed->enemiesRoot) cJSON_Delete(ed->enemiesRoot);

    // Закрываем шрифт
    if (ed->font) TTF_CloseFont(ed->font);

    // Удаляем тайлы карты
    if (ed->tiles) {
        for (int i = 0; i < ed->tile_count; i++) {
            SDL_DestroyTexture(ed->tiles[i]);
        }
        free(ed->tiles);
    }

    // Освобождаем текущую карту
    free_map_layout(&ed->currentMap);
}

void switch_battle(BattleEditor* ed, int newIdx) {
    if (newIdx < 0 || newIdx >= ed->entryCount) return;
    ed->currentEntryIndex = newIdx;

    // Очистка предыдущей карты
    free_map_layout(&ed->currentMap);

    // Загружаем новую карту
    BattleEntry* entry = &ed->entries[newIdx];
    if (load_map_layout(entry->map_id, &ed->currentMap)) {
        load_tileset(ed);
    } else {
        ed->currentMap.w = 0;
        ed->currentMap.h = 0;
    }

    // Загружаем набор спрайтов для этой битвы
    load_spriteset(ed, entry->folder);
    load_terrain(ed, entry->folder);

    // Сбрасываем камеру и пересчитываем масштаб
    recalc_tile_size(ed);
    ed->camera_x = ed->camera_y = 0;

    // Очищаем старые слоты союзников и врагов (текстуры уже удалены в load_spriteset)
    for (int i = 0; i < MAX_ALLIES; i++) {
        if (ed->allies[i].tex) SDL_DestroyTexture(ed->allies[i].tex);
        memset(&ed->allies[i], 0, sizeof(UnitSlot));
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (ed->enemies[i].tex) SDL_DestroyTexture(ed->enemies[i].tex);
        memset(&ed->enemies[i], 0, sizeof(UnitSlot));
    }
}

static void save_spriteset(BattleEditor* ed) {
    if (ed->entryCount <= 0) return;
    BattleEntry* be = &ed->entries[ed->currentEntryIndex];
    char path[256];
    snprintf(path, sizeof(path), "../data/battles/%s/spriteset.json", be->folder);

    // Читаем существующий файл, чтобы сохранить ai_points и ai_regions
    cJSON* root = NULL;
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
        char* buf = malloc(len+1);
        if (buf) {
            fread(buf, 1, len, f);
            buf[len] = '\0';
            root = cJSON_Parse(buf);
            free(buf);
        }
        fclose(f);
    }
    if (!root) root = cJSON_CreateObject();

    // Обновляем allies
    cJSON* alliesArr = cJSON_CreateArray();
    for (int i = 0; i < MAX_ALLIES; i++) {
        if (!ed->allies[i].used) continue;
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "actor_id", ed->allies[i].id);
        cJSON_AddNumberToObject(item, "x", ed->allies[i].x);
        cJSON_AddNumberToObject(item, "y", ed->allies[i].y);
        cJSON_AddItemToArray(alliesArr, item);
    }
    cJSON_DeleteItemFromObject(root, "allies");
    cJSON_AddItemToObject(root, "allies", alliesArr);

    // Обновляем enemies (только базовые поля, ai-поля пока не сохраняем)
    cJSON* enemiesArr = cJSON_CreateArray();
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!ed->enemies[i].used) continue;
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "enemy_id", ed->enemies[i].id);
        cJSON_AddNumberToObject(item, "x", ed->enemies[i].x);
        cJSON_AddNumberToObject(item, "y", ed->enemies[i].y);
        cJSON_AddItemToArray(enemiesArr, item);
    }
    cJSON_DeleteItemFromObject(root, "enemies");
    cJSON_AddItemToObject(root, "enemies", enemiesArr);

    char* jsonStr = cJSON_Print(root);
    cJSON_Delete(root);
    f = fopen(path, "wb");
    if (f) {
        fputs(jsonStr, f);
        fclose(f);
        printf("Spriteset saved: %s\n", path);
    }
    free(jsonStr);
}

static void save_terrain(BattleEditor* ed) {
    if (ed->entryCount <= 0) return;
    BattleEntry* be = &ed->entries[ed->currentEntryIndex];
    char path[256];
    snprintf(path, sizeof(path), "../data/battles/%s/terrain.json", be->folder);

    int w = ed->currentMap.w > 0 ? ed->currentMap.w : FALLBACK_MAP_W;
    int h = ed->currentMap.h > 0 ? ed->currentMap.h : FALLBACK_MAP_H;

    cJSON* root = cJSON_CreateArray();
    for (int y = 0; y < h; y++) {
        cJSON* row = cJSON_CreateArray();
        for (int x = 0; x < w; x++) {
            cJSON_AddItemToArray(row, cJSON_CreateNumber(ed->terrain[y][x]));
        }
        cJSON_AddItemToArray(root, row);
    }

    char* jsonStr = cJSON_Print(root);
    cJSON_Delete(root);
    FILE* f = fopen(path, "wb");
    if (f) {
        fputs(jsonStr, f);
        fclose(f);
        printf("Terrain saved: %s\n", path);
    }
    free(jsonStr);
}

void save_current_battle(BattleEditor* ed) {
    save_spriteset(ed);
    save_terrain(ed);
    printf("Battle saved.\n");
}

void set_unit_from_json(UnitSlot* unit, cJSON* array, int id) {
    cJSON* item = cJSON_GetArrayItem(array, id);
    if (!item) return;
    unit->id = id;
    strcpy(unit->name, json_string(item, "name"));
    strcpy(unit->mapsprite, json_string(item, "mapsprite"));
    unit->used = 1;
}