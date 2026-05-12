// items_editor.c
#include "items_editor.h"
#include <stdio.h>
#include <string.h>
#include <SDL_ttf.h>
#include <ctype.h>
#include <windows.h>
#include <commdlg.h>

extern cJSON *spells_json;
extern int spells_count;
extern cJSON *items_json;
extern int items_count;
extern TTF_Font *g_font;
extern int g_font_ok;
extern char error_msg[256];
int draw_text_ext(SDL_Renderer *r, int x, int y, const char *text, SDL_Color color);
char* open_file_dialog(const char* initialDir);

static cJSON *items = NULL;
static int item_count = 0;
static int selected_item = -1;

static char  name_buf[21] = {0};
static int   price_val = 0;
static int   range_min = 0, range_max = 0;
static char  use_spell[64] = {0};
static int   spell_level = 1;
static char  icon_path[256] = {0};

// новые поля для категории и боевых характеристик
static char  category_buf[32] = {"consumable"};
static int   attack_val = 0;
static int   defense_val = 0;
static const char *category_list[] = {"consumable", "Weapon", "Ring"};
static const int category_count = 3;

static char spell_names[100][64];
static int  spell_name_count = 0;
static int  selected_spell_idx = -1;

static int  current_levels[20];
static int  current_level_count = 0;
static int  selected_level_idx = -1;

static int save_timer = 0;
#define SAVE_BLINK_DURATION 42
static int item_scroll = 0;
static int g_window_height = 680;

#define MAX_ITEM_FIELDS 10
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
    int is_special;
} ItemField;
static ItemField item_fields[MAX_ITEM_FIELDS];
static int item_field_count = 0;
static int item_active_field = -1;

static void build_spell_name_list(void);
static void update_levels_for_spell(void);
static void commit_item_changes(void);
static void load_item_fields(void);
static void open_item_fields(void);
static void commit_item_field(int idx);
static int  handle_item_input(SDL_Event *evt, int idx);
static void draw_item_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *fallback_text);
static void add_new_item(void);
static void delete_item(void);

// ---------- реализация ----------

static void build_spell_name_list(void) {
    spell_name_count = 0;
    for (int i = 0; i < spells_count; i++) {
        cJSON *spell = cJSON_GetArrayItem(spells_json, i);
        const char *name = cJSON_GetObjectItem(spell, "name")->valuestring;
        int exists = 0;
        for (int j = 0; j < spell_name_count; j++)
            if (strcmp(spell_names[j], name) == 0) { exists = 1; break; }
        if (!exists && spell_name_count < 100) {
            strncpy(spell_names[spell_name_count], name, 63);
            spell_names[spell_name_count][63] = '\0';
            spell_name_count++;
        }
    }
}

static void update_levels_for_spell(void) {
    current_level_count = 0;
    selected_level_idx = -1;
    if (selected_spell_idx < 0 || selected_spell_idx >= spell_name_count) return;
    const char *sn = spell_names[selected_spell_idx];
    for (int i = 0; i < spells_count; i++) {
        cJSON *spell = cJSON_GetArrayItem(spells_json, i);
        if (strcmp(cJSON_GetObjectItem(spell, "name")->valuestring, sn) == 0) {
            int lv = cJSON_GetObjectItem(spell, "level")->valueint;
            if (current_level_count < 20) current_levels[current_level_count++] = lv;
        }
    }
    for (int i = 0; i < current_level_count-1; i++)
        for (int j = i+1; j < current_level_count; j++)
            if (current_levels[i] > current_levels[j]) {
                int tmp = current_levels[i]; current_levels[i] = current_levels[j]; current_levels[j] = tmp;
            }
    if (current_level_count > 0) {
        int found = 0;
        for (int i = 0; i < current_level_count; i++)
            if (current_levels[i] == spell_level) { selected_level_idx = i; found = 1; break; }
        if (!found) { spell_level = current_levels[0]; selected_level_idx = 0; }
    }
}

static void commit_item_changes(void) {
    if (selected_item < 0) return;
    cJSON *it = cJSON_GetArrayItem(items, selected_item);
    cJSON *nm = cJSON_GetObjectItem(it, "name");
    if (nm && cJSON_IsString(nm)) cJSON_SetValuestring(nm, name_buf);
    cJSON *pr = cJSON_GetObjectItem(it, "price");
    if (pr && cJSON_IsNumber(pr)) { pr->valueint = price_val; pr->valuedouble = price_val; }
    cJSON *rng = cJSON_GetObjectItem(it, "range");
    if (!rng) {
        rng = cJSON_CreateArray(); cJSON_AddItemToArray(rng, cJSON_CreateNumber(0)); cJSON_AddItemToArray(rng, cJSON_CreateNumber(0));
        cJSON_ReplaceItemInObject(it, "range", rng);
    }
    if (cJSON_GetArraySize(rng) >= 2) {
        cJSON_GetArrayItem(rng,0)->valueint = range_min; cJSON_GetArrayItem(rng,0)->valuedouble = range_min;
        cJSON_GetArrayItem(rng,1)->valueint = range_max; cJSON_GetArrayItem(rng,1)->valuedouble = range_max;
    }
    cJSON *us = cJSON_GetObjectItem(it, "use_spell");
    if (us && cJSON_IsString(us)) cJSON_SetValuestring(us, use_spell);
    cJSON *sl = cJSON_GetObjectItem(it, "spell_level");
    if (sl && cJSON_IsNumber(sl)) { sl->valueint = spell_level; sl->valuedouble = spell_level; }
    cJSON *ic = cJSON_GetObjectItem(it, "icon");
    if (ic && cJSON_IsString(ic)) cJSON_SetValuestring(ic, icon_path);

    // сохраняем категорию
    cJSON *ct = cJSON_GetObjectItem(it, "category");
    if (ct && cJSON_IsString(ct)) cJSON_SetValuestring(ct, category_buf);

    // сохраняем атаку и защиту
    cJSON *atk = cJSON_GetObjectItem(it, "attack");
    if (atk && cJSON_IsNumber(atk)) { atk->valueint = attack_val; atk->valuedouble = attack_val; }
    else { cJSON_AddNumberToObject(it, "attack", attack_val); }
    cJSON *def = cJSON_GetObjectItem(it, "defense");
    if (def && cJSON_IsNumber(def)) { def->valueint = defense_val; def->valuedouble = defense_val; }
    else { cJSON_AddNumberToObject(it, "defense", defense_val); }
}

static void load_item_fields(void) {
    if (selected_item < 0) return;
    cJSON *it = cJSON_GetArrayItem(items, selected_item);
    const char *s = cJSON_GetObjectItem(it, "name")->valuestring;
    strncpy(name_buf, s ? s : "", 20); name_buf[20] = '\0';
    price_val = cJSON_GetObjectItem(it, "price")->valueint;
    cJSON *rng = cJSON_GetObjectItem(it, "range");
    if (rng && cJSON_IsArray(rng) && cJSON_GetArraySize(rng) >= 2) {
        range_min = cJSON_GetArrayItem(rng,0)->valueint;
        range_max = cJSON_GetArrayItem(rng,1)->valueint;
    } else { range_min = range_max = 0; }
    const char *us = cJSON_GetObjectItem(it, "use_spell")->valuestring;
    strncpy(use_spell, us ? us : "", 63); use_spell[63] = '\0';
    spell_level = cJSON_GetObjectItem(it, "spell_level")->valueint;
    const char *ic = cJSON_GetObjectItem(it, "icon")->valuestring;
    strncpy(icon_path, ic ? ic : "", 255); icon_path[255] = '\0';

    // читаем категорию, атаку, защиту
    const char *cat = cJSON_GetObjectItem(it, "category")->valuestring;
    strncpy(category_buf, cat ? cat : "consumable", 31);
    category_buf[31] = '\0';
    cJSON *atk = cJSON_GetObjectItem(it, "attack");
    attack_val = (atk && cJSON_IsNumber(atk)) ? atk->valueint : 0;
    cJSON *def = cJSON_GetObjectItem(it, "defense");
    defense_val = (def && cJSON_IsNumber(def)) ? def->valueint : 0;

    selected_spell_idx = -1;
    for (int i = 0; i < spell_name_count; i++)
        if (strcmp(spell_names[i], use_spell) == 0) { selected_spell_idx = i; break; }
    update_levels_for_spell();
    open_item_fields();
}

// оригинальная open_item_fields, но с добавлением полей 7 (attack) и 8 (defense)
static void open_item_fields(void) {
    item_field_count = 0;
    item_active_field = -1;
    int base_x = 360, base_y = 80, off = 100;

    // 0: Name
    snprintf(item_fields[0].text, sizeof(item_fields[0].text), "%s", name_buf);
    item_fields[0].cursor = strlen(item_fields[0].text);
    item_fields[0].active = 0;
    item_fields[0].json_obj = cJSON_GetArrayItem(items, selected_item);
    item_fields[0].json_key = "name";
    item_fields[0].is_numeric = 0; item_fields[0].max_len = 20; item_fields[0].array_index = -1; item_fields[0].is_special = 0;
    item_fields[0].rect = (SDL_Rect){base_x+off, base_y + 0*35, 190, 22};

    // 1: Price
    snprintf(item_fields[1].text, sizeof(item_fields[1].text), "%d", price_val);
    item_fields[1].cursor = strlen(item_fields[1].text);
    item_fields[1].active = 0;
    item_fields[1].json_obj = NULL; item_fields[1].json_key = NULL;
    item_fields[1].is_numeric = 1; item_fields[1].max_len = 5; item_fields[1].array_index = -1; item_fields[1].is_special = 0;
    item_fields[1].rect = (SDL_Rect){base_x+off, base_y + 2*35, 150, 22};

    // 2: Range Min
    snprintf(item_fields[2].text, sizeof(item_fields[2].text), "%d", range_min);
    item_fields[2].cursor = strlen(item_fields[2].text);
    item_fields[2].active = 0;
    item_fields[2].json_obj = NULL; item_fields[2].json_key = NULL;
    item_fields[2].is_numeric = 1; item_fields[2].max_len = 2; item_fields[2].array_index = 0; item_fields[2].is_special = 0;
    item_fields[2].rect = (SDL_Rect){base_x+off, base_y + 3*35, 150, 22};

    // 3: Range Max
    snprintf(item_fields[3].text, sizeof(item_fields[3].text), "%d", range_max);
    item_fields[3].cursor = strlen(item_fields[3].text);
    item_fields[3].active = 0;
    item_fields[3].json_obj = NULL; item_fields[3].json_key = NULL;
    item_fields[3].is_numeric = 1; item_fields[3].max_len = 2; item_fields[3].array_index = 1; item_fields[3].is_special = 0;
    item_fields[3].rect = (SDL_Rect){base_x+off, base_y + 4*35, 150, 22};

    // 4: Use Spell (special)
    snprintf(item_fields[4].text, sizeof(item_fields[4].text), "%s", use_spell);
    item_fields[4].cursor = 0;
    item_fields[4].active = 0;
    item_fields[4].json_obj = NULL; item_fields[4].json_key = "use_spell";
    item_fields[4].is_numeric = 0; item_fields[4].max_len = 0; item_fields[4].array_index = -1; item_fields[4].is_special = 1;
    item_fields[4].rect = (SDL_Rect){base_x+off, base_y + 5*35, 150, 22};

    // 5: Spell Level (special)
    snprintf(item_fields[5].text, sizeof(item_fields[5].text), "%d", spell_level);
    item_fields[5].cursor = 0;
    item_fields[5].active = 0;
    item_fields[5].json_obj = NULL; item_fields[5].json_key = "spell_level";
    item_fields[5].is_numeric = 1; item_fields[5].max_len = 2; item_fields[5].array_index = -1; item_fields[5].is_special = 2;
    item_fields[5].rect = (SDL_Rect){base_x+off, base_y + 6*35, 150, 22};

    // 6: Icon
    snprintf(item_fields[6].text, sizeof(item_fields[6].text), "%s", icon_path);
    item_fields[6].cursor = strlen(item_fields[6].text);
    item_fields[6].active = 0;
    item_fields[6].json_obj = NULL; item_fields[6].json_key = "icon";
    item_fields[6].is_numeric = 0; item_fields[6].max_len = 0; item_fields[6].array_index = -1; item_fields[6].is_special = 0;
    item_fields[6].rect = (SDL_Rect){base_x+off, base_y + 7*35, 150, 22};

    // новые поля: 7 - Attack, 8 - Defense (позиция справа от категории)
    snprintf(item_fields[7].text, sizeof(item_fields[7].text), "%d", attack_val);
    item_fields[7].cursor = strlen(item_fields[7].text);
    item_fields[7].active = 0;
    item_fields[7].json_obj = NULL; item_fields[7].json_key = "attack";
    item_fields[7].is_numeric = 1; item_fields[7].max_len = 4; item_fields[7].array_index = -1; item_fields[7].is_special = 0;
    item_fields[7].rect = (SDL_Rect){720, 95, 50, 22};   // y=95 соответствует строке категории (py+10+1*35=95)

    snprintf(item_fields[8].text, sizeof(item_fields[8].text), "%d", defense_val);
    item_fields[8].cursor = strlen(item_fields[8].text);
    item_fields[8].active = 0;
    item_fields[8].json_obj = NULL; item_fields[8].json_key = "defense";
    item_fields[8].is_numeric = 1; item_fields[8].max_len = 4; item_fields[8].array_index = -1; item_fields[8].is_special = 0;
    item_fields[8].rect = (SDL_Rect){820, 95, 50, 22};

    item_field_count = 9;   // теперь 9 полей (индексы 0..8)
}

static void commit_item_field(int idx) {
    if (idx < 0 || idx >= item_field_count) return;
    ItemField *f = &item_fields[idx];
    if (f->is_special) {
        if (f->is_special == 1) {
            strncpy(use_spell, f->text, 63); use_spell[63] = '\0';
            selected_spell_idx = -1;
            for (int i = 0; i < spell_name_count; i++)
                if (strcmp(spell_names[i], use_spell) == 0) { selected_spell_idx = i; break; }
            update_levels_for_spell();
        } else if (f->is_special == 2) {
            spell_level = atoi(f->text);
            selected_level_idx = -1;
            for (int i = 0; i < current_level_count; i++)
                if (current_levels[i] == spell_level) { selected_level_idx = i; break; }
        }
    } else {
        if (f->json_obj && f->json_key) {
            if (f->is_numeric) {
                int v = atoi(f->text);
                cJSON *num = cJSON_GetObjectItem(f->json_obj, f->json_key);
                if (num && cJSON_IsNumber(num)) { num->valueint = v; num->valuedouble = v; }
            } else {
                cJSON *str = cJSON_GetObjectItem(f->json_obj, f->json_key);
                if (str && cJSON_IsString(str)) cJSON_SetValuestring(str, f->text);
            }
        } else if (f->array_index >= 0) {
            cJSON *it = cJSON_GetArrayItem(items, selected_item);
            cJSON *rng = cJSON_GetObjectItem(it, "range");
            if (!rng) { rng = cJSON_CreateArray(); cJSON_AddItemToArray(rng, cJSON_CreateNumber(0)); cJSON_AddItemToArray(rng, cJSON_CreateNumber(0)); cJSON_ReplaceItemInObject(it, "range", rng); }
            if (cJSON_GetArraySize(rng) > f->array_index) {
                cJSON *elem = cJSON_GetArrayItem(rng, f->array_index);
                if (elem && cJSON_IsNumber(elem)) { int v = atoi(f->text); elem->valueint = v; elem->valuedouble = v; }
            }
        }
    }
    if (idx == 0) { strncpy(name_buf, f->text, 20); name_buf[20] = '\0'; }
    else if (idx == 1) price_val = atoi(f->text);
    else if (idx == 2) range_min = atoi(f->text);
    else if (idx == 3) range_max = atoi(f->text);
    else if (idx == 6) { strncpy(icon_path, f->text, 255); icon_path[255] = '\0'; }
    else if (idx == 7) {
        attack_val = atoi(f->text);
        // немедленно сохраняем в JSON
        if (selected_item >= 0) {
            cJSON *it = cJSON_GetArrayItem(items, selected_item);
            cJSON *atk = cJSON_GetObjectItem(it, "attack");
            if (atk && cJSON_IsNumber(atk)) {
                atk->valueint = attack_val;
                atk->valuedouble = attack_val;
            } else {
                cJSON_AddNumberToObject(it, "attack", attack_val);
            }
        }
    }
    else if (idx == 8) {
        defense_val = atoi(f->text);
        // немедленно сохраняем в JSON
        if (selected_item >= 0) {
            cJSON *it = cJSON_GetArrayItem(items, selected_item);
            cJSON *def = cJSON_GetObjectItem(it, "defense");
            if (def && cJSON_IsNumber(def)) {
                def->valueint = defense_val;
                def->valuedouble = defense_val;
            } else {
                cJSON_AddNumberToObject(it, "defense", defense_val);
            }
        }
    }
    f->active = 0;
}

static int handle_item_input(SDL_Event *evt, int idx) {
    ItemField *f = &item_fields[idx];
    SDL_Rect r = f->rect;
    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) {
            if (item_active_field >= 0 && item_active_field != idx) commit_item_field(item_active_field);
            item_active_field = idx;
            f->active = 1;
            f->cursor = strlen(f->text);
            return 1;
        }
        if (f->active) {
            commit_item_field(idx);
            int best = -1, bestDist = 1000;
            for (int i = 0; i < item_field_count; i++) {
                if (i == 6) continue;
                SDL_Rect fr = item_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                item_fields[best].active = 1;
                item_fields[best].cursor = strlen(item_fields[best].text);
                item_active_field = best;
            } else item_active_field = -1;
            return 1;
        } else {
            int best = -1, bestDist = 1000;
            for (int i = 0; i < item_field_count; i++) {
                if (i == 6) continue;
                SDL_Rect fr = item_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                if (item_active_field >= 0) commit_item_field(item_active_field);
                item_fields[best].active = 1;
                item_fields[best].cursor = strlen(item_fields[best].text);
                item_active_field = best;
                return 1;
            }
        }
    } else if (evt->type == SDL_KEYDOWN && f->active && !f->is_special) {
        if (evt->key.keysym.sym == SDLK_UP) {
            if (idx == 0) return 1;
            commit_item_field(idx);
            int new_idx = idx - 1;
            if (new_idx == 1) new_idx = 0;
            item_fields[new_idx].active = 1;
            item_fields[new_idx].cursor = strlen(item_fields[new_idx].text);
            item_active_field = new_idx;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_DOWN) {
            if (idx >= 3) return 1;
            commit_item_field(idx);
            int new_idx = idx + 1;
            item_fields[new_idx].active = 1;
            item_fields[new_idx].cursor = strlen(item_fields[new_idx].text);
            item_active_field = new_idx;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
            if (f->cursor > 0) {
                memmove(f->text + f->cursor - 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->cursor--;
            }
            return 1;
        } else if (evt->key.keysym.sym == SDLK_RETURN || evt->key.keysym.sym == SDLK_KP_ENTER) {
            commit_item_field(idx);
            return 1;
        } else if (evt->key.keysym.sym == SDLK_LEFT && f->cursor > 0) { f->cursor--; return 1; }
        else if (evt->key.keysym.sym == SDLK_RIGHT && f->cursor < strlen(f->text)) { f->cursor++; return 1; }
    } else if (evt->type == SDL_TEXTINPUT && f->active && !f->is_special) {
        char ch = evt->text.text[0];
        if (f->is_numeric) {
            if (isdigit(ch) || (ch == '-' && f->cursor == 0 && (f->text[0] == '\0' || f->text[0] != '-'))) {
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

static void draw_item_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *fallback_text) {
    SDL_Color white = {255,255,255}, black = {0,0,0}, gray = {100,100,100};
    draw_text_ext(r, x, y + 3, label, white);
    int fx = x + 100;
    SDL_Rect rect = {fx, y, w, h};
    SDL_SetRenderDrawColor(r, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, white.r, white.g, white.b, 255); SDL_RenderDrawRect(r, &rect);

    const char *text_to_draw = fallback_text;
    if (idx >= 0 && idx < item_field_count && item_fields[idx].active)
        text_to_draw = item_fields[idx].text;

    draw_text_ext(r, fx + 5, y + 3, text_to_draw, black);

    if (idx >= 0 && idx < item_field_count && item_fields[idx].active) {
        ItemField *f = &item_fields[idx];
        char before[256] = {0};
        if (f->cursor > 0) {
            strncpy(before, f->text, f->cursor);
            before[f->cursor] = '\0';
        }
        SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, before, black);
        int offset = fx + 5 + (s ? s->w : 0);
        if (s) SDL_FreeSurface(s);
        draw_text_ext(r, offset, y + 3, "|", black);
    }
}

static void add_new_item(void) {
    if (!items) return;
    int new_id = 0;
    for (int i = 0; i < item_count; i++) {
        int id = cJSON_GetObjectItem(cJSON_GetArrayItem(items, i), "id")->valueint;
        if (id >= new_id) new_id = id + 1;
    }
    char new_name[64]; int idx = 0;
    while (1) {
        snprintf(new_name, sizeof(new_name), "Item%02d", idx);
        int found = 0;
        for (int i = 0; i < item_count; i++)
            if (strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(items, i), "name")->valuestring, new_name) == 0) found = 1;
        if (!found) break;
        idx++;
    }
    cJSON *it = cJSON_CreateObject();
    cJSON_AddNumberToObject(it, "id", new_id);
    cJSON_AddStringToObject(it, "name", new_name);
    cJSON_AddStringToObject(it, "category", "consumable");
    cJSON_AddNumberToObject(it, "price", 10);
    cJSON *rng = cJSON_CreateArray(); cJSON_AddItemToArray(rng, cJSON_CreateNumber(0)); cJSON_AddItemToArray(rng, cJSON_CreateNumber(1));
    cJSON_AddItemToObject(it, "range", rng);
    cJSON_AddStringToObject(it, "use_spell", "HEAL");
    cJSON_AddNumberToObject(it, "spell_level", 1);
    cJSON_AddStringToObject(it, "icon", "assets/items/empty.png");
    cJSON_AddNumberToObject(it, "attack", 0);
    cJSON_AddNumberToObject(it, "defense", 0);
    cJSON_AddItemToArray(items, it);
    item_count++;
    if (selected_item >= 0) commit_item_changes();
    selected_item = item_count - 1;
    load_item_fields();
}

static void delete_item(void) {
    if (selected_item < 0 || !items) return;
    cJSON *new_arr = cJSON_CreateArray();
    for (int i = 0; i < item_count; i++) {
        if (i == selected_item) continue;
        cJSON_AddItemToArray(new_arr, cJSON_Duplicate(cJSON_GetArrayItem(items, i), 1));
    }
    cJSON_Delete(items);
    items = new_arr;
    item_count--;
    if (item_count == 0) {
        selected_item = -1;
        item_active_field = -1;
        item_field_count = 0;
    } else {
        if (selected_item >= item_count) selected_item = item_count - 1;
        if (selected_item >= 0) load_item_fields();
    }
}

void items_reload(void) {
    if (items) { cJSON_Delete(items); items = NULL; }
    item_count = 0;
    FILE *f = fopen("../data/items/items.json", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(len+1); fread(buf, 1, len, f); buf[len] = '\0'; fclose(f);
    cJSON *root = cJSON_Parse(buf); free(buf);
    if (root) {
        cJSON *arr = cJSON_GetObjectItem(root, "items");
        if (arr && cJSON_IsArray(arr)) {
            if (items_json) {
                items_json = arr;
                items_count = cJSON_GetArraySize(arr);
                items = items_json;
                item_count = items_count;
                selected_item = -1;
                item_scroll = 0;
                save_timer = 0;
                build_spell_name_list();
                item_field_count = 0;
                item_active_field = -1;
                if (item_count > 0) { selected_item = 0; load_item_fields(); }
            }
        } else {
            cJSON_Delete(root);
        }
    }
}

// ---------- публичные функции ----------
void items_init(cJSON *json_array, int count) {
    items = json_array;
    item_count = count;
    selected_item = -1;
    item_scroll = 0;
    save_timer = 0;
    build_spell_name_list();
    item_field_count = 0;
    item_active_field = -1;
    if (item_count > 0) { selected_item = 0; load_item_fields(); }
}

void items_save_to_file(void) {
    if (!items) return;
    cJSON *root = cJSON_CreateObject();
    cJSON *dup = cJSON_Duplicate(items, 1);
    cJSON_AddItemToObject(root, "items", dup);
    char *js = cJSON_PrintBuffered(root, 0, 1);
    FILE *f = fopen("../data/items/items.json", "w");
    if (f) { fputs(js, f); fclose(f); error_msg[0] = '\0'; }
    else snprintf(error_msg, sizeof(error_msg), "Failed to save items.json");
    free(js);
    cJSON_Delete(root);
    save_timer = SAVE_BLINK_DURATION;
}

int items_is_edit_active(void) { return item_active_field >= 0; }

void items_update_timer(void) {
    if (save_timer > 0) save_timer--;
}

void items_set_window_height(int h) {
    g_window_height = h;
}

void items_reset_selection(void) {
    if (item_count > 0) { selected_item = 0; load_item_fields(); }
    else { selected_item = -1; item_active_field = -1; item_field_count = 0; }
}

void items_adjust_scroll(int delta) {
    item_scroll -= delta;
    if (item_scroll < 0) item_scroll = 0;
    int total_h = items_get_total_height();
    int visible_h = g_window_height - 35;
    int max_scroll = total_h > visible_h ? total_h - visible_h : 0;
    if (item_scroll > max_scroll) item_scroll = max_scroll;
}

int items_get_scroll(void) { return item_scroll; }

void items_draw_list(SDL_Renderer *renderer, int y_offset, int scroll) {
    if (!items) return;
    int y = y_offset - scroll;
    SDL_Color white = {255,255,255};
    for (int i = 0; i < item_count; i++) {
        if (y + 20 < y_offset || y > y_offset + 600) { y += 20; continue; }
        cJSON *it = cJSON_GetArrayItem(items, i);
        char buf[256];
        snprintf(buf, sizeof(buf), "%d: %s", cJSON_GetObjectItem(it, "id")->valueint, cJSON_GetObjectItem(it, "name")->valuestring);
        SDL_Rect rr = {10, y, 290, 20};
        if (i == selected_item) { SDL_SetRenderDrawColor(renderer, 100,100,255,128); SDL_RenderFillRect(renderer, &rr); }
        draw_text_ext(renderer, 10, y, buf, white);
        y += 20;
    }
}

int items_get_total_height(void) { return item_count * 20; }

void items_handle_click(int mx, int my, int y_offset, int scroll) {
    int y = y_offset - scroll;
    for (int i = 0; i < item_count; i++) {
        if (mx >= 10 && mx < 300 && my >= y && my < y+20) {
            if (selected_item != i) {
                if (selected_item >= 0) commit_item_changes();
                selected_item = i;
                load_item_fields();
            }
            return;
        }
        y += 20;
    }
}

void items_draw_edit_panel(SDL_Renderer *renderer, int px, int py) {
    if (selected_item < 0) return;
    SDL_Color black = {0,0,0,255};
    SDL_Color white = {255,255,255}, gray = {100,100,100}, blue = {70,70,120};
    SDL_Rect panel = {px, py, 580, 500};
    SDL_SetRenderDrawColor(renderer, 60,60,60,255); SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &panel);

    int y = py + 10;

    // Name
    draw_item_field(renderer, px+10, y, 190, 22, 0, "Name:", name_buf);
    y += 35;

    // ---- Интерактивная категория ----
    draw_text_ext(renderer, px+10, y+3, "Category:", white);
    SDL_Rect cat_rect = {px+110, y, 150, 22};
    SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(renderer, &cat_rect);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &cat_rect);
    draw_text_ext(renderer, px+115, y+3, category_buf, black);

    SDL_Rect cat_prev = {px+270, y, 20, 22};
    SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &cat_prev);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &cat_prev);
    draw_text_ext(renderer, px+275, y+3, "<", white);

    SDL_Rect cat_next = {px+295, y, 20, 22};
    SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &cat_next);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &cat_next);
    draw_text_ext(renderer, px+300, y+3, ">", white);

    // Attack / Defense (только для Weapon и Ring)
    if (strcmp(category_buf, "consumable") != 0) {
        // AT:
        draw_text_ext(renderer, px+330, y+3, "AT:", white);
        SDL_Rect atk_rect = item_fields[7].rect;
        SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, 255);
        SDL_RenderFillRect(renderer, &atk_rect);
        SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
        SDL_RenderDrawRect(renderer, &atk_rect);
        draw_text_ext(renderer, atk_rect.x+5, atk_rect.y+3, item_fields[7].text, black);
        if (item_active_field == 7 && item_fields[7].active) {
            char before[256] = {0};
            if (item_fields[7].cursor > 0) {
                strncpy(before, item_fields[7].text, item_fields[7].cursor);
                before[item_fields[7].cursor] = '\0';
            }
            SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, before, black);
            int offset = atk_rect.x+5 + (s ? s->w : 0);
            if (s) SDL_FreeSurface(s);
            draw_text_ext(renderer, offset, atk_rect.y+3, "|", black);
        }

        // DF:
        draw_text_ext(renderer, atk_rect.x+atk_rect.w+10, y+3, "DF:", white);
        SDL_Rect def_rect = item_fields[8].rect;
        SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, 255);
        SDL_RenderFillRect(renderer, &def_rect);
        SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
        SDL_RenderDrawRect(renderer, &def_rect);
        draw_text_ext(renderer, def_rect.x+5, def_rect.y+3, item_fields[8].text, black);
        if (item_active_field == 8 && item_fields[8].active) {
            char before[256] = {0};
            if (item_fields[8].cursor > 0) {
                strncpy(before, item_fields[8].text, item_fields[8].cursor);
                before[item_fields[8].cursor] = '\0';
            }
            SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, before, black);
            int offset = def_rect.x+5 + (s ? s->w : 0);
            if (s) SDL_FreeSurface(s);
            draw_text_ext(renderer, offset, def_rect.y+3, "|", black);
        }
    }

    y += 35;

    // Price
    char price_str[16]; snprintf(price_str, sizeof(price_str), "%d", price_val);
    draw_item_field(renderer, px+10, y, 150, 22, 1, "Price:", price_str);
    y += 35;

    // Range Min
    char rmin_str[8]; snprintf(rmin_str, sizeof(rmin_str), "%d", range_min);
    draw_item_field(renderer, px+10, y, 150, 22, 2, "Range Min:", rmin_str);
    y += 35;

    // Range Max
    char rmax_str[8]; snprintf(rmax_str, sizeof(rmax_str), "%d", range_max);
    draw_item_field(renderer, px+10, y, 150, 22, 3, "Range Max:", rmax_str);
    y += 35;

    // Use Spell
    draw_text_ext(renderer, px+10, y+3, "Use Spell:", white);
    SDL_Rect use_rect = {px+110, y, 150, 22};
    SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(renderer, &use_rect);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &use_rect);
    draw_text_ext(renderer, px+115, y+3, use_spell, black);
    SDL_Rect prev_s = {px+270, y, 20, 22}; SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &prev_s);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &prev_s);
    draw_text_ext(renderer, px+275, y+3, "<", white);
    SDL_Rect next_s = {px+295, y, 20, 22}; SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &next_s);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &next_s);
    draw_text_ext(renderer, px+300, y+3, ">", white);
    y += 35;

    // Spell Level
    char lvl_str[8]; snprintf(lvl_str, sizeof(lvl_str), "%d", spell_level);
    draw_text_ext(renderer, px+10, y+3, "Spell Level:", white);
    SDL_Rect lvl_rect = {px+110, y, 150, 22};
    SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(renderer, &lvl_rect);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &lvl_rect);
    draw_text_ext(renderer, px+115, y+3, lvl_str, black);
    SDL_Rect prev_l = {px+270, y, 20, 22}; SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &prev_l);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &prev_l);
    draw_text_ext(renderer, px+275, y+3, "<", white);
    SDL_Rect next_l = {px+295, y, 20, 22}; SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &next_l);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &next_l);
    draw_text_ext(renderer, px+300, y+3, ">", white);
    y += 35;

    // Icon
    const char *short_name = strrchr(icon_path, '/') ? strrchr(icon_path, '/')+1 : icon_path;
    draw_item_field(renderer, px+10, y, 150, 22, 6, "Icon:", short_name);
    SDL_Rect browse = {px+10+100+150+5, y, 70, 22};
    SDL_SetRenderDrawColor(renderer, 100,100,200,255); SDL_RenderFillRect(renderer, &browse);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &browse);
    draw_text_ext(renderer, browse.x+5, browse.y+3, "Browse", white);
    y += 40;

    // Buttons
    int btn_y = y + 5;
    SDL_Rect del_btn = {px+10, btn_y, 80, 30};
    SDL_SetRenderDrawColor(renderer, 200,80,80,255); SDL_RenderFillRect(renderer, &del_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &del_btn);
    draw_text_ext(renderer, del_btn.x+8, del_btn.y+5, "Del Item", white);

    SDL_Rect save_btn = {px+95, btn_y, 70, 30};
    SDL_Color save_col = (save_timer > 0) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,0,255};
    SDL_SetRenderDrawColor(renderer, save_col.r, save_col.g, save_col.b, 255); SDL_RenderFillRect(renderer, &save_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &save_btn);
    draw_text_ext(renderer, save_btn.x+12, save_btn.y+5, "SAVE", black);

    SDL_Rect refresh_btn = {px+170, btn_y, 80, 30};
    SDL_SetRenderDrawColor(renderer, 180,180,255,255); SDL_RenderFillRect(renderer, &refresh_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &refresh_btn);
    draw_text_ext(renderer, refresh_btn.x+12, refresh_btn.y+5, "Refresh", black);

    SDL_Rect add_btn = {px+255, btn_y, 90, 30};
    SDL_SetRenderDrawColor(renderer, 100,200,100,255); SDL_RenderFillRect(renderer, &add_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &add_btn);
    draw_text_ext(renderer, add_btn.x+5, add_btn.y+5, "Add Item", white);
}

void items_handle_input(SDL_Event *evt) {
    for (int i = 0; i < item_field_count; i++) {
        if (handle_item_input(evt, i)) break;
    }
    if (item_active_field >= 0 && (evt->type == SDL_KEYDOWN || evt->type == SDL_TEXTINPUT))
        return;

    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (selected_item < 0) return;
        int px = 360, py = 50;

        // Browse
        SDL_Rect browse = {px+10+100+150+5, py+10 + 7*35, 70, 22};
        if (mx >= browse.x && mx < browse.x+browse.w && my >= browse.y && my < browse.y+browse.h) {
            char *path = open_file_dialog("..\\assets\\items\\");
            if (path) {
                const char *filename = strrchr(path, '\\');
                if (!filename) filename = path; else filename++;
                snprintf(icon_path, sizeof(icon_path), "assets/items/%s", filename);
                // обновить текст в поле ввода, чтобы интерфейс сразу отобразил новое имя
                if (item_field_count > 6) {
                    strncpy(item_fields[6].text, icon_path, sizeof(item_fields[6].text)-1);
                    item_fields[6].text[sizeof(item_fields[6].text)-1] = '\0';
                }
                commit_item_changes();
                free(path);
            }
            return;
        }

        // Buttons
        int btn_y = py + 10 + 7*35 + 45;
        SDL_Rect del_btn = {px+10, btn_y, 80, 30};
        SDL_Rect save_btn = {px+95, btn_y, 70, 30};
        SDL_Rect refresh_btn = {px+170, btn_y, 80, 30};
        SDL_Rect add_btn = {px+255, btn_y, 90, 30};

        if (mx >= del_btn.x && mx < del_btn.x+del_btn.w && my >= del_btn.y && my < del_btn.y+del_btn.h) {
            delete_item();
            return;
        }
        if (mx >= save_btn.x && mx < save_btn.x+save_btn.w && my >= save_btn.y && my < save_btn.y+save_btn.h) {
            items_save_to_file();
            return;
        }
        if (mx >= refresh_btn.x && mx < refresh_btn.x+refresh_btn.w && my >= refresh_btn.y && my < refresh_btn.y+refresh_btn.h) {
            items_reload();
            return;
        }
        if (mx >= add_btn.x && mx < add_btn.x+add_btn.w && my >= add_btn.y && my < add_btn.y+add_btn.h) {
            add_new_item();
            return;
        }

        // Category arrows
        {
            int cat_y = py + 10 + 1*35;
            SDL_Rect cat_prev = {px+270, cat_y, 20, 22};
            SDL_Rect cat_next = {px+295, cat_y, 20, 22};
            if (mx >= cat_prev.x && mx < cat_prev.x+cat_prev.w && my >= cat_prev.y && my < cat_prev.y+cat_prev.h) {
                int idx = -1;
                for (int i = 0; i < category_count; i++)
                    if (strcmp(category_buf, category_list[i]) == 0) { idx = i; break; }
                if (idx > 0) {
                    strncpy(category_buf, category_list[idx-1], 31);
                    commit_item_changes();
                }
                return;
            }
            if (mx >= cat_next.x && mx < cat_next.x+cat_next.w && my >= cat_next.y && my < cat_next.y+cat_next.h) {
                int idx = -1;
                for (int i = 0; i < category_count; i++)
                    if (strcmp(category_buf, category_list[i]) == 0) { idx = i; break; }
                if (idx < category_count-1) {
                    strncpy(category_buf, category_list[idx+1], 31);
                    commit_item_changes();
                }
                return;
            }
        }

        // Use Spell arrows
        {
            int us_y = py + 10 + 5*35;
            SDL_Rect prev_s = {px+270, us_y, 20, 22};
            SDL_Rect next_s = {px+295, us_y, 20, 22};
            if (mx >= prev_s.x && mx < prev_s.x+prev_s.w && my >= prev_s.y && my < prev_s.y+prev_s.h) {
                if (spell_name_count > 0 && selected_spell_idx > 0) {
                    selected_spell_idx--;
                    strncpy(use_spell, spell_names[selected_spell_idx], 63);
                    update_levels_for_spell();
                    commit_item_changes();
                }
                return;
            }
            if (mx >= next_s.x && mx < next_s.x+next_s.w && my >= next_s.y && my < next_s.y+next_s.h) {
                if (spell_name_count > 0 && selected_spell_idx < spell_name_count-1) {
                    selected_spell_idx++;
                    strncpy(use_spell, spell_names[selected_spell_idx], 63);
                    update_levels_for_spell();
                    commit_item_changes();
                }
                return;
            }
        }

        // Spell Level arrows
        {
            int lv_y = py + 10 + 6*35;
            SDL_Rect prev_l = {px+270, lv_y, 20, 22};
            SDL_Rect next_l = {px+295, lv_y, 20, 22};
            if (mx >= prev_l.x && mx < prev_l.x+prev_l.w && my >= prev_l.y && my < prev_l.y+prev_l.h) {
                if (current_level_count > 0 && selected_level_idx > 0) {
                    selected_level_idx--;
                    spell_level = current_levels[selected_level_idx];
                    commit_item_changes();
                }
                return;
            }
            if (mx >= next_l.x && mx < next_l.x+next_l.w && my >= next_l.y && my < next_l.y+next_l.h) {
                if (current_level_count > 0 && selected_level_idx < current_level_count-1) {
                    selected_level_idx++;
                    spell_level = current_levels[selected_level_idx];
                    commit_item_changes();
                }
                return;
            }
        }
    }
}