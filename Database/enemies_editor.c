// enemies_editor.c
#include "enemies_editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <windows.h>

// Внешние данные
extern TTF_Font *g_font;
extern int g_font_ok;
extern char error_msg[256];
int draw_text_ext(SDL_Renderer *r, int x, int y, const char *text, SDL_Color color);
char* open_file_dialog(const char* initialDir);

// ---------- ЛОКАЛЬНЫЕ ДАННЫЕ ----------
static cJSON *enemies_array = NULL;
static int enemies_count = 0;
static int selected_index = -1;
static int scroll_offset = 0;
static int window_height = 680;

// Поля редактирования
enum {
    EF_ID, EF_NAME, EF_PORTRAIT, EF_MAPSPRITE, EF_BATTLESPRITE,
    EF_RACE, EF_STATUS, EF_LEVEL, EF_SPELLPOWER,
    EF_MAXHP, EF_MAXMP, EF_BASEATT, EF_BASEDEF, EF_BASEAGI, EF_BASEMOV,
    EF_RESISTANCE,
    EF_PROVESS,
    EF_ITEMS,
    EF_SPELLS,
    EF_INITSTATUS, EF_MOVETYPE, EF_AIBITFIELD,
    EF_COUNT
};

typedef struct {
    char text[256];
    int cursor;
    int active;
    cJSON *json_obj;
    const char *json_key;
    int is_numeric;
    int max_len;
    SDL_Rect rect;
    int is_special;   // 0=обычное, 1=race, 2=status, 3=portrait, 4=mapsprite, -1=только чтение
} EditField;

static EditField edit_fields[EF_COUNT];
static int edit_field_count = 0;
static int active_field_index = -1;
static int save_timer = 0;

// Для портретов
static char portrait_list[200][64];
static int portrait_list_count = 0;
static int selected_portrait_idx = -1;
static SDL_Texture *portrait_tex = NULL;
static char loaded_portrait_name[64] = "";

// Для спрайтов карты
static char mapsprite_list[200][64];
static int mapsprite_list_count = 0;
static int selected_mapsprite_idx = -1;

// Раса и статус
static const char *race_options[] = {"Enterran", "Cadrian", "Human"};
static const int race_count = 3;
static int race_index = 0;

static const char *status_options[] = {"normal", "hyper", "ultra"};
static const int status_count = 3;
static int status_index = 0;

// Буферы для редактируемых полей
static char name_buf[64] = "";
static char portrait_buf[64] = "";
static char mapsprite_buf[64] = "";
static char battlesprite_buf[64] = "";

// ---------- ПРОТОТИПЫ ----------
static void open_edit_fields(cJSON *enemy);

// ---------- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ----------
static const char* json_string(cJSON *item, const char *key) {
    cJSON *f = cJSON_GetObjectItem(item, key);
    return (f && cJSON_IsString(f)) ? f->valuestring : "";
}

static int json_int(cJSON *item, const char *key, int def) {
    cJSON *f = cJSON_GetObjectItem(item, key);
    return (f && cJSON_IsNumber(f)) ? f->valueint : def;
}

static void draw_text_ext_local(SDL_Renderer *r, int x, int y, const char *text, SDL_Color color) {
    if (!g_font_ok) return;
    SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, text, color);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect d = {x, y, s->w, s->h};
    SDL_RenderCopy(r, t, NULL, &d);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}
#define draw_text draw_text_ext_local

// ---------- ДЛЯ КНОПОК С ДИНАМИЧЕСКОЙ ШИРИНОЙ ----------
static SDL_Rect save_btn_rect, del_btn_rect, add_btn_rect, refresh_btn_rect;

static int get_text_width(const char *text) {
    if (!g_font_ok) return 0;
    int w, h;
    if (TTF_SizeUTF8(g_font, text, &w, &h) == 0)
        return w;
    return 0;
}

static SDL_Rect make_button(int x, int y, const char *label, int pad_x, int pad_y) {
    int text_w = get_text_width(label);
    int text_h = 0;
    TTF_SizeUTF8(g_font, label, NULL, &text_h);
    int btn_w = text_w + 2 * pad_x;
    int btn_h = text_h + 2 * pad_y;
    SDL_Rect btn = {x, y, btn_w, btn_h};
    return btn;
}

static void draw_button(SDL_Renderer *r, SDL_Rect btn, const char *label, SDL_Color bg, SDL_Color border, SDL_Color text_col) {
    // Фон
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(r, &btn);
    // Рамка
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(r, &btn);
    // Текст по центру
    int text_w = get_text_width(label);
    int text_h = 0;
    TTF_SizeUTF8(g_font, label, NULL, &text_h);
    int text_x = btn.x + (btn.w - text_w) / 2;
    int text_y = btn.y + (btn.h - text_h) / 2;
    if (text_x < btn.x + 5) text_x = btn.x + 5;  // минимальный отступ
    draw_text(r, text_x, text_y, label, text_col);
}

static void scan_portrait_folder(void);
static void scan_mapsprite_folder(void);

// Инициализация редактора
void enemies_init(cJSON *arr, int count) {
    enemies_array = arr;
    enemies_count = count;
    selected_index = -1;
    scroll_offset = 0;
    edit_field_count = 0;
    active_field_index = -1;
    scan_portrait_folder();
    scan_mapsprite_folder();
    if (enemies_count > 0) {
        selected_index = 0;
        open_edit_fields(cJSON_GetArrayItem(enemies_array, 0));
    }
}

void enemies_set_window_height(int h) { window_height = h; }
int enemies_get_scroll(void) { return scroll_offset; }
void enemies_adjust_scroll(int delta) { scroll_offset -= delta; }
void enemies_reset_selection(void) {
    selected_index = -1;
    edit_field_count = 0;
    active_field_index = -1;
    if (enemies_count > 0) {
        selected_index = 0;
        open_edit_fields(cJSON_GetArrayItem(enemies_array, 0));
    }
}
int enemies_is_edit_active(void) { return active_field_index >= 0; }
void enemies_update_timer(void) { if (save_timer > 0) save_timer--; }

// ---------- СКАНИРОВАНИЕ ПАПОК ----------
static void scan_portrait_folder(void) {
    portrait_list_count = 0;
    selected_portrait_idx = -1;
    if (portrait_tex) { SDL_DestroyTexture(portrait_tex); portrait_tex = NULL; }

    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile("../assets/ui/portraits/*.png", &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const char *filename = findFileData.cFileName;
        char name[64];
        strncpy(name, filename, 63);
        name[63] = '\0';
        char *dot = strrchr(name, '.');
        if (dot) *dot = '\0';
        if (strstr(name, "_blink") || strstr(name, "_talk")) continue;
        if (portrait_list_count < 200) {
            snprintf(portrait_list[portrait_list_count], sizeof(portrait_list[portrait_list_count]), "%s", name);
            portrait_list_count++;
        }
    } while (FindNextFile(hFind, &findFileData) != 0);
    FindClose(hFind);

    for (int i = 0; i < portrait_list_count; i++) {
        if (strcmp(portrait_list[i], portrait_buf) == 0) { selected_portrait_idx = i; break; }
    }
}

static void scan_mapsprite_folder(void) {
    mapsprite_list_count = 0;
    selected_mapsprite_idx = -1;

    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile("../assets/mapsprites_enemy/*.png", &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const char *filename = findFileData.cFileName;
        char name[64];
        strncpy(name, filename, 63);
        name[63] = '\0';
        char *dot = strrchr(name, '.');
        if (dot) *dot = '\0';
        if (mapsprite_list_count < 200) {
            snprintf(mapsprite_list[mapsprite_list_count], sizeof(mapsprite_list[mapsprite_list_count]), "%s", name);
            mapsprite_list_count++;
        }
    } while (FindNextFile(hFind, &findFileData) != 0);
    FindClose(hFind);

    for (int i = 0; i < mapsprite_list_count; i++) {
        if (strcmp(mapsprite_list[i], mapsprite_buf) == 0) { selected_mapsprite_idx = i; break; }
    }
}

static void load_portrait_texture(SDL_Renderer *renderer) {
    if (portrait_tex) { SDL_DestroyTexture(portrait_tex); portrait_tex = NULL; }
    loaded_portrait_name[0] = '\0';
    if (selected_portrait_idx < 0 || selected_portrait_idx >= portrait_list_count) return;
    char path[256];
    snprintf(path, sizeof(path), "../assets/ui/portraits/%s.png", portrait_list[selected_portrait_idx]);
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) return;
    portrait_tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (portrait_tex) {
        strncpy(loaded_portrait_name, portrait_list[selected_portrait_idx], 63);
        loaded_portrait_name[63] = '\0';
    }
}

// ---------- СОХРАНЕНИЕ ----------
static int save_enemies_to_file(void) {
    if (!enemies_array) return 0;
    cJSON *root = cJSON_CreateObject();
    cJSON *dup = cJSON_Duplicate(enemies_array, 1);
    cJSON_AddItemToObject(root, "enemies", dup);
    char *js = cJSON_PrintBuffered(root, 0, 1);
    FILE *f = fopen("../data/enemies/enemies.json", "w");
    if (f) { fputs(js, f); fclose(f); free(js); cJSON_Delete(root); return 1; }
    else { free(js); cJSON_Delete(root); return 0; }
}

// ---------- РЕДАКТИРОВАНИЕ ----------
static void update_race_status_fields(void) {
    cJSON *enemy = cJSON_GetArrayItem(enemies_array, selected_index);
    if (!enemy) return;
    cJSON *race = cJSON_GetObjectItem(enemy, "race");
    if (race && cJSON_IsString(race)) cJSON_SetValuestring(race, race_options[race_index]);
    cJSON *status = cJSON_GetObjectItem(enemy, "status");
    if (status && cJSON_IsString(status)) cJSON_SetValuestring(status, status_options[status_index]);
}

static void open_edit_fields(cJSON *enemy) {
    edit_field_count = 0;
    active_field_index = -1;
    if (!enemy) return;

    int base_x = 360 + 10;
    int base_y = 60;                // исправлено с 80 на 60
    int field_offset = 100;
    int line_h = 22, gap = 28;

    // ID (только для чтения)
    EditField *f = &edit_fields[EF_ID];
    snprintf(f->text, sizeof(f->text), "%d", json_int(enemy, "id", -1));
    f->active = 0; f->json_obj = enemy; f->json_key = "id"; f->is_numeric = 1; f->max_len = 0; f->is_special = -1;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_ID*gap, 150, line_h};
    edit_field_count = EF_ID+1;

    // Name
    const char *nm = json_string(enemy, "name");
    strncpy(name_buf, nm, 63); name_buf[63] = '\0';
    f = &edit_fields[EF_NAME];
    snprintf(f->text, sizeof(f->text), "%s", name_buf);
    f->active = 0; f->json_obj = enemy; f->json_key = "name"; f->is_numeric = 0; f->max_len = 0; f->is_special = 0;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_NAME*gap, 150, line_h};
    edit_field_count = EF_NAME+1;

    // Portrait
    const char *prt = json_string(enemy, "portrait");
    strncpy(portrait_buf, prt, 63); portrait_buf[63] = '\0';
    for (int i = 0; i < portrait_list_count; i++) {
        if (strcmp(portrait_list[i], portrait_buf) == 0) { selected_portrait_idx = i; break; }
    }
    f = &edit_fields[EF_PORTRAIT];
    snprintf(f->text, sizeof(f->text), "%s", portrait_buf);
    f->active = 0; f->json_obj = enemy; f->json_key = "portrait"; f->is_numeric = 0; f->max_len = 0; f->is_special = 3;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_PORTRAIT*gap, 150, line_h};
    edit_field_count = EF_PORTRAIT+1;

    // Mapsprite
    const char *msp = json_string(enemy, "mapsprite");
    strncpy(mapsprite_buf, msp, 63); mapsprite_buf[63] = '\0';
    for (int i = 0; i < mapsprite_list_count; i++) {
        if (strcmp(mapsprite_list[i], mapsprite_buf) == 0) { selected_mapsprite_idx = i; break; }
    }
    f = &edit_fields[EF_MAPSPRITE];
    snprintf(f->text, sizeof(f->text), "%s", mapsprite_buf);
    f->active = 0; f->json_obj = enemy; f->json_key = "mapsprite"; f->is_numeric = 0; f->max_len = 0; f->is_special = 4;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_MAPSPRITE*gap, 150, line_h};
    edit_field_count = EF_MAPSPRITE+1;

    // Battle Sprite
    const char *bspr = json_string(enemy, "battle_sprite");
    strncpy(battlesprite_buf, bspr, 63); battlesprite_buf[63] = '\0';
    f = &edit_fields[EF_BATTLESPRITE];
    snprintf(f->text, sizeof(f->text), "%s", battlesprite_buf);
    f->active = 0; f->json_obj = enemy; f->json_key = "battle_sprite"; f->is_numeric = 0; f->max_len = 0; f->is_special = 0;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_BATTLESPRITE*gap, 150, line_h};
    edit_field_count = EF_BATTLESPRITE+1;

    // Race
    const char *rc = json_string(enemy, "race");
    race_index = 0;
    for (int i = 0; i < race_count; i++) { if (strcmp(rc, race_options[i]) == 0) { race_index = i; break; } }
    f = &edit_fields[EF_RACE];
    snprintf(f->text, sizeof(f->text), "%s", race_options[race_index]);
    f->active = 0; f->json_obj = enemy; f->json_key = "race"; f->is_numeric = 0; f->max_len = 0; f->is_special = 1;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_RACE*gap, 150, line_h};
    edit_field_count = EF_RACE+1;

    // Status
    const char *st = json_string(enemy, "status");
    status_index = 0;
    for (int i = 0; i < status_count; i++) { if (strcmp(st, status_options[i]) == 0) { status_index = i; break; } }
    f = &edit_fields[EF_STATUS];
    snprintf(f->text, sizeof(f->text), "%s", status_options[status_index]);
    f->active = 0; f->json_obj = enemy; f->json_key = "status"; f->is_numeric = 0; f->max_len = 0; f->is_special = 2;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_STATUS*gap, 150, line_h};
    edit_field_count = EF_STATUS+1;

    // Level
    f = &edit_fields[EF_LEVEL];
    snprintf(f->text, sizeof(f->text), "%d", json_int(enemy, "level", 0));
    f->active = 0; f->json_obj = enemy; f->json_key = "level"; f->is_numeric = 1; f->max_len = 4; f->is_special = 0;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_LEVEL*gap, 150, line_h};
    edit_field_count = EF_LEVEL+1;

    // Spell power
    f = &edit_fields[EF_SPELLPOWER];
    snprintf(f->text, sizeof(f->text), "%s", json_string(enemy, "spell_power"));
    f->active = 0; f->json_obj = enemy; f->json_key = "spell_power"; f->is_numeric = 0; f->max_len = 0; f->is_special = 0;
    f->rect = (SDL_Rect){base_x+field_offset, base_y + EF_SPELLPOWER*gap, 150, line_h};
    edit_field_count = EF_SPELLPOWER+1;

    // Stats
    cJSON *stats = cJSON_GetObjectItem(enemy, "stats");
    const char *stat_keys[] = {"max_hp","max_mp","base_att","base_def","base_agi","base_mov"};
    int stat_max_len[] = {5,5,5,5,5,3};
    for (int i = 0; i < 6; i++) {
        f = &edit_fields[EF_MAXHP + i];
        snprintf(f->text, sizeof(f->text), "%d", json_int(stats, stat_keys[i], 0));
        f->active = 0; f->json_obj = stats; f->json_key = stat_keys[i]; f->is_numeric = 1; f->max_len = stat_max_len[i]; f->is_special = 0;
        f->rect = (SDL_Rect){base_x+field_offset, base_y + (EF_MAXHP+i)*gap, 150, line_h};
        edit_field_count = EF_MAXHP + i + 1;
    }

    // Остальные поля только для чтения (код без изменений)
    for (int i = EF_RESISTANCE; i < EF_COUNT; i++) {
        f = &edit_fields[i];
        f->active = 0; f->is_special = 0; f->json_obj = NULL; f->json_key = NULL;
        if (i == EF_RESISTANCE) {
            cJSON *res = cJSON_GetObjectItem(enemy, "resistance");
            char *tmp = res ? cJSON_PrintUnformatted(res) : strdup("{}");
            snprintf(f->text, sizeof(f->text), "%s", tmp); free(tmp);
        }
        else if (i >= EF_PROVESS && i <= EF_SPELLS) {
            const char *arr_keys[] = {"prowess","items","spells"};
            int idx2 = i - EF_PROVESS;
            cJSON *arr = cJSON_GetObjectItem(enemy, arr_keys[idx2]);
            char *tmp = arr ? cJSON_PrintUnformatted(arr) : strdup("[]");
            snprintf(f->text, sizeof(f->text), "%s", tmp); free(tmp);
        }
        else if (i == EF_INITSTATUS) { snprintf(f->text, sizeof(f->text), "%s", json_string(enemy, "initial_status")); }
        else if (i == EF_MOVETYPE) { snprintf(f->text, sizeof(f->text), "%s", json_string(enemy, "move_type")); }
        else if (i == EF_AIBITFIELD) {
            cJSON *ai = cJSON_GetObjectItem(enemy, "ai_bitfield");
            char *tmp = ai ? cJSON_PrintUnformatted(ai) : strdup("[]");
            snprintf(f->text, sizeof(f->text), "%s", tmp); free(tmp);
        }
        f->rect = (SDL_Rect){base_x+field_offset, base_y + i*gap, 200, line_h};
        edit_field_count = i+1;
    }
}

static void commit_field(int idx) {
    if (idx < 0 || idx >= edit_field_count) return;
    EditField *f = &edit_fields[idx];
    if (!f->json_obj) return;

    if (f->is_special == 1) { update_race_status_fields(); return; }
    if (f->is_special == 2) { update_race_status_fields(); return; }
    if (f->is_special == 3) {
        cJSON *str = cJSON_GetObjectItem(f->json_obj, "portrait");
        if (str) cJSON_SetValuestring(str, portrait_buf);
        return;
    }
    if (f->is_special == 4) {
        cJSON *str = cJSON_GetObjectItem(f->json_obj, "mapsprite");
        if (str) cJSON_SetValuestring(str, mapsprite_buf);
        return;
    }

    if (f->json_key) {
        if (f->is_numeric) {
            int val = atoi(f->text);
            cJSON *num = cJSON_GetObjectItem(f->json_obj, f->json_key);
            if (num && cJSON_IsNumber(num)) { num->valueint = val; num->valuedouble = val; }
        } else {
            cJSON *str = cJSON_GetObjectItem(f->json_obj, f->json_key);
            if (str && cJSON_IsString(str)) cJSON_SetValuestring(str, f->text);
        }
    }
    f->active = 0;
}

// Обработка ввода текстовых полей
void enemies_handle_input(SDL_Event *evt) {
    if (active_field_index >= 0 && active_field_index < edit_field_count) {
        EditField *f = &edit_fields[active_field_index];
        if (f->is_special != 0 && f->is_special != -1) return;

        if (evt->type == SDL_KEYDOWN) {
            if (evt->key.keysym.sym == SDLK_BACKSPACE && f->cursor > 0) {
                memmove(f->text + f->cursor - 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->cursor--;
            } else if (evt->key.keysym.sym == SDLK_RETURN || evt->key.keysym.sym == SDLK_KP_ENTER) {
                commit_field(active_field_index);
                active_field_index = -1;
            } else if (evt->key.keysym.sym == SDLK_LEFT && f->cursor > 0) { f->cursor--; }
            else if (evt->key.keysym.sym == SDLK_RIGHT && f->cursor < strlen(f->text)) { f->cursor++; }
        } else if (evt->type == SDL_TEXTINPUT) {
            char ch = evt->text.text[0];
            if (f->is_numeric) {
                if (isdigit(ch) || (ch == '-' && f->cursor == 0 && f->text[0] == '\0')) {
                    if (f->max_len > 0 && strlen(f->text) >= f->max_len) return;
                    if (strlen(f->text) < 255) {
                        memmove(f->text + f->cursor + 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                        f->text[f->cursor++] = ch;
                    }
                }
            } else {
                if (ch >= 32 && ch <= 126 && strlen(f->text) < 255) {
                    if (f->max_len > 0 && strlen(f->text) >= f->max_len) return;
                    memmove(f->text + f->cursor + 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                    f->text[f->cursor++] = ch;
                }
            }
        }
    }
}

// ---------- ОТРИСОВКА ПОЛЯ ВВОДА ----------
static void draw_enemy_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *fallback_text) {
    SDL_Color white = {255,255,255}, black = {0,0,0}, gray = {100,100,100};
    draw_text(r, x, y + 3, label, white);
    int fx = x + 100;
    SDL_Rect rect = {fx, y, w, h};
    SDL_SetRenderDrawColor(r, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, white.r, white.g, white.b, 255); SDL_RenderDrawRect(r, &rect);

    const char *text_to_draw = fallback_text;
    if (idx >= 0 && idx < edit_field_count && edit_fields[idx].active)
        text_to_draw = edit_fields[idx].text;

    draw_text(r, fx + 5, y + 3, text_to_draw, black);

    if (idx >= 0 && idx < edit_field_count && edit_fields[idx].active) {
        EditField *f = &edit_fields[idx];
        char before[256] = {0};
        if (f->cursor > 0) {
            strncpy(before, f->text, f->cursor);
            before[f->cursor] = '\0';
        }
        SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, before, black);
        int offset = fx + 5 + (s ? s->w : 0);
        if (s) SDL_FreeSurface(s);
        draw_text(r, offset, y + 3, "|", black);
    }
}

// ---------- ОТРИСОВКА СПИСКА ----------
void enemies_draw_list(SDL_Renderer *r, int start_y, int scroll) {
    SDL_Color white = {255,255,255}, green = {0,255,0};
    int y = start_y - scroll;
    int line_h = 20;
    for (int i = 0; i < enemies_count; i++) {
        cJSON *enemy = cJSON_GetArrayItem(enemies_array, i);
        const char *name = json_string(enemy, "name");
        int id = json_int(enemy, "id", -1);
        char buf[128];
        snprintf(buf, sizeof(buf), "%d: %s", id, name);
        if (y >= start_y && y < window_height - 40) {
            SDL_Rect rect = {10, y, 250, line_h};
            if (i == selected_index) {
                SDL_SetRenderDrawColor(r, 0,100,0,128);
                SDL_RenderFillRect(r, &rect);
            }
            draw_text(r, 10, y, buf, (i == selected_index) ? green : white);
        }
        y += line_h;
    }
}

void enemies_handle_click(int mx, int my, int start_y, int scroll) {
    int y = start_y - scroll;
    int line_h = 20;
    for (int i = 0; i < enemies_count; i++) {
        if (my >= y && my < y + line_h && mx >= 10 && mx < 260) {
            if (selected_index != i) {
                // коммитим предыдущее поле
                if (active_field_index >= 0) commit_field(active_field_index);
                active_field_index = -1;
                selected_index = i;
                open_edit_fields(cJSON_GetArrayItem(enemies_array, i));
            }
            return;
        }
        y += line_h;
    }
}

// Отрисовка панели редактирования
void enemies_draw_edit_panel(SDL_Renderer *r, int px, int py) {
    if (selected_index < 0) {
        draw_text(r, px+30, py+50, "Select an enemy to edit", (SDL_Color){255,255,255,255});
        return;
    }

    SDL_Color white = {255,255,255}, black = {0,0,0}, yellow = {255,255,0};
    SDL_Rect panel = {px, py, 580, 590};
    SDL_SetRenderDrawColor(r, 60,60,60,255); SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &panel);

    int y = py + 10;
    int base_x = px + 10;
    int field_offset = 100;
    char buf[256];

    // ID
    snprintf(buf, sizeof(buf), "%d", json_int(cJSON_GetArrayItem(enemies_array, selected_index), "id", -1));
    draw_text(r, base_x, y+3, "ID:", white);
    draw_text(r, base_x+field_offset, y+3, buf, white);
    y += 28;

    // Name
    draw_enemy_field(r, base_x, y, 150, 22, EF_NAME, "Name:", name_buf);
    y += 28;

    // Portrait
    draw_text(r, base_x, y+3, "Portrait:", white);
    draw_text(r, base_x+field_offset, y+3, portrait_buf, white);
    SDL_Rect brow1 = {base_x+field_offset+150+5, y, 70, 22};
    SDL_SetRenderDrawColor(r, 100,100,200,255); SDL_RenderFillRect(r, &brow1);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &brow1);
    draw_text(r, brow1.x+5, brow1.y+3, "Browse", white);

    SDL_Rect port_prev = {brow1.x + brow1.w + 5, y, 20, 22};
    SDL_SetRenderDrawColor(r, 70,70,120,255); SDL_RenderFillRect(r, &port_prev);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &port_prev);
    draw_text(r, port_prev.x+5, port_prev.y+3, "<", white);

    SDL_Rect port_next = {port_prev.x + 25, y, 20, 22};
    SDL_SetRenderDrawColor(r, 70,70,120,255); SDL_RenderFillRect(r, &port_next);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &port_next);
    draw_text(r, port_next.x+5, port_next.y+3, ">", white);

    // Миниатюра портрета
    int thumb_x = px + 580 - 144;
    int thumb_y = py + 10;
    if (selected_portrait_idx >= 0 && (portrait_tex == NULL || strcmp(loaded_portrait_name, portrait_list[selected_portrait_idx]) != 0))
        load_portrait_texture(r);
    if (portrait_tex) {
        SDL_Rect thumb_rect = {thumb_x, thumb_y, 134, 208};
        SDL_RenderCopy(r, portrait_tex, NULL, &thumb_rect);
    }
    y += 28;

    // Mapsprite
    draw_text(r, base_x, y+3, "Mapsprite:", white);
    draw_text(r, base_x+field_offset, y+3, mapsprite_buf, white);
    SDL_Rect brow2 = {base_x+field_offset+150+5, y, 70, 22};
    SDL_SetRenderDrawColor(r, 100,100,200,255); SDL_RenderFillRect(r, &brow2);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &brow2);
    draw_text(r, brow2.x+5, brow2.y+3, "Browse", white);

    SDL_Rect map_prev = {brow2.x + brow2.w + 5, y, 20, 22};
    SDL_SetRenderDrawColor(r, 70,70,120,255); SDL_RenderFillRect(r, &map_prev);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &map_prev);
    draw_text(r, map_prev.x+5, map_prev.y+3, "<", white);

    SDL_Rect map_next = {map_prev.x + 25, y, 20, 22};
    SDL_SetRenderDrawColor(r, 70,70,120,255); SDL_RenderFillRect(r, &map_next);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &map_next);
    draw_text(r, map_next.x+5, map_next.y+3, ">", white);
    y += 28;

    // Battle Sprite
    draw_enemy_field(r, base_x, y, 150, 22, EF_BATTLESPRITE, "Battle Spr:", battlesprite_buf);
    y += 28;

    // Race
    draw_text(r, base_x, y+3, "Race:", white);
    draw_text(r, base_x+field_offset, y+3, race_options[race_index], white);
    SDL_Rect race_prev = {base_x+field_offset+150+5, y, 20, 22};
    SDL_SetRenderDrawColor(r, 70,70,120,255); SDL_RenderFillRect(r, &race_prev);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &race_prev);
    draw_text(r, race_prev.x+5, race_prev.y+3, "<", white);
    SDL_Rect race_next = {race_prev.x + 25, y, 20, 22};
    SDL_SetRenderDrawColor(r, 70,70,120,255); SDL_RenderFillRect(r, &race_next);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &race_next);
    draw_text(r, race_next.x+5, race_next.y+3, ">", white);
    y += 28;

    // Status
    draw_text(r, base_x, y+3, "Status:", white);
    draw_text(r, base_x+field_offset, y+3, status_options[status_index], white);
    SDL_Rect status_prev = {base_x+field_offset+150+5, y, 20, 22};
    SDL_SetRenderDrawColor(r, 70,70,120,255); SDL_RenderFillRect(r, &status_prev);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &status_prev);
    draw_text(r, status_prev.x+5, status_prev.y+3, "<", white);
    SDL_Rect status_next = {status_prev.x + 25, y, 20, 22};
    SDL_SetRenderDrawColor(r, 70,70,120,255); SDL_RenderFillRect(r, &status_next);
    SDL_SetRenderDrawColor(r, 255,255,255,255); SDL_RenderDrawRect(r, &status_next);
    draw_text(r, status_next.x+5, status_next.y+3, ">", white);
    y += 28;

    // Level
    draw_enemy_field(r, base_x, y, 150, 22, EF_LEVEL, "Level:", edit_fields[EF_LEVEL].text);
    y += 28;

    // Spell power
    draw_enemy_field(r, base_x, y, 150, 22, EF_SPELLPOWER, "Spell Pwr:", edit_fields[EF_SPELLPOWER].text);
    y += 28;

    // Stats
    const char* stat_labels[] = {"max_hp:", "max_mp:", "base_att:", "base_def:", "base_agi:", "base_mov:"};
    for (int i = 0; i < 6; i++) {
        draw_enemy_field(r, base_x, y, 150, 22, EF_MAXHP + i, stat_labels[i], edit_fields[EF_MAXHP + i].text);
        y += 28;
    }

    // Кнопки действий
    int btn_y = py + 480;
    int pad_x = 10, pad_y = 5;
    int next_x = px + 10;   // первый отступ от левого края панели

    del_btn_rect = make_button(next_x, btn_y, "Del Enemy", pad_x, pad_y);
    draw_button(r, del_btn_rect, "Del Enemy", (SDL_Color){200,80,80}, white, white);
    next_x += del_btn_rect.w + 10;   // отступ 10 между кнопками

    save_btn_rect = make_button(next_x, btn_y, "Save", pad_x, pad_y);
    draw_button(r, save_btn_rect, "Save", yellow, black, black);
    next_x += save_btn_rect.w + 10;

    add_btn_rect = make_button(next_x, btn_y, "Add Enemy", pad_x, pad_y);
    draw_button(r, add_btn_rect, "Add Enemy", (SDL_Color){100,200,100}, white, white);
    next_x += add_btn_rect.w + 10;

    refresh_btn_rect = make_button(next_x, btn_y, "Refresh", pad_x, pad_y);
    draw_button(r, refresh_btn_rect, "Refresh", (SDL_Color){180,180,255}, black, black);
}

// Обработка кликов по панели редактирования
void enemies_handle_edit_panel_click(int mx, int my, int px, int py) {
    if (selected_index < 0) return;
    int base_x = px + 10;
    int field_offset = 100;

    // Обработка стрелок и кнопок
    int y = py + 10;

    // Portrait
    int y_port = y + EF_PORTRAIT*28;
    SDL_Rect brow1 = {base_x+field_offset+150+5, y_port, 70, 22};
    if (mx >= brow1.x && mx < brow1.x+brow1.w && my >= brow1.y && my < brow1.y+brow1.h) {
        char *path = open_file_dialog("..\\assets\\ui\\portraits");
        if (path) {
            const char *fname = strrchr(path, '\\'); if (!fname) fname = path; else fname++;
            char tmp[128]; strncpy(tmp, fname, 127); tmp[127] = '\0'; char *dot = strrchr(tmp, '.'); if (dot) *dot = '\0';
            strncpy(portrait_buf, tmp, 63); portrait_buf[63] = '\0';
            free(path);
            cJSON *enemy = cJSON_GetArrayItem(enemies_array, selected_index);
            cJSON *prt = cJSON_GetObjectItem(enemy, "portrait");
            if (prt) cJSON_SetValuestring(prt, portrait_buf);
            selected_portrait_idx = -1;
            for (int i = 0; i < portrait_list_count; i++) { if (strcmp(portrait_list[i], portrait_buf) == 0) { selected_portrait_idx = i; break; } }
            loaded_portrait_name[0] = '\0';
        }
        return;
    }
    SDL_Rect port_prev = {brow1.x + brow1.w + 5, y_port, 20, 22};
    if (mx >= port_prev.x && mx < port_prev.x+port_prev.w && my >= port_prev.y && my < port_prev.y+port_prev.h) {
        if (portrait_list_count > 0) {
            if (selected_portrait_idx > 0) selected_portrait_idx--;
            else selected_portrait_idx = portrait_list_count - 1;
            strncpy(portrait_buf, portrait_list[selected_portrait_idx], 63); portrait_buf[63] = '\0';
            cJSON *enemy = cJSON_GetArrayItem(enemies_array, selected_index);
            cJSON *prt = cJSON_GetObjectItem(enemy, "portrait");
            if (prt) cJSON_SetValuestring(prt, portrait_buf);
            loaded_portrait_name[0] = '\0';
        }
        return;
    }
    SDL_Rect port_next = {port_prev.x + 25, y_port, 20, 22};
    if (mx >= port_next.x && mx < port_next.x+port_next.w && my >= port_next.y && my < port_next.y+port_next.h) {
        if (portrait_list_count > 0) {
            selected_portrait_idx = (selected_portrait_idx + 1) % portrait_list_count;
            strncpy(portrait_buf, portrait_list[selected_portrait_idx], 63); portrait_buf[63] = '\0';
            cJSON *enemy = cJSON_GetArrayItem(enemies_array, selected_index);
            cJSON *prt = cJSON_GetObjectItem(enemy, "portrait");
            if (prt) cJSON_SetValuestring(prt, portrait_buf);
            loaded_portrait_name[0] = '\0';
        }
        return;
    }

    // Mapsprite
    int y_map = y + EF_MAPSPRITE*28;
    SDL_Rect brow2 = {base_x+field_offset+150+5, y_map, 70, 22};
    if (mx >= brow2.x && mx < brow2.x+brow2.w && my >= brow2.y && my < brow2.y+brow2.h) {
        char *path = open_file_dialog("..\\assets\\mapsprites_enemy");
        if (path) {
            const char *fname = strrchr(path, '\\'); if (!fname) fname = path; else fname++;
            char tmp[128]; strncpy(tmp, fname, 127); tmp[127] = '\0'; char *dot = strrchr(tmp, '.'); if (dot) *dot = '\0';
            strncpy(mapsprite_buf, tmp, 63); mapsprite_buf[63] = '\0';
            free(path);
            cJSON *enemy = cJSON_GetArrayItem(enemies_array, selected_index);
            cJSON *msp = cJSON_GetObjectItem(enemy, "mapsprite");
            if (msp) cJSON_SetValuestring(msp, mapsprite_buf);
            selected_mapsprite_idx = -1;
            for (int i = 0; i < mapsprite_list_count; i++) { if (strcmp(mapsprite_list[i], mapsprite_buf) == 0) { selected_mapsprite_idx = i; break; } }
        }
        return;
    }
    SDL_Rect map_prev = {brow2.x + brow2.w + 5, y_map, 20, 22};
    if (mx >= map_prev.x && mx < map_prev.x+map_prev.w && my >= map_prev.y && my < map_prev.y+map_prev.h) {
        if (mapsprite_list_count > 0) {
            if (selected_mapsprite_idx > 0) selected_mapsprite_idx--;
            else selected_mapsprite_idx = mapsprite_list_count - 1;
            strncpy(mapsprite_buf, mapsprite_list[selected_mapsprite_idx], 63); mapsprite_buf[63] = '\0';
            cJSON *enemy = cJSON_GetArrayItem(enemies_array, selected_index);
            cJSON *msp = cJSON_GetObjectItem(enemy, "mapsprite");
            if (msp) cJSON_SetValuestring(msp, mapsprite_buf);
        }
        return;
    }
    SDL_Rect map_next = {map_prev.x + 25, y_map, 20, 22};
    if (mx >= map_next.x && mx < map_next.x+map_next.w && my >= map_next.y && my < map_next.y+map_next.h) {
        if (mapsprite_list_count > 0) {
            selected_mapsprite_idx = (selected_mapsprite_idx + 1) % mapsprite_list_count;
            strncpy(mapsprite_buf, mapsprite_list[selected_mapsprite_idx], 63); mapsprite_buf[63] = '\0';
            cJSON *enemy = cJSON_GetArrayItem(enemies_array, selected_index);
            cJSON *msp = cJSON_GetObjectItem(enemy, "mapsprite");
            if (msp) cJSON_SetValuestring(msp, mapsprite_buf);
        }
        return;
    }

    // Race
    int y_race = y + EF_RACE*28;
    SDL_Rect race_prev = {base_x+field_offset+150+5, y_race, 20, 22};
    if (mx >= race_prev.x && mx < race_prev.x+race_prev.w && my >= race_prev.y && my < race_prev.y+race_prev.h) {
        race_index = (race_index - 1 + race_count) % race_count;
        update_race_status_fields();
        snprintf(edit_fields[EF_RACE].text, sizeof(edit_fields[EF_RACE].text), "%s", race_options[race_index]);
        return;
    }
    SDL_Rect race_next = {race_prev.x + 25, y_race, 20, 22};
    if (mx >= race_next.x && mx < race_next.x+race_next.w && my >= race_next.y && my < race_next.y+race_next.h) {
        race_index = (race_index + 1) % race_count;
        update_race_status_fields();
        snprintf(edit_fields[EF_RACE].text, sizeof(edit_fields[EF_RACE].text), "%s", race_options[race_index]);
        return;
    }

    // Status
    int y_status = y + EF_STATUS*28;
    SDL_Rect status_prev = {base_x+field_offset+150+5, y_status, 20, 22};
    if (mx >= status_prev.x && mx < status_prev.x+status_prev.w && my >= status_prev.y && my < status_prev.y+status_prev.h) {
        status_index = (status_index - 1 + status_count) % status_count;
        update_race_status_fields();
        snprintf(edit_fields[EF_STATUS].text, sizeof(edit_fields[EF_STATUS].text), "%s", status_options[status_index]);
        return;
    }
    SDL_Rect status_next = {status_prev.x + 25, y_status, 20, 22};
    if (mx >= status_next.x && mx < status_next.x+status_next.w && my >= status_next.y && my < status_next.y+status_next.h) {
        status_index = (status_index + 1) % status_count;
        update_race_status_fields();
        snprintf(edit_fields[EF_STATUS].text, sizeof(edit_fields[EF_STATUS].text), "%s", status_options[status_index]);
        return;
    }

    // Кнопки действий (используем динамические прямоугольники)
    if (mx >= save_btn_rect.x && mx < save_btn_rect.x+save_btn_rect.w &&
        my >= save_btn_rect.y && my < save_btn_rect.y+save_btn_rect.h) {
        if (active_field_index >= 0) commit_field(active_field_index);
        active_field_index = -1;
        if (save_enemies_to_file()) save_timer = 60;
        return;
    }
    if (mx >= del_btn_rect.x && mx < del_btn_rect.x+del_btn_rect.w &&
        my >= del_btn_rect.y && my < del_btn_rect.y+del_btn_rect.h) {
        if (active_field_index >= 0) commit_field(active_field_index);
        active_field_index = -1;
        if (selected_index >= 0 && enemies_count > 0) {
            cJSON *new_arr = cJSON_CreateArray();
            for (int i = 0; i < enemies_count; i++) {
                if (i == selected_index) continue;
                cJSON_AddItemToArray(new_arr, cJSON_Duplicate(cJSON_GetArrayItem(enemies_array, i), 1));
            }
            cJSON_Delete(enemies_array);
            enemies_array = new_arr;
            enemies_count--;
            if (enemies_count == 0) { selected_index = -1; edit_field_count = 0; }
            else {
                if (selected_index >= enemies_count) selected_index = enemies_count - 1;
                if (selected_index >= 0) open_edit_fields(cJSON_GetArrayItem(enemies_array, selected_index));
            }
        }
        return;
    }
    if (mx >= add_btn_rect.x && mx < add_btn_rect.x+add_btn_rect.w &&
        my >= add_btn_rect.y && my < add_btn_rect.y+add_btn_rect.h) {
        if (active_field_index >= 0) commit_field(active_field_index);
        active_field_index = -1;
        cJSON *new_enemy = cJSON_CreateObject();
        int new_id = 0;
        for (int i = 0; i < enemies_count; i++) {
            int id = json_int(cJSON_GetArrayItem(enemies_array, i), "id", -1);
            if (id >= new_id) new_id = id + 1;
        }
        cJSON_AddNumberToObject(new_enemy, "id", new_id);
        cJSON_AddStringToObject(new_enemy, "name", "New Enemy");
        cJSON_AddStringToObject(new_enemy, "portrait", portrait_list_count > 0 ? portrait_list[0] : "Mushra");
        cJSON_AddStringToObject(new_enemy, "mapsprite", mapsprite_list_count > 0 ? mapsprite_list[0] : "hero");
        cJSON_AddStringToObject(new_enemy, "battle_sprite", "");
        cJSON_AddStringToObject(new_enemy, "race", "Enterran");
        cJSON_AddStringToObject(new_enemy, "status", "normal");
        cJSON_AddItemToArray(enemies_array, new_enemy);
        enemies_count++;
        selected_index = enemies_count - 1;
        open_edit_fields(new_enemy);
        return;
    }
    if (mx >= refresh_btn_rect.x && mx < refresh_btn_rect.x+refresh_btn_rect.w &&
        my >= refresh_btn_rect.y && my < refresh_btn_rect.y+refresh_btn_rect.h) {
        if (active_field_index >= 0) commit_field(active_field_index);
        active_field_index = -1;
        if (enemies_array) cJSON_Delete(enemies_array);
        enemies_array = NULL;
        enemies_count = 0;
        selected_index = -1;
        FILE *f = fopen("../data/enemies/enemies.json", "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
            char *buf = malloc(len+1); fread(buf, 1, len, f); buf[len] = '\0'; fclose(f);
            cJSON *root = cJSON_Parse(buf); free(buf);
            if (root) {
                cJSON *arr = cJSON_GetObjectItem(root, "enemies");
                if (arr && cJSON_IsArray(arr)) {
                    enemies_array = cJSON_Duplicate(arr, 1);
                    enemies_count = cJSON_GetArraySize(arr);
                    if (enemies_count > 0) { selected_index = 0; open_edit_fields(cJSON_GetArrayItem(enemies_array, 0)); }
                }
                cJSON_Delete(root);
            }
        }
        return;
    }

    // Активация текстовых полей (is_special == 0 или -1, но -1 только для чтения)
    if (active_field_index >= 0) {
        commit_field(active_field_index);
        active_field_index = -1;
    }
    for (int i = 0; i < edit_field_count; i++) {
        if (edit_fields[i].is_special != 0 && edit_fields[i].is_special != -1) continue; // пропускаем специальные (1,2,3,4)
        SDL_Rect r = edit_fields[i].rect;
        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) {
            active_field_index = i;
            edit_fields[i].active = 1;
            edit_fields[i].cursor = strlen(edit_fields[i].text);
            return;
        }
    }
}

void enemies_clamp_scroll(int start_y) {
    int line_h = 20;
    int total_height = enemies_count * line_h;
    int visible_height = window_height - start_y - 20;
    int max_scroll = total_height - visible_height;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset < 0) scroll_offset = 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
}