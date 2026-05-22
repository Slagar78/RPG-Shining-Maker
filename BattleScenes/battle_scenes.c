// Battle Scenes Editor – финальная версия
// Масштаб персонажей строго соответствует фону (как в игре)
// Раздельные строки для idle/attack/dodge, чекбокс Anim справа в заголовке
// Удержание кнопок для плавного изменения X/Y/Dur
// Добавлена поддержка ground (земли) для союзника

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
#define LOGICAL_W           1024
#define LOGICAL_H           768
#define LEFT_PANEL_W        220
#define PREVIEW_X           (LEFT_PANEL_W + 20)
#define PREVIEW_Y           8
#define PREVIEW_W           (LOGICAL_W - LEFT_PANEL_W - 40)
#define PREVIEW_H           576
#define FONT_SIZE           16

// ─── Цвета ────────────────────────────────────
static const SDL_Color BG_COLOR          = { 25, 25, 40, 255 };
static const SDL_Color PANEL_BG          = { 35, 35, 55, 255 };
static const SDL_Color BUTTON_COLOR      = { 70, 50, 120, 255 };
static const SDL_Color BUTTON_HOVER      = { 110, 70, 180, 255 };
static const SDL_Color TEXT_COLOR        = { 220, 220, 240, 255 };
static const SDL_Color HIGHLIGHT_COLOR   = { 255, 255, 100, 255 };
static const SDL_Color FIELD_BG          = { 40, 40, 60, 255 };
static const SDL_Color CHECKBOX_ON       = { 100, 200, 100, 255 };
static const SDL_Color CHECKBOX_OFF      = { 100, 100, 100, 255 };

// ─── Запись битвы ────────────────────────────
typedef struct {
    char folder[64];
    char name[64];
    char map_id[64];
    char music[256];
    float music_volume;
    int battle_x, battle_y;
    int battle_width, battle_height;
    char background[512];
    char ground[512];   // +++ GROUND
} BattleEntry;

// ─── Анимация ────────────────────────────────
#define MAX_ANIM_FRAMES 32
typedef struct {
    char name[32];
    int frame_count;
    SDL_Texture *frames[MAX_ANIM_FRAMES];
    float frame_durations[MAX_ANIM_FRAMES];
    int offset_x[MAX_ANIM_FRAMES];
    int offset_y[MAX_ANIM_FRAMES];
    bool animate;
} AnimPhase;

typedef struct {
    AnimPhase phases[3];
    int current_phase;
    int current_frame[3];
    bool loaded;
    char json_path[512];
    float anim_timer;
} AnimationSet;

// ─── Редактор ─────────────────────────────────
typedef struct Editor {
    BattleEntry *entries;
    int entry_count;
    int current_index;
    int list_scroll;

    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    SDL_Texture  *bg_tex;
    SDL_Texture  *ground_tex;   // +++ GROUND

    AnimationSet ally_anim;
    AnimationSet enemy_anim;
    int active_slot;
    int active_phase;

    // для автоповтора
    Uint32 repeat_timer;
    int repeat_button_id;
    SDL_Rect repeat_button_rect;
    AnimationSet *repeat_as;
    int repeat_phase;
    int repeat_what; // 0=X-,1=X+,2=Y-,3=Y+,4=Dur-,5=Dur+

    // для текстового ввода
    char input_buffer[16];
    int input_len;
    bool input_active;          // true – редактируем поле
    int input_target;           // 0=X, 1=Y
    AnimationSet *input_as;
    int input_phase;
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
        cJSON *gr = cJSON_GetObjectItem(item, "ground");   // +++ GROUND
        safe_strcpy(be->ground, sizeof(be->ground), gr ? gr->valuestring : "");
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
        cJSON_AddStringToObject(item, "ground", be->ground);   // +++ GROUND
        cJSON_AddItemToArray(arr, item);
    }
    char *str = cJSON_Print(arr);
    FILE *f = fopen("../data/battles/entries.json", "wb");
    if (f) { fputs(str, f); fclose(f); }
    free(str);
    cJSON_Delete(arr);
}

// ─── Загрузка фона ───────────────────────────
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

// ─── Загрузка ground ─────────────────────────
void load_ground_texture(Editor *ed, const char *rel_path) {
    if (ed->ground_tex) { SDL_DestroyTexture(ed->ground_tex); ed->ground_tex = NULL; }
    if (!rel_path || !rel_path[0]) return;
    char full[512];
    snprintf(full, sizeof(full), "../%s", rel_path);
    SDL_Surface *surf = IMG_Load(full);
    if (!surf) return;
    ed->ground_tex = SDL_CreateTextureFromSurface(ed->renderer, surf);
    SDL_FreeSurface(surf);
}

// ─── Загрузка / сохранение анимаций ──────────
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
    for (int i = 0; i < 3; i++) as->current_frame[i] = 0;
    as->json_path[0] = '\0';
}

bool load_animation_from_json(AnimationSet *as, SDL_Renderer *renderer, const char *json_path) {
    free_animation_set(as);

    FILE *f = fopen(json_path, "r");
    if (!f) { printf("Cannot open animation JSON: %s\n", json_path); return false; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root) { printf("Failed to parse JSON: %s\n", json_path); return false; }

    char base_dir[512];
    extract_directory(json_path, base_dir, sizeof(base_dir));
    safe_strcpy(as->json_path, sizeof(as->json_path), json_path);

    const char *phase_keys[] = {"idle", "attack", "dodge"};
    for (int p = 0; p < 3; p++) {
        cJSON *phase = cJSON_GetObjectItem(root, phase_keys[p]);
        if (!phase) continue;
        cJSON *frames = cJSON_GetObjectItem(phase, "frames");
        if (!cJSON_IsArray(frames)) continue;

        int n = cJSON_GetArraySize(frames);
        if (n > MAX_ANIM_FRAMES) n = MAX_ANIM_FRAMES;
        AnimPhase *ap = &as->phases[p];
        safe_strcpy(ap->name, sizeof(ap->name), phase_keys[p]);
        ap->frame_count = n;

        // Значения по умолчанию из старого формата (общие на фазу)
        cJSON *phase_ox = cJSON_GetObjectItem(phase, "offset_x");
        cJSON *phase_oy = cJSON_GetObjectItem(phase, "offset_y");
        int default_ox = phase_ox ? phase_ox->valueint : 0;
        int default_oy = phase_oy ? phase_oy->valueint : 0;

        for (int i = 0; i < n; i++) {
            cJSON *frame = cJSON_GetArrayItem(frames, i);
            cJSON *file_json = cJSON_GetObjectItem(frame, "file");
            cJSON *dur_json  = cJSON_GetObjectItem(frame, "duration");
            if (!file_json) continue;
            const char *filename = file_json->valuestring;
            ap->frame_durations[i] = dur_json ? (float)dur_json->valuedouble : 0.3f;
            
            cJSON *frame_ox = cJSON_GetObjectItem(frame, "offset_x");
            cJSON *frame_oy = cJSON_GetObjectItem(frame, "offset_y");
            ap->offset_x[i] = frame_ox ? frame_ox->valueint : default_ox;
            ap->offset_y[i] = frame_oy ? frame_oy->valueint : default_oy;

            char full_path[768];
            snprintf(full_path, sizeof(full_path), "%s%s", base_dir, filename);
            SDL_Surface *surf = IMG_Load(full_path);
            if (surf) {
                ap->frames[i] = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FreeSurface(surf);
            } else {
                printf("Failed to load frame: %s\n", full_path);
                ap->frames[i] = NULL;
            }
        }

        ap->animate = false;
    }
    cJSON_Delete(root);

    as->current_phase = 0;
    for (int i = 0; i < 3; i++) as->current_frame[i] = 0;
    as->loaded = true;
    printf("Animation loaded: %s\n", json_path);
    return true;
}

void save_animation_to_json(AnimationSet *as) {
    if (!as->loaded || !as->json_path[0]) {
        printf("No animation loaded to save.\n");
        return;
    }
    FILE *f = fopen(as->json_path, "r");
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

    const char *phase_keys[] = {"idle", "attack", "dodge"};
    for (int p = 0; p < 3; p++) {
        AnimPhase *ap = &as->phases[p];
        if (ap->frame_count <= 0) continue;

        cJSON *phase = cJSON_GetObjectItem(root, phase_keys[p]);
        if (!phase) {
            phase = cJSON_CreateObject();
            cJSON_AddItemToObject(root, phase_keys[p], phase);
        }

        cJSON *frames = cJSON_GetObjectItem(phase, "frames");
        if (cJSON_IsArray(frames)) {
            int n = cJSON_GetArraySize(frames);
            for (int i = 0; i < n && i < ap->frame_count; i++) {
                cJSON *frame = cJSON_GetArrayItem(frames, i);
                if (frame) {
                    // Удаляем старые значения и добавляем новые с округлением
                    double clean_dur = round(ap->frame_durations[i] * 100.0) / 100.0;
                    cJSON_DeleteItemFromObject(frame, "duration");
                    cJSON_AddNumberToObject(frame, "duration", clean_dur);

                    cJSON_DeleteItemFromObject(frame, "offset_x");
                    cJSON_AddNumberToObject(frame, "offset_x", ap->offset_x[i]);

                    cJSON_DeleteItemFromObject(frame, "offset_y");
                    cJSON_AddNumberToObject(frame, "offset_y", ap->offset_y[i]);
                }
            }
        }
    }

    char *str = cJSON_Print(root);
    f = fopen(as->json_path, "wb");
    if (f) { fputs(str, f); fclose(f); printf("Animation saved: %s\n", as->json_path); }
    free(str);
    cJSON_Delete(root);
}

void rescan_frames(AnimationSet *as, SDL_Renderer *renderer) {
    if (!as->loaded || !as->json_path[0]) {
        printf("No animation loaded, cannot rescan.\n");
        return;
    }
    char folder[512];
    safe_strcpy(folder, sizeof(folder), as->json_path);
    char *slash = strrchr(folder, '\\');
    if (!slash) slash = strrchr(folder, '/');
    if (slash) *(slash + 1) = '\0';
    else folder[0] = '\0';

    // Очищаем старые текстуры, но сохраняем путь
    char saved_path[512];
    safe_strcpy(saved_path, sizeof(saved_path), as->json_path);
    free_animation_set(as);
    safe_strcpy(as->json_path, sizeof(as->json_path), saved_path);
    as->loaded = true;

    const char *prefix[3] = {"idle", "frame_attack", "frame_dodge"};
    const char *phase_keys[3] = {"idle", "attack", "dodge"};

    for (int p = 0; p < 3; p++) {
        AnimPhase *ap = &as->phases[p];
        safe_strcpy(ap->name, sizeof(ap->name), phase_keys[p]);
        int max_frames = 0;
        for (int n = 1; n < MAX_ANIM_FRAMES; n++) {
            char filename[256];
            snprintf(filename, sizeof(filename), "%s%s_%d.png", folder, prefix[p], n);
            FILE *test = fopen(filename, "rb");
            if (!test) break;
            fclose(test);
            max_frames = n;
        }
        ap->frame_count = max_frames;
        for (int i = 0; i < max_frames; i++) {
            char filename[256];
            snprintf(filename, sizeof(filename), "%s%s_%d.png", folder, prefix[p], i + 1);
            SDL_Surface *surf = IMG_Load(filename);
            if (surf) {
                ap->frames[i] = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FreeSurface(surf);
            } else {
                ap->frames[i] = NULL;
            }
            ap->frame_durations[i] = 0.3f;   // по умолчанию
            ap->offset_x[i] = 0;
            ap->offset_y[i] = 0;
        }
    }
    printf("Frames rescanned for %s\n", as->json_path);
}

// ─── Отрисовка интерфейса ─────────────────────
void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, SDL_Color color) {
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, color);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_Rect dst = { cx - s->w/2, cy - s->h/2, s->w, s->h };
    SDL_RenderCopy(ren, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

bool draw_button(SDL_Renderer *ren, TTF_Font *font, SDL_Rect rect, const char *text, int mx, int my) {
    bool hover = (mx >= rect.x && mx < rect.x+rect.w && my >= rect.y && my < rect.y+rect.h);
    SDL_Color col = hover ? BUTTON_HOVER : BUTTON_COLOR;
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, 255);
    SDL_RenderFillRect(ren, &rect);
    SDL_SetRenderDrawColor(ren, 200,200,200,255);
    SDL_RenderDrawRect(ren, &rect);
    draw_text_centered(ren, font, text, rect.x + rect.w/2, rect.y + rect.h/2, TEXT_COLOR);
    return hover;
}

void draw_arrow_button(SDL_Renderer *ren, TTF_Font *font, SDL_Rect rect, const char *symbol, int mx, int my) {
    bool hover = (mx >= rect.x && mx < rect.x+rect.w && my >= rect.y && my < rect.y+rect.h);
    SDL_Color col = hover ? BUTTON_HOVER : BUTTON_COLOR;
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, 255);
    SDL_RenderFillRect(ren, &rect);
    SDL_SetRenderDrawColor(ren, 200,200,200,255);
    SDL_RenderDrawRect(ren, &rect);
    draw_text_centered(ren, font, symbol, rect.x + rect.w/2, rect.y + rect.h/2, TEXT_COLOR);
}

void draw_checkbox(SDL_Renderer *ren, SDL_Rect rect, bool checked, int mx, int my) {
    bool hover = (mx >= rect.x && mx < rect.x+rect.w && my >= rect.y && my < rect.y+rect.h);
    SDL_Color col = checked ? CHECKBOX_ON : CHECKBOX_OFF;
    if (hover) { col.r += 30; col.g += 30; col.b += 30; }
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, 255);
    SDL_RenderFillRect(ren, &rect);
    SDL_SetRenderDrawColor(ren, 255,255,255,255);
    SDL_RenderDrawRect(ren, &rect);
    if (checked) {
        SDL_SetRenderDrawColor(ren, 255,255,255,255);
        SDL_RenderDrawLine(ren, rect.x+3, rect.y+3, rect.x+rect.w-3, rect.y+rect.h-3);
        SDL_RenderDrawLine(ren, rect.x+rect.w-3, rect.y+3, rect.x+3, rect.y+rect.h-3);
    }
}

void draw_ui(Editor *ed) {
    SDL_SetRenderDrawColor(ed->renderer, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, 255);
    SDL_RenderClear(ed->renderer);

    // Левая панель
    SDL_Rect panel = {0, 0, LEFT_PANEL_W, LOGICAL_H};
    SDL_SetRenderDrawColor(ed->renderer, PANEL_BG.r, PANEL_BG.g, PANEL_BG.b, 255);
    SDL_RenderFillRect(ed->renderer, &panel);

    draw_text_centered(ed->renderer, ed->font, "BATTLES", LEFT_PANEL_W/2, 15, TEXT_COLOR);

    int line_height = FONT_SIZE + 4;
    int max_vis = (LOGICAL_H - 160) / line_height;
    int total = ed->entry_count;
    if (ed->list_scroll > total - max_vis) ed->list_scroll = total - max_vis;
    if (ed->list_scroll < 0) ed->list_scroll = 0;

    for (int i = 0; i < max_vis; i++) {
        int idx = ed->list_scroll + i;
        if (idx >= total) break;
        int y = 40 + i * line_height;
        SDL_Color color = (idx == ed->current_index) ? HIGHLIGHT_COLOR : TEXT_COLOR;
        draw_text_centered(ed->renderer, ed->font, ed->entries[idx].name,
                           LEFT_PANEL_W/2, y + line_height/2, color);
    }

    // Кнопки в левой панели
    int btn_y_base = LOGICAL_H - 130;
    SDL_Rect btn_save    = {5,  btn_y_base, 100, 26};
    SDL_Rect btn_bg      = {110,btn_y_base, 105, 26};
    SDL_Rect btn_load_ally = {5,  btn_y_base+32, 100, 26};
    SDL_Rect btn_load_enemy = {110,btn_y_base+32, 105, 26};
    SDL_Rect btn_save_ally = {5,  btn_y_base+64, 100, 26};
    SDL_Rect btn_save_enemy= {110,btn_y_base+64, 105, 26};
    SDL_Rect btn_rescan = {5, btn_y_base+96, 210, 26};  // шире, чтобы вместить "Rescan Frames"

    int mx, my;
    SDL_GetMouseState(&mx, &my);
    int win_w, win_h;
    SDL_GetWindowSize(ed->window, &win_w, &win_h);
    int logical_mx = (int)((float)mx * LOGICAL_W / win_w);
    int logical_my = (int)((float)my * LOGICAL_H / win_h);

    draw_button(ed->renderer, ed->font, btn_save, "Save", logical_mx, logical_my);
    draw_button(ed->renderer, ed->font, btn_bg, "Change BG", logical_mx, logical_my);
    draw_button(ed->renderer, ed->font, btn_load_ally, "Load Ally", logical_mx, logical_my);
    draw_button(ed->renderer, ed->font, btn_load_enemy, "Load Enemy", logical_mx, logical_my);
    draw_button(ed->renderer, ed->font, btn_save_ally, "Save Ally", logical_mx, logical_my);
    draw_button(ed->renderer, ed->font, btn_save_enemy, "Save Enemy", logical_mx, logical_my);
    draw_button(ed->renderer, ed->font, btn_rescan, "Rescan Frames", logical_mx, logical_my);

    // Правая область предпросмотра
    if (ed->entry_count > 0) {
        BattleEntry *entry = &ed->entries[ed->current_index];
        char info[128];
        snprintf(info, sizeof(info), "Battle: %s", entry->name);
        draw_text_centered(ed->renderer, ed->font, info, PREVIEW_X + PREVIEW_W/2, PREVIEW_Y - 5, TEXT_COLOR);

        SDL_Rect preview_rect = {PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H};

        // === Фон как в игре ===
        float bg_scale = (float)PREVIEW_W / 1152.0f;
        int work_h = (int)(672.0f * bg_scale);
        int bar_h = (PREVIEW_H - work_h) / 2;
        if (bar_h < 0) bar_h = 0;

        if (ed->bg_tex) {
            SDL_SetRenderDrawColor(ed->renderer, 255, 0, 255, 255);
            SDL_RenderFillRect(ed->renderer, &preview_rect);

            SDL_Rect top_bar = {PREVIEW_X, PREVIEW_Y, PREVIEW_W, bar_h};
            SDL_Rect bot_bar = {PREVIEW_X, PREVIEW_Y + PREVIEW_H - bar_h, PREVIEW_W, bar_h};
            SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(ed->renderer, &top_bar);
            SDL_RenderFillRect(ed->renderer, &bot_bar);

            int tex_w, tex_h;
            SDL_QueryTexture(ed->bg_tex, NULL, NULL, &tex_w, &tex_h);
            int draw_w = (int)(tex_w * bg_scale);
            int draw_h = (int)(tex_h * bg_scale);
            SDL_Rect dst_bg = {
                PREVIEW_X + (PREVIEW_W - draw_w) / 2,
                PREVIEW_Y + bar_h + (work_h - draw_h) / 2,
                draw_w, draw_h
            };
            SDL_RenderCopy(ed->renderer, ed->bg_tex, NULL, &dst_bg);
        } else {
            SDL_SetRenderDrawColor(ed->renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(ed->renderer, &preview_rect);
            draw_text_centered(ed->renderer, ed->font, "No background",
                               PREVIEW_X + PREVIEW_W/2, PREVIEW_Y + PREVIEW_H/2,
                               (SDL_Color){150,150,150,255});
        }
        SDL_SetRenderDrawColor(ed->renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(ed->renderer, &preview_rect);

        // === GROUND (только под союзником, если текстура загружена) ===
        if (ed->ground_tex) {
            // Параметры из игры (Ruby BattleScene)
            const int ALLY_X = 750;
            const int ALLY_Y = 480;
            const float GROUND_SCALE = 2.0f;
            const int GROUND_OFFSET_X = 136;
            const int GROUND_OFFSET_Y = 422;
            const int TOP_BAR_H = 144;

            int gw, gh;
            SDL_QueryTexture(ed->ground_tex, NULL, NULL, &gw, &gh);
            int scaled_w = (int)(gw * GROUND_SCALE);
            int scaled_h = (int)(gh * GROUND_SCALE);

            // Позиция земли в игровых координатах
            int game_gx = ALLY_X - scaled_w / 2 + GROUND_OFFSET_X;
            int game_gy = ALLY_Y - scaled_h + GROUND_OFFSET_Y;

            // Пересчёт в координаты предпросмотра (с учётом bar_h)
            int preview_gx = PREVIEW_X + (int)(game_gx * bg_scale);
            int preview_gy = PREVIEW_Y + bar_h + (int)((game_gy - TOP_BAR_H) * bg_scale);

            SDL_Rect ground_rect = {
                preview_gx,
                preview_gy,
                (int)(scaled_w * bg_scale),
                (int)(scaled_h * bg_scale)
            };
            SDL_RenderCopy(ed->renderer, ed->ground_tex, NULL, &ground_rect);
        }

        // === Спрайты союзника и врага ===        
        AnimationSet *sets[2] = {&ed->ally_anim, &ed->enemy_anim};

        for (int s = 0; s < 2; s++) {
            AnimationSet *as = sets[s];
            if (!as->loaded) continue;
            AnimPhase *phase = &as->phases[as->current_phase];
            if (phase->frame_count == 0) continue;
            int idx = as->current_frame[as->current_phase] % phase->frame_count;
            SDL_Texture *tex = phase->frames[idx];
            if (!tex) continue;

            int tw, th;
            SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
            int dw = (int)(tw * bg_scale);
            int dh = (int)(th * bg_scale);
            int cur_frame = as->current_frame[as->current_phase] % phase->frame_count;

            int cx, cy;

            if (s == 0) {
                // Союзник: используем координаты из Ruby (ALLY_X=750, ALLY_Y=480)
                int ally_game_x = 750;
                int ally_game_y = 480;
                int top_bar_height = 144;
                int ally_y_from_top_of_bg = ally_game_y - top_bar_height;

                int ally_base_x = PREVIEW_X + (int)(ally_game_x * bg_scale);
                int ally_base_y = PREVIEW_Y + bar_h + (int)(ally_y_from_top_of_bg * bg_scale);

                cx = ally_base_x + (int)(phase->offset_x[cur_frame] * bg_scale);
                cy = ally_base_y + (int)(phase->offset_y[cur_frame] * bg_scale);

                SDL_Rect dst = { cx, cy, dw, dh };
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                SDL_RenderCopy(ed->renderer, tex, NULL, &dst);
            }
            else {
                // Враг: фиксированные координаты (70, 410)
                int enemy_game_x = 124;
                int enemy_game_y = 370;
                int top_bar_height = 144;
                int enemy_y_from_top_of_bg = enemy_game_y - top_bar_height;

                int enemy_base_x = PREVIEW_X + (int)(enemy_game_x * bg_scale);
                int enemy_base_y = PREVIEW_Y + bar_h + (int)(enemy_y_from_top_of_bg * bg_scale);

                cx = enemy_base_x + (int)(phase->offset_x[cur_frame] * bg_scale);
                cy = enemy_base_y + (int)(phase->offset_y[cur_frame] * bg_scale);

                SDL_Rect dst = { cx, cy, dw, dh };
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                SDL_RenderCopy(ed->renderer, tex, NULL, &dst);
            }
        }
    } else {
        draw_text_centered(ed->renderer, ed->font, "No battles found",
                           PREVIEW_X + PREVIEW_W/2, PREVIEW_Y + PREVIEW_H/2, TEXT_COLOR);
    }

    // Нижняя панель управления (без изменений)
    int panel_y = PREVIEW_Y + PREVIEW_H + 4;
    int row_h = 20;
    const char *phase_names[] = {"Idle", "Attack", "Dodge"};

    for (int s = 0; s < 2; s++) {
        AnimationSet *as = (s == 0) ? &ed->ally_anim : &ed->enemy_anim;
        bool is_ally = (s == 0);
        int group_y = panel_y + s * (4 * row_h + 8);

        SDL_Color group_color = (ed->active_slot == s) ? HIGHLIGHT_COLOR : TEXT_COLOR;
        draw_text_centered(ed->renderer, ed->font, is_ally ? "Ally" : "Enemy",
                           PREVIEW_X + 30, group_y + row_h/2, group_color);

        for (int p = 0; p < 3; p++) {
            int y = group_y + row_h + 4 + p * row_h;
            AnimPhase *phase = &as->phases[p];
            bool has_frames = (phase->frame_count > 0);
            bool active = (ed->active_slot == s && ed->active_phase == p);

            if (active) {
                SDL_Rect row_bg = {PREVIEW_X, y, PREVIEW_W, row_h};
                SDL_SetRenderDrawColor(ed->renderer, 60, 60, 90, 255);
                SDL_RenderFillRect(ed->renderer, &row_bg);
            }

            SDL_Rect phase_rect = {PREVIEW_X + 5, y, 55, row_h};
            bool hover_phase = (logical_mx >= phase_rect.x && logical_mx < phase_rect.x+phase_rect.w &&
                                logical_my >= phase_rect.y && logical_my < phase_rect.y+phase_rect.h);
            SDL_Color phase_col = active ? HIGHLIGHT_COLOR : (hover_phase ? BUTTON_HOVER : TEXT_COLOR);
            draw_text_centered(ed->renderer, ed->font, phase_names[p],
                               phase_rect.x + phase_rect.w/2, y + row_h/2, phase_col);

            int cur_idx = 0;
            int ox = 0, oy = 0;
            float dur = 0.0f;
            char buf_frame[16] = "0/0";
            if (has_frames) {
                cur_idx = as->current_frame[p] % phase->frame_count;
                ox = phase->offset_x[cur_idx];
                oy = phase->offset_y[cur_idx];
                dur = phase->frame_durations[cur_idx];
                snprintf(buf_frame, sizeof(buf_frame), "%d/%d", as->current_frame[p] + 1, phase->frame_count);
            }

            char buf_x[16];
            snprintf(buf_x, sizeof(buf_x), "%d", ox);
            SDL_Rect x_label = {PREVIEW_X + 65, y, 20, row_h};
            draw_text_centered(ed->renderer, ed->font, "X:", x_label.x + x_label.w/2, y + row_h/2, TEXT_COLOR);


            // Offset X (редактируемый)
            SDL_Rect x_val = {PREVIEW_X + 90, y, 35, row_h};
            SDL_SetRenderDrawColor(ed->renderer, FIELD_BG.r, FIELD_BG.g, FIELD_BG.b, 255);
            SDL_RenderFillRect(ed->renderer, &x_val);
            if (ed->input_active && ed->input_target == 0 &&
                ed->input_as == as && ed->input_phase == p) {
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
            } else {
                SDL_SetRenderDrawColor(ed->renderer, 150,150,150,255);
            }
            SDL_RenderDrawRect(ed->renderer, &x_val);
            char show_x[16];
            if (ed->input_active && ed->input_target == 0 &&
                ed->input_as == as && ed->input_phase == p) {
                snprintf(show_x, sizeof(show_x), "%s", ed->input_buffer);
            } else {
                snprintf(show_x, sizeof(show_x), "%d", ox);
            }
            draw_text_centered(ed->renderer, ed->font, show_x,
                               x_val.x + x_val.w/2, y + row_h/2, TEXT_COLOR);
            SDL_Rect x_dec = {PREVIEW_X + 130, y, 16, row_h};
            SDL_Rect x_inc = {PREVIEW_X + 148, y, 16, row_h};
            draw_arrow_button(ed->renderer, ed->font, x_dec, "<", logical_mx, logical_my);
            draw_arrow_button(ed->renderer, ed->font, x_inc, ">", logical_mx, logical_my);

            // Offset Y (редактируемый)
            SDL_Rect y_val = {PREVIEW_X + 200, y, 35, row_h};
            SDL_SetRenderDrawColor(ed->renderer, FIELD_BG.r, FIELD_BG.g, FIELD_BG.b, 255);
            SDL_RenderFillRect(ed->renderer, &y_val);
            if (ed->input_active && ed->input_target == 1 &&
                ed->input_as == as && ed->input_phase == p) {
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
            } else {
                SDL_SetRenderDrawColor(ed->renderer, 150,150,150,255);
            }
            SDL_RenderDrawRect(ed->renderer, &y_val);
            char show_y[16];
            if (ed->input_active && ed->input_target == 1 &&
                ed->input_as == as && ed->input_phase == p) {
                snprintf(show_y, sizeof(show_y), "%s", ed->input_buffer);
            } else {
                snprintf(show_y, sizeof(show_y), "%d", oy);
            }
            draw_text_centered(ed->renderer, ed->font, show_y,
                               y_val.x + y_val.w/2, y + row_h/2, TEXT_COLOR);
            SDL_Rect y_dec = {PREVIEW_X + 240, y, 16, row_h};
            SDL_Rect y_inc = {PREVIEW_X + 258, y, 16, row_h};
            draw_arrow_button(ed->renderer, ed->font, y_dec, "<", logical_mx, logical_my);
            draw_arrow_button(ed->renderer, ed->font, y_inc, ">", logical_mx, logical_my);

            // Duration
            char buf_dur[16];
            snprintf(buf_dur, sizeof(buf_dur), "%.2f", dur);
            SDL_Rect dur_label = {PREVIEW_X + 285, y, 35, row_h};
            draw_text_centered(ed->renderer, ed->font, "Dur:", dur_label.x + dur_label.w/2, y + row_h/2, TEXT_COLOR);
            SDL_Rect dur_val = {PREVIEW_X + 325, y, 45, row_h};
            SDL_SetRenderDrawColor(ed->renderer, FIELD_BG.r, FIELD_BG.g, FIELD_BG.b, 255);
            SDL_RenderFillRect(ed->renderer, &dur_val);
            SDL_SetRenderDrawColor(ed->renderer, 150,150,150,255);
            SDL_RenderDrawRect(ed->renderer, &dur_val);
            draw_text_centered(ed->renderer, ed->font, buf_dur, dur_val.x + dur_val.w/2, y + row_h/2, TEXT_COLOR);
            SDL_Rect dur_dec = {PREVIEW_X + 375, y, 16, row_h};
            SDL_Rect dur_inc = {PREVIEW_X + 393, y, 16, row_h};
            draw_arrow_button(ed->renderer, ed->font, dur_dec, "-", logical_mx, logical_my);
            draw_arrow_button(ed->renderer, ed->font, dur_inc, "+", logical_mx, logical_my);

            SDL_Rect frame_rect = {PREVIEW_X + 430, y, 50, row_h};
            draw_text_centered(ed->renderer, ed->font, buf_frame, frame_rect.x + frame_rect.w/2, y + row_h/2, TEXT_COLOR);
            SDL_Rect frame_dec = {PREVIEW_X + 485, y, 16, row_h};
            SDL_Rect frame_inc = {PREVIEW_X + 503, y, 16, row_h};
            draw_arrow_button(ed->renderer, ed->font, frame_dec, "<", logical_mx, logical_my);
            draw_arrow_button(ed->renderer, ed->font, frame_inc, ">", logical_mx, logical_my);

            SDL_Rect anim_box = {PREVIEW_X + 535, y + (row_h - 14)/2, 14, 14};
            draw_checkbox(ed->renderer, anim_box, phase->animate, logical_mx, logical_my);
            draw_text_centered(ed->renderer, ed->font, "Anim", PREVIEW_X + 575, y + row_h/2, TEXT_COLOR);
        }
    }

    SDL_RenderPresent(ed->renderer);
}

// ─── Обработка событий ────────────────────────
void handle_input(Editor *ed, bool *running) {
    SDL_Event e;

    if (ed->repeat_button_id != 0) {
        Uint32 now = SDL_GetTicks();
        int raw_mx, raw_my;
        SDL_GetMouseState(&raw_mx, &raw_my);
        int win_w, win_h;
        SDL_GetWindowSize(ed->window, &win_w, &win_h);
        int mx = (int)((float)raw_mx * LOGICAL_W / win_w);
        int my = (int)((float)raw_my * LOGICAL_H / win_h);

        if (mx >= ed->repeat_button_rect.x && mx < ed->repeat_button_rect.x+ed->repeat_button_rect.w &&
            my >= ed->repeat_button_rect.y && my < ed->repeat_button_rect.y+ed->repeat_button_rect.h) {
            if (now - ed->repeat_timer > 180) {
                ed->repeat_timer = now;
                AnimationSet *as = ed->repeat_as;
                if (as && as->loaded) {
                    AnimPhase *phase = &as->phases[ed->repeat_phase];
                    if (phase->frame_count > 0) {
                        int rep_idx = as->current_frame[ed->repeat_phase] % phase->frame_count;
                        int step = (SDL_GetModState() & KMOD_SHIFT) ? 10 : 1;
                        switch (ed->repeat_what) {
                            case 0: phase->offset_x[rep_idx] -= step; break;
                            case 1: phase->offset_x[rep_idx] += step; break;
                            case 2: phase->offset_y[rep_idx] -= step; break;
                            case 3: phase->offset_y[rep_idx] += step; break;
                            case 4: {
                                int idx = as->current_frame[ed->repeat_phase] % phase->frame_count;
                                float dur_step = 0.01f;
                                phase->frame_durations[idx] -= dur_step;
                                if (phase->frame_durations[idx] < 0.01f) phase->frame_durations[idx] = 0.01f;
                                break;
                            }
                            case 5: {
                                int idx = as->current_frame[ed->repeat_phase] % phase->frame_count;
                                float dur_step = 0.01f;
                                phase->frame_durations[idx] += dur_step;
                                if (phase->frame_durations[idx] > 5.0f) phase->frame_durations[idx] = 5.0f;
                                break;
                            }
                        }
                    }
                }
            }
        } else {
            ed->repeat_button_id = 0;
        }
    }

    // Автопроигрывание анимаций – каждый кадр
    for (int s = 0; s < 2; s++) {
        AnimationSet *as = (s == 0) ? &ed->ally_anim : &ed->enemy_anim;
        if (!as->loaded) continue;
        AnimPhase *cur = &as->phases[as->current_phase];
        if (cur->animate && cur->frame_count > 0) {
            int idx = as->current_frame[as->current_phase] % cur->frame_count;
            float dur = cur->frame_durations[idx];
            as->anim_timer += 16.0f / 1000.0f;
            if (as->anim_timer >= dur) {
                as->anim_timer -= dur;
                as->current_frame[as->current_phase] =
                    (as->current_frame[as->current_phase] + 1) % cur->frame_count;
            }
        }
    }

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *running = false; return; }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            if (ed->input_active) {
                ed->input_active = false;
                SDL_StopTextInput();
                continue;
            } else {
                *running = false; return;
            }
        }

        // Если активен ввод чисел – обрабатываем текст, клавиши и выход по клику/колёсику
        if (ed->input_active) {
            // Мышь: если кликнули вне текущего поля ввода – автоприменение и выход из ввода
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                // Определяем слот по input_as
                int s = (ed->input_as == &ed->ally_anim) ? 0 : 1;
                // Вычисляем group_y и row_h (как в draw_ui)
                int panel_y = PREVIEW_Y + PREVIEW_H + 4;
                int row_h = 20;
                int group_y = panel_y + s * (4 * row_h + 8);
                // Прямоугольник активного поля
                SDL_Rect active_field;
                if (ed->input_target == 0) {
                    active_field = (SDL_Rect){PREVIEW_X + 90, group_y + row_h + 4 + ed->input_phase * row_h, 35, row_h};
                } else {
                    active_field = (SDL_Rect){PREVIEW_X + 200, group_y + row_h + 4 + ed->input_phase * row_h, 35, row_h};
                }
                // Координаты мыши
                int raw_mx, raw_my;
                SDL_GetMouseState(&raw_mx, &raw_my);
                int win_w, win_h;
                SDL_GetWindowSize(ed->window, &win_w, &win_h);
                int mx = (int)((float)raw_mx * LOGICAL_W / win_w);
                int my = (int)((float)raw_my * LOGICAL_H / win_h);
                if (!(mx >= active_field.x && mx < active_field.x+active_field.w &&
                      my >= active_field.y && my < active_field.y+active_field.h)) {
                    // Клик вне поля – сохраняем число и выходим
                    int val = atoi(ed->input_buffer);
                    AnimPhase *ph = &ed->input_as->phases[ed->input_phase];
                    if (ph->frame_count > 0) {
                        int idx = ed->input_as->current_frame[ed->input_phase] % ph->frame_count;
                        if (ed->input_target == 0) ph->offset_x[idx] = val;
                        else                         ph->offset_y[idx] = val;
                    }
                    ed->input_active = false;
                    SDL_StopTextInput();
                    // не делаем continue, чтобы клик обработался дальше (можно сразу нажать кнопку)
                } else {
                    continue; // клик внутри поля – остаёмся в вводе
                }
            }
            // Колёсико мыши – выход с сохранением
            else if (e.type == SDL_MOUSEWHEEL) {
                int val = atoi(ed->input_buffer);
                AnimPhase *ph = &ed->input_as->phases[ed->input_phase];
                if (ph->frame_count > 0) {
                    int idx = ed->input_as->current_frame[ed->input_phase] % ph->frame_count;
                    if (ed->input_target == 0) ph->offset_x[idx] = val;
                    else                         ph->offset_y[idx] = val;
                }
                ed->input_active = false;
                SDL_StopTextInput();
                // не делаем continue – событие колеса пойдёт в обычную обработку (прокрутка списка)
            }
            // Текстовый ввод
            else if (e.type == SDL_TEXTINPUT) {
                const char *text = e.text.text;
                bool valid = true;
                for (const char *c = text; *c; ++c) {
                    if (*c == '-') {
                        if (ed->input_len != 0) { valid = false; break; }
                    } else if (*c < '0' || *c > '9') {
                        valid = false; break;
                    }
                }
                if (valid) {
                    int add_len = strlen(text);
                    if (ed->input_len + add_len <= 4) {
                        strcat(ed->input_buffer, text);
                        ed->input_len += add_len;
                    }
                }
                continue;
            }
            // Клавиши
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                    int val = atoi(ed->input_buffer);
                    AnimPhase *ph = &ed->input_as->phases[ed->input_phase];
                    if (ph->frame_count > 0) {
                        int idx = ed->input_as->current_frame[ed->input_phase] % ph->frame_count;
                        if (ed->input_target == 0) ph->offset_x[idx] = val;
                        else                         ph->offset_y[idx] = val;
                    }
                    ed->input_active = false;
                    SDL_StopTextInput();
                } else if (e.key.keysym.sym == SDLK_BACKSPACE && ed->input_len > 0) {
                    ed->input_buffer[--ed->input_len] = '\0';
                } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    ed->input_active = false;
                    SDL_StopTextInput();
                }
                continue;
            }
            // Остальные события игнорируем
            continue;
        }

        // Обычная обработка (как было раньше)
        int raw_mx, raw_my;
        SDL_GetMouseState(&raw_mx, &raw_my);
        int win_w, win_h;
        SDL_GetWindowSize(ed->window, &win_w, &win_h);
        int mx = (int)((float)raw_mx * LOGICAL_W / win_w);
        int my = (int)((float)raw_my * LOGICAL_H / win_h);

        if (e.type == SDL_MOUSEWHEEL) {
            ed->list_scroll -= e.wheel.y;
            int max_vis = (LOGICAL_H - 160) / (FONT_SIZE + 4);
            if (ed->list_scroll < 0) ed->list_scroll = 0;
            if (ed->list_scroll > ed->entry_count - max_vis)
                ed->list_scroll = ed->entry_count - max_vis;
        }

        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            ed->repeat_button_id = 0;

            // Левая панель
            int btn_y_base = LOGICAL_H - 130;
            SDL_Rect btn_save    = {5,  btn_y_base, 100, 26};
            SDL_Rect btn_bg      = {110,btn_y_base, 105, 26};
            SDL_Rect btn_load_ally = {5,  btn_y_base+32, 100, 26};
            SDL_Rect btn_load_enemy = {110,btn_y_base+32, 105, 26};
            SDL_Rect btn_save_ally = {5,  btn_y_base+64, 100, 26};
            SDL_Rect btn_save_enemy= {110,btn_y_base+64, 105, 26};

            if (mx >= btn_save.x && mx < btn_save.x+btn_save.w && my >= btn_save.y && my < btn_save.y+btn_save.h) {
                save_entries(ed); printf("Entries saved.\n"); continue;
            }
            if (mx >= btn_bg.x && mx < btn_bg.x+btn_bg.w && my >= btn_bg.y && my < btn_bg.y+btn_bg.h) {
                if (ed->entry_count > 0) {
                    char path[512]; char abs_initial[MAX_PATH];
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
            if (mx >= btn_load_ally.x && mx < btn_load_ally.x+btn_load_ally.w && my >= btn_load_ally.y && my < btn_load_ally.y+btn_load_ally.h) {
                char path[512]; char abs_initial[MAX_PATH];
                GetFullPathNameA("../assets/battles/battlesprites/allies", MAX_PATH, abs_initial, NULL);
                if (open_json_dialog(path, sizeof(path), abs_initial)) {
                    if (load_animation_from_json(&ed->ally_anim, ed->renderer, path))
                        ed->active_slot = 0;
                }
                continue;
            }
            if (mx >= btn_load_enemy.x && mx < btn_load_enemy.x+btn_load_enemy.w && my >= btn_load_enemy.y && my < btn_load_enemy.y+btn_load_enemy.h) {
                char path[512]; char abs_initial[MAX_PATH];
                GetFullPathNameA("../assets/battles/battlesprites/enemies", MAX_PATH, abs_initial, NULL);
                if (open_json_dialog(path, sizeof(path), abs_initial)) {
                    if (load_animation_from_json(&ed->enemy_anim, ed->renderer, path))
                        ed->active_slot = 1;
                }
                continue;
            }
            if (mx >= btn_save_ally.x && mx < btn_save_ally.x+btn_save_ally.w && my >= btn_save_ally.y && my < btn_save_ally.y+btn_save_ally.h) {
                save_animation_to_json(&ed->ally_anim);
                continue;
            }
            if (mx >= btn_save_enemy.x && mx < btn_save_enemy.x+btn_save_enemy.w && my >= btn_save_enemy.y && my < btn_save_enemy.y+btn_save_enemy.h) {
                save_animation_to_json(&ed->enemy_anim);
                continue;
            }

            SDL_Rect btn_rescan = {5, btn_y_base+96, 210, 26};
            if (mx >= btn_rescan.x && mx < btn_rescan.x+btn_rescan.w &&
                my >= btn_rescan.y && my < btn_rescan.y+btn_rescan.h) {
                AnimationSet *as = (ed->active_slot == 0) ? &ed->ally_anim : &ed->enemy_anim;
                if (as->loaded) {
                    rescan_frames(as, ed->renderer);
                    as->current_frame[0] = 0;
                    as->current_frame[1] = 0;
                    as->current_frame[2] = 0;
                    as->current_phase = 0;
                } else {
                    printf("Load an animation first.\n");
                }
                continue;
            }

            // Список битв
            int line_height = FONT_SIZE + 4;
            int max_vis = (LOGICAL_H - 160) / line_height;
            if (mx >= 0 && mx < LEFT_PANEL_W) {
                for (int i = 0; i < max_vis; i++) {
                    int idx = ed->list_scroll + i;
                    if (idx >= ed->entry_count) break;
                    int y = 40 + i * line_height;
                    if (my >= y && my < y + line_height) {
                        if (idx != ed->current_index) {
                            ed->current_index = idx;
                            load_background_texture(ed, ed->entries[idx].background);
                            load_ground_texture(ed, ed->entries[idx].ground);
                        }
                        break;
                    }
                }
            }

            // Панель управления анимациями
            int panel_y = PREVIEW_Y + PREVIEW_H + 4;
            int row_h = 20;
            for (int s = 0; s < 2; s++) {
                AnimationSet *as = (s == 0) ? &ed->ally_anim : &ed->enemy_anim;
                int group_y = panel_y + s * (4 * row_h + 8);

                for (int p = 0; p < 3; p++) {
                    int y = group_y + row_h + 4 + p * row_h;
                    if (my < y || my >= y + row_h) continue;

                    // Чекбокс Anim
                    SDL_Rect anim_check = {PREVIEW_X + 535, y + (row_h - 14)/2, 14, 14};
                    if (mx >= anim_check.x && mx < anim_check.x + anim_check.w &&
                        my >= anim_check.y && my < anim_check.y + anim_check.h) {
                        if (as->loaded) {
                            AnimPhase *phase = &as->phases[p];
                            if (phase->animate) {
                                phase->animate = false;
                            } else {
                                for (int pp = 0; pp < 3; pp++) {
                                    if (pp != p) as->phases[pp].animate = false;
                                }
                                phase->animate = true;
                                as->current_phase = p;
                                ed->active_slot = s;
                                ed->active_phase = p;
                                as->anim_timer = 0;
                            }
                        }
                        continue;
                    }

                    // Клик по метке фазы
                    SDL_Rect phase_rect = {PREVIEW_X + 5, y, 55, row_h};
                    if (mx >= phase_rect.x && mx < phase_rect.x+phase_rect.w) {
                        bool any_anim = false;
                        for (int pp = 0; pp < 3; pp++) {
                            if (as->phases[pp].animate) { any_anim = true; break; }
                        }
                        if (any_anim && !as->phases[p].animate) {
                            continue;
                        }
                        ed->active_slot = s;
                        ed->active_phase = p;
                        as->current_phase = p;
                        continue;
                    }

                    if (!as->loaded) continue;
                    AnimPhase *phase = &as->phases[p];
                    if (phase->animate) continue;
                    if (phase->frame_count == 0) continue;
                    int idx = as->current_frame[p] % phase->frame_count;

                    // Клик по полю X (начало ввода)
                    SDL_Rect x_val = {PREVIEW_X + 90, y, 35, row_h};
                    if (mx >= x_val.x && mx < x_val.x+x_val.w && my >= y && my < y+row_h) {
                        ed->input_active = true;
                        ed->input_target = 0;
                        ed->input_as = as;
                        ed->input_phase = p;
                        snprintf(ed->input_buffer, sizeof(ed->input_buffer), "%d", phase->offset_x[idx]);
                        ed->input_len = strlen(ed->input_buffer);
                        SDL_StartTextInput();
                        continue;
                    }
                    // Клик по полю Y
                    SDL_Rect y_val = {PREVIEW_X + 200, y, 35, row_h};
                    if (mx >= y_val.x && mx < y_val.x+y_val.w && my >= y && my < y+row_h) {
                        ed->input_active = true;
                        ed->input_target = 1;
                        ed->input_as = as;
                        ed->input_phase = p;
                        snprintf(ed->input_buffer, sizeof(ed->input_buffer), "%d", phase->offset_y[idx]);
                        ed->input_len = strlen(ed->input_buffer);
                        SDL_StartTextInput();
                        continue;
                    }

                    // Offset X (кнопки)
                    SDL_Rect x_dec = {PREVIEW_X + 130, y, 16, row_h};
                    SDL_Rect x_inc = {PREVIEW_X + 148, y, 16, row_h};
                    int step = (SDL_GetModState() & KMOD_SHIFT) ? 10 : 1;
                    if (mx >= x_dec.x && mx < x_dec.x+x_dec.w) {
                        phase->offset_x[idx] -= step;
                        ed->repeat_button_id = 1; ed->repeat_timer = SDL_GetTicks();
                        ed->repeat_button_rect = x_dec; ed->repeat_as = as; ed->repeat_phase = p; ed->repeat_what = 0;
                    }
                    if (mx >= x_inc.x && mx < x_inc.x+x_inc.w) {
                        phase->offset_x[idx] += step;
                        ed->repeat_button_id = 1; ed->repeat_timer = SDL_GetTicks();
                        ed->repeat_button_rect = x_inc; ed->repeat_as = as; ed->repeat_phase = p; ed->repeat_what = 1;
                    }

                    // Offset Y (кнопки)
                    SDL_Rect y_dec = {PREVIEW_X + 240, y, 16, row_h};
                    SDL_Rect y_inc = {PREVIEW_X + 258, y, 16, row_h};
                    if (mx >= y_dec.x && mx < y_dec.x+y_dec.w) {
                        phase->offset_y[idx] -= step;
                        ed->repeat_button_id = 1; ed->repeat_timer = SDL_GetTicks();
                        ed->repeat_button_rect = y_dec; ed->repeat_as = as; ed->repeat_phase = p; ed->repeat_what = 2;
                    }
                    if (mx >= y_inc.x && mx < y_inc.x+y_inc.w) {
                        phase->offset_y[idx] += step;
                        ed->repeat_button_id = 1; ed->repeat_timer = SDL_GetTicks();
                        ed->repeat_button_rect = y_inc; ed->repeat_as = as; ed->repeat_phase = p; ed->repeat_what = 3;
                    }

                    // Duration
                    SDL_Rect dur_dec = {PREVIEW_X + 375, y, 16, row_h};
                    SDL_Rect dur_inc = {PREVIEW_X + 393, y, 16, row_h};
                    if (phase->frame_count > 0) {
                        int idx = as->current_frame[p] % phase->frame_count;
                        float dur_step = 0.01f;
                        if (mx >= dur_dec.x && mx < dur_dec.x+dur_dec.w) {
                            phase->frame_durations[idx] -= dur_step;
                            if (phase->frame_durations[idx] < 0.01f) phase->frame_durations[idx] = 0.01f;
                            ed->repeat_button_id = 1; ed->repeat_timer = SDL_GetTicks();
                            ed->repeat_button_rect = dur_dec; ed->repeat_as = as; ed->repeat_phase = p; ed->repeat_what = 4;
                        }
                        if (mx >= dur_inc.x && mx < dur_inc.x+dur_inc.w) {
                            phase->frame_durations[idx] += dur_step;
                            if (phase->frame_durations[idx] > 5.0f) phase->frame_durations[idx] = 5.0f;
                            ed->repeat_button_id = 1; ed->repeat_timer = SDL_GetTicks();
                            ed->repeat_button_rect = dur_inc; ed->repeat_as = as; ed->repeat_phase = p; ed->repeat_what = 5;
                        }
                    }

                    // Переключение кадров
                    SDL_Rect frame_dec = {PREVIEW_X + 485, y, 16, row_h};
                    SDL_Rect frame_inc = {PREVIEW_X + 503, y, 16, row_h};
                    if (mx >= frame_dec.x && mx < frame_dec.x+frame_dec.w) {
                        as->current_frame[p]--;
                        if (as->current_frame[p] < 0) as->current_frame[p] = phase->frame_count - 1;
                    }
                    if (mx >= frame_inc.x && mx < frame_inc.x+frame_inc.w) {
                        as->current_frame[p] = (as->current_frame[p] + 1) % phase->frame_count;
                    }
                }
            }
        }

        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            ed->repeat_button_id = 0;
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
    ed.ground_tex = NULL;
    ed.active_slot = 0;
    ed.repeat_button_id = 0;

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
        load_ground_texture(&ed, ed.entries[0].ground);   // загружаем землю для первой битвы
    }

    bool running = true;
    while (running) {
        handle_input(&ed, &running);
        draw_ui(&ed);
        SDL_Delay(16);
    }

    free_animation_set(&ed.ally_anim);
    free_animation_set(&ed.enemy_anim);
    if (ed.bg_tex) SDL_DestroyTexture(ed.bg_tex);
    if (ed.ground_tex) SDL_DestroyTexture(ed.ground_tex);
    if (ed.font) TTF_CloseFont(ed.font);
    free(ed.entries);
    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}