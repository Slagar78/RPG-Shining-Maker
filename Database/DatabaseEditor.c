// Database/DatabaseEditor.c
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "../cJSON.h"
#include "items_editor.h"
#include "actors_editor.h"
#include "classes_editor.h"
#include "enemies_editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <commdlg.h>
#include "formatter.h"
#include "system_editor.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 800

// Глобальные данные
cJSON *spells_json = NULL;
int spells_count = 0;
cJSON *spells_root = NULL;
cJSON *items_json = NULL;
int items_count = 0;
cJSON *actors_json = NULL;
int actors_count = 0;
cJSON *classes_json = NULL;
int classes_count = 0;
cJSON *enemies_json = NULL;
int enemies_count = 0;
cJSON *curves_json = NULL;
int curves_count = 0;
cJSON *start_inventory_json = NULL;
int start_inventory_count = 0;

char error_msg[256] = "";

typedef enum { TAB_SPELLS, TAB_ITEMS, TAB_ACTORS, TAB_CLASSES, TAB_ENEMIES, TAB_SYSTEM, TAB_FORMAT } Tab;
Tab current_tab = TAB_SPELLS;

TTF_Font *g_font = NULL;
int g_font_ok = 0;

// Группировка заклинаний
#define MAX_SPELL_GROUPS 100
typedef struct {
    char name[64];
    int level_count;
    cJSON* levels[20];
    int expanded;
} SpellGroup;
SpellGroup groups[MAX_SPELL_GROUPS];
int group_count = 0;

// Выделение
int selected_line = -1, selected_group = -1, selected_is_group = 0;

// Редактирование
#define MAX_EDIT_FIELDS 10
typedef struct {
    char text[256];
    int cursor;
    int active;
    cJSON *json_obj;
    const char *json_key;
    int is_numeric;
    int max_len;
    int array_index;
    SDL_Rect rect;
} EditField;
EditField edit_fields[MAX_EDIT_FIELDS];
int edit_field_count = 0;
int active_field_index = -1;

// Кнопка Save (spells)
int save_timer = 0;
#define SAVE_BLINK_DURATION 42

// Прокрутка списка заклинаний
int spell_scroll = 0;

int format_timer = 0;

// Прототипы
void build_spell_groups();
void get_current_data(cJSON **arr, int *count);
void add_new_spell();
int get_max_spell_id();
void get_next_magic_name(char *buf, int size);
void normalize_spell_name(char *name);

// Безопасное получение строки/числа из cJSON
const char* json_string(cJSON *item, const char *key) {
    cJSON *field = cJSON_GetObjectItem(item, key);
    return (field && cJSON_IsString(field)) ? field->valuestring : "?";
}
int json_int(cJSON *item, const char *key, int def) {
    cJSON *field = cJSON_GetObjectItem(item, key);
    return (field && cJSON_IsNumber(field)) ? field->valueint : def;
}

// Загрузка JSON с сохранением корня (для spells)
int load_json_file_with_root(const char *filename, cJSON **out_array, const char *key, int *count, cJSON **out_root) {
    FILE *f = fopen(filename, "rb");
    if (!f) { snprintf(error_msg, sizeof(error_msg), "Cannot open %s", filename); return 0; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len+1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { snprintf(error_msg, sizeof(error_msg), "JSON parse error"); return 0; }
    cJSON *arr = cJSON_GetObjectItem(root, key);
    if (!arr || !cJSON_IsArray(arr)) {
        snprintf(error_msg, sizeof(error_msg), "Missing '%s' array", key);
        cJSON_Delete(root);
        return 0;
    }
    *out_array = arr;
    *count = cJSON_GetArraySize(arr);
    *out_root = root;
    error_msg[0] = '\0';
    return 1;
}

int load_json_file(const char *filename, cJSON **out_array, const char *key, int *count) {
    cJSON *root = NULL;
    return load_json_file_with_root(filename, out_array, key, count, &root);
}

void load_all() {
    load_json_file_with_root("../data/spells/spells.json", &spells_json, "spells", &spells_count, &spells_root);
    load_json_file("../data/items/items.json", &items_json, "items", &items_count);
    load_json_file("../data/actors/actors.json", &actors_json, "actors", &actors_count);
    load_json_file("../data/actors/classes.json", &classes_json, "classes", &classes_count);
	load_json_file("../data/actors/growth_curves.json", &curves_json, "curves", &curves_count);
	load_json_file("../data/actors/start_inventory.json", &start_inventory_json, "start_inventory", &start_inventory_count);
    load_json_file("../data/enemies/enemies.json", &enemies_json, "enemies", &enemies_count);
}

void reload_spells() {
    if (spells_root) {
        cJSON_Delete(spells_root);
        spells_root = NULL;
    }
    spells_json = NULL;
    spells_count = 0;
    load_json_file_with_root("../data/spells/spells.json", &spells_json, "spells", &spells_count, &spells_root);
    selected_line = selected_group = -1;
    selected_is_group = 0;
    edit_field_count = 0;
    active_field_index = -1;
    build_spell_groups();
    if (error_msg[0] == 'C' || error_msg[0] == 'M' || error_msg[0] == 'J') {
        error_msg[0] = '\0';
    }
}

void build_spell_groups() {
    if (!spells_json) return;
    group_count = 0;
    for (int i = 0; i < spells_count; i++) {
        cJSON *spell = cJSON_GetArrayItem(spells_json, i);
        const char *name = json_string(spell, "name");
        int found = -1;
        for (int g = 0; g < group_count; g++)
            if (strcasecmp(groups[g].name, name) == 0) { found = g; break; }
        if (found == -1) {
            if (group_count >= MAX_SPELL_GROUPS) break;
            strncpy(groups[group_count].name, name, 63);
            groups[group_count].name[63] = '\0';
            groups[group_count].level_count = 0;
            groups[group_count].expanded = 0;   // по умолчанию свёрнуты
            found = group_count++;
        }
        if (groups[found].level_count < 20)
            groups[found].levels[groups[found].level_count++] = spell;
    }
    spell_scroll = 0;
}

int draw_text_ext(SDL_Renderer *r, int x, int y, const char *text, SDL_Color color) {
    if (!g_font_ok) return 0;
    SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, text, color);
    if (!s) return 0;
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect d = {x, y, s->w, s->h};
    SDL_RenderCopy(r, t, NULL, &d);
    int w = s->w;
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
    return w;
}

char* open_file_dialog(const char* initialDir) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    char szInitialDir[260];
    strncpy(szInitialDir, initialDir, sizeof(szInitialDir) - 1);
    szInitialDir[sizeof(szInitialDir) - 1] = '\0';
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrFilter = "PNG Images\0*.png\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) {
        char *result = malloc(strlen(ofn.lpstrFile) + 1);
        strcpy(result, ofn.lpstrFile);
        return result;
    }
    return NULL;
}

void remove_spell_index(int idx) {
    if (idx < 0 || idx >= spells_count) return;
    cJSON *new_arr = cJSON_CreateArray();
    for (int i = 0; i < spells_count; i++) {
        if (i == idx) continue;
        cJSON_AddItemToArray(new_arr, cJSON_Duplicate(cJSON_GetArrayItem(spells_json, i), 1));
    }
    cJSON_Delete(spells_json);
    spells_json = new_arr;
    spells_count = cJSON_GetArraySize(new_arr);
}

int has_duplicate_spells() {
    for (int i = 0; i < spells_count; i++) {
        cJSON *a = cJSON_GetArrayItem(spells_json, i);
        const char *name_a = json_string(a, "name");
        int level_a = json_int(a, "level", -1);
        for (int j = i + 1; j < spells_count; j++) {
            cJSON *b = cJSON_GetArrayItem(spells_json, j);
            if (strcasecmp(json_string(b, "name"), name_a) == 0 &&
            json_int(b, "level", -1) == level_a) {
            return 1;
            }
        }
    }
    return 0;
}

int save_spells_to_file() {
    if (!spells_json) return 0;
    if (has_duplicate_spells()) {
        snprintf(error_msg, sizeof(error_msg), "Duplicate spell detected (same name and level)!");
        return 0;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON *dup = cJSON_Duplicate(spells_json, 1);
    cJSON_AddItemToObject(root, "spells", dup);
    char *json_str = cJSON_PrintBuffered(root, 0, 1);
    FILE *f = fopen("../data/spells/spells.json", "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
        error_msg[0] = '\0';
    } else {
        snprintf(error_msg, sizeof(error_msg), "Failed to save spells.json");
    }
    free(json_str);
    cJSON_Delete(root);
    return 1;
}

int save_start_inventory_to_file(void) {
    if (!start_inventory_json) return 0;
    cJSON *root = cJSON_CreateObject();
    cJSON *dup = cJSON_Duplicate(start_inventory_json, 1);
    cJSON_AddItemToObject(root, "start_inventory", dup);
    char *js = cJSON_PrintBuffered(root, 0, 1);
    FILE *f = fopen("../data/actors/start_inventory.json", "w");
    if (f) {
        fputs(js, f);
        fclose(f);
        error_msg[0] = '\0';
        free(js);
        cJSON_Delete(root);
        return 1;
    } else {
        snprintf(error_msg, sizeof(error_msg), "Failed to save start_inventory.json");
        free(js);
        cJSON_Delete(root);
        return 0;
    }
}

// Поля редактирования (добавлено поле Type, порядок: Name, Level, MP, Type, Power, Range Min, Range Max, Radius, Icon)
void open_edit_fields(cJSON *item) {
    edit_field_count = 0;
    active_field_index = -1;
    if (!item) return;
    int base_x = 360;
    int base_y = 80;
    int field_offset = 100;

    snprintf(edit_fields[0].text, sizeof(edit_fields[0].text), "%s", json_string(item, "name"));
    edit_fields[0].active = 0; edit_fields[0].json_obj = item; edit_fields[0].json_key = "name"; edit_fields[0].is_numeric = 0; edit_fields[0].max_len = 0; edit_fields[0].array_index = -1;
    edit_fields[0].rect = (SDL_Rect){base_x+field_offset, base_y + 0*35, 150, 22};

    snprintf(edit_fields[1].text, sizeof(edit_fields[1].text), "%d", json_int(item, "level", 0));
    edit_fields[1].active = 0; edit_fields[1].json_obj = item; edit_fields[1].json_key = "level"; edit_fields[1].is_numeric = 1; edit_fields[1].max_len = 3; edit_fields[1].array_index = -1;
    edit_fields[1].rect = (SDL_Rect){base_x+field_offset, base_y + 1*35, 150, 22};

    snprintf(edit_fields[2].text, sizeof(edit_fields[2].text), "%d", json_int(item, "mp", 0));
    edit_fields[2].active = 0; edit_fields[2].json_obj = item; edit_fields[2].json_key = "mp"; edit_fields[2].is_numeric = 1; edit_fields[2].max_len = 5; edit_fields[2].array_index = -1;
    edit_fields[2].rect = (SDL_Rect){base_x+field_offset, base_y + 2*35, 150, 22};

    // Type – обычное текстовое поле (без выпадающего меню)
    snprintf(edit_fields[3].text, sizeof(edit_fields[3].text), "%s", json_string(item, "type"));
    edit_fields[3].active = 0; edit_fields[3].json_obj = item; edit_fields[3].json_key = "type"; edit_fields[3].is_numeric = 0; edit_fields[3].max_len = 0; edit_fields[3].array_index = -1;
    edit_fields[3].rect = (SDL_Rect){base_x+field_offset, base_y + 3*35, 150, 22};

    snprintf(edit_fields[4].text, sizeof(edit_fields[4].text), "%d", json_int(item, "power", 0));
    edit_fields[4].active = 0; edit_fields[4].json_obj = item; edit_fields[4].json_key = "power"; edit_fields[4].is_numeric = 1; edit_fields[4].max_len = 5; edit_fields[4].array_index = -1;
    edit_fields[4].rect = (SDL_Rect){base_x+field_offset, base_y + 4*35, 150, 22};

    cJSON *rangeArr = cJSON_GetObjectItem(item, "range");
    int rmin = 0, rmax = 0;
    if (rangeArr && cJSON_IsArray(rangeArr) && cJSON_GetArraySize(rangeArr) >= 2) {
        rmin = cJSON_GetArrayItem(rangeArr, 0)->valueint;
        rmax = cJSON_GetArrayItem(rangeArr, 1)->valueint;
    }
    snprintf(edit_fields[5].text, sizeof(edit_fields[5].text), "%d", rmin);
    edit_fields[5].active = 0; edit_fields[5].json_obj = item; edit_fields[5].json_key = NULL; edit_fields[5].is_numeric = 1; edit_fields[5].max_len = 2; edit_fields[5].array_index = 0;
    edit_fields[5].rect = (SDL_Rect){base_x+field_offset, base_y + 5*35, 150, 22};

    snprintf(edit_fields[6].text, sizeof(edit_fields[6].text), "%d", rmax);
    edit_fields[6].active = 0; edit_fields[6].json_obj = item; edit_fields[6].json_key = NULL; edit_fields[6].is_numeric = 1; edit_fields[6].max_len = 2; edit_fields[6].array_index = 1;
    edit_fields[6].rect = (SDL_Rect){base_x+field_offset, base_y + 6*35, 150, 22};

    snprintf(edit_fields[7].text, sizeof(edit_fields[7].text), "%d", json_int(item, "radius", 0));
    edit_fields[7].active = 0; edit_fields[7].json_obj = item; edit_fields[7].json_key = "radius"; edit_fields[7].is_numeric = 1; edit_fields[7].max_len = 2; edit_fields[7].array_index = -1;
    edit_fields[7].rect = (SDL_Rect){base_x+field_offset, base_y + 7*35, 150, 22};

    snprintf(edit_fields[8].text, sizeof(edit_fields[8].text), "%s", json_string(item, "icon"));
    edit_fields[8].active = 0; edit_fields[8].json_obj = item; edit_fields[8].json_key = "icon"; edit_fields[8].is_numeric = 0; edit_fields[8].max_len = 0; edit_fields[8].array_index = -1;
    edit_fields[8].rect = (SDL_Rect){base_x+field_offset, base_y + 8*35, 150, 22};

    edit_field_count = 9;
}

void commit_field(int idx) {
    if (idx < 0 || idx >= edit_field_count) return;
    EditField *f = &edit_fields[idx];
    if (!f->json_obj) return;
    if (f->json_key) {
    if (f->is_numeric) {
        // ... (числовые поля)
    } else {
        // Для имени нормализуем регистр
        if (strcmp(f->json_key, "name") == 0) {
            normalize_spell_name(f->text);
        }
        cJSON *str = cJSON_GetObjectItem(f->json_obj, f->json_key);
        if (str && cJSON_IsString(str)) cJSON_SetValuestring(str, f->text);
    }
}
	else if (f->array_index >= 0) {
        cJSON *arr = cJSON_GetObjectItem(f->json_obj, "range");
        if (!arr) {
            arr = cJSON_CreateArray();
            cJSON_ReplaceItemInObject(f->json_obj, "range", arr);
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(0));
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(0));
        }
        while (cJSON_GetArraySize(arr) <= f->array_index)
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(0));
        cJSON *elem = cJSON_GetArrayItem(arr, f->array_index);
        if (elem && cJSON_IsNumber(elem)) { int v = atoi(f->text); elem->valueint = v; elem->valuedouble = v; }
    }
    f->active = 0;
}

int handle_edit_input(SDL_Event *evt, int field_idx) {
    EditField *f = &edit_fields[field_idx];
    SDL_Rect r = f->rect;
    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) {
            if (active_field_index >= 0 && active_field_index != field_idx) commit_field(active_field_index);
            active_field_index = field_idx;
            f->active = 1;
            f->cursor = strlen(f->text);
            return 1;
        }
        if (f->active) {
            commit_field(field_idx);
            int best_idx = -1, best_dist = 1000;
            for (int i = 0; i < edit_field_count; i++) {
                if (i == 8) continue; // пропускаем Icon
                SDL_Rect fr = edit_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < best_dist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) {
                    best_dist = dist;
                    best_idx = i;
                }
            }
            if (best_idx >= 0) {
                edit_fields[best_idx].active = 1;
                edit_fields[best_idx].cursor = strlen(edit_fields[best_idx].text);
                active_field_index = best_idx;
            } else {
                active_field_index = -1;
            }
            return 1;
        } else {
            int best_idx = -1, best_dist = 1000;
            for (int i = 0; i < edit_field_count; i++) {
                if (i == 8) continue;
                SDL_Rect fr = edit_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < best_dist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) {
                    best_dist = dist;
                    best_idx = i;
                }
            }
            if (best_idx >= 0) {
                if (active_field_index >= 0) commit_field(active_field_index);
                edit_fields[best_idx].active = 1;
                edit_fields[best_idx].cursor = strlen(edit_fields[best_idx].text);
                active_field_index = best_idx;
                return 1;
            }
        }
    } else if (evt->type == SDL_KEYDOWN && f->active) {
        if (evt->key.keysym.sym == SDLK_UP) {
            if (field_idx == 0) return 1;
            commit_field(field_idx);
            int new_idx = field_idx - 1;
            if (new_idx == 3) new_idx = 2;   // пропускаем Type (индекс 3)
            edit_fields[new_idx].active = 1;
            edit_fields[new_idx].cursor = strlen(edit_fields[new_idx].text);
            active_field_index = new_idx;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_DOWN) {
            if (field_idx >= 7) return 1;     // останавливаемся на Radius (7)
            commit_field(field_idx);
            int new_idx = field_idx + 1;
            if (new_idx == 3) new_idx = 4;   // пропускаем Type (индекс 3)
            edit_fields[new_idx].active = 1;
            edit_fields[new_idx].cursor = strlen(edit_fields[new_idx].text);
            active_field_index = new_idx;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
            if (f->cursor > 0) {
                memmove(f->text + f->cursor - 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->cursor--;
            }
            return 1;
        } else if (evt->key.keysym.sym == SDLK_RETURN || evt->key.keysym.sym == SDLK_KP_ENTER) {
            commit_field(field_idx);
            return 1;
        } else if (evt->key.keysym.sym == SDLK_LEFT && f->cursor > 0) { f->cursor--; return 1; }
        else if (evt->key.keysym.sym == SDLK_RIGHT && f->cursor < strlen(f->text)) { f->cursor++; return 1; }
    } else if (evt->type == SDL_TEXTINPUT && f->active) {
        char ch = evt->text.text[0];
        if (f->is_numeric) {
            if (isdigit(ch) || (ch == '-' && f->cursor == 0 && f->text[0] == '\0')) {
                if (f->max_len > 0 && strlen(f->text) >= f->max_len) return 1;
                if (strlen(f->text) < 255) {
                    memmove(f->text + f->cursor + 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                    f->text[f->cursor++] = ch;
                }
            }
        } else {
            if (ch >= 32 && ch <= 126 && strlen(f->text) < 255) {
                if (f->max_len > 0 && strlen(f->text) >= f->max_len) return 1;
                memmove(f->text + f->cursor + 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->text[f->cursor++] = ch;
            }
        }
        return 1;
    }
    return 0;
}

void draw_edit_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *display_text) {
    SDL_Color white = {255,255,255}, black = {0,0,0}, gray = {100,100,100};
    draw_text_ext(r, x, y + 3, label, white);
    int field_x = x + 100;
    SDL_Rect rect = {field_x, y, w, h};
    SDL_SetRenderDrawColor(r, gray.r, gray.g, gray.b, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, white.r, white.g, white.b, 255);
    SDL_RenderDrawRect(r, &rect);
    draw_text_ext(r, field_x+5, y+3, display_text, black);
    if (edit_fields[idx].active) {
		
        char before[256] = {0};
        if (edit_fields[idx].cursor > 0) {
            strncpy(before, edit_fields[idx].text, edit_fields[idx].cursor);
            before[edit_fields[idx].cursor] = '\0';
        }
		
        SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, before, black);
        int offset = field_x+5 + (s ? s->w : 0);
        if (s) SDL_FreeSurface(s);
        draw_text_ext(r, offset, y+3, "|", black);
    }
}

void add_new_spell() {
    if (!spells_json) return;
    int new_id = get_max_spell_id() + 1;
    char new_name[64];
    get_next_magic_name(new_name, sizeof(new_name));
	normalize_spell_name(new_name);

    cJSON *new_spell = cJSON_CreateObject();
    cJSON_AddNumberToObject(new_spell, "id", new_id);
    cJSON_AddStringToObject(new_spell, "name", new_name);
    cJSON_AddNumberToObject(new_spell, "level", 1);
    cJSON_AddNumberToObject(new_spell, "mp", 5);
    cJSON_AddStringToObject(new_spell, "type", "heal");
    cJSON *range = cJSON_CreateArray();
    cJSON_AddItemToArray(range, cJSON_CreateNumber(0));
    cJSON_AddItemToArray(range, cJSON_CreateNumber(3));
    cJSON_AddItemToObject(new_spell, "range", range);
    cJSON_AddNumberToObject(new_spell, "radius", 1);
    cJSON_AddNumberToObject(new_spell, "power", 5);
    cJSON_AddStringToObject(new_spell, "icon", "assets/spells/magic_empty.png");

    cJSON_AddItemToArray(spells_json, new_spell);
    spells_count++;

    build_spell_groups();
    selected_group = -1;
    for (int g = 0; g < group_count; g++) {
        if (strcmp(groups[g].name, new_name) == 0) {
            selected_group = g;
            break;
        }
    }
    selected_line = 0;
    if (selected_group >= 0 && groups[selected_group].level_count > 0) {
        open_edit_fields(groups[selected_group].levels[0]);
    } else {
        edit_field_count = 0;
        active_field_index = -1;
    }
}

int get_max_spell_id() {
    int max = -1;
    for (int i = 0; i < spells_count; i++) {
        cJSON *spell = cJSON_GetArrayItem(spells_json, i);
        int id = json_int(spell, "id", -1);
        if (id > max) max = id;
    }
    return max;
}

void get_next_magic_name(char *buf, int size) {
    int idx = 0;
    while (1) {
        snprintf(buf, size, "magic%02d", idx);
        int found = 0;
        for (int i = 0; i < spells_count; i++) {
            if (strcmp(json_string(cJSON_GetArrayItem(spells_json, i), "name"), buf) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) break;
        idx++;
    }
}

void normalize_spell_name(char *name) {
    if (!name || !*name) return;
    name[0] = toupper((unsigned char)name[0]);
    for (int i = 1; name[i]; i++) {
        name[i] = tolower((unsigned char)name[i]);
    }
}

// Проверка клика по кнопкам управления Spells
int check_spell_buttons(int mx, int my, int px, int py, int *action) {
    int btn_y = py + 320;

    // Browse (рядом с Icon)
    if (edit_field_count > 0) {
        int field_y = py + 10 + 8*35;
        SDL_Rect browse_btn = {px + 10 + 100 + 150 + 5, field_y, 70, 22};
        if (mx >= browse_btn.x && mx < browse_btn.x + browse_btn.w &&
            my >= browse_btn.y && my < browse_btn.y + browse_btn.h) {
            *action = 1; return 1;
        }
    }

    SDL_Rect save_btn = {px + 130, btn_y, 80, 30};
    if (mx >= save_btn.x && mx < save_btn.x + save_btn.w &&
        my >= save_btn.y && my < save_btn.y + save_btn.h) {
        *action = 2; return 1;
    }

    SDL_Rect refresh_btn = {px + 215, btn_y, 85, 30};
    if (mx >= refresh_btn.x && mx < refresh_btn.x + refresh_btn.w &&
        my >= refresh_btn.y && my < refresh_btn.y + refresh_btn.h) {
        *action = 6; return 1;
    }

    SDL_Rect del_spell_btn = {px + 10, btn_y, 115, 30};
    if (mx >= del_spell_btn.x && mx < del_spell_btn.x + del_spell_btn.w &&
        my >= del_spell_btn.y && my < del_spell_btn.y + del_spell_btn.h) {
        *action = 3; return 1;
    }

    SDL_Rect add_btn = {px + 305, btn_y, 90, 30};
    if (mx >= add_btn.x && mx < add_btn.x + add_btn.w &&
        my >= add_btn.y && my < add_btn.y + add_btn.h) {
        *action = 5; return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window *win = SDL_CreateWindow("Database Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    g_font = TTF_OpenFont("../assets/ui/fonts/DejaVuSans.ttf", 16);
    g_font_ok = (g_font != NULL);
    load_all();
    if (spells_json) build_spell_groups();
    items_init(items_json, items_count);
	items_set_window_height(WINDOW_HEIGHT);
    actors_init(actors_json, actors_count);
    actors_set_window_height(WINDOW_HEIGHT);
    classes_init(classes_json, classes_count);
    classes_set_window_height(WINDOW_HEIGHT);
    if (enemies_json) {
    enemies_init(enemies_json, enemies_count);
    enemies_set_window_height(WINDOW_HEIGHT);
    }
    system_init();

    SDL_Color white = {255,255,255}, red = {255,80,80}, blue = {80,160,255}, green_text = {0,200,0};
    SDL_Color cyan = {0,200,255}, yellow = {255,255,0,255}, bright_green = {0,255,0,255};
    SDL_Color black = {0,0,0,255};
    SDL_Color tab_inactive = {180,180,200}, tab_active = {100,100,255};

    const char *tab_names[] = {"Spells", "Items", "Actors", "Classes", "Enemies", "System", "Format"};
    int tab_count = 7, tab_w = 80, tab_h = 30;
    int running = 1;

    while (running) {
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_QUIT) running = 0;

            // Прокрутка колёсиком мыши
            if (evt.type == SDL_MOUSEWHEEL) {
            if (current_tab == TAB_SPELLS) spell_scroll -= evt.wheel.y * 30;
            else if (current_tab == TAB_ITEMS) items_adjust_scroll(evt.wheel.y * 30);
			else if (current_tab == TAB_ACTORS) actors_adjust_scroll(evt.wheel.y * 30);
			else if (current_tab == TAB_CLASSES) classes_adjust_scroll(evt.wheel.y * 30);
            else if (current_tab == TAB_ENEMIES) {
                enemies_adjust_scroll(evt.wheel.y * 30);
                enemies_clamp_scroll(tab_h + 5);
                }
            }

            // Обработка полей ввода (Spells)
            if (current_tab == TAB_SPELLS) {
                for (int i = 0; i < edit_field_count; i++)
                    if (handle_edit_input(&evt, i)) break;
                if (active_field_index >= 0 && (evt.type == SDL_KEYDOWN || evt.type == SDL_TEXTINPUT))
                    continue;
            }
            // Обработка полей ввода (Items)
            else if (current_tab == TAB_ITEMS) {
                items_handle_input(&evt);
                if (items_is_edit_active() && (evt.type == SDL_KEYDOWN || evt.type == SDL_TEXTINPUT))
                    continue;
            }
			else if (current_tab == TAB_ACTORS) {
                actors_handle_input(&evt);
                if (actors_is_edit_active() && (evt.type == SDL_KEYDOWN || evt.type == SDL_TEXTINPUT))
                    continue;
            }
			else if (current_tab == TAB_CLASSES) {
                classes_handle_input(&evt);
                if (classes_is_edit_active() && (evt.type == SDL_KEYDOWN || evt.type == SDL_TEXTINPUT))
                    continue;
            }

            else if (current_tab == TAB_ENEMIES) {
                enemies_handle_input(&evt);
                if (enemies_is_edit_active() && (evt.type == SDL_KEYDOWN || evt.type == SDL_TEXTINPUT))
                    continue;
            }
            else if (current_tab == TAB_SYSTEM) {
                system_handle_input(&evt);
                if (system_is_edit_active() && (evt.type == SDL_KEYDOWN || evt.type == SDL_TEXTINPUT))
                    continue;
            }
            else if (current_tab == TAB_FORMAT) {
            }

            // Клик левой кнопкой мыши
            if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == SDL_BUTTON_LEFT) {
                int mx = evt.button.x, my = evt.button.y;

                // Переключение вкладок
                if (my < tab_h) {
                    for (int i = 0; i < tab_count; i++) {
                        if (mx >= i*tab_w && mx < (i+1)*tab_w) {
                            current_tab = i;
                            selected_line = selected_group = -1;
                            selected_is_group = 0;
                            edit_field_count = 0; active_field_index = -1;
                            spell_scroll = 0;
                            items_reset_selection();
							actors_reset_selection();
							classes_reset_selection();
                            enemies_reset_selection();
                            system_reset_selection();
                            if (current_tab == TAB_SPELLS && spells_json) build_spell_groups();
                            break;
                        }
                    }
                    continue;
                }

                // Вкладка SPELLS
                if (current_tab == TAB_SPELLS) {
                    int px = 360, py = tab_h + 20;
                    int action = 0;
                    if (check_spell_buttons(mx, my, px, py, &action)) {
                        switch (action) {
                            case 1: // Browse иконки
                                if (edit_field_count > 0) {
                                    char *path = open_file_dialog("..\\assets\\spells\\");
                                    if (path) {
                                        const char *filename = strrchr(path, '\\');
                                        if (!filename) filename = path; else filename++;
                                        snprintf(edit_fields[8].text, sizeof(edit_fields[8].text), "assets/spells/%s", filename);
                                        edit_fields[8].cursor = strlen(edit_fields[8].text);
                                        commit_field(8);
                                        free(path);
                                    }
                                }
                                break;
                            case 2: // Save spells
                                if (save_spells_to_file())
                                    save_timer = SAVE_BLINK_DURATION;
                                break;
                            case 3: { // Delete spell (один уровень)
                                if (selected_group >= 0 && selected_line >= 0) {
                                    const char *spell_name = json_string(groups[selected_group].levels[0], "name");
                                    int max_level = -1, remove_idx = -1;
                                    for (int i = 0; i < spells_count; i++) {
                                        cJSON *sp = cJSON_GetArrayItem(spells_json, i);
                                        if (strcmp(json_string(sp, "name"), spell_name) == 0) {
                                            int lv = json_int(sp, "level", 0);
                                            if (lv > max_level) { max_level = lv; remove_idx = i; }
                                        }
                                    }
                                    if (remove_idx >= 0) remove_spell_index(remove_idx);
                                    selected_line = selected_group = -1;
                                    edit_field_count = 0;
                                    build_spell_groups();
                                }
                                break;
                            }
                            case 5: add_new_spell(); break;   // Add Spell
                            case 6: reload_spells(); break;   // Refresh
                        }
                        continue;
                    }

                    // Клик по списку заклинаний
                    int list_y = tab_h + 5 - spell_scroll;
                    int line_h = 20;
                    if (spells_json) {
                        int y = list_y, found = 0;
                        for (int g = 0; g < group_count && !found; g++) {
                            if (mx >= 10 && mx < 45 && my >= y && my < y+line_h) {
                                groups[g].expanded = !groups[g].expanded;
                                selected_line = selected_group = -1;
                                edit_field_count = 0; active_field_index = -1;
                                found = 1; break;
                            }
                            y += line_h;
                            if (groups[g].expanded) {
                                for (int l = 0; l < groups[g].level_count; l++) {
                                    if (mx >= 10 && mx < 300 && my >= y && my < y+line_h) {
                                        selected_line = l; selected_group = g;
                                        open_edit_fields(groups[g].levels[l]);
                                        found = 1; break;
                                    }
                                    y += line_h;
                                }
                            }
                        }
                    }
                }
                // Вкладка ITEMS
                else if (current_tab == TAB_ITEMS) {
                    items_handle_click(mx, my, tab_h + 5, items_get_scroll());
                }
				else if (current_tab == TAB_ACTORS) {
                actors_handle_click(mx, my, tab_h + 5, actors_get_scroll());
                }
				else if (current_tab == TAB_CLASSES) {
                    classes_handle_click(mx, my, tab_h + 5, classes_get_scroll());
                }
                else if (current_tab == TAB_ENEMIES) {
                    enemies_handle_edit_panel_click(mx, my, 360, tab_h + 20);
                    enemies_handle_click(mx, my, tab_h + 5, enemies_get_scroll());
                }
					else if (current_tab == TAB_FORMAT) {
						
                    int px = 360, py = tab_h + 20;
                    int btn_y = py + 80;
                    SDL_Rect format_btn = {px + 150, btn_y, 120, 30};
                    if (mx >= format_btn.x && mx < format_btn.x+format_btn.w &&
                        my >= format_btn.y && my < format_btn.y+format_btn.h) {
                        format_all_json();
                        format_timer = 90;  // ~1.5 секунды при 60 FPS
                    }
                }
            }
        }

        // --- ОТРИСОВКА ---
        SDL_SetRenderDrawColor(renderer, 30,30,30,255);
        SDL_RenderClear(renderer);

        // Вкладки
        for (int i = 0; i < tab_count; i++) {
            SDL_Rect tr = {i*tab_w, 0, tab_w, tab_h};
            SDL_Color c = (i == current_tab) ? tab_active : tab_inactive;
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
            SDL_RenderFillRect(renderer, &tr);
            SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &tr);
            if (g_font_ok) draw_text_ext(renderer, i*tab_w+5, 5, tab_names[i], black);
        }

        // Содержимое вкладок
        if (error_msg[0]) draw_text_ext(renderer, 10, tab_h+5, error_msg, red);
        else if (current_tab == TAB_SPELLS && spells_json) {
            int list_y = tab_h + 5 - spell_scroll;
            int y = list_y;
            int total_height = 0;
            for (int g = 0; g < group_count; g++) {
                total_height += 20;
                if (groups[g].expanded) total_height += groups[g].level_count * 20;
            }
            int max_scroll = total_height - (WINDOW_HEIGHT - tab_h - 5 - 50);
            if (max_scroll < 0) max_scroll = 0;
            if (spell_scroll < 0) spell_scroll = 0;
            if (spell_scroll > max_scroll) spell_scroll = max_scroll;

            SDL_Rect clip = {10, tab_h+5, 300, WINDOW_HEIGHT - tab_h - 5};
            SDL_RenderSetClipRect(renderer, &clip);

            for (int g = 0; g < group_count; g++) {
                char truncated_name[51];
                strncpy(truncated_name, groups[g].name, 50);
                truncated_name[50] = '\0';
                if (y + 20 >= clip.y && y <= clip.y+clip.h) {
                    int cur_x = 10;
                    cur_x += draw_text_ext(renderer, cur_x, y, groups[g].expanded ? "[-]" : "[+]", green_text);
                    cur_x += draw_text_ext(renderer, cur_x, y, truncated_name, red);
                    cur_x += draw_text_ext(renderer, cur_x, y, "(", blue);
                    char count_str[16];
                    snprintf(count_str, sizeof(count_str), "%d", groups[g].level_count);
                    cur_x += draw_text_ext(renderer, cur_x, y, count_str, cyan);
                    cur_x += draw_text_ext(renderer, cur_x, y, ")", blue);
                }
                y += 20;
                if (groups[g].expanded) {
                    for (int l = 0; l < groups[g].level_count; l++) {
                        if (y + 20 >= clip.y && y <= clip.y+clip.h) {
                            char lvl_buf[32];
                            snprintf(lvl_buf, sizeof(lvl_buf), "    Lv %d", json_int(groups[g].levels[l], "level", 0));
                            SDL_Rect rr = {10, y, 290, 20};
                            if (selected_group == g && selected_line == l) {
                                SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 128);
                                SDL_RenderFillRect(renderer, &rr);
                            }
                            draw_text_ext(renderer, 10, y, lvl_buf, white);
                        }
                        y += 20;
                    }
                }
            }
            SDL_RenderSetClipRect(renderer, NULL);
        }
        else if (current_tab == TAB_ITEMS) {
            items_draw_list(renderer, tab_h + 5, items_get_scroll());
        }
		else if (current_tab == TAB_ACTORS) {
            actors_draw_list(renderer, tab_h + 5, actors_get_scroll());
        }
		else if (current_tab == TAB_CLASSES) {
            classes_draw_list(renderer, tab_h + 5, classes_get_scroll());
        }
        else if (current_tab == TAB_ENEMIES) {
            enemies_draw_list(renderer, tab_h + 5, enemies_get_scroll());
        }

        // Панель редактирования Spells
        if (current_tab == TAB_SPELLS) {
            int px = 360, py = tab_h + 20;
            SDL_SetRenderDrawColor(renderer, 60,60,60,255);
            SDL_Rect panel = {px, py, 580, 500};
            SDL_RenderFillRect(renderer, &panel);
            SDL_SetRenderDrawColor(renderer, 255,255,255,255);
            SDL_RenderDrawRect(renderer, &panel);

            int y = py + 10;
            if (edit_field_count > 0) {
                draw_edit_field(renderer, px+10, y,   150, 22, 0, "Name:", edit_fields[0].text);
                draw_edit_field(renderer, px+10, y+35,150, 22, 1, "Level:", edit_fields[1].text);
                draw_edit_field(renderer, px+10, y+70,150, 22, 2, "MP:", edit_fields[2].text);
                draw_edit_field(renderer, px+10, y+105,150,22, 3, "Type:", edit_fields[3].text);
                draw_edit_field(renderer, px+10, y+140,150,22, 4, "Power:", edit_fields[4].text);
                draw_edit_field(renderer, px+10, y+175,150,22, 5, "Range Min:", edit_fields[5].text);
                draw_edit_field(renderer, px+10, y+210,150,22, 6, "Range Max:", edit_fields[6].text);
                draw_edit_field(renderer, px+10, y+245,150,22, 7, "Radius:", edit_fields[7].text);
                const char *short_name = strrchr(edit_fields[8].text, '/') ? strrchr(edit_fields[8].text, '/')+1 : edit_fields[8].text;
                draw_edit_field(renderer, px+10, y+280,150,22, 8, "Icon:", short_name);

                SDL_Rect browse_btn = {px+10+100+150+5, y+280, 70, 22};
                SDL_SetRenderDrawColor(renderer, 100,100,200,255); SDL_RenderFillRect(renderer, &browse_btn);
                SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &browse_btn);
                draw_text_ext(renderer, browse_btn.x+5, browse_btn.y+3, "Browse", white);
            } else {
                draw_text_ext(renderer, px+30, y+50, "Select a spell from the list to edit", white);
                draw_text_ext(renderer, px+30, y+80, "Press SAVE to write changes to spells.json", white);
            }

            // Кнопки управления Spells
            int btn_y = py + 320;
            SDL_Rect save_btn = {px+130, btn_y, 80, 30};
            SDL_Color save_col = (save_timer > 0) ? bright_green : yellow;
            SDL_SetRenderDrawColor(renderer, save_col.r, save_col.g, save_col.b, 255);
            SDL_RenderFillRect(renderer, &save_btn);
            SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &save_btn);
            draw_text_ext(renderer, save_btn.x+10, save_btn.y+5, "SAVE", black);

            SDL_Rect refresh_btn = {px+215, btn_y, 85, 30};
            SDL_SetRenderDrawColor(renderer, 180,180,255,255); SDL_RenderFillRect(renderer, &refresh_btn);
            SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &refresh_btn);
            draw_text_ext(renderer, refresh_btn.x+8, refresh_btn.y+5, "Refresh", black);

            SDL_Rect del_spell_btn = {px+10, btn_y, 115, 30};
            SDL_SetRenderDrawColor(renderer, 200,80,80,255); SDL_RenderFillRect(renderer, &del_spell_btn);
            SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &del_spell_btn);
            draw_text_ext(renderer, del_spell_btn.x+5, del_spell_btn.y+5, "Del Spell", white);

            SDL_Rect add_btn = {px+305, btn_y, 90, 30};
            SDL_SetRenderDrawColor(renderer, 100,200,100,255); SDL_RenderFillRect(renderer, &add_btn);
            SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &add_btn);
            draw_text_ext(renderer, add_btn.x+5, add_btn.y+5, "Add Spell", white);
        }

        // 
        if (current_tab == TAB_ITEMS) {
            items_draw_edit_panel(renderer, 360, tab_h + 20);
        }
		if (current_tab == TAB_ACTORS) {
           actors_draw_edit_panel(renderer, 360, tab_h + 20);
        }
		if (current_tab == TAB_CLASSES) {
           classes_draw_edit_panel(renderer, 360, tab_h + 20);
        }
        if (current_tab == TAB_ENEMIES) {
            enemies_draw_edit_panel(renderer, 360, tab_h + 20);
        }

        if (current_tab == TAB_SYSTEM) {
            system_draw_edit_panel(renderer, 360, tab_h + 20);
        }

        if (current_tab == TAB_FORMAT) {
            int px = 360, py = tab_h + 20;
            SDL_SetRenderDrawColor(renderer, 60,60,60,255);
            SDL_Rect panel = {px, py, 580, 200};
            SDL_RenderFillRect(renderer, &panel);
            SDL_SetRenderDrawColor(renderer, 255,255,255,255);
            SDL_RenderDrawRect(renderer, &panel);

            draw_text_ext(renderer, px+30, py+20, "Remove extra spaces from JSON files", white);
            draw_text_ext(renderer, px+30, py+50, "Press the button to format all files", white);

            int btn_y = py + 80;
            SDL_Rect format_btn = {px + 150, btn_y, 120, 30};
            SDL_Color btn_col = (format_timer > 0) ? bright_green : yellow;
            SDL_SetRenderDrawColor(renderer, btn_col.r, btn_col.g, btn_col.b, 255);
            SDL_RenderFillRect(renderer, &format_btn);
            SDL_SetRenderDrawColor(renderer, 0,0,0,255);
            SDL_RenderDrawRect(renderer, &format_btn);
            draw_text_ext(renderer, format_btn.x+15, format_btn.y+5, "Format All", black);
        }
        if (save_timer > 0) save_timer--;
		if (format_timer > 0) format_timer--;
			actors_update_timer();
            items_update_timer();  //
			classes_update_timer();
            enemies_update_timer();
            system_update_timer();

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // Очистка
    system_free();
    if (g_font) TTF_CloseFont(g_font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

void get_current_data(cJSON **arr, int *count) {
    switch (current_tab) {
        case TAB_SPELLS: *arr = spells_json; *count = spells_count; break;
        case TAB_ITEMS:  *arr = items_json;  *count = items_count;  break;
        case TAB_ACTORS: *arr = actors_json; *count = actors_count; break;
        case TAB_CLASSES:*arr = classes_json;*count = classes_count;break;
        default: *arr = NULL; *count = 0;
    }
}