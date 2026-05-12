// classes_editor.c
#include "classes_editor.h"
#include <stdio.h>
#include <string.h>
#include <SDL_ttf.h>
#include <ctype.h>
#include <windows.h>
#include <commdlg.h>

// Кроссплатформенное регистронезависимое сравнение строк
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

extern cJSON *spells_json;
extern int spells_count;
extern cJSON *classes_json;
extern int classes_count;
extern TTF_Font *g_font;
extern int g_font_ok;
extern char error_msg[256];
int draw_text_ext(SDL_Renderer *r, int x, int y, const char *text, SDL_Color color);
char* open_file_dialog();

// ========== ОПРЕДЕЛЕНИЯ КОНСТАНТ (все собраны в одном месте) ==========
#define MAX_SPELLS_PER_CLASS 12
#define MAX_VISIBLE_SPELLS 4
#define MAX_CLASS_FIELDS 30
#define BLINK_SPEED 30
#define SAVE_BLINK_DURATION 42
// ======================================================================

static cJSON *classes = NULL;
static int class_count = 0;
static int selected_class = -1;
static int last_spell_btns_y = 0;
static int up_arrow_rect_y = 0;
static int down_arrow_rect_y = 0;

static int spell_levels[MAX_SPELLS_PER_CLASS];
static int spell_levels_count = 0;
static int selected_spell_level_idx = -1;

static char name_buf[11] = {0};
static char full_name_buf[17] = {0};
static int move_val = 5;
static char move_type_buf[32] = {"REGULAR"};

typedef struct {
    int start;
    int projected;
    char curve[64];
} GrowthBlock;
static GrowthBlock growth[5];
static const char *growth_names[] = {"HP", "MP", "Attack", "Defense", "Agility"};

typedef struct {
    int level;
    char spell_name[64];
    int spell_level;
} ClassSpellEntry;
static ClassSpellEntry class_spells[MAX_SPELLS_PER_CLASS];
static int class_spell_count = 0;
static int selected_spell_entry = -1;

static char curve_names[10][64];
static int curve_count = 0;
static int selected_curve_idx[5];

static char spell_names[100][64];
static int spell_name_count = 0;

static int save_timer = 0;
static int class_scroll = 0;
static int spell_list_scroll = 0;

typedef struct {
    char text[256];
    int cursor;
    int active;
    int is_numeric;
    int max_len;
    SDL_Rect rect;
    int field_id;
    int is_curve;
} ClassField;
static ClassField class_fields[MAX_CLASS_FIELDS];
static int class_field_count = 0;
static int class_active_field = -1;

static int last_btn_y = 0;
static int g_window_height = 680; // значение по умолчанию

// Прототипы функций
static void build_curve_list(void);
static void build_spell_name_list(void);
static void commit_class_changes(void);
static void load_class_fields(void);
static void open_class_fields(void);
static void commit_class_field(int idx);
static int  handle_class_input(SDL_Event *evt, int idx);
static void draw_class_field(SDL_Renderer *r, int x, int y, int w, int h, int idx,
                             const char *label, const char *display, int field_extra_x);
static void add_new_class(void);
static void delete_class(void);
static void update_spell_levels(void);

// static void build_curve_list
static void build_curve_list(void) {
    curve_count = 0;
    extern cJSON *curves_json;
    if (curves_json) {
        for (int i = 0; i < cJSON_GetArraySize(curves_json); i++) {
            cJSON *c = cJSON_GetArrayItem(curves_json, i);
            const char *id = cJSON_GetObjectItem(c, "id")->valuestring;
            if (curve_count < 10) {
                strncpy(curve_names[curve_count], id, 63);
                curve_names[curve_count][63] = '\0';
                curve_count++;
            }
        }
    }
    if (curve_count == 0) {
        static const char *hard[] = {"LINEAR","LATE","EARLY","MIDDLE","EARLYANDLATE","VERY_LATE","MIDDLE_PEAK"};
        for (int i = 0; i < 7; i++) {
            strncpy(curve_names[i], hard[i], 63);
            curve_names[i][63] = '\0';
        }
        curve_count = 7;
    }
    for (int i = 0; i < 5; i++) selected_curve_idx[i] = 0;
}

static void build_spell_name_list(void) {
    spell_name_count = 0;
    for (int i = 0; i < spells_count; i++) {
        cJSON *sp = cJSON_GetArrayItem(spells_json, i);
        const char *nm = cJSON_GetObjectItem(sp, "name")->valuestring;
        int exists = 0;
        for (int j = 0; j < spell_name_count; j++)
            if (strcmp(spell_names[j], nm) == 0) { exists = 1; break; }
        if (!exists && spell_name_count < 100) {
            strncpy(spell_names[spell_name_count], nm, 63);
            spell_names[spell_name_count][63] = '\0';
            spell_name_count++;
        }
    }
}

static void commit_class_changes(void) {
    if (selected_class < 0) return;
    cJSON *cls = cJSON_GetArrayItem(classes, selected_class);
    cJSON *nm = cJSON_GetObjectItem(cls, "name");
    if (nm) cJSON_SetValuestring(nm, name_buf);
    cJSON *fn = cJSON_GetObjectItem(cls, "full_name");
    if (fn) cJSON_SetValuestring(fn, full_name_buf);
    cJSON *mv = cJSON_GetObjectItem(cls, "move");
    if (mv && cJSON_IsNumber(mv)) { mv->valueint = move_val; mv->valuedouble = move_val; }
    cJSON *mt = cJSON_GetObjectItem(cls, "move_type");
    if (mt) cJSON_SetValuestring(mt, move_type_buf);

    const char *gkeys[] = {"hp_growth","mp_growth","attack_growth","defense_growth","agility_growth"};
    for (int i = 0; i < 5; i++) {
        cJSON *gr = cJSON_GetObjectItem(cls, gkeys[i]);
        if (!gr) { gr = cJSON_CreateObject(); cJSON_ReplaceItemInObject(cls, gkeys[i], gr); }
        cJSON *st = cJSON_GetObjectItem(gr, "start");
        if (st && cJSON_IsNumber(st)) { st->valueint = growth[i].start; st->valuedouble = growth[i].start; }
        cJSON *pr = cJSON_GetObjectItem(gr, "projected");
        if (pr && cJSON_IsNumber(pr)) { pr->valueint = growth[i].projected; pr->valuedouble = growth[i].projected; }
        cJSON *cu = cJSON_GetObjectItem(gr, "curve");
        if (cu && cJSON_IsString(cu)) cJSON_SetValuestring(cu, growth[i].curve);
    }

    cJSON *spells = cJSON_CreateArray();
    for (int i = 0; i < class_spell_count; i++) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "level", class_spells[i].level);
        cJSON_AddStringToObject(entry, "spell", class_spells[i].spell_name);
        cJSON_AddNumberToObject(entry, "spell_level", class_spells[i].spell_level);
        cJSON_AddItemToArray(spells, entry);
    }
    cJSON_ReplaceItemInObject(cls, "spell_list", spells);
}

static void load_class_fields(void) {
    if (selected_class < 0) return;
    cJSON *cls = cJSON_GetArrayItem(classes, selected_class);
    const char *s = cJSON_GetObjectItem(cls, "name")->valuestring;
    strncpy(name_buf, s ? s : "", 10); name_buf[10] = '\0';
    const char *fn = cJSON_GetObjectItem(cls, "full_name")->valuestring;
    strncpy(full_name_buf, fn ? fn : "", 16); full_name_buf[16] = '\0';
    move_val = cJSON_GetObjectItem(cls, "move")->valueint;
    const char *mt = cJSON_GetObjectItem(cls, "move_type")->valuestring;
    strncpy(move_type_buf, mt ? mt : "", 31); move_type_buf[31] = '\0';

    const char *gkeys[] = {"hp_growth","mp_growth","attack_growth","defense_growth","agility_growth"};
    for (int i = 0; i < 5; i++) {
        cJSON *gr = cJSON_GetObjectItem(cls, gkeys[i]);
        if (gr) {
            growth[i].start = cJSON_GetObjectItem(gr, "start")->valueint;
            growth[i].projected = cJSON_GetObjectItem(gr, "projected")->valueint;
            const char *cv = cJSON_GetObjectItem(gr, "curve")->valuestring;
            strncpy(growth[i].curve, cv ? cv : "LINEAR", 63);
        } else {
            growth[i].start = 10; growth[i].projected = 30;
            strncpy(growth[i].curve, "LINEAR", 63);
        }
        for (int j = 0; j < curve_count; j++)
            if (strcmp(curve_names[j], growth[i].curve) == 0) { selected_curve_idx[i] = j; break; }
    }

    cJSON *spells = cJSON_GetObjectItem(cls, "spell_list");
    class_spell_count = 0;
    if (spells && cJSON_IsArray(spells)) {
        int sz = cJSON_GetArraySize(spells);
        for (int i = 0; i < sz && i < MAX_SPELLS_PER_CLASS; i++) {
            cJSON *ent = cJSON_GetArrayItem(spells, i);
            class_spells[i].level = cJSON_GetObjectItem(ent, "level")->valueint;
            const char *sp = cJSON_GetObjectItem(ent, "spell")->valuestring;
            strncpy(class_spells[i].spell_name, sp ? sp : "HEAL", 63);
            class_spells[i].spell_level = cJSON_GetObjectItem(ent, "spell_level")->valueint;
            class_spell_count++;
        }
    }
    selected_spell_entry = -1;
    open_class_fields();
}

static void open_class_fields(void) {
    class_field_count = 0;
    class_active_field = -1;
    int px = 360, py = 50;
    int base_y = py + 10;

    // Name поле (rect должен совпадать с визуальным полем: x + 60 + 30)
    snprintf(class_fields[0].text, sizeof(class_fields[0].text), "%s", name_buf);
    class_fields[0].cursor = strlen(class_fields[0].text);
    class_fields[0].active = 0; class_fields[0].is_numeric = 0; class_fields[0].max_len = 10;
    class_fields[0].field_id = 0; class_fields[0].is_curve = 0;
    class_fields[0].rect = (SDL_Rect){px + 10 + 60 + 30, base_y + 0*35, 150, 22};   // 460

    // Full Name
    snprintf(class_fields[1].text, sizeof(class_fields[1].text), "%s", full_name_buf);
    class_fields[1].cursor = strlen(class_fields[1].text);
    class_fields[1].active = 0; class_fields[1].is_numeric = 0; class_fields[1].max_len = 16;
    class_fields[1].field_id = 1; class_fields[1].is_curve = 0;
    class_fields[1].rect = (SDL_Rect){px + 10 + 60 + 30, base_y + 1*35, 150, 22};

    // Move (укороченное)
    snprintf(class_fields[2].text, sizeof(class_fields[2].text), "%d", move_val);
    class_fields[2].cursor = strlen(class_fields[2].text);
    class_fields[2].active = 0; class_fields[2].is_numeric = 1; class_fields[2].max_len = 3;
    class_fields[2].field_id = 2; class_fields[2].is_curve = 0;
    class_fields[2].rect = (SDL_Rect){px+10+60, base_y + 2*35, 70, 22};

    int growth_start_y = base_y + 3*35;          // ← исправлено: 3 вместо 4
    int idx = 3;
	for (int s = 0; s < 5; s++) {
    // Start
    snprintf(class_fields[idx].text, sizeof(class_fields[idx].text), "%d", growth[s].start);
    class_fields[idx].cursor = strlen(class_fields[idx].text);
    class_fields[idx].active = 0; class_fields[idx].is_numeric = 1; class_fields[idx].max_len = 5;
    class_fields[idx].field_id = 100 + s*2;
    class_fields[idx].is_curve = 0;
    class_fields[idx].rect = (SDL_Rect){px+80+60, growth_start_y + s*35, 70, 22};  // ← +60
    idx++;
    // Projected
    snprintf(class_fields[idx].text, sizeof(class_fields[idx].text), "%d", growth[s].projected);
    class_fields[idx].cursor = strlen(class_fields[idx].text);
    class_fields[idx].active = 0; class_fields[idx].is_numeric = 1; class_fields[idx].max_len = 5;
    class_fields[idx].field_id = 100 + s*2 + 1;
    class_fields[idx].is_curve = 0;
    class_fields[idx].rect = (SDL_Rect){px+190+60, growth_start_y + s*35, 70, 22}; // ← +60
    idx++;
    // Curve
    class_fields[idx].field_id = 200 + s;
    class_fields[idx].is_curve = 1;
    class_fields[idx].rect = (SDL_Rect){0,0,0,0};
    idx++;
}
    class_field_count = idx;
}

static void commit_class_field(int idx) {
    if (idx < 0 || idx >= class_field_count) return;
    ClassField *f = &class_fields[idx];
    if (f->is_curve) return;
    int id = f->field_id;
    if (id < 100) {
        if (id == 0) { strncpy(name_buf, f->text, 10); name_buf[10] = '\0'; }
        else if (id == 1) { strncpy(full_name_buf, f->text, 16); full_name_buf[16] = '\0'; }
        else if (id == 2) move_val = atoi(f->text);
    } else if (id >= 100 && id < 200) {
        int stat = (id - 100) / 2;
        int is_proj = (id - 100) % 2;
        if (!is_proj) growth[stat].start = atoi(f->text);
        else growth[stat].projected = atoi(f->text);
    }
    f->active = 0;
}

static int handle_class_input(SDL_Event *evt, int idx) {
    ClassField *f = &class_fields[idx];
    SDL_Rect r = f->rect;
    if (f->is_curve) return 0;

    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) {
            if (class_active_field >= 0 && class_active_field != idx) commit_class_field(class_active_field);
            class_active_field = idx;
            f->active = 1;
            f->cursor = strlen(f->text);
            return 1;
        }
        if (f->active) {
            commit_class_field(idx);
            int best = -1, bestDist = 1000;
            for (int i = 0; i < class_field_count; i++) {
                if (class_fields[i].is_curve) continue;
                SDL_Rect fr = class_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                class_fields[best].active = 1;
                class_fields[best].cursor = strlen(class_fields[best].text);
                class_active_field = best;
            } else class_active_field = -1;
            return 1;
        } else {
            int best = -1, bestDist = 1000;
            for (int i = 0; i < class_field_count; i++) {
                if (class_fields[i].is_curve) continue;
                SDL_Rect fr = class_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                if (class_active_field >= 0) commit_class_field(class_active_field);
                class_fields[best].active = 1;
                class_fields[best].cursor = strlen(class_fields[best].text);
                class_active_field = best;
                return 1;
            }
        }
    } else if (evt->type == SDL_KEYDOWN && f->active) {
        if (evt->key.keysym.sym == SDLK_UP) {
            if (idx == 0) return 1;
            commit_class_field(idx);
            int new_idx = idx - 1;
            while (new_idx >= 0 && class_fields[new_idx].is_curve) new_idx--;
            if (new_idx >= 0) {
                class_fields[new_idx].active = 1;
                class_fields[new_idx].cursor = strlen(class_fields[new_idx].text);
                class_active_field = new_idx;
            } else class_active_field = -1;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_DOWN) {
            commit_class_field(idx);
            int new_idx = idx + 1;
            while (new_idx < class_field_count && class_fields[new_idx].is_curve) new_idx++;
            if (new_idx < class_field_count) {
                class_fields[new_idx].active = 1;
                class_fields[new_idx].cursor = strlen(class_fields[new_idx].text);
                class_active_field = new_idx;
            } else class_active_field = -1;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
            if (f->cursor > 0) {
                memmove(f->text + f->cursor - 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->cursor--;
            }
            return 1;
        } else if (evt->key.keysym.sym == SDLK_RETURN || evt->key.keysym.sym == SDLK_KP_ENTER) {
            commit_class_field(idx);
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

static void draw_class_field(SDL_Renderer *r, int x, int y, int w, int h, int idx,
                             const char *label, const char *display, int field_extra_x)
{
    SDL_Color white = {255,255,255}, black = {0,0,0}, gray = {100,100,100};
    draw_text_ext(r, x, y + 3, label, white);
    int fx = x + 60 + field_extra_x;          // поле сдвигается на field_extra_x
    SDL_Rect rect = {fx, y, w, h};
    SDL_SetRenderDrawColor(r, gray.r, gray.g, gray.b, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, white.r, white.g, white.b, 255);
    SDL_RenderDrawRect(r, &rect);

    const char *text_to_draw = display;
    if (idx >= 0 && idx < class_field_count && class_fields[idx].active)
        text_to_draw = class_fields[idx].text;
    draw_text_ext(r, fx + 5, y + 3, text_to_draw, black);

    if (idx >= 0 && idx < class_field_count && class_fields[idx].active) {
        ClassField *f = &class_fields[idx];
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

void classes_draw_edit_panel(SDL_Renderer *renderer, int px, int py) {
    if (selected_class < 0) return;
    SDL_Color black = {0,0,0,255}, white = {255,255,255}, blue = {70,70,120};
    SDL_Color gray = {100,100,100};
    SDL_Rect panel = {px, py, 580, 550};
    SDL_SetRenderDrawColor(renderer, 60,60,60,255); SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &panel);

    int y = py + 10;
    cJSON *cls = cJSON_GetArrayItem(classes, selected_class);
    int cid = cJSON_GetObjectItem(cls, "id")->valueint;
    char idstr[16]; snprintf(idstr, sizeof(idstr), "ID:%d", cid);
    draw_text_ext(renderer, px + 280, y + 3, idstr, white);

    draw_class_field(renderer, px + 10, y, 150, 22, 0, "Name:", name_buf, 30);
    y += 35;
    draw_class_field(renderer, px + 10, y, 150, 22, 1, "Full Name:", full_name_buf, 30);
    y += 35;
    char mvstr[8]; snprintf(mvstr, sizeof(mvstr), "%d", move_val);
    draw_class_field(renderer, px + 10, y, 70, 22, 2, "Move:", mvstr, 0);
    y += 35;

    for (int s = 0; s < 5; s++) {
        char start_str[8], proj_str[8];
        snprintf(start_str, sizeof(start_str), "%d", growth[s].start);
        snprintf(proj_str, sizeof(proj_str), "%d", growth[s].projected);
        int base_idx = 3 + s*3;
        draw_text_ext(renderer, px+10, y+3, growth_names[s], white);
        draw_class_field(renderer, px+80, y, 70, 22, base_idx, "Start", start_str, 0);
        draw_text_ext(renderer, px+225, y+3, "→", white);
        draw_class_field(renderer, px+190, y, 70, 22, base_idx+1, "", proj_str, 0);
        draw_text_ext(renderer, px+350, y+3, growth[s].curve, white);
        SDL_Rect prev = {px+480, y, 20, 22};
        SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &prev);
        SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &prev);
        draw_text_ext(renderer, px+485, y+3, "<", white);
        SDL_Rect next = {px+505, y, 20, 22};
        SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &next);
        SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &next);
        draw_text_ext(renderer, px+510, y+3, ">", white);
        y += 35;
    }

    y += 10;
    draw_text_ext(renderer, px+10, y, "Spell List:", white);
    y += 22;

    const int visible = MAX_VISIBLE_SPELLS;
    const int total = class_spell_count;

    // ----- Стрелка вверх -----
    int up_possible = (spell_list_scroll > 0);
    SDL_Color up_color = up_possible ? blue : gray;
    up_arrow_rect_y = y;
    SDL_Rect up_arrow = {px+20, y, 30, 30};
    SDL_SetRenderDrawColor(renderer, up_color.r, up_color.g, up_color.b, 255);
    SDL_RenderFillRect(renderer, &up_arrow);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
    SDL_RenderDrawRect(renderer, &up_arrow);
    draw_text_ext(renderer, up_arrow.x+10, up_arrow.y+5, "^", white);
    y += 30;

    // ----- Список заклинаний с кнопками имени (слева) и уровня (справа) -----
    for (int i = spell_list_scroll; i < total && i < spell_list_scroll + visible; i++) {
        // Кнопки выбора имени (только для выбранного заклинания)
        if (i == selected_spell_entry) {
            // Кнопка "<<" (предыдущее имя)
            SDL_Rect prev_name_btn = {px+20, y, 20, 20};
            SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255);
            SDL_RenderFillRect(renderer, &prev_name_btn);
            SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
            SDL_RenderDrawRect(renderer, &prev_name_btn);
            draw_text_ext(renderer, prev_name_btn.x+5, prev_name_btn.y+3, "<", white);

            // Кнопка ">>" (следующее имя)
            SDL_Rect next_name_btn = {px+45, y, 20, 20};
            SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255);
            SDL_RenderFillRect(renderer, &next_name_btn);
            SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
            SDL_RenderDrawRect(renderer, &next_name_btn);
            draw_text_ext(renderer, next_name_btn.x+5, next_name_btn.y+3, ">", white);
        }

        // Текст заклинания (сдвинут вправо)
        char buf[128];
        snprintf(buf, sizeof(buf), "%s (Lv %d)", class_spells[i].spell_name, class_spells[i].spell_level);
        SDL_Rect rr = {px+70, y, 360, 20};
        if (i == selected_spell_entry) {
            SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 128);
            SDL_RenderFillRect(renderer, &rr);
        }
        draw_text_ext(renderer, px+70, y, buf, white);

        // Кнопки выбора уровня (только для выбранного заклинания)
        if (i == selected_spell_entry && spell_levels_count > 0) {
            SDL_Rect prev_lvl = {px+440, y, 20, 20};
            SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255);
            SDL_RenderFillRect(renderer, &prev_lvl);
            SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
            SDL_RenderDrawRect(renderer, &prev_lvl);
            draw_text_ext(renderer, prev_lvl.x+5, prev_lvl.y+3, "<", white);

            SDL_Rect next_lvl = {px+465, y, 20, 20};
            SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255);
            SDL_RenderFillRect(renderer, &next_lvl);
            SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
            SDL_RenderDrawRect(renderer, &next_lvl);
            draw_text_ext(renderer, next_lvl.x+5, next_lvl.y+3, ">", white);
        }
        y += 20;
    }

    // ----- Стрелка вниз -----
    int down_possible = (spell_list_scroll + visible < total);
    SDL_Color down_color = down_possible ? blue : gray;
    down_arrow_rect_y = y;
    SDL_Rect down_arrow = {px+20, y, 30, 30};
    SDL_SetRenderDrawColor(renderer, down_color.r, down_color.g, down_color.b, 255);
    SDL_RenderFillRect(renderer, &down_arrow);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
    SDL_RenderDrawRect(renderer, &down_arrow);
    draw_text_ext(renderer, down_arrow.x+10, down_arrow.y+5, "v", white);
    y += 30;

    // Кнопки Add / Delete (без изменений)
    SDL_Rect spell_add_btn = {px+20, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 100,200,100,255);
    SDL_RenderFillRect(renderer, &spell_add_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255);
    SDL_RenderDrawRect(renderer, &spell_add_btn);
    draw_text_ext(renderer, spell_add_btn.x+5, spell_add_btn.y+2, "+", black);

    SDL_Rect spell_del_btn = {px+50, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 200,80,80,255);
    SDL_RenderFillRect(renderer, &spell_del_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255);
    SDL_RenderDrawRect(renderer, &spell_del_btn);
    draw_text_ext(renderer, spell_del_btn.x+5, spell_del_btn.y+2, "-", black);
    last_spell_btns_y = y;

    y += 24;
    int btn_y = y + 10;
    last_btn_y = btn_y;

    SDL_Rect save_btn = {px+130, btn_y, 80, 30};
    SDL_Color save_col = (save_timer > 0) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,0,255};
    SDL_SetRenderDrawColor(renderer, save_col.r, save_col.g, save_col.b, 255); SDL_RenderFillRect(renderer, &save_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &save_btn);
    draw_text_ext(renderer, save_btn.x+15, save_btn.y+5, "SAVE", black);

    SDL_Rect refresh_btn = {px+220, btn_y, 80, 30};
    SDL_SetRenderDrawColor(renderer, 180,180,255,255); SDL_RenderFillRect(renderer, &refresh_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &refresh_btn);
    draw_text_ext(renderer, refresh_btn.x+12, refresh_btn.y+5, "Refresh", black);

    SDL_Rect del_btn = {px+10, btn_y, 100, 30};
    SDL_SetRenderDrawColor(renderer, 200,80,80,255); SDL_RenderFillRect(renderer, &del_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &del_btn);
    draw_text_ext(renderer, del_btn.x+15, del_btn.y+5, "Del Class", white);

    SDL_Rect add_btn = {px+310, btn_y, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100,200,100,255); SDL_RenderFillRect(renderer, &add_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &add_btn);
    draw_text_ext(renderer, add_btn.x+10, add_btn.y+5, "Add Class", white);
}

void classes_handle_input(SDL_Event *evt) {
    for (int i = 0; i < class_field_count; i++) {
        if (handle_class_input(evt, i)) break;
    }
    if (class_active_field >= 0 && (evt->type == SDL_KEYDOWN || evt->type == SDL_TEXTINPUT))
        return;

    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (selected_class < 0) return;

        int px = 360, py = 50;
        int base_y = py + 10;

        // Стрелки кривых роста
        int growth_offset_y = base_y + 3 * 35;
        for (int s = 0; s < 5; s++) {
            SDL_Rect prev = {px + 480, growth_offset_y + s * 35, 20, 22};
            SDL_Rect next = {px + 505, growth_offset_y + s * 35, 20, 22};
            if (mx >= prev.x && mx < prev.x + prev.w && my >= prev.y && my < prev.y + prev.h) {
                if (curve_count > 0 && selected_curve_idx[s] > 0) {
                    selected_curve_idx[s]--;
                    strncpy(growth[s].curve, curve_names[selected_curve_idx[s]], 63);
                    commit_class_changes();
                }
                return;
            }
            if (mx >= next.x && mx < next.x + next.w && my >= next.y && my < next.y + next.h) {
                if (curve_count > 0 && selected_curve_idx[s] < curve_count - 1) {
                    selected_curve_idx[s]++;
                    strncpy(growth[s].curve, curve_names[selected_curve_idx[s]], 63);
                    commit_class_changes();
                }
                return;
            }
        }

        // Стрелка вверх списка заклинаний
        SDL_Rect up_arrow = {px + 20, up_arrow_rect_y, 30, 30};
        if (spell_list_scroll > 0 && mx >= up_arrow.x && mx < up_arrow.x + up_arrow.w &&
            my >= up_arrow.y && my < up_arrow.y + up_arrow.h) {
            spell_list_scroll--;
            return;
        }

        // Y первого элемента списка (с учётом стрелки вверх)
        int first_item_y = up_arrow_rect_y + 30;
        int visible = MAX_VISIBLE_SPELLS;
        int total = class_spell_count;

        // Элементы списка, кнопки имени и уровня
        for (int i = spell_list_scroll; i < total && i < spell_list_scroll + visible; i++) {
            int item_y = first_item_y + (i - spell_list_scroll) * 20;

            // Кнопки выбора имени (только для выделенного заклинания)
            if (i == selected_spell_entry) {
                SDL_Rect prev_name_btn = {px + 20, item_y, 20, 20};
                if (mx >= prev_name_btn.x && mx < prev_name_btn.x + prev_name_btn.w &&
                    my >= prev_name_btn.y && my < prev_name_btn.y + prev_name_btn.h) {
                    // Переключиться на предыдущее имя заклинания
                    if (spell_name_count > 0) {
                        int current_idx = -1;
                        for (int k = 0; k < spell_name_count; k++) {
                            if (strcasecmp(spell_names[k], class_spells[i].spell_name) == 0) {
                                current_idx = k;
                                break;
                            }
                        }
                        if (current_idx == -1) current_idx = 0;
                        int new_idx = current_idx - 1;
                        if (new_idx < 0) new_idx = spell_name_count - 1;
                        strncpy(class_spells[i].spell_name, spell_names[new_idx], 63);
                        class_spells[i].spell_name[63] = '\0';
                        // Сбросить уровень на минимальный доступный для нового имени
                        update_spell_levels();
                        if (spell_levels_count > 0)
                            class_spells[i].spell_level = spell_levels[0];
                        else
                            class_spells[i].spell_level = 1;
                        commit_class_changes();
                    }
                    return;
                }

                SDL_Rect next_name_btn = {px + 45, item_y, 20, 20};
                if (mx >= next_name_btn.x && mx < next_name_btn.x + next_name_btn.w &&
                    my >= next_name_btn.y && my < next_name_btn.y + next_name_btn.h) {
                    // Переключиться на следующее имя заклинания
                    if (spell_name_count > 0) {
                        int current_idx = -1;
                        for (int k = 0; k < spell_name_count; k++) {
                            if (strcasecmp(spell_names[k], class_spells[i].spell_name) == 0) {
                                current_idx = k;
                                break;
                            }
                        }
                        if (current_idx == -1) current_idx = 0;
                        int new_idx = current_idx + 1;
                        if (new_idx >= spell_name_count) new_idx = 0;
                        strncpy(class_spells[i].spell_name, spell_names[new_idx], 63);
                        class_spells[i].spell_name[63] = '\0';
                        update_spell_levels();
                        if (spell_levels_count > 0)
                            class_spells[i].spell_level = spell_levels[0];
                        else
                            class_spells[i].spell_level = 1;
                        commit_class_changes();
                    }
                    return;
                }
            }

            // Область клика по тексту заклинания (сдвинута вправо)
            SDL_Rect item = {px + 70, item_y, 360, 20};
            if (mx >= item.x && mx < item.x + item.w && my >= item.y && my < item.y + item.h) {
                selected_spell_entry = i;
                update_spell_levels();
                return;
            }

            // Кнопки выбора уровня (только для выделенного заклинания)
            if (i == selected_spell_entry && spell_levels_count > 0) {
                SDL_Rect prev_lvl = {px + 440, item_y, 20, 20};
                if (mx >= prev_lvl.x && mx < prev_lvl.x + prev_lvl.w &&
                    my >= prev_lvl.y && my < prev_lvl.y + prev_lvl.h) {
                    if (selected_spell_level_idx > 0) {
                        selected_spell_level_idx--;
                        class_spells[selected_spell_entry].spell_level = spell_levels[selected_spell_level_idx];
                        commit_class_changes();
                    }
                    return;
                }
                SDL_Rect next_lvl = {px + 465, item_y, 20, 20};
                if (mx >= next_lvl.x && mx < next_lvl.x + next_lvl.w &&
                    my >= next_lvl.y && my < next_lvl.y + next_lvl.h) {
                    if (selected_spell_level_idx < spell_levels_count - 1) {
                        selected_spell_level_idx++;
                        class_spells[selected_spell_entry].spell_level = spell_levels[selected_spell_level_idx];
                        commit_class_changes();
                    }
                    return;
                }
            }
        }

        // Стрелка вниз
        SDL_Rect down_arrow = {px + 20, down_arrow_rect_y, 30, 30};
        if (spell_list_scroll + visible < total &&
            mx >= down_arrow.x && mx < down_arrow.x + down_arrow.w &&
            my >= down_arrow.y && my < down_arrow.y + down_arrow.h) {
            spell_list_scroll++;
            return;
        }

        // Кнопки + / - (без изменений)
        SDL_Rect spell_add_btn = {px + 20, last_spell_btns_y, 20, 20};
        SDL_Rect spell_del_btn = {px + 50, last_spell_btns_y, 20, 20};

        if (mx >= spell_add_btn.x && mx < spell_add_btn.x + spell_add_btn.w &&
            my >= spell_add_btn.y && my < spell_add_btn.y + spell_add_btn.h) {
            if (class_spell_count < MAX_SPELLS_PER_CLASS) {
                class_spells[class_spell_count].level = 1;
                strncpy(class_spells[class_spell_count].spell_name, "Heal", 63);
                class_spells[class_spell_count].spell_level = 1;
                class_spell_count++;
                if (class_spell_count > visible)
                    spell_list_scroll = class_spell_count - visible;
                commit_class_changes();
            }
            return;
        }

        if (mx >= spell_del_btn.x && mx < spell_del_btn.x + spell_del_btn.w &&
            my >= spell_del_btn.y && my < spell_del_btn.y + spell_del_btn.h) {
            int idx_to_remove = (selected_spell_entry >= 0) ? selected_spell_entry : class_spell_count - 1;
            if (class_spell_count > 0 && idx_to_remove >= 0 && idx_to_remove < class_spell_count) {
                for (int i = idx_to_remove; i < class_spell_count - 1; i++)
                    class_spells[i] = class_spells[i + 1];
                class_spell_count--;
                if (selected_spell_entry >= class_spell_count) selected_spell_entry = -1;
                if (spell_list_scroll > 0 && class_spell_count <= spell_list_scroll)
                    spell_list_scroll = class_spell_count - visible;
                if (spell_list_scroll < 0) spell_list_scroll = 0;
                commit_class_changes();
            }
            return;
        }

        // Основные кнопки
        int btn_y = last_btn_y;
        SDL_Rect save_btn   = {px + 130, btn_y, 80, 30};
        SDL_Rect refresh_btn= {px + 220, btn_y, 80, 30};
        SDL_Rect del_btn    = {px + 10,  btn_y, 100, 30};
        SDL_Rect add_btn    = {px + 310, btn_y, 100, 30};

        if (mx >= save_btn.x && mx < save_btn.x + save_btn.w &&
            my >= save_btn.y && my < save_btn.y + save_btn.h) {
            // Фиксируем активное поле (если есть)
            if (class_active_field >= 0)
                commit_class_field(class_active_field);
            // Переносим все буферы (name, move, growth…) в JSON-структуру
            commit_class_changes();
            // Теперь сохраняем актуальные данные на диск
            classes_save_to_file();
            return;
        }
        if (mx >= refresh_btn.x && mx < refresh_btn.x + refresh_btn.w &&
            my >= refresh_btn.y && my < refresh_btn.y + refresh_btn.h) {
            // Сначала фиксируем всё, что наредактировали
            if (class_active_field >= 0)
                commit_class_field(class_active_field);
            commit_class_changes();
            // Сохраняем, чтобы не потерять правки при перечитывании файла
            classes_save_to_file();
            // Теперь перечитываем файл (увидим только что сохранённые данные)
            classes_reload();
            return;
        }
        if (mx >= del_btn.x && mx < del_btn.x + del_btn.w &&
            my >= del_btn.y && my < del_btn.y + del_btn.h) {
            delete_class();
            return;
        }
        if (mx >= add_btn.x && mx < add_btn.x + add_btn.w &&
            my >= add_btn.y && my < add_btn.y + add_btn.h) {
            add_new_class();
            return;
        }
    }
}

static void add_new_class(void) {
    if (!classes) return;
    int new_id = 0;
    for (int i = 0; i < class_count; i++) {
        int id = cJSON_GetObjectItem(cJSON_GetArrayItem(classes, i), "id")->valueint;
        if (id >= new_id) new_id = id + 1;
    }
    char new_name[64]; int idx = 0;
    while (1) {
        snprintf(new_name, sizeof(new_name), "Class%02d", idx);
        int found = 0;
        for (int i = 0; i < class_count; i++)
            if (strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(classes, i), "name")->valuestring, new_name) == 0) found = 1;
        if (!found) break;
        idx++;
    }
    cJSON *cls = cJSON_CreateObject();
    cJSON_AddNumberToObject(cls, "id", new_id);
    cJSON_AddStringToObject(cls, "name", new_name);
    cJSON_AddStringToObject(cls, "full_name", "FullName");
    cJSON_AddNumberToObject(cls, "move", 5);
    cJSON_AddStringToObject(cls, "move_type", "REGULAR");
    const char *gkeys[] = {"hp_growth","mp_growth","attack_growth","defense_growth","agility_growth"};
    for (int i = 0; i < 5; i++) {
        cJSON *gr = cJSON_CreateObject();
        cJSON_AddNumberToObject(gr, "start", 10);
        cJSON_AddNumberToObject(gr, "projected", 30);
        cJSON_AddStringToObject(gr, "curve", "LINEAR");
        cJSON_AddItemToObject(cls, gkeys[i], gr);
    }
    cJSON *spells = cJSON_CreateArray();
    cJSON_AddItemToObject(cls, "spell_list", spells);
    cJSON_AddItemToArray(classes, cls);
    class_count++;
    if (selected_class >= 0) commit_class_changes();
    selected_class = class_count - 1;
    load_class_fields();
}

static void delete_class(void) {
    if (selected_class < 0 || !classes) return;
    cJSON *new_arr = cJSON_CreateArray();
    for (int i = 0; i < class_count; i++) {
        if (i == selected_class) continue;
        cJSON_AddItemToArray(new_arr, cJSON_Duplicate(cJSON_GetArrayItem(classes, i), 1));
    }
    cJSON_Delete(classes);
    classes = new_arr;
    class_count--;
    if (class_count == 0) {
        selected_class = -1;
        class_active_field = -1;
        class_field_count = 0;
    } else {
        if (selected_class >= class_count) selected_class = class_count - 1;
        if (selected_class >= 0) load_class_fields();
    }
}

void classes_reload(void) {
    if (classes) { cJSON_Delete(classes); classes = NULL; }
    FILE *f = fopen("../data/actors/classes.json", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(len+1); fread(buf, 1, len, f); buf[len] = '\0'; fclose(f);
    cJSON *root = cJSON_Parse(buf); free(buf);
    if (root) {
        cJSON *arr = cJSON_GetObjectItem(root, "classes");
        if (arr && cJSON_IsArray(arr)) {
            classes_json = arr;
            classes_count = cJSON_GetArraySize(arr);
            classes = classes_json;
            class_count = classes_count;
            selected_class = -1;
            class_scroll = 0;
            save_timer = 0;
            build_curve_list();
            build_spell_name_list();
            class_field_count = 0;
            class_active_field = -1;
            if (class_count > 0) { selected_class = 0; load_class_fields(); }
        } else cJSON_Delete(root);
    }
}

void classes_init(cJSON *json_array, int count) {
    classes = json_array;
    class_count = count;
    selected_class = -1;
    class_scroll = 0;
    save_timer = 0;
    build_curve_list();
    build_spell_name_list();
    class_field_count = 0;
    class_active_field = -1;
    if (class_count > 0) { selected_class = 0; load_class_fields(); }
}

void classes_save_to_file(void) {
    if (!classes) return;
    cJSON *root = cJSON_CreateObject();
    cJSON *dup = cJSON_Duplicate(classes, 1);
    cJSON_AddItemToObject(root, "classes", dup);
    char *js = cJSON_PrintBuffered(root, 0, 1);
    FILE *f = fopen("../data/actors/classes.json", "w");
    if (f) { fputs(js, f); fclose(f); error_msg[0] = '\0'; }
    else snprintf(error_msg, sizeof(error_msg), "Failed to save classes.json");
    free(js); cJSON_Delete(root);
    save_timer = SAVE_BLINK_DURATION;
}

int classes_is_edit_active(void) { return class_active_field >= 0; }
void classes_reset_selection(void) {
    if (class_count > 0) { selected_class = 0; load_class_fields(); }
    else { selected_class = -1; class_active_field = -1; class_field_count = 0; }
}
void classes_adjust_scroll(int delta) {
    class_scroll -= delta;
    // Не выше первого элемента
    if (class_scroll < 0) class_scroll = 0;
    // Не ниже последнего элемента
    int total_h = classes_get_total_height();
    int visible_h = g_window_height - 35; // 35 = tab_h + 5 (высота вкладок и отступ)
    int max_scroll = total_h > visible_h ? total_h - visible_h : 0;
    if (class_scroll > max_scroll) class_scroll = max_scroll;
}

int classes_get_scroll(void) { return class_scroll; }

void classes_draw_list(SDL_Renderer *renderer, int y_offset, int scroll) {
    if (!classes) return;
    int y = y_offset - scroll;
    SDL_Color white = {255,255,255};
    for (int i = 0; i < class_count; i++) {
        if (y + 20 < y_offset || y > y_offset + 600) { y += 20; continue; }
        cJSON *cls = cJSON_GetArrayItem(classes, i);
        int id = cJSON_GetObjectItem(cls, "id")->valueint;
        const char *nm = cJSON_GetObjectItem(cls, "name")->valuestring;
        char buf[256];
        snprintf(buf, sizeof(buf), "%d: %s", id, nm);
        SDL_Rect rr = {10, y, 290, 20};
        if (i == selected_class) { SDL_SetRenderDrawColor(renderer, 100,100,255,128); SDL_RenderFillRect(renderer, &rr); }
        draw_text_ext(renderer, 10, y, buf, white);
        y += 20;
    }
}
int classes_get_total_height(void) { return class_count * 20; }
void classes_handle_click(int mx, int my, int y_offset, int scroll) {
    int y = y_offset - scroll;
    for (int i = 0; i < class_count; i++) {
        if (mx >= 10 && mx < 300 && my >= y && my < y+20) {
            if (selected_class != i) {
                if (selected_class >= 0) commit_class_changes();
                selected_class = i;
                load_class_fields();
            }
            return;
        }
        y += 20;
    }
}

static void update_spell_levels(void) {
    spell_levels_count = 0;
    selected_spell_level_idx = -1;
    
    if (selected_spell_entry < 0 || selected_spell_entry >= class_spell_count) return;
    
    const char *spell_name = class_spells[selected_spell_entry].spell_name;
    for (int i = 0; i < spells_count; i++) {
        cJSON *spell = cJSON_GetArrayItem(spells_json, i);
        if (_stricmp(cJSON_GetObjectItem(spell, "name")->valuestring, spell_name) == 0) {
            int lv = cJSON_GetObjectItem(spell, "level")->valueint;
            if (spell_levels_count < MAX_SPELLS_PER_CLASS) {
                spell_levels[spell_levels_count++] = lv;
            }
        }
    }
    
    // Сортируем уровни для удобства
    for (int i = 0; i < spell_levels_count - 1; i++) {
        for (int j = i + 1; j < spell_levels_count; j++) {
            if (spell_levels[i] > spell_levels[j]) {
                int tmp = spell_levels[i];
                spell_levels[i] = spell_levels[j];
                spell_levels[j] = tmp;
            }
        }
    }
    
    // Находим текущий уровень
    int current_level = class_spells[selected_spell_entry].spell_level;
    for (int i = 0; i < spell_levels_count; i++) {
        if (spell_levels[i] == current_level) {
            selected_spell_level_idx = i;
            break;
        }
    }
}

void classes_set_window_height(int h) {
    g_window_height = h;
}
void classes_update_timer(void) { if (save_timer > 0) save_timer--; }