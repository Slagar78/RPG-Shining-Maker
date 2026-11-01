// actors_editor.c
#include "actors_editor.h"
#include <stdio.h>
#include <string.h>
#include <SDL_ttf.h>
#include <ctype.h>
#include <windows.h>
#include <commdlg.h>
#include <SDL_image.h>

// Внешние данные из DatabaseEditor.c
extern cJSON *classes_json;
extern int classes_count;
extern cJSON *actors_json;
extern int actors_count;
extern cJSON *start_inventory_json;
extern int start_inventory_count;
extern TTF_Font *g_font;
extern int g_font_ok;
extern char error_msg[256];

int draw_text_ext(SDL_Renderer *r, int x, int y, const char *text, SDL_Color color);
char* open_file_dialog(const char* initialDir);

// ---------- ЛОКАЛЬНЫЕ ДАННЫЕ ----------
static cJSON *actors = NULL;
static int actor_count = 0;
static int selected_actor = -1;

static char  name_buf[11] = {0};
static int   class_id_val = 0;
static char  portrait_buf[64] = {0};
static char  mapsprite_buf[64] = {0};
static int   level_val = 1;

typedef struct {
    int id;
    char name[64];
} ClassEntry;
static ClassEntry class_list[200];
static int class_list_count = 0;
static int selected_class_idx = -1;

static int save_timer = 0;
#define SAVE_BLINK_DURATION 42
static int actor_scroll = 0;
static int g_window_height = 680;
static int last_actor_btns_y = 0;

// Стартовый инвентарь
static int start_level_val = 1;
static char start_items[4][64];
static int start_items_equipped[4];
static int has_start_data = 0;
static char item_names[200][64];
static int item_name_count = 0;

// ---------- ПОРТРЕТЫ ----------
static char portrait_list[200][64];
static int portrait_list_count = 0;
static int selected_portrait_idx = -1;
static SDL_Texture *portrait_tex = NULL;
static char loaded_portrait_name[64] = "";

static void build_item_name_list(void);
static void scan_portrait_folder(void);

static void reload_start_inventory(void) {
    if (start_inventory_json) {
        cJSON_Delete(start_inventory_json);
        start_inventory_json = NULL;
    }
    FILE *f = fopen("../data/actors/start_inventory.json", "rb");
    if (!f) {
        start_inventory_json = cJSON_CreateArray();
        start_inventory_count = 0;
        return;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (root && cJSON_IsObject(root)) {
        cJSON *inv_arr = cJSON_GetObjectItem(root, "start_inventory");
        if (inv_arr && cJSON_IsArray(inv_arr)) {
            start_inventory_json = cJSON_Duplicate(inv_arr, 1);
            start_inventory_count = cJSON_GetArraySize(inv_arr);
        } else {
            start_inventory_json = cJSON_CreateArray();
            start_inventory_count = 0;
        }
        cJSON_Delete(root);
    } else {
        start_inventory_json = cJSON_CreateArray();
        start_inventory_count = 0;
    }
}

// поля ввода
#define MAX_ACTOR_FIELDS 20
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
} ActorField;
static ActorField actor_fields[MAX_ACTOR_FIELDS];
static int actor_field_count = 0;
static int actor_active_field = -1;

// прототипы
static void build_class_list(void);
static void commit_actor_changes(void);
static void load_actor_fields(void);
static void open_actor_fields(void);
static void commit_actor_field(int idx);
static int  handle_actor_input(SDL_Event *evt, int idx);
static void draw_actor_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *fallback_text);
static void add_new_actor(void);
static void delete_actor(void);
static cJSON* find_start_inventory_entry(int actor_id);
static void save_start_inventory_for_actor(int actor_id);
extern int save_start_inventory_to_file(void); // объявлена в DatabaseEditor.c

// ---------- РЕАЛИЗАЦИИ ----------
static void build_class_list(void) {
    class_list_count = 0;
    for (int i = 0; i < classes_count; i++) {
        cJSON *cls = cJSON_GetArrayItem(classes_json, i);
        int id = cJSON_GetObjectItem(cls, "id")->valueint;
        const char *name = cJSON_GetObjectItem(cls, "name")->valuestring;
        if (class_list_count < 200) {
            class_list[class_list_count].id = id;
            strncpy(class_list[class_list_count].name, name, 63);
            class_list[class_list_count].name[63] = '\0';
            class_list_count++;
        }
    }
}

static void build_item_name_list(void) {
    item_name_count = 0;
    extern cJSON *items_json;
    extern int items_count;
    if (!items_json) return;
    for (int i = 0; i < items_count; i++) {
        cJSON *it = cJSON_GetArrayItem(items_json, i);
        const char *name = cJSON_GetObjectItem(it, "name")->valuestring;
        int exists = 0;
        for (int j = 0; j < item_name_count; j++)
            if (strcmp(item_names[j], name) == 0) { exists = 1; break; }
        if (!exists && item_name_count < 200) {
            strncpy(item_names[item_name_count], name, 63);
            item_names[item_name_count][63] = '\0';
            item_name_count++;
        }
    }
    if (item_name_count < 200) {
        strcpy(item_names[item_name_count], "NOTHING");
        item_name_count++;
    }
}

static void scan_portrait_folder(void) {
    portrait_list_count = 0;
    selected_portrait_idx = -1;
    if (portrait_tex) {
        SDL_DestroyTexture(portrait_tex);
        portrait_tex = NULL;
    }

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

        if (strstr(name, "_blink") || strstr(name, "_talk"))
            continue;

        if (portrait_list_count < 200) {
            snprintf(portrait_list[portrait_list_count], sizeof(portrait_list[portrait_list_count]), "%s", name);
            portrait_list_count++;
        }
    } while (FindNextFile(hFind, &findFileData) != 0);
    FindClose(hFind);

    for (int i = 0; i < portrait_list_count; i++) {
        if (strcmp(portrait_list[i], portrait_buf) == 0) {
            selected_portrait_idx = i;
            break;
        }
    }
}

static void update_portrait_index(void) {
    selected_portrait_idx = -1;
    for (int i = 0; i < portrait_list_count; i++) {
        if (strcmp(portrait_list[i], portrait_buf) == 0) {
            selected_portrait_idx = i;
            break;
        }
    }
}

static cJSON* find_start_inventory_entry(int actor_id) {
    for (int i = 0; i < start_inventory_count; i++) {
        cJSON *entry = cJSON_GetArrayItem(start_inventory_json, i);
        if (cJSON_GetObjectItem(entry, "actor_id")->valueint == actor_id)
            return entry;
    }
    return NULL;
}

static void save_start_inventory_for_actor(int actor_id) {
    cJSON *entry = find_start_inventory_entry(actor_id);
    if (!entry) return;                // если записи нет – ничего не делаем
    cJSON *st_lvl = cJSON_GetObjectItem(entry, "start_level");
    if (!st_lvl) cJSON_AddNumberToObject(entry, "start_level", start_level_val);
    else st_lvl->valueint = start_level_val;

    cJSON *items_arr = cJSON_GetObjectItem(entry, "items");
    if (items_arr) cJSON_DeleteItemFromObject(entry, "items");
    items_arr = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        cJSON *slot = cJSON_CreateObject();
        cJSON_AddStringToObject(slot, "item", start_items[i]);
        cJSON_AddBoolToObject(slot, "equipped", start_items_equipped[i] ? 1 : 0);
        cJSON_AddItemToArray(items_arr, slot);
    }
    cJSON_AddItemToObject(entry, "items", items_arr);
}

static void commit_actor_changes(void) {
    if (selected_actor < 0) return;
    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
    cJSON *nm = cJSON_GetObjectItem(act, "name");
    if (nm && cJSON_IsString(nm)) cJSON_SetValuestring(nm, name_buf);
    cJSON *cid = cJSON_GetObjectItem(act, "class_id");
    if (cid && cJSON_IsNumber(cid)) { cid->valueint = class_id_val; cid->valuedouble = class_id_val; }
    cJSON *prt = cJSON_GetObjectItem(act, "portrait");
    if (prt && cJSON_IsString(prt)) cJSON_SetValuestring(prt, portrait_buf);
    cJSON *ms = cJSON_GetObjectItem(act, "mapsprite");
    if (ms && cJSON_IsString(ms)) cJSON_SetValuestring(ms, mapsprite_buf);
    cJSON *lv = cJSON_GetObjectItem(act, "level");
    if (lv && cJSON_IsNumber(lv)) { lv->valueint = level_val; lv->valuedouble = level_val; }
    // инвентарь сохраняется отдельно
}

static void load_actor_fields(void) {
    if (selected_actor < 0) return;
    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
    const char *s = cJSON_GetObjectItem(act, "name")->valuestring;
    strncpy(name_buf, s ? s : "", 10); name_buf[10] = '\0';
    class_id_val = cJSON_GetObjectItem(act, "class_id")->valueint;
    const char *p = cJSON_GetObjectItem(act, "portrait")->valuestring;
    strncpy(portrait_buf, p ? p : "", 63); portrait_buf[63] = '\0';
    const char *m = cJSON_GetObjectItem(act, "mapsprite")->valuestring;
    strncpy(mapsprite_buf, m ? m : "", 63); mapsprite_buf[63] = '\0';
    level_val = cJSON_GetObjectItem(act, "level")->valueint;

    selected_class_idx = -1;
    for (int i = 0; i < class_list_count; i++) {
        if (class_list[i].id == class_id_val) { selected_class_idx = i; break; }
    }

    // загрузка стартового инвентаря из start_inventory.json
    int actor_id = cJSON_GetObjectItem(act, "id")->valueint;
    cJSON *entry = find_start_inventory_entry(actor_id);
    has_start_data = (entry != NULL);
    if (has_start_data) {
        cJSON *st_lvl = cJSON_GetObjectItem(entry, "start_level");
        start_level_val = (st_lvl && cJSON_IsNumber(st_lvl)) ? st_lvl->valueint : 1;
        if (start_level_val < 1) start_level_val = 1;

        cJSON *items_arr = cJSON_GetObjectItem(entry, "items");
        if (items_arr && cJSON_IsArray(items_arr)) {
            int sz = cJSON_GetArraySize(items_arr);
            if (sz > 4) sz = 4;
            for (int i = 0; i < sz; i++) {
                cJSON *slot = cJSON_GetArrayItem(items_arr, i);
                const char *item_name = cJSON_GetObjectItem(slot, "item")->valuestring;
                strncpy(start_items[i], item_name ? item_name : "NOTHING", 63);
                start_items[i][63] = '\0';
                cJSON *eq = cJSON_GetObjectItem(slot, "equipped");
                start_items_equipped[i] = (eq && eq->valueint != 0) ? 1 : 0;
            }
            for (int i = sz; i < 4; i++) {
                strcpy(start_items[i], "NOTHING");
                start_items_equipped[i] = 0;
            }
        } else {
            for (int i = 0; i < 4; i++) {
                strcpy(start_items[i], "NOTHING");
                start_items_equipped[i] = 0;
            }
        }
    } else {
        start_level_val = 1;
        for (int i = 0; i < 4; i++) {
            strcpy(start_items[i], "NOTHING");
            start_items_equipped[i] = 0;
        }
    }
	update_portrait_index();
    open_actor_fields();
}

static void open_actor_fields(void) {
    actor_field_count = 0;
    actor_active_field = -1;
    int base_x = 360, base_y = 60; // py + 10
    int off = 100;

    snprintf(actor_fields[0].text, sizeof(actor_fields[0].text), "%s", name_buf);
    actor_fields[0].cursor = strlen(actor_fields[0].text);
    actor_fields[0].active = 0;
    actor_fields[0].json_obj = cJSON_GetArrayItem(actors, selected_actor);
    actor_fields[0].json_key = "name";
    actor_fields[0].is_numeric = 0; actor_fields[0].max_len = 10; actor_fields[0].array_index = -1; actor_fields[0].is_special = 0;
    actor_fields[0].rect = (SDL_Rect){base_x+off, base_y + 0*35, 150, 22};

    snprintf(actor_fields[1].text, sizeof(actor_fields[1].text), "%d", class_id_val);
    actor_fields[1].cursor = 0;
    actor_fields[1].active = 0;
    actor_fields[1].json_obj = NULL; actor_fields[1].json_key = "class_id";
    actor_fields[1].is_numeric = 1; actor_fields[1].max_len = 0; actor_fields[1].array_index = -1;
    actor_fields[1].is_special = 1;
    actor_fields[1].rect = (SDL_Rect){base_x+off, base_y + 1*35, 150, 22};

    snprintf(actor_fields[2].text, sizeof(actor_fields[2].text), "%d", level_val);
    actor_fields[2].cursor = strlen(actor_fields[2].text);
    actor_fields[2].active = 0;
    actor_fields[2].json_obj = NULL; actor_fields[2].json_key = "level";
    actor_fields[2].is_numeric = 1; actor_fields[2].max_len = 4; actor_fields[2].array_index = -1; actor_fields[2].is_special = 0;
    actor_fields[2].rect = (SDL_Rect){base_x+off, base_y + 2*35, 150, 22};

    snprintf(actor_fields[3].text, sizeof(actor_fields[3].text), "%s", portrait_buf);
    actor_fields[3].cursor = strlen(actor_fields[3].text);
    actor_fields[3].active = 0;
    actor_fields[3].json_obj = NULL; actor_fields[3].json_key = "portrait";
    actor_fields[3].is_numeric = 0; actor_fields[3].max_len = 0; actor_fields[3].array_index = -1;
    actor_fields[3].is_special = 2;
    actor_fields[3].rect = (SDL_Rect){base_x+off, base_y + 3*35, 150, 22};

    snprintf(actor_fields[4].text, sizeof(actor_fields[4].text), "%s", mapsprite_buf);
    actor_fields[4].cursor = strlen(actor_fields[4].text);
    actor_fields[4].active = 0;
    actor_fields[4].json_obj = NULL; actor_fields[4].json_key = "mapsprite";
    actor_fields[4].is_numeric = 0; actor_fields[4].max_len = 0; actor_fields[4].array_index = -1;
    actor_fields[4].is_special = 3;
    actor_fields[4].rect = (SDL_Rect){base_x+off, base_y + 4*35, 150, 22};

    snprintf(actor_fields[5].text, sizeof(actor_fields[5].text), "%d", start_level_val);
    actor_fields[5].cursor = strlen(actor_fields[5].text);
    actor_fields[5].active = 0;
    actor_fields[5].json_obj = NULL;
    actor_fields[5].json_key = "start_level";
    actor_fields[5].is_numeric = 1;
    actor_fields[5].max_len = 3;
    actor_fields[5].array_index = -1;
    actor_fields[5].is_special = 0;
    actor_fields[5].rect = (SDL_Rect){base_x+off, base_y + 5*35, 150, 22};

    actor_field_count = 6;
}

static void commit_actor_field(int idx) {
    if (idx < 0 || idx >= actor_field_count) return;
    ActorField *f = &actor_fields[idx];
    if (f->is_special == 1) {
        class_id_val = atoi(f->text);
        selected_class_idx = -1;
        for (int i = 0; i < class_list_count; i++)
            if (class_list[i].id == class_id_val) { selected_class_idx = i; break; }
    } else if (f->is_special == 2) {
        strncpy(portrait_buf, f->text, 63); portrait_buf[63] = '\0';
    } else if (f->is_special == 3) {
        strncpy(mapsprite_buf, f->text, 63); mapsprite_buf[63] = '\0';
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
        }
    }
    if (idx == 0) { strncpy(name_buf, f->text, 10); name_buf[10] = '\0'; }
    else if (idx == 2) level_val = atoi(f->text);
    else if (idx == 5) start_level_val = atoi(f->text);
    f->active = 0;
}

static int handle_actor_input(SDL_Event *evt, int idx) {
    ActorField *f = &actor_fields[idx];
    SDL_Rect r = f->rect;
    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) {
            if (actor_active_field >= 0 && actor_active_field != idx) commit_actor_field(actor_active_field);
            actor_active_field = idx;
            f->active = 1;
            f->cursor = strlen(f->text);
            return 1;
        }
        if (f->active) {
            commit_actor_field(idx);
            int best = -1, bestDist = 1000;
            for (int i = 0; i < actor_field_count; i++) {
                if (i == 3 || i == 4) continue;
                SDL_Rect fr = actor_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                actor_fields[best].active = 1;
                actor_fields[best].cursor = strlen(actor_fields[best].text);
                actor_active_field = best;
            } else actor_active_field = -1;
            return 1;
        } else {
            int best = -1, bestDist = 1000;
            for (int i = 0; i < actor_field_count; i++) {
                if (i == 3 || i == 4) continue;
                SDL_Rect fr = actor_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                if (actor_active_field >= 0) commit_actor_field(actor_active_field);
                actor_fields[best].active = 1;
                actor_fields[best].cursor = strlen(actor_fields[best].text);
                actor_active_field = best;
                return 1;
            }
        }
    } else if (evt->type == SDL_KEYDOWN && f->active && f->is_special == 0) {
        if (evt->key.keysym.sym == SDLK_UP) {
            if (idx == 0) return 1;
            commit_actor_field(idx);
            int new_idx = idx - 1;
            actor_fields[new_idx].active = 1;
            actor_fields[new_idx].cursor = strlen(actor_fields[new_idx].text);
            actor_active_field = new_idx;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_DOWN) {
            if (idx >= 2) return 1;
            commit_actor_field(idx);
            int new_idx = idx + 1;
            actor_fields[new_idx].active = 1;
            actor_fields[new_idx].cursor = strlen(actor_fields[new_idx].text);
            actor_active_field = new_idx;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
            if (f->cursor > 0) {
                memmove(f->text + f->cursor - 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->cursor--;
            }
            return 1;
        } else if (evt->key.keysym.sym == SDLK_RETURN || evt->key.keysym.sym == SDLK_KP_ENTER) {
            commit_actor_field(idx);
            return 1;
        } else if (evt->key.keysym.sym == SDLK_LEFT && f->cursor > 0) { f->cursor--; return 1; }
        else if (evt->key.keysym.sym == SDLK_RIGHT && f->cursor < strlen(f->text)) { f->cursor++; return 1; }
    } else if (evt->type == SDL_TEXTINPUT && f->active && f->is_special == 0) {
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

static void draw_actor_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *fallback_text) {
    SDL_Color white = {255,255,255}, black = {0,0,0}, gray = {100,100,100};
    draw_text_ext(r, x, y + 3, label, white);
    int fx = x + 100;
    SDL_Rect rect = {fx, y, w, h};
    SDL_SetRenderDrawColor(r, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, white.r, white.g, white.b, 255); SDL_RenderDrawRect(r, &rect);

    const char *text_to_draw = fallback_text;
    if (idx >= 0 && idx < actor_field_count && actor_fields[idx].active)
        text_to_draw = actor_fields[idx].text;

    draw_text_ext(r, fx + 5, y + 3, text_to_draw, black);

    if (idx >= 0 && idx < actor_field_count && actor_fields[idx].active) {
        ActorField *f = &actor_fields[idx];
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

static void load_portrait_texture(SDL_Renderer *renderer) {
    if (portrait_tex) {
        SDL_DestroyTexture(portrait_tex);
        portrait_tex = NULL;
    }
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

static void browse_asset(char *buffer, int buf_size, const char *initialDir) {
    char *path = open_file_dialog(initialDir);
    if (!path) return;
    const char *fname = strrchr(path, '\\');
    if (!fname) fname = path; else fname++;
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%s", fname);
    char *dot = strrchr(tmp, '.');
    if (dot) *dot = '\0';
    size_t len = strlen(tmp);
    if (len >= (size_t)buf_size) len = buf_size - 1;
    memcpy(buffer, tmp, len);
    buffer[len] = '\0';
    free(path);
}

static void add_new_actor(void) {
    if (!actors) return;
    int new_id = 0;
    for (int i = 0; i < actor_count; i++) {
        int id = cJSON_GetObjectItem(cJSON_GetArrayItem(actors, i), "id")->valueint;
        if (id >= new_id) new_id = id + 1;
    }
    char new_name[64]; int idx = 0;
    while (1) {
        snprintf(new_name, sizeof(new_name), "Actor%02d", idx);
        int found = 0;
        for (int i = 0; i < actor_count; i++)
            if (strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(actors, i), "name")->valuestring, new_name) == 0) found = 1;
        if (!found) break;
        idx++;
    }
    cJSON *act = cJSON_CreateObject();
    cJSON_AddNumberToObject(act, "id", new_id);
    cJSON_AddStringToObject(act, "name", new_name);
    cJSON_AddNumberToObject(act, "class_id", 0);
    cJSON_AddStringToObject(act, "portrait", "Mushra");
    cJSON_AddStringToObject(act, "mapsprite", "mapsprite000");
    cJSON_AddNumberToObject(act, "level", 1);
    cJSON_AddNumberToObject(act, "exp", 0);
    cJSON_AddNumberToObject(act, "kills", 0);
    cJSON_AddNumberToObject(act, "defeats", 0);
    cJSON_AddItemToArray(actors, act);
    actor_count++;
    if (selected_actor >= 0) commit_actor_changes();
    selected_actor = actor_count - 1;
    load_actor_fields();
}

static void delete_actor(void) {
    if (selected_actor < 0 || !actors) return;
    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
    int actor_id = cJSON_GetObjectItem(act, "id")->valueint;

    // Удаляем запись в start_inventory.json
    cJSON *inv_entry = find_start_inventory_entry(actor_id);
    if (inv_entry) {
        for (int i = 0; i < start_inventory_count; i++) {
            if (cJSON_GetArrayItem(start_inventory_json, i) == inv_entry) {
                cJSON_DeleteItemFromArray(start_inventory_json, i);
                start_inventory_count--;
                break;
            }
        }
    }

    cJSON *new_arr = cJSON_CreateArray();
    for (int i = 0; i < actor_count; i++) {
        if (i == selected_actor) continue;
        cJSON_AddItemToArray(new_arr, cJSON_Duplicate(cJSON_GetArrayItem(actors, i), 1));
    }
    cJSON_Delete(actors);
    actors = new_arr;
    actor_count--;
    if (actor_count == 0) {
        selected_actor = -1;
        actor_active_field = -1;
        actor_field_count = 0;
    } else {
        if (selected_actor >= actor_count) selected_actor = actor_count - 1;
        if (selected_actor >= 0) load_actor_fields();
    }
}

void actors_reload(void) {
    if (actors) { cJSON_Delete(actors); actors = NULL; }
    FILE *f = fopen("../data/actors/actors.json", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(len+1); fread(buf, 1, len, f); buf[len] = '\0'; fclose(f);
    cJSON *root = cJSON_Parse(buf); free(buf);
    if (root) {
        cJSON *arr = cJSON_GetObjectItem(root, "actors");
        if (arr && cJSON_IsArray(arr)) {
            actors_json = arr;
            actors_count = cJSON_GetArraySize(arr);
            actors = actors_json;
            actor_count = actors_count;
            selected_actor = -1;
            actor_scroll = 0;
            save_timer = 0;
            build_class_list();
            build_item_name_list();
			scan_portrait_folder();
            actor_field_count = 0;
            actor_active_field = -1;

            // Перезагружаем start_inventory.json
            if (start_inventory_json) { cJSON_Delete(start_inventory_json); start_inventory_json = NULL; }
            FILE *f2 = fopen("../data/actors/start_inventory.json", "rb");
            if (f2) {
                fseek(f2, 0, SEEK_END); long len2 = ftell(f2); fseek(f2, 0, SEEK_SET);
                char *buf2 = malloc(len2+1); fread(buf2, 1, len2, f2); buf2[len2] = '\0'; fclose(f2);
                cJSON *root2 = cJSON_Parse(buf2); free(buf2);
                if (root2 && cJSON_IsObject(root2)) {
                    cJSON *inv_arr = cJSON_GetObjectItem(root2, "start_inventory");
                    if (inv_arr && cJSON_IsArray(inv_arr)) {
                        start_inventory_json = cJSON_Duplicate(inv_arr, 1);
                        start_inventory_count = cJSON_GetArraySize(inv_arr);
                    } else {
                        start_inventory_json = cJSON_CreateArray();
                        start_inventory_count = 0;
                    }
                } else {
                    start_inventory_json = cJSON_CreateArray();
                    start_inventory_count = 0;
                }
                cJSON_Delete(root2);
            } else {
                start_inventory_json = cJSON_CreateArray();
                start_inventory_count = 0;
            }

            if (actor_count > 0) { selected_actor = 0; load_actor_fields(); }
        } else cJSON_Delete(root);
    }
}

// ---------- ПУБЛИЧНЫЕ ФУНКЦИИ ----------
void actors_init(cJSON *json_array, int count) {
    if (!start_inventory_json) {
        start_inventory_json = cJSON_CreateArray();
        start_inventory_count = 0;
    }
    actors = json_array;
    actor_count = count;
    selected_actor = -1;
    actor_scroll = 0;
    save_timer = 0;
    build_class_list();
    build_item_name_list();
	scan_portrait_folder();
    actor_field_count = 0;
    actor_active_field = -1;
    if (actor_count > 0) { selected_actor = 0; load_actor_fields(); }
}

void actors_save_to_file(void) {
    if (!actors) return;
    cJSON *root = cJSON_CreateObject();
    cJSON *dup = cJSON_Duplicate(actors, 1);
    cJSON_AddItemToObject(root, "actors", dup);
    char *js = cJSON_PrintBuffered(root, 0, 1);
    FILE *f = fopen("../data/actors/actors.json", "w");
    if (f) { fputs(js, f); fclose(f); error_msg[0] = '\0'; }
    else snprintf(error_msg, sizeof(error_msg), "Failed to save actors.json");
    free(js); cJSON_Delete(root);
    save_timer = SAVE_BLINK_DURATION;
}

int actors_is_edit_active(void) { return actor_active_field >= 0; }
void actors_update_timer(void) { if (save_timer > 0) save_timer--; }

void actors_reset_selection(void) {
    if (actor_count > 0) { selected_actor = 0; load_actor_fields(); }
    else { selected_actor = -1; actor_active_field = -1; actor_field_count = 0; }
}

void actors_adjust_scroll(int delta) {
    actor_scroll -= delta;
    if (actor_scroll < 0) actor_scroll = 0;
    int total_h = actors_get_total_height();
    int visible_h = g_window_height - 35;
    int max_scroll = total_h > visible_h ? total_h - visible_h : 0;
    if (actor_scroll > max_scroll) actor_scroll = max_scroll;
}
int actors_get_scroll(void) { return actor_scroll; }

void actors_draw_list(SDL_Renderer *renderer, int y_offset, int scroll) {
    if (!actors) return;
    int y = y_offset - scroll;
    SDL_Color white = {255,255,255};
    for (int i = 0; i < actor_count; i++) {
        if (y + 20 < y_offset || y > y_offset + 600) { y += 20; continue; }
        cJSON *act = cJSON_GetArrayItem(actors, i);
        int id = cJSON_GetObjectItem(act, "id")->valueint;
        const char *name = cJSON_GetObjectItem(act, "name")->valuestring;
        int cid = cJSON_GetObjectItem(act, "class_id")->valueint;
        const char *cname = "???";
        for (int k = 0; k < class_list_count; k++) {
            if (class_list[k].id == cid) { cname = class_list[k].name; break; }
        }
        char buf[256];
        snprintf(buf, sizeof(buf), "%d: %s (Class %d: %s)", id, name, cid, cname);
        SDL_Rect rr = {10, y, 290, 20};
        if (i == selected_actor) { SDL_SetRenderDrawColor(renderer, 100,100,255,128); SDL_RenderFillRect(renderer, &rr); }
        draw_text_ext(renderer, 10, y, buf, white);
        y += 20;
    }
}

int actors_get_total_height(void) { return actor_count * 20; }

void actors_handle_click(int mx, int my, int y_offset, int scroll) {
    int y = y_offset - scroll;
    for (int i = 0; i < actor_count; i++) {
        if (mx >= 10 && mx < 300 && my >= y && my < y+20) {
            if (selected_actor != i) {
                // Сохраняем старый инвентарь в файл (если были изменения, они уже там)
                if (selected_actor >= 0) {
                    commit_actor_changes();              // сохраняем поля актёра
                    // инвентарь уже на диске благодаря немедленной записи
                }
                // Перезагружаем start_inventory.json в память
                reload_start_inventory();
                selected_actor = i;
                load_actor_fields();
                loaded_portrait_name[0] = '\0';  // чтобы загрузить новый портрет
            }
            return;
        }
        y += 20;
    }
}

void actors_draw_edit_panel(SDL_Renderer *renderer, int px, int py) {
    if (selected_actor < 0) return;
    SDL_Color black = {0,0,0,255}, white = {255,255,255}, blue = {70,70,120};
    SDL_Color green = {0,200,0}, red = {200,0,0};
    SDL_Rect panel = {px, py, 580, 650};
    SDL_SetRenderDrawColor(renderer, 60,60,60,255); SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &panel);

    int y = py + 10;
    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
    int actor_id = cJSON_GetObjectItem(act, "id")->valueint;
    char idstr[16]; snprintf(idstr, sizeof(idstr), "ID:%d", actor_id);

    draw_actor_field(renderer, px+10, y, 150, 22, 0, "Name:", name_buf);
    draw_text_ext(renderer, px+270, y+3, idstr, white);
    y += 35;

    draw_text_ext(renderer, px+10, y+3, "Class:", white);
    const char *cname = "???";
    for (int i = 0; i < class_list_count; i++)
        if (class_list[i].id == class_id_val) { cname = class_list[i].name; break; }
    char class_label[128]; snprintf(class_label, sizeof(class_label), "%d: %s", class_id_val, cname);
    draw_text_ext(renderer, px+110, y+3, class_label, white);
    SDL_Rect prev_c = {px+270, y, 20, 22}; SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &prev_c);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &prev_c);
    draw_text_ext(renderer, px+275, y+3, "<", white);
    SDL_Rect next_c = {px+295, y, 20, 22}; SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &next_c);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &next_c);
    draw_text_ext(renderer, px+300, y+3, ">", white);
    y += 35;

    char lvlstr[8]; snprintf(lvlstr, sizeof(lvlstr), "%d", level_val);
    draw_actor_field(renderer, px+10, y, 150, 22, 2, "Level:", lvlstr);
    y += 35;

    draw_text_ext(renderer, px+10, y+3, "Portrait:", white);
    draw_text_ext(renderer, px+110, y+3, portrait_buf, white);
    SDL_Rect brow1 = {px+10+100+150+5, y, 70, 22};
    SDL_SetRenderDrawColor(renderer, 100,100,200,255); SDL_RenderFillRect(renderer, &brow1);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &brow1);
    draw_text_ext(renderer, brow1.x+5, brow1.y+3, "Browse", white);

    // Стрелки выбора портрета
    SDL_Rect port_prev = {brow1.x + brow1.w + 5, y, 20, 22};
    SDL_SetRenderDrawColor(renderer, 70,70,120,255); SDL_RenderFillRect(renderer, &port_prev);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &port_prev);
    draw_text_ext(renderer, port_prev.x+5, port_prev.y+3, "<", white);

    SDL_Rect port_next = {port_prev.x + 25, y, 20, 22};
    SDL_SetRenderDrawColor(renderer, 70,70,120,255); SDL_RenderFillRect(renderer, &port_next);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &port_next);
    draw_text_ext(renderer, port_next.x+5, port_next.y+3, ">", white);

    y += 35;

    // Миниатюра портрета (внутри панели, справа)
    int thumb_x = px + 580 - 144;   // отступ 10px от правого края панели
    int thumb_y = py + 10;
    SDL_Rect thumb_rect = {thumb_x, thumb_y, 134, 208};

    // Загрузка текстуры, если нужно
    if (selected_portrait_idx >= 0 && 
        (portrait_tex == NULL || strcmp(loaded_portrait_name, portrait_list[selected_portrait_idx]) != 0)) {
        load_portrait_texture(renderer);
    }

    if (portrait_tex) {
        SDL_RenderCopy(renderer, portrait_tex, NULL, &thumb_rect);
    } else {
        SDL_SetRenderDrawColor(renderer, 80,80,80,255);
        SDL_RenderFillRect(renderer, &thumb_rect);
        SDL_SetRenderDrawColor(renderer, 255,255,255,255);
        SDL_RenderDrawRect(renderer, &thumb_rect);
        draw_text_ext(renderer, thumb_x+20, thumb_y+90, "No portrait", white);
    }
    draw_text_ext(renderer, px+10, y+3, "Mapsprite:", white);
    draw_text_ext(renderer, px+110, y+3, mapsprite_buf, white);
    SDL_Rect brow2 = {px+10+100+150+5, y, 70, 22};
    SDL_SetRenderDrawColor(renderer, 100,100,200,255); SDL_RenderFillRect(renderer, &brow2);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &brow2);
    draw_text_ext(renderer, brow2.x+5, brow2.y+3, "Browse", white);
    y += 35;
    // ===== ИНВЕНТАРЬ (всегда резервируем место) =====
    if (has_start_data) {
        draw_text_ext(renderer, px+10, y, "Start Inventory", white);
    }
    y += 25;

    if (has_start_data) {
        draw_text_ext(renderer, px+10, y+3, "Start Class:", white);
        draw_text_ext(renderer, px+110, y+3, cname, white);
    }
    y += 25;

    if (has_start_data) {
        char start_lvl_str[8]; snprintf(start_lvl_str, sizeof(start_lvl_str), "%d", start_level_val);
        draw_actor_field(renderer, px+10, y, 150, 22, 5, "Start Level:", start_lvl_str);
    }
    y += 35;

    for (int i = 0; i < 4; i++) {
        if (has_start_data) {
            draw_text_ext(renderer, px+10, y+3, start_items[i], white);
            SDL_Rect prev_item = {px+220, y, 20, 22};
            SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &prev_item);
            SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &prev_item);
            draw_text_ext(renderer, prev_item.x+5, prev_item.y+3, "<", white);
            SDL_Rect next_item = {px+245, y, 20, 22};
            SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &next_item);
            SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &next_item);
            draw_text_ext(renderer, next_item.x+5, next_item.y+3, ">", white);
            SDL_Rect nothing_btn = {px+275, y, 70, 22};
            SDL_SetRenderDrawColor(renderer, 180,180,180,255); SDL_RenderFillRect(renderer, &nothing_btn);
            SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &nothing_btn);
            draw_text_ext(renderer, nothing_btn.x+5, nothing_btn.y+3, "NOTHING", black);
            SDL_Rect eq_rect = {px+355, y, 22, 22};
            SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderFillRect(renderer, &eq_rect);
            SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &eq_rect);
            if (start_items_equipped[i])
                draw_text_ext(renderer, eq_rect.x+3, eq_rect.y+2, "✔", green);
            else
                draw_text_ext(renderer, eq_rect.x+3, eq_rect.y+2, "✘", red);
        }
        y += 30;
    }
    y += 5;

    // ===== НИЖНИЕ КНОПКИ (позиция теперь не зависит от наличия инвентаря) =====
    last_actor_btns_y = y;

    SDL_Rect del_btn = {px+10, y, 85, 30};
    SDL_SetRenderDrawColor(renderer, 200,80,80,255); SDL_RenderFillRect(renderer, &del_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &del_btn);
    draw_text_ext(renderer, del_btn.x+10, del_btn.y+5, "Del Actor", white);

    SDL_Rect save_btn = {px+100, y, 70, 30};
    SDL_Color save_col = (save_timer > 0) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,0,255};
    SDL_SetRenderDrawColor(renderer, save_col.r, save_col.g, save_col.b, 255); SDL_RenderFillRect(renderer, &save_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &save_btn);
    draw_text_ext(renderer, save_btn.x+12, save_btn.y+5, "SAVE", black);

    SDL_Rect refresh_btn = {px+175, y, 80, 30};
    SDL_SetRenderDrawColor(renderer, 180,180,255,255); SDL_RenderFillRect(renderer, &refresh_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &refresh_btn);
    draw_text_ext(renderer, refresh_btn.x+12, refresh_btn.y+5, "Refresh", black);

    SDL_Rect add_btn = {px+260, y, 95, 30};
    SDL_SetRenderDrawColor(renderer, 100,200,100,255); SDL_RenderFillRect(renderer, &add_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &add_btn);
    draw_text_ext(renderer, add_btn.x+5, add_btn.y+5, "Add Actor", white);
}

void actors_handle_input(SDL_Event *evt) {
    for (int i = 0; i < actor_field_count; i++) {
        if (handle_actor_input(evt, i)) break;
    }
    if (actor_active_field >= 0 && (evt->type == SDL_KEYDOWN || evt->type == SDL_TEXTINPUT))
        return;
    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {		
        int mx = evt->button.x, my = evt->button.y;
        if (selected_actor < 0) return;
        int px = 360, py = 50;

        // Browse Portrait
        SDL_Rect brow1 = {px+10+100+150+5, py+10 + 3*35, 70, 22};
        if (mx >= brow1.x && mx < brow1.x+brow1.w && my >= brow1.y && my < brow1.y+brow1.h) {
            char *path = open_file_dialog("..\\assets\\ui\\portraits");
            if (path) {
                const char *fname = strrchr(path, '\\');
                if (!fname) fname = path; else fname++;
                char tmp[128];
                strncpy(tmp, fname, 127); tmp[127] = '\0';
                char *dot = strrchr(tmp, '.');
                if (dot) *dot = '\0';
                strncpy(portrait_buf, tmp, 63);
                portrait_buf[63] = '\0';
                free(path);
                commit_actor_changes();
                update_portrait_index();
                loaded_portrait_name[0] = '\0';
            }
            return;
        }
        // Browse Mapsprite
        SDL_Rect brow2 = {px+10+100+150+5, py+10 + 4*35, 70, 22};
        if (mx >= brow2.x && mx < brow2.x+brow2.w && my >= brow2.y && my < brow2.y+brow2.h) {
            browse_asset(mapsprite_buf, sizeof(mapsprite_buf), "..\\assets\\mapsprites\\");
            commit_actor_changes();
            return;
        }

        // Стрелки выбора портрета
        int port_y = py + 10 + 3*35; // та же строка, где был Browse
        SDL_Rect port_prev_btn = {px+10+100+150+5 + 70 + 5, port_y, 20, 22};
        SDL_Rect port_next_btn = {port_prev_btn.x + 25, port_y, 20, 22};
        if (mx >= port_prev_btn.x && mx < port_prev_btn.x+port_prev_btn.w && my >= port_prev_btn.y && my < port_prev_btn.y+port_prev_btn.h) {
            if (portrait_list_count > 0) {
                if (selected_portrait_idx > 0) selected_portrait_idx--;
                else selected_portrait_idx = portrait_list_count - 1;
                strncpy(portrait_buf, portrait_list[selected_portrait_idx], 63);
                portrait_buf[63] = '\0';
                commit_actor_changes();
                loaded_portrait_name[0] = '\0'; // сброс кэша текстуры
            }
            return;
        }
        if (mx >= port_next_btn.x && mx < port_next_btn.x+port_next_btn.w && my >= port_next_btn.y && my < port_next_btn.y+port_next_btn.h) {
            if (portrait_list_count > 0) {
                if (selected_portrait_idx < portrait_list_count - 1) selected_portrait_idx++;
                else selected_portrait_idx = 0;
                strncpy(portrait_buf, portrait_list[selected_portrait_idx], 63);
                portrait_buf[63] = '\0';
                commit_actor_changes();
                loaded_portrait_name[0] = '\0';
            }
            return;
        }

        // Стартовый инвентарь
        if (has_start_data) {
            int start_y = py + 10 + 5*35 + 25 + 25 + 35;
            for (int slot = 0; slot < 4; slot++) {
                int y_slot = start_y + slot * 30;
                SDL_Rect prev_item = {px+220, y_slot, 20, 22};
                if (mx >= prev_item.x && mx < prev_item.x+prev_item.w && my >= prev_item.y && my < prev_item.y+prev_item.h) {
                    int cur_idx = -1;
                    for (int k = 0; k < item_name_count; k++) {
                        if (strcmp(item_names[k], start_items[slot]) == 0) { cur_idx = k; break; }
                    }
                    if (cur_idx == -1) cur_idx = 0;
                    int new_idx = cur_idx - 1;
                    if (new_idx < 0) new_idx = item_name_count - 1;
                    strncpy(start_items[slot], item_names[new_idx], 63);
                    start_items[slot][63] = '\0';
                    // Сохраняем в память и сразу на диск
                    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
                    int actor_id = cJSON_GetObjectItem(act, "id")->valueint;
                    save_start_inventory_for_actor(actor_id);
                    save_start_inventory_to_file();
                    return;
                }
                SDL_Rect next_item = {px+245, y_slot, 20, 22};
                if (mx >= next_item.x && mx < next_item.x+next_item.w && my >= next_item.y && my < next_item.y+next_item.h) {
                    int cur_idx = -1;
                    for (int k = 0; k < item_name_count; k++) {
                        if (strcmp(item_names[k], start_items[slot]) == 0) { cur_idx = k; break; }
                    }
                    if (cur_idx == -1) cur_idx = 0;
                    int new_idx = cur_idx + 1;
                    if (new_idx >= item_name_count) new_idx = 0;
                    strncpy(start_items[slot], item_names[new_idx], 63);
                    start_items[slot][63] = '\0';
                    // Сохраняем в память и сразу на диск
                    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
                    int actor_id = cJSON_GetObjectItem(act, "id")->valueint;
                    save_start_inventory_for_actor(actor_id);
                    save_start_inventory_to_file();
                    return;
                }
                SDL_Rect nothing_btn = {px+275, y_slot, 70, 22};
                if (mx >= nothing_btn.x && mx < nothing_btn.x+nothing_btn.w && my >= nothing_btn.y && my < nothing_btn.y+nothing_btn.h) {
                    strcpy(start_items[slot], "NOTHING");
                    // Сохраняем в память и сразу на диск
                    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
                    int actor_id = cJSON_GetObjectItem(act, "id")->valueint;
                    save_start_inventory_for_actor(actor_id);
                    save_start_inventory_to_file();
                    return;
                }
                SDL_Rect eq_rect = {px+355, y_slot, 22, 22};
                if (mx >= eq_rect.x && mx < eq_rect.x+eq_rect.w && my >= eq_rect.y && my < eq_rect.y+eq_rect.h) {
                    start_items_equipped[slot] = !start_items_equipped[slot];
                    // Сохраняем в память и сразу на диск
                    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
                    int actor_id = cJSON_GetObjectItem(act, "id")->valueint;
                    save_start_inventory_for_actor(actor_id);
                    save_start_inventory_to_file();
                    return;
                }
            }
        }

        // Кнопки нижнего ряда
        int btn_y = last_actor_btns_y;
        SDL_Rect del_btn = {px+10, btn_y, 85, 30};
        SDL_Rect save_btn = {px+100, btn_y, 70, 30};
        SDL_Rect refresh_btn = {px+175, btn_y, 80, 30};
        SDL_Rect add_btn = {px+260, btn_y, 95, 30};

        if (mx >= del_btn.x && mx < del_btn.x+del_btn.w && my >= del_btn.y && my < del_btn.y+del_btn.h) {
            delete_actor(); return;
        }
        if (mx >= save_btn.x && mx < save_btn.x+save_btn.w && my >= save_btn.y && my < save_btn.y+save_btn.h) {
            if (actor_active_field >= 0)
                commit_actor_field(actor_active_field);
            commit_actor_changes();
            // Сохраняем инвентарь в память и на диск
            if (selected_actor >= 0 && has_start_data) {
                cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
                int aid = cJSON_GetObjectItem(act, "id")->valueint;
                save_start_inventory_for_actor(aid);
                save_start_inventory_to_file();
            }
            actors_save_to_file();
            return;
        }
        if (mx >= refresh_btn.x && mx < refresh_btn.x+refresh_btn.w && my >= refresh_btn.y && my < refresh_btn.y+refresh_btn.h) {
            if (actor_active_field >= 0)
                commit_actor_field(actor_active_field);
            commit_actor_changes();
            if (selected_actor >= 0 && has_start_data) {
                cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
                int aid = cJSON_GetObjectItem(act, "id")->valueint;
                save_start_inventory_for_actor(aid);
                save_start_inventory_to_file();
            }
            actors_save_to_file();
            actors_reload();
            return;
        }
        if (mx >= add_btn.x && mx < add_btn.x+add_btn.w && my >= add_btn.y && my < add_btn.y+add_btn.h) {
            add_new_actor(); return;
        }

        // Кнопки Class (< >)
        SDL_Rect prev_c = {px+270, py+10 + 1*35, 20, 22};
        SDL_Rect next_c = {px+295, py+10 + 1*35, 20, 22};
        if (mx >= prev_c.x && mx < prev_c.x+prev_c.w && my >= prev_c.y && my < prev_c.y+prev_c.h) {
            if (class_list_count > 0 && selected_class_idx > 0) {
                selected_class_idx--;
                class_id_val = class_list[selected_class_idx].id;
                commit_actor_changes();
            }
            return;
        }
        if (mx >= next_c.x && mx < next_c.x+next_c.w && my >= next_c.y && my < next_c.y+next_c.h) {
            if (class_list_count > 0 && selected_class_idx < class_list_count-1) {
                selected_class_idx++;
                class_id_val = class_list[selected_class_idx].id;
                commit_actor_changes();
            }
            return;
        }
    }
}

void actors_set_window_height(int h) {
    g_window_height = h;
}