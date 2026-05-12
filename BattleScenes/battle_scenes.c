// Battle Scenes Editor – редактор фона битв и анимаций
// Адаптивное окно 1024×768, загрузка и сохранение data/battles/entries.json
// Редактирование анимаций: кадры, длительность, offset

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <windows.h>
#include <commdlg.h>
#include "cJSON.h"

// ─── Геометрия ───────────────────────────────
#define LOGICAL_W          1024
#define LOGICAL_H          768
#define LEFT_PANEL_W        220
#define PREVIEW_X           (LEFT_PANEL_W + 20)
#define PREVIEW_Y           ((LOGICAL_H - PREVIEW_H) / 2)   // автоматически центрируется
#define PREVIEW_W           (LOGICAL_W - LEFT_PANEL_W - 40)
#define PREVIEW_H           672    // как в игре (960 - 2*144)
#define FONT_SIZE           18

// ─── Цвета ────────────────────────────────────
static const SDL_Color BG_COLOR        = { 25, 25, 40, 255 };
static const SDL_Color PANEL_BG        = { 35, 35, 55, 255 };
static const SDL_Color BUTTON_COLOR    = { 70, 50, 120, 255 };
static const SDL_Color BUTTON_HOVER    = { 110, 70, 180, 255 };
static const SDL_Color TEXT_COLOR      = { 220, 220, 240, 255 };
static const SDL_Color HIGHLIGHT_COLOR = { 255, 255, 100, 255 };

// ─── Запись битвы ────────────────────────────
typedef struct {
    char folder[64];
    char name[64];
    char map_id[64];
    char music[256];
    float music_volume;
    int battle_x;
    int battle_y;
    int battle_width;
    int battle_height;
    char background[512];
} BattleEntry;

// ─── Анимация ────────────────────────────────
#define MAX_ANIM_FRAMES 32
typedef struct {
    char name[32];
    int frame_count;
    SDL_Texture *frames[MAX_ANIM_FRAMES];
    float frame_durations[MAX_ANIM_FRAMES];   // длительность каждого кадра
    int offset_x;
    int offset_y;
} AnimPhase;

typedef struct {
    AnimPhase phases[3];    // 0:idle, 1:attack, 2:defense
    int current_phase;      // 0..2
    int current_frame;      // индекс в текущей фазе
    bool loaded;
    char json_path[512];    // путь к animation.json, чтобы сохранять
} AnimationSet;

typedef struct Editor {
    BattleEntry *entries;
    int entry_count;
    int current_index;
    int list_scroll;

    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    SDL_Texture  *bg_tex;
    char          bg_display_name[64];
    char          bg_full_path[256];

    AnimationSet anim_set;
} Editor;

// ─── Вспомогательные функции ──────────────────
void safe_strcpy(char *dest, size_t size, const char *src) {
    if (size > 0) snprintf(dest, size, "%s", src);
}

bool open_file_dialog(char *out, size_t out_len, const char *initial_dir) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Images\0*.png\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = initial_dir;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) {
        safe_strcpy(out, out_len, ofn.lpstrFile);
        return true;
    }
    return false;
}

bool open_json_dialog(char *out, size_t out_len, const char *initial_dir) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Animation JSON\0animation.json\0All JSON\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = initial_dir;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) {
        safe_strcpy(out, out_len, ofn.lpstrFile);
        return true;
    }
    return false;
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

// ─── Загрузка entries.json ────────────────────
static cJSON* load_entries_json(void) {
    const char *path = "../data/battles/entries.json";
    FILE *f = fopen(path, "r");
    if (!f) return cJSON_CreateArray();
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(len + 1);
    if (!data) { fclose(f); return cJSON_CreateArray(); }
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);
    cJSON *root = cJSON_Parse(data);
    free(data);
    return root ? root : cJSON_CreateArray();
}

static void load_entries(Editor *ed) {
    free(ed->entries);
    ed->entries = NULL;
    ed->entry_count = 0;

    cJSON *arr = load_entries_json();
    if (!cJSON_IsArray(arr)) { cJSON_Delete(arr); return; }
    int count = cJSON_GetArraySize(arr);
    ed->entries = malloc(sizeof(BattleEntry) * count);
    ed->entry_count = count;
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        BattleEntry *be = &ed->entries[i];
        safe_strcpy(be->folder, sizeof(be->folder), cJSON_GetObjectItem(item, "folder")->valuestring);
        safe_strcpy(be->name,   sizeof(be->name),   cJSON_GetObjectItem(item, "name")->valuestring);
        safe_strcpy(be->map_id, sizeof(be->map_id), cJSON_GetObjectItem(item, "map_id")->valuestring);
        safe_strcpy(be->music,  sizeof(be->music),  cJSON_GetObjectItem(item, "music")->valuestring);
        cJSON *vol = cJSON_GetObjectItem(item, "music_volume");
        be->music_volume = (vol && cJSON_IsNumber(vol)) ? (float)vol->valuedouble : 0.8f;
        be->battle_x      = cJSON_GetObjectItem(item, "battle_x")->valueint;
        be->battle_y      = cJSON_GetObjectItem(item, "battle_y")->valueint;
        be->battle_width  = cJSON_GetObjectItem(item, "battle_width")->valueint;
        be->battle_height = cJSON_GetObjectItem(item, "battle_height")->valueint;
        cJSON *bg = cJSON_GetObjectItem(item, "background");
        safe_strcpy(be->background, sizeof(be->background), bg ? bg->valuestring : "");
    }
    cJSON_Delete(arr);
}

static void save_entries(Editor *ed) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < ed->entry_count; i++) {
        BattleEntry *be = &ed->entries[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "folder", be->folder);
        cJSON_AddStringToObject(item, "name", be->name);
        cJSON_AddStringToObject(item, "map_id", be->map_id);
        cJSON_AddStringToObject(item, "music", be->music);
        cJSON_AddNumberToObject(item, "music_volume", be->music_volume);
        cJSON_AddNumberToObject(item, "battle_x", be->battle_x);
        cJSON_AddNumberToObject(item, "battle_y", be->battle_y);
        cJSON_AddNumberToObject(item, "battle_width", be->battle_width);
        cJSON_AddNumberToObject(item, "battle_height", be->battle_height);
        cJSON_AddStringToObject(item, "background", be->background);
        cJSON_AddItemToArray(arr, item);
    }
    char *str = cJSON_Print(arr);
    FILE *f = fopen("../data/battles/entries.json", "wb");
    if (f) { fputs(str, f); fclose(f); }
    free(str);
    cJSON_Delete(arr);
}

// ─── Загрузка текстуры фона ───────────────────
void load_background_texture(Editor *ed, const char *rel_path) {
    if (ed->bg_tex) { SDL_DestroyTexture(ed->bg_tex); ed->bg_tex = NULL; }
    if (!rel_path || !rel_path[0]) return;

    char full[512];
    snprintf(full, sizeof(full), "../%s", rel_path);
    SDL_Surface *surf = IMG_Load(full);
    if (!surf) return;
    ed->bg_tex = SDL_CreateTextureFromSurface(ed->renderer, surf);
    SDL_FreeSurface(surf);
}

// ─── Загрузка анимаций из JSON ────────────────
static void extract_directory(const char *filepath, char *dir, size_t dir_size) {
    safe_strcpy(dir, dir_size, filepath);
    char *slash = strrchr(dir, '\\');
    if (!slash) slash = strrchr(dir, '/');
    if (slash) *(slash + 1) = '\0';
    else dir[0] = '\0';
}

void free_animation_set(AnimationSet *as) {
    if (!as->loaded) return;
    for (int p = 0; p < 3; p++) {
        for (int f = 0; f < as->phases[p].frame_count; f++) {
            if (as->phases[p].frames[f])
                SDL_DestroyTexture(as->phases[p].frames[f]);
        }
        as->phases[p].frame_count = 0;
    }
    as->loaded = false;
    as->current_phase = 0;
    as->current_frame = 0;
    as->json_path[0] = '\0';
}

bool load_animation_from_json(Editor *ed, const char *json_path) {
    free_animation_set(&ed->anim_set);

    FILE *f = fopen(json_path, "r");
    if (!f) {
        printf("Cannot open animation JSON: %s\n", json_path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root) {
        printf("Failed to parse animation JSON: %s\n", json_path);
        return false;
    }

    char base_dir[512];
    extract_directory(json_path, base_dir, sizeof(base_dir));
    safe_strcpy(ed->anim_set.json_path, sizeof(ed->anim_set.json_path), json_path);

    const char *phase_keys[] = {"idle", "attack", "defense"};
    for (int p = 0; p < 3; p++) {
        cJSON *phase = cJSON_GetObjectItem(root, phase_keys[p]);
        if (!phase) continue;

        cJSON *frames = cJSON_GetObjectItem(phase, "frames");
        if (!cJSON_IsArray(frames)) continue;

        int n = cJSON_GetArraySize(frames);
        if (n > MAX_ANIM_FRAMES) n = MAX_ANIM_FRAMES;

        AnimPhase *ap = &ed->anim_set.phases[p];
        safe_strcpy(ap->name, sizeof(ap->name), phase_keys[p]);
        ap->frame_count = n;

        for (int i = 0; i < n; i++) {
            cJSON *frame = cJSON_GetArrayItem(frames, i);
            cJSON *file_json = cJSON_GetObjectItem(frame, "file");
            cJSON *dur_json  = cJSON_GetObjectItem(frame, "duration");
            if (!file_json) continue;
            const char *filename = file_json->valuestring;
            ap->frame_durations[i] = dur_json ? (float)dur_json->valuedouble : 0.3f;

            char full_path[768];
            snprintf(full_path, sizeof(full_path), "%s%s", base_dir, filename);

            SDL_Surface *surf = IMG_Load(full_path);
            if (surf) {
                ap->frames[i] = SDL_CreateTextureFromSurface(ed->renderer, surf);
                SDL_FreeSurface(surf);
            } else {
                printf("Failed to load animation frame: %s\n", full_path);
                ap->frames[i] = NULL;
            }
        }

        cJSON *ox = cJSON_GetObjectItem(phase, "offset_x");
        cJSON *oy = cJSON_GetObjectItem(phase, "offset_y");
        ap->offset_x = ox ? ox->valueint : 0;
        ap->offset_y = oy ? oy->valueint : 0;
    }

    cJSON_Delete(root);

    ed->anim_set.current_phase = 0;
    ed->anim_set.current_frame = 0;
    for (int p = 0; p < 3; p++) {
        if (ed->anim_set.phases[p].frame_count > 0) {
            ed->anim_set.current_phase = p;
            break;
        }
    }
    ed->anim_set.loaded = true;

    printf("Animation loaded: %s\n", json_path);
    return true;
}

// ─── Сохранение анимации в JSON ───────────────
void save_animation_to_json(Editor *ed) {
    if (!ed->anim_set.loaded || !ed->anim_set.json_path[0]) {
        printf("No animation loaded to save.\n");
        return;
    }

    // Читаем существующий JSON, чтобы не потерять имена файлов
    FILE *f = fopen(ed->anim_set.json_path, "r");
    cJSON *root = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *data = malloc(len + 1);
        fread(data, 1, len, f);
        data[len] = '\0';
        fclose(f);
        root = cJSON_Parse(data);
        free(data);
    }
    if (!root) root = cJSON_CreateObject();

    const char *phase_keys[] = {"idle", "attack", "defense"};
    for (int p = 0; p < 3; p++) {
        AnimPhase *ap = &ed->anim_set.phases[p];
        if (ap->frame_count <= 0) continue;

        cJSON *phase = cJSON_GetObjectItem(root, phase_keys[p]);
        if (!phase) {
            phase = cJSON_CreateObject();
            cJSON_AddItemToObject(root, phase_keys[p], phase);
        }

        // Обновляем offset
        cJSON_ReplaceItemInObject(phase, "offset_x", cJSON_CreateNumber(ap->offset_x));
        cJSON_ReplaceItemInObject(phase, "offset_y", cJSON_CreateNumber(ap->offset_y));

        // Обновляем длительности кадров
        cJSON *frames = cJSON_GetObjectItem(phase, "frames");
        if (cJSON_IsArray(frames)) {
            int n = cJSON_GetArraySize(frames);
            for (int i = 0; i < n && i < ap->frame_count; i++) {
                cJSON *frame = cJSON_GetArrayItem(frames, i);
                if (frame) {
                    cJSON_ReplaceItemInObject(frame, "duration", cJSON_CreateNumber(ap->frame_durations[i]));
                }
            }
        }
    }

    char *str = cJSON_Print(root);
    f = fopen(ed->anim_set.json_path, "wb");
    if (f) {
        fputs(str, f);
        fclose(f);
        printf("Animation saved: %s\n", ed->anim_set.json_path);
    }
    free(str);
    cJSON_Delete(root);
}

// ─── Отрисовка интерфейса ─────────────────────
void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text,
                        int cx, int cy, SDL_Color color) {
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, color);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_Rect dst = { cx - s->w/2, cy - s->h/2, s->w, s->h };
    SDL_RenderCopy(ren, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

void draw_ui(Editor *ed) {
    SDL_SetRenderDrawColor(ed->renderer, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, 255);
    SDL_RenderClear(ed->renderer);

    // Левая панель
    SDL_Rect panel = {0, 0, LEFT_PANEL_W, LOGICAL_H};
    SDL_SetRenderDrawColor(ed->renderer, PANEL_BG.r, PANEL_BG.g, PANEL_BG.b, 255);
    SDL_RenderFillRect(ed->renderer, &panel);

    draw_text_centered(ed->renderer, ed->font, "BATTLES",
                       LEFT_PANEL_W/2, 20, TEXT_COLOR);

    int line_height = FONT_SIZE + 4;
    int max_vis = (LOGICAL_H - 60) / line_height;
    int total = ed->entry_count;
    if (ed->list_scroll > total - max_vis) ed->list_scroll = total - max_vis;
    if (ed->list_scroll < 0) ed->list_scroll = 0;

    for (int i = 0; i < max_vis; i++) {
        int idx = ed->list_scroll + i;
        if (idx >= total) break;
        int y = 50 + i * line_height;
        SDL_Color color = (idx == ed->current_index) ? HIGHLIGHT_COLOR : TEXT_COLOR;
        draw_text_centered(ed->renderer, ed->font, ed->entries[idx].name,
                           LEFT_PANEL_W/2, y + line_height/2, color);
    }

    // Кнопки внизу левой панели
    SDL_Rect btn_save   = {10, LOGICAL_H - 40, 70, 28};
    SDL_Rect btn_bg     = {85, LOGICAL_H - 40, 100, 28};
    SDL_Rect btn_enemy  = {190, LOGICAL_H - 40, 110, 28};
    SDL_Rect btn_ally   = {305, LOGICAL_H - 40, 110, 28};
    SDL_Rect btn_save_anim = {420, LOGICAL_H - 40, 100, 28};

    int mx, my;
    SDL_GetMouseState(&mx, &my);
    int win_w, win_h;
    SDL_GetWindowSize(ed->window, &win_w, &win_h);
    int logical_mx = (int)((float)mx * LOGICAL_W / win_w);
    int logical_my = (int)((float)my * LOGICAL_H / win_h);

    SDL_Color save_col = BUTTON_COLOR;
    if (logical_mx >= btn_save.x && logical_mx < btn_save.x+btn_save.w &&
        logical_my >= btn_save.y && logical_my < btn_save.y+btn_save.h)
        save_col = BUTTON_HOVER;
    SDL_SetRenderDrawColor(ed->renderer, save_col.r, save_col.g, save_col.b, 255);
    SDL_RenderFillRect(ed->renderer, &btn_save);
    draw_text_centered(ed->renderer, ed->font, "Save",
                       btn_save.x + btn_save.w/2, btn_save.y + btn_save.h/2, TEXT_COLOR);

    SDL_Color bg_col = BUTTON_COLOR;
    if (logical_mx >= btn_bg.x && logical_mx < btn_bg.x+btn_bg.w &&
        logical_my >= btn_bg.y && logical_my < btn_bg.y+btn_bg.h)
        bg_col = BUTTON_HOVER;
    SDL_SetRenderDrawColor(ed->renderer, bg_col.r, bg_col.g, bg_col.b, 255);
    SDL_RenderFillRect(ed->renderer, &btn_bg);
    draw_text_centered(ed->renderer, ed->font, "Change BG",
                       btn_bg.x + btn_bg.w/2, btn_bg.y + btn_bg.h/2, TEXT_COLOR);

    SDL_Color enemy_col = BUTTON_COLOR;
    if (logical_mx >= btn_enemy.x && logical_mx < btn_enemy.x+btn_enemy.w &&
        logical_my >= btn_enemy.y && logical_my < btn_enemy.y+btn_enemy.h)
        enemy_col = BUTTON_HOVER;
    SDL_SetRenderDrawColor(ed->renderer, enemy_col.r, enemy_col.g, enemy_col.b, 255);
    SDL_RenderFillRect(ed->renderer, &btn_enemy);
    draw_text_centered(ed->renderer, ed->font, "Enemy Anim",
                       btn_enemy.x + btn_enemy.w/2, btn_enemy.y + btn_enemy.h/2, TEXT_COLOR);

    SDL_Color ally_col = BUTTON_COLOR;
    if (logical_mx >= btn_ally.x && logical_mx < btn_ally.x+btn_ally.w &&
        logical_my >= btn_ally.y && logical_my < btn_ally.y+btn_ally.h)
        ally_col = BUTTON_HOVER;
    SDL_SetRenderDrawColor(ed->renderer, ally_col.r, ally_col.g, ally_col.b, 255);
    SDL_RenderFillRect(ed->renderer, &btn_ally);
    draw_text_centered(ed->renderer, ed->font, "Ally Anim",
                       btn_ally.x + btn_ally.w/2, btn_ally.y + btn_ally.h/2, TEXT_COLOR);

    // Кнопка Save Anim
    SDL_Color save_anim_col = BUTTON_COLOR;
    if (logical_mx >= btn_save_anim.x && logical_mx < btn_save_anim.x+btn_save_anim.w &&
        logical_my >= btn_save_anim.y && logical_my < btn_save_anim.y+btn_save_anim.h)
        save_anim_col = BUTTON_HOVER;
    SDL_SetRenderDrawColor(ed->renderer, save_anim_col.r, save_anim_col.g, save_anim_col.b, 255);
    SDL_RenderFillRect(ed->renderer, &btn_save_anim);
    draw_text_centered(ed->renderer, ed->font, "Save Anim",
                       btn_save_anim.x + btn_save_anim.w/2, btn_save_anim.y + btn_save_anim.h/2, TEXT_COLOR);

    // Правая область предпросмотра
    if (ed->entry_count > 0) {
        BattleEntry *entry = &ed->entries[ed->current_index];

        char info[128];
        snprintf(info, sizeof(info), "Battle: %s", entry->name);
        draw_text_centered(ed->renderer, ed->font, info,
                           PREVIEW_X + PREVIEW_W/2, PREVIEW_Y - 5, TEXT_COLOR);

        SDL_Rect preview_rect = {PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H};

        // === 1. Фон битвы (всегда, если есть) ===
        if (ed->bg_tex) {
            int tex_w, tex_h;
            SDL_QueryTexture(ed->bg_tex, NULL, NULL, &tex_w, &tex_h);
            float scale = fminf((float)PREVIEW_W / tex_w, (float)(PREVIEW_H - 288) / tex_h);
            int draw_w = (int)(tex_w * scale);
            int draw_h = (int)(tex_h * scale);
            SDL_Rect dst = {
                PREVIEW_X + (PREVIEW_W - draw_w)/2,
                PREVIEW_Y + 144 + ((PREVIEW_H - 288) - draw_h)/2,
                draw_w, draw_h
            };
            SDL_SetRenderDrawColor(ed->renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(ed->renderer, &preview_rect);
            SDL_RenderCopy(ed->renderer, ed->bg_tex, NULL, &dst);
        } else {
            SDL_SetRenderDrawColor(ed->renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(ed->renderer, &preview_rect);
            draw_text_centered(ed->renderer, ed->font, "No background",
                               PREVIEW_X + PREVIEW_W/2, PREVIEW_Y + PREVIEW_H/2,
                               (SDL_Color){150,150,150,255});
        }

        // Рамка вокруг предпросмотра
        SDL_SetRenderDrawColor(ed->renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(ed->renderer, &preview_rect);

        // === 2. Чёрные полосы и спрайт анимации ===
        if (ed->anim_set.loaded) {
            // Чёрные полосы 144px сверху и снизу
            SDL_Rect top_bar = {PREVIEW_X, PREVIEW_Y, PREVIEW_W, 144};
            SDL_Rect bot_bar = {PREVIEW_X, PREVIEW_Y + PREVIEW_H - 144, PREVIEW_W, 144};
            SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(ed->renderer, &top_bar);
            SDL_RenderFillRect(ed->renderer, &bot_bar);

            AnimPhase *phase = &ed->anim_set.phases[ed->anim_set.current_phase];
            if (phase->frame_count > 0) {
                int idx = ed->anim_set.current_frame % phase->frame_count;

                SDL_Texture *tex = phase->frames[idx];
                if (tex) {
                    int tw, th;
                    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);

                    float sprite_scale = 0.5f;
                    float fit_scale = fminf((float)(PREVIEW_W - 20) / (tw * sprite_scale),
                                            (float)(PREVIEW_H - 288) / (th * sprite_scale));
                    if (fit_scale > 1.0f) fit_scale = 1.0f;

                    int dw = (int)(tw * sprite_scale * fit_scale);
                    int dh = (int)(th * sprite_scale * fit_scale);

                    int center_x = PREVIEW_X + PREVIEW_W/2;
                    int center_y = PREVIEW_Y + 144 + (PREVIEW_H - 288)/2;

                    int cx = center_x + (int)(phase->offset_x * sprite_scale * fit_scale);
                    int cy = center_y + (int)(phase->offset_y * sprite_scale * fit_scale) - dh/2;

                    SDL_Rect dst = { cx - dw/2, cy, dw, dh };

                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                    SDL_RenderCopy(ed->renderer, tex, NULL, &dst);
                }
            }

            // Информация о фазе, кадре, длительности и offset
            float dur = (phase->frame_count > 0) ? phase->frame_durations[ed->anim_set.current_frame % phase->frame_count] : 0.0f;
            char phase_info[256];
            snprintf(phase_info, sizeof(phase_info), "%s frame %d/%d  dur: %.2f  offset(%d, %d)",
                     phase->name,
                     ed->anim_set.current_frame + 1,
                     phase->frame_count,
                     dur,
                     phase->offset_x, phase->offset_y);
            draw_text_centered(ed->renderer, ed->font, phase_info,
                               PREVIEW_X + PREVIEW_W/2,
                               PREVIEW_Y + PREVIEW_H + 15, TEXT_COLOR);

            // Подсказка управления
            draw_text_centered(ed->renderer, ed->font,
                "Arrows:nav  Ctrl+Arrows:offset  W/S:duration  SaveAnim:save",
                PREVIEW_X + PREVIEW_W/2,
                PREVIEW_Y + PREVIEW_H + 40,
                (SDL_Color){180, 180, 200, 255});
        } else {
            if (entry->background[0]) {
                const char *fname = strrchr(entry->background, '\\');
                if (!fname) fname = strrchr(entry->background, '/');
                if (fname) fname++; else fname = entry->background;
                char bg_txt[1024];
                snprintf(bg_txt, sizeof(bg_txt), "File: %s", fname);
                draw_text_centered(ed->renderer, ed->font, bg_txt,
                                   PREVIEW_X + PREVIEW_W/2,
                                   PREVIEW_Y + PREVIEW_H + 20, TEXT_COLOR);
            }
        }
    } else {
        draw_text_centered(ed->renderer, ed->font, "No battles found",
                           PREVIEW_X + PREVIEW_W/2, PREVIEW_Y + PREVIEW_H/2, TEXT_COLOR);
    }

    SDL_RenderPresent(ed->renderer);
}

// ─── Обработка событий ────────────────────────
void handle_input(Editor *ed, bool *running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *running = false; return; }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) { *running = false; return; }

        int raw_mx, raw_my;
        SDL_GetMouseState(&raw_mx, &raw_my);
        int win_w, win_h;
        SDL_GetWindowSize(ed->window, &win_w, &win_h);
        int mx = (int)((float)raw_mx * LOGICAL_W / win_w);
        int my = (int)((float)raw_my * LOGICAL_H / win_h);

        // Навигация и редактирование анимации (только если загружена)
        if (ed->anim_set.loaded) {
            AnimPhase *phase = &ed->anim_set.phases[ed->anim_set.current_phase];
            bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;

            if (e.type == SDL_KEYDOWN) {
                // Изменение offset стрелками с Ctrl
                if (ctrl) {
                    int step = (SDL_GetModState() & KMOD_SHIFT) ? 10 : 1;
                    switch (e.key.keysym.sym) {
                        case SDLK_UP:    phase->offset_y -= step; break;
                        case SDLK_DOWN:  phase->offset_y += step; break;
                        case SDLK_LEFT:  phase->offset_x -= step; break;
                        case SDLK_RIGHT: phase->offset_x += step; break;
                        default: break;
                    }
                }
                // Навигация без Ctrl
                else {
                    switch (e.key.keysym.sym) {
                        case SDLK_LEFT:
                            ed->anim_set.current_frame--;
                            if (ed->anim_set.current_frame < 0)
                                ed->anim_set.current_frame = phase->frame_count - 1;
                            break;
                        case SDLK_RIGHT:
                            ed->anim_set.current_frame++;
                            if (ed->anim_set.current_frame >= phase->frame_count)
                                ed->anim_set.current_frame = 0;
                            break;
                        case SDLK_UP:
                            ed->anim_set.current_phase--;
                            if (ed->anim_set.current_phase < 0) ed->anim_set.current_phase = 2;
                            ed->anim_set.current_frame = 0;
                            break;
                        case SDLK_DOWN:
                            ed->anim_set.current_phase = (ed->anim_set.current_phase + 1) % 3;
                            ed->anim_set.current_frame = 0;
                            break;
                        // Изменение длительности кадра
                        case SDLK_w: {
                            if (phase->frame_count > 0) {
                                int idx = ed->anim_set.current_frame % phase->frame_count;
                                float step = (SDL_GetModState() & KMOD_SHIFT) ? 0.01f : 0.05f;
                                phase->frame_durations[idx] += step;
                                if (phase->frame_durations[idx] > 5.0f) phase->frame_durations[idx] = 5.0f;
                            }
                            break;
                        }
                        case SDLK_s: {
                            if (phase->frame_count > 0) {
                                int idx = ed->anim_set.current_frame % phase->frame_count;
                                float step = (SDL_GetModState() & KMOD_SHIFT) ? 0.01f : 0.05f;
                                phase->frame_durations[idx] -= step;
                                if (phase->frame_durations[idx] < 0.01f) phase->frame_durations[idx] = 0.01f;
                            }
                            break;
                        }
                        default: break;
                    }
                }
            }
        }

        // Колесо мыши для прокрутки списка
        if (e.type == SDL_MOUSEWHEEL) {
            ed->list_scroll -= e.wheel.y;
            int max_vis = (LOGICAL_H - 60) / (FONT_SIZE + 4);
            if (ed->list_scroll < 0) ed->list_scroll = 0;
            if (ed->list_scroll > ed->entry_count - max_vis)
                ed->list_scroll = ed->entry_count - max_vis;
        }

        // Клик левой кнопкой
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            // Save entries
            SDL_Rect btn_save = {10, LOGICAL_H - 40, 70, 28};
            if (mx >= btn_save.x && mx < btn_save.x+btn_save.w &&
                my >= btn_save.y && my < btn_save.y+btn_save.h) {
                save_entries(ed);
                printf("Entries saved.\n");
                continue;
            }

            // Change BG
            SDL_Rect btn_bg = {85, LOGICAL_H - 40, 100, 28};
            if (mx >= btn_bg.x && mx < btn_bg.x+btn_bg.w &&
                my >= btn_bg.y && my < btn_bg.y+btn_bg.h) {
                if (ed->entry_count > 0) {
                    char path[512];
                    char abs_initial[MAX_PATH];
                    GetFullPathNameA("../assets/battles/backgrounds", MAX_PATH, abs_initial, NULL);
                    if (open_file_dialog(path, sizeof(path), abs_initial)) {
                        char rel[512];
                        get_relative_path(path, rel, sizeof(rel));
                        safe_strcpy(ed->entries[ed->current_index].background,
                                    sizeof(ed->entries[ed->current_index].background), rel);
                        load_background_texture(ed, rel);
                        printf("Background changed to: %s\n", rel);
                    }
                }
                continue;
            }

            // Enemy Anim
            SDL_Rect btn_enemy = {190, LOGICAL_H - 40, 110, 28};
            if (mx >= btn_enemy.x && mx < btn_enemy.x+btn_enemy.w &&
                my >= btn_enemy.y && my < btn_enemy.y+btn_enemy.h) {
                if (ed->entry_count > 0) {
                    char path[512];
                    char abs_initial[MAX_PATH];
                    GetFullPathNameA("../assets/battles/battlesprites/enemies", MAX_PATH, abs_initial, NULL);
                    if (open_json_dialog(path, sizeof(path), abs_initial)) {
                        printf("Loading enemy animation: %s\n", path);
                        load_animation_from_json(ed, path);
                    }
                }
                continue;
            }

            // Ally Anim
            SDL_Rect btn_ally = {305, LOGICAL_H - 40, 110, 28};
            if (mx >= btn_ally.x && mx < btn_ally.x+btn_ally.w &&
                my >= btn_ally.y && my < btn_ally.y+btn_ally.h) {
                if (ed->entry_count > 0) {
                    char path[512];
                    char abs_initial[MAX_PATH];
                    GetFullPathNameA("../assets/battles/battlesprites/allies", MAX_PATH, abs_initial, NULL);
                    if (open_json_dialog(path, sizeof(path), abs_initial)) {
                        printf("Loading ally animation: %s\n", path);
                        load_animation_from_json(ed, path);
                    }
                }
                continue;
            }

            // Save Anim
            SDL_Rect btn_save_anim = {420, LOGICAL_H - 40, 100, 28};
            if (mx >= btn_save_anim.x && mx < btn_save_anim.x+btn_save_anim.w &&
                my >= btn_save_anim.y && my < btn_save_anim.y+btn_save_anim.h) {
                if (ed->anim_set.loaded) {
                    save_animation_to_json(ed);
                } else {
                    printf("No animation loaded.\n");
                }
                continue;
            }

            // Выбор элемента списка – сбрасывает анимацию
            int line_height = FONT_SIZE + 4;
            int max_vis = (LOGICAL_H - 60) / line_height;
            if (mx >= 0 && mx < LEFT_PANEL_W) {
                for (int i = 0; i < max_vis; i++) {
                    int idx = ed->list_scroll + i;
                    if (idx >= ed->entry_count) break;
                    int y = 50 + i * line_height;
                    if (my >= y && my < y + line_height) {
                        if (idx != ed->current_index) {
                            ed->current_index = idx;
                            load_background_texture(ed, ed->entries[idx].background);
                            free_animation_set(&ed->anim_set);
                        }
                        break;
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) return 1;
    if (TTF_Init() != 0) return 1;

    Editor ed;
    memset(&ed, 0, sizeof(ed));
    ed.current_index = 0;
    ed.list_scroll = 0;
    ed.bg_tex = NULL;
    ed.anim_set.loaded = false;

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int screenW = workArea.right - workArea.left;
    int screenH = workArea.bottom - workArea.top;
    int winW = LOGICAL_W, winH = LOGICAL_H;
    if (screenW < LOGICAL_W || screenH < LOGICAL_H) {
        float scale = fminf((float)screenW / LOGICAL_W, (float)screenH / LOGICAL_H) * 0.92f;
        winW = (int)(LOGICAL_W * scale);
        winH = (int)(LOGICAL_H * scale);
    }

    ed.window = SDL_CreateWindow("Battle Scenes Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!ed.window) return 1;

    ed.renderer = SDL_CreateRenderer(ed.window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ed.renderer) return 1;

    SDL_RenderSetLogicalSize(ed.renderer, LOGICAL_W, LOGICAL_H);
    SDL_SetWindowMinimumSize(ed.window, 800, 600);

    ed.font = TTF_OpenFont("Font/NotoSans-Regular.ttf", FONT_SIZE);
    if (!ed.font) ed.font = TTF_OpenFont("Font/main.ttf", FONT_SIZE);
    if (!ed.font) ed.font = TTF_OpenFont("Font/arial.ttf", FONT_SIZE);
    if (!ed.font) ed.font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", FONT_SIZE);
    if (!ed.font) { printf("No font\n"); return 1; }

    load_entries(&ed);
    if (ed.entry_count > 0) {
        ed.current_index = 0;
        load_background_texture(&ed, ed.entries[0].background);
    }

    bool running = true;
    while (running) {
        handle_input(&ed, &running);
        draw_ui(&ed);
        SDL_Delay(16);
    }

    free_animation_set(&ed.anim_set);
    if (ed.bg_tex) SDL_DestroyTexture(ed.bg_tex);
    if (ed.font) TTF_CloseFont(ed.font);
    free(ed.entries);
    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}