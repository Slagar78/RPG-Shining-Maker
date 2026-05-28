#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <windows.h>
#include <commdlg.h>

// ─── Геометрия окна ──────────────────────────
#define WINDOW_W 1024
#define WINDOW_H 600
#define TOOLBAR_H 34

#define LEFT_PANEL_W 300
#define RIGHT_PANEL_W 220
#define CENTER_X LEFT_PANEL_W
#define CENTER_Y TOOLBAR_H
#define CENTER_W (WINDOW_W - LEFT_PANEL_W - RIGHT_PANEL_W)
#define CENTER_H (WINDOW_H - TOOLBAR_H)

#define TILE_SIZE 48
#define PALETTE_TILE_SIZE 32
#define PALETTE_COLS 8
#define PALETTE_START_X 10
#define PALETTE_START_Y 140

#define FONT_SIZE 16

#define MODE_A 0   // просмотр без анимации
#define MODE_B 1   // режим анимации: замена, просмотр переключения

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    // Оригинальный тайлсет (неизменный)
    SDL_Texture **tiles;
    SDL_Surface **tile_surfs;
    // Заменённые тайлы (кастомные)
    SDL_Texture **custom_tiles;
    SDL_Surface **custom_surfs;
    bool         *tile_modified;   // true если есть замена
    int           tile_count;
    int           tileset_cols, tileset_rows;
    bool          tileset_loaded;
    char          tileset_fullpath[512];

    int palette_scroll;
    int selected_tile;
    int mode;

    // Анимация в центре
    Uint32 anim_timer;
    bool   show_anim;
    float  anim_delay;   // секунд

    // Эффект кнопки Save
    Uint32 save_anim_timer;
    bool   save_anim_active;
} Editor;

static bool file_exists(const char *path);

// Прототипы
void editor_init(Editor *ed);
void free_tileset(Editor *ed);
bool open_file_dialog(char *out, size_t len, const char *dir);
int  load_tileset(Editor *ed, const char *path);
void save_tileset(Editor *ed);
void replace_tile_from_file(Editor *ed, int index);
void render_left_panel(Editor *ed);
void render_center(Editor *ed);
void render_right_panel(Editor *ed);
void handle_input(Editor *ed, bool *running);
void get_logical_mouse(Editor *ed, int *mx, int *my);

void safe_strcpy(char *dst, size_t sz, const char *src) {
    if (sz) snprintf(dst, sz, "%s", src);
}

SDL_Texture* create_text_texture(SDL_Renderer *r, TTF_Font *f, const char *text, SDL_Color c) {
    SDL_Surface *s = TTF_RenderUTF8_Blended(f, text, c);
    if (!s) return NULL;
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    return t;
}

void draw_text_centered(SDL_Renderer *r, TTF_Font *f, const char *text, int cx, int cy, SDL_Color c) {
    SDL_Texture *t = create_text_texture(r, f, text, c);
    if (!t) return;
    int tw, th;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    SDL_Rect dst = { cx - tw/2, cy - th/2, tw, th };
    SDL_RenderCopy(r, t, NULL, &dst);
    SDL_DestroyTexture(t);
}

void get_logical_mouse(Editor *ed, int *mx, int *my) {
    SDL_GetMouseState(mx, my);
    int win_w, win_h;
    SDL_GetWindowSize(ed->window, &win_w, &win_h);
    if (win_w != WINDOW_W || win_h != WINDOW_H) {
        *mx = (int)((float)*mx * WINDOW_W / win_w + 0.5f);
        *my = (int)((float)*my * WINDOW_H / win_h + 0.5f);
    }
}

// ─── Инициализация ────────────────────────────
void editor_init(Editor *ed) {
    memset(ed, 0, sizeof(Editor));
    ed->mode = MODE_A;
    ed->anim_delay = 0.5f;
    ed->show_anim = false;
    ed->save_anim_active = false;
}

// ─── Загрузка тайлсета (с подгрузкой анимации) ──
void free_tileset(Editor *ed) {
    if (ed->tiles) {
        for (int i = 0; i < ed->tile_count; i++) SDL_DestroyTexture(ed->tiles[i]);
        free(ed->tiles); ed->tiles = NULL;
    }
    if (ed->tile_surfs) {
        for (int i = 0; i < ed->tile_count; i++) SDL_FreeSurface(ed->tile_surfs[i]);
        free(ed->tile_surfs); ed->tile_surfs = NULL;
    }
    if (ed->custom_tiles) {
        for (int i = 0; i < ed->tile_count; i++) if (ed->custom_tiles[i]) SDL_DestroyTexture(ed->custom_tiles[i]);
        free(ed->custom_tiles); ed->custom_tiles = NULL;
    }
    if (ed->custom_surfs) {
        for (int i = 0; i < ed->tile_count; i++) if (ed->custom_surfs[i]) SDL_FreeSurface(ed->custom_surfs[i]);
        free(ed->custom_surfs); ed->custom_surfs = NULL;
    }
    free(ed->tile_modified); ed->tile_modified = NULL;
    ed->tile_count = 0;
    ed->tileset_loaded = false;
}

int load_tileset(Editor *ed, const char *path) {
    free_tileset(ed);
    safe_strcpy(ed->tileset_fullpath, sizeof(ed->tileset_fullpath), path);

    char full[512];
    if (path[0] && path[1] == ':')
        snprintf(full, sizeof(full), "%s", path);
    else
        snprintf(full, sizeof(full), "../%s", path);

    SDL_Surface *surf = IMG_Load(full);
    if (!surf) return 0;

    ed->tileset_cols = surf->w / TILE_SIZE;
    ed->tileset_rows = surf->h / TILE_SIZE;
    int strips = ed->tileset_cols / PALETTE_COLS;
    ed->tile_count = ed->tileset_cols * ed->tileset_rows;

    ed->tiles = malloc(ed->tile_count * sizeof(SDL_Texture*));
    ed->tile_surfs = malloc(ed->tile_count * sizeof(SDL_Surface*));
    ed->tile_modified = calloc(ed->tile_count, sizeof(bool));
    ed->custom_tiles = calloc(ed->tile_count, sizeof(SDL_Texture*));
    ed->custom_surfs = calloc(ed->tile_count, sizeof(SDL_Surface*));

    // Нарезка оригинального тайлсета
    int idx = 0;
    for (int strip = 0; strip < strips; strip++) {
        int sc = strip * PALETTE_COLS, ec = sc + PALETTE_COLS;
        for (int r = 0; r < ed->tileset_rows; r++) {
            for (int c = sc; c < ec; c++) {
                SDL_Rect src = { c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE };
                SDL_Surface *ts = SDL_CreateRGBSurfaceWithFormat(0, TILE_SIZE, TILE_SIZE, 32, SDL_PIXELFORMAT_RGBA8888);
                SDL_BlitSurface(surf, &src, ts, NULL);
                ed->tile_surfs[idx] = ts;
                ed->tiles[idx] = SDL_CreateTextureFromSurface(ed->renderer, ts);
                idx++;
            }
        }
    }
    SDL_FreeSurface(surf);

    // ─── Попытка загрузить существующую анимацию ───
    char base[256];
    const char *slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\');
    if (slash) {
        size_t len = strlen(slash + 1);
        if (len >= sizeof(base)) len = sizeof(base) - 1;
        memcpy(base, slash + 1, len);
        base[len] = '\0';
    } else {
        size_t len = strlen(path);
        if (len >= sizeof(base)) len = sizeof(base) - 1;
        memcpy(base, path, len);
        base[len] = '\0';
    }
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    char anim_png[768], anim_json[768];
    snprintf(anim_png, sizeof(anim_png), "assets/tilesets/Animation_tiles/%s_animation.png", base);
    snprintf(anim_json, sizeof(anim_json), "assets/tilesets/Animation_tiles/%s_animation.json", base);

    if (file_exists(anim_png) && file_exists(anim_json)) {
        SDL_Surface *anim_surf = IMG_Load(anim_png);
        if (anim_surf) {
            // Читаем JSON список индексов
            FILE *fj = fopen(anim_json, "r");
            if (fj) {
                fseek(fj, 0, SEEK_END);
                long len = ftell(fj);
                fseek(fj, 0, SEEK_SET);
                char *jsondata = malloc(len + 1);
                fread(jsondata, 1, len, fj);
                jsondata[len] = '\0';
                fclose(fj);

                // Простейший парсинг JSON-массива чисел
                // Формат: [123, 456, ...]
                char *p = jsondata;
                while (*p) {
                    if (*p >= '0' && *p <= '9') {
                        int num = atoi(p);
                        if (num >= 0 && num < ed->tile_count) {
                            // Вычисляем координаты тайла в anim_surf
                            int strip_num = num / (PALETTE_COLS * ed->tileset_rows);
                            int local = num % (PALETTE_COLS * ed->tileset_rows);
                            int col = strip_num * PALETTE_COLS + (local % PALETTE_COLS);
                            int row = local / PALETTE_COLS;
                            SDL_Rect src_rect = { col * TILE_SIZE, row * TILE_SIZE, TILE_SIZE, TILE_SIZE };
                            SDL_Surface *custom = SDL_CreateRGBSurfaceWithFormat(0, TILE_SIZE, TILE_SIZE, 32, SDL_PIXELFORMAT_RGBA8888);
                            SDL_BlitSurface(anim_surf, &src_rect, custom, NULL);
                            ed->custom_surfs[num] = custom;
                            ed->custom_tiles[num] = SDL_CreateTextureFromSurface(ed->renderer, custom);
                            ed->tile_modified[num] = true;
                        }
                        while (*p >= '0' && *p <= '9') p++;
                    } else p++;
                }
                free(jsondata);
            }
            SDL_FreeSurface(anim_surf);
        }
    }

    ed->tileset_loaded = true;
    ed->palette_scroll = 0;
    ed->selected_tile = 0;
    return 1;
}

// Существование файла (вспомогательная)
static bool file_exists(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

// ─── Диалог открытия файла ────────────────────
bool open_file_dialog(char *out, size_t len, const char *dir) {
    OPENFILENAMEA ofn = {0};
    char file[260] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.lpstrFilter = "PNG Files\0*.png\0All Files\0*.*\0";
    ofn.lpstrInitialDir = dir;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) {
        safe_strcpy(out, len, file);
        return true;
    }
    return false;
}

// ─── Замена одного тайла (режим B, вызывается из центра) ──
void replace_tile_from_file(Editor *ed, int index) {
    if (!ed->tileset_loaded || index < 0 || index >= ed->tile_count) return;

    char path[256];
    if (!open_file_dialog(path, sizeof(path), NULL)) return;

    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        MessageBoxA(NULL, "Failed to load image.", "Error", MB_ICONERROR);
        return;
    }
    if (surf->w != 48 || surf->h != 48) {
        SDL_FreeSurface(surf);
        MessageBoxA(NULL, "Image must be exactly 48x48 pixels.", "Wrong Size", MB_ICONWARNING);
        return;
    }

    SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(0, TILE_SIZE, TILE_SIZE, 32, SDL_PIXELFORMAT_RGBA8888);
    SDL_BlitScaled(surf, NULL, scaled, NULL);
    SDL_FreeSurface(surf);

    // Заменяем кастомный тайл
    if (ed->custom_surfs[index]) SDL_FreeSurface(ed->custom_surfs[index]);
    if (ed->custom_tiles[index]) SDL_DestroyTexture(ed->custom_tiles[index]);
    ed->custom_surfs[index] = scaled;
    ed->custom_tiles[index] = SDL_CreateTextureFromSurface(ed->renderer, scaled);
    ed->tile_modified[index] = true;
}

// ─── Сохранение тайлсета + JSON ───────────────
void save_tileset(Editor *ed) {
    if (!ed->tileset_loaded) {
        MessageBoxA(NULL, "No tileset loaded.", "Save", MB_OK);
        return;
    }
    if (ed->tileset_fullpath[0] == '\0') {
        MessageBoxA(NULL, "Tileset path is empty.", "Save", MB_OK);
        return;
    }

    char base[256];
    const char *slash = strrchr(ed->tileset_fullpath, '/');
    if (!slash) slash = strrchr(ed->tileset_fullpath, '\\');
    if (slash) {
        size_t len = strlen(slash + 1);
        if (len >= sizeof(base)) len = sizeof(base) - 1;
        memcpy(base, slash + 1, len);
        base[len] = '\0';
    } else {
        size_t len = strlen(ed->tileset_fullpath);
        if (len >= sizeof(base)) len = sizeof(base) - 1;
        memcpy(base, ed->tileset_fullpath, len);
        base[len] = '\0';
    }
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    const char *target_dir = "../assets/tilesets/Animation_tiles";
    CreateDirectoryA(target_dir, NULL);

    char png_path[768], json_path[768];
    snprintf(png_path, sizeof(png_path), "%s/%s_animation.png", target_dir, base);
    snprintf(json_path, sizeof(json_path), "%s/%s_animation.json", target_dir, base);

    // Собираем изображение (только кастомные тайлы, остальное прозрачно)
    int total_w = ed->tileset_cols * TILE_SIZE;
    int total_h = ed->tileset_rows * TILE_SIZE;
    SDL_Surface *result = SDL_CreateRGBSurfaceWithFormat(0, total_w, total_h, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!result) return;
    SDL_FillRect(result, NULL, SDL_MapRGBA(result->format, 0,0,0,0));

    int idx = 0;
    for (int strip = 0; strip < ed->tileset_cols / PALETTE_COLS; strip++) {
        for (int r = 0; r < ed->tileset_rows; r++) {
            for (int c = 0; c < PALETTE_COLS; c++) {
                if (ed->tile_modified[idx] && ed->custom_surfs[idx]) {
                    int global_x = strip * PALETTE_COLS + c;
                    int global_y = r;
                    SDL_Rect dest = { global_x * TILE_SIZE, global_y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
                    SDL_BlitSurface(ed->custom_surfs[idx], NULL, result, &dest);
                }
                idx++;
            }
        }
    }

    if (IMG_SavePNG(result, png_path) != 0) {
        SDL_FreeSurface(result);
        MessageBoxA(NULL, "Failed to save PNG.", "Error", MB_ICONERROR);
        return;
    }
    SDL_FreeSurface(result);

    // JSON
    FILE *fjson = fopen(json_path, "w");
    if (!fjson) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Could not write JSON:\n%s", json_path);
        MessageBoxA(NULL, buf, "Warning", MB_OK);
        return;
    }
    fprintf(fjson, "[");
    bool first = true;
    for (int i = 0; i < ed->tile_count; i++) {
        if (ed->tile_modified[i]) {
            if (!first) fprintf(fjson, ", ");
            fprintf(fjson, "%d", i);
            first = false;
        }
    }
    fprintf(fjson, "]\n");
    fclose(fjson);

    ed->save_anim_active = true;
    ed->save_anim_timer = SDL_GetTicks();

    char msg[2048];
    snprintf(msg, sizeof(msg), "Saved:\n%s\n%s", png_path, json_path);
    MessageBoxA(NULL, msg, "Save Successful", MB_OK);
}

// ─── Отрисовка панелей ────────────────────────
void render_left_panel(Editor *ed) {
    SDL_Rect bg = {0,0, LEFT_PANEL_W, WINDOW_H};
    SDL_SetRenderDrawColor(ed->renderer, 50,50,50,255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "TILESET", LEFT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    SDL_Rect load_btn = {10,35, LEFT_PANEL_W-20, 26};
    SDL_SetRenderDrawColor(ed->renderer, 80,80,80,255);
    SDL_RenderFillRect(ed->renderer, &load_btn);
    draw_text_centered(ed->renderer, ed->font, "Load Tileset", load_btn.x+load_btn.w/2, load_btn.y+load_btn.h/2, (SDL_Color){255,255,255,255});

    SDL_Rect btnA = {10,70, (LEFT_PANEL_W-30)/2, 24}, btnB = {btnA.x+btnA.w+10,70, btnA.w,24};
    if (ed->mode == MODE_A) {
        SDL_SetRenderDrawColor(ed->renderer, 100,160,100,255);
        SDL_RenderFillRect(ed->renderer, &btnA);
        SDL_SetRenderDrawColor(ed->renderer, 140,140,140,255);
        SDL_RenderFillRect(ed->renderer, &btnB);
    } else {
        SDL_SetRenderDrawColor(ed->renderer, 140,140,140,255);
        SDL_RenderFillRect(ed->renderer, &btnA);
        SDL_SetRenderDrawColor(ed->renderer, 100,160,100,255);
        SDL_RenderFillRect(ed->renderer, &btnB);
    }
    draw_text_centered(ed->renderer, ed->font, "A", btnA.x+btnA.w/2, btnA.y+btnA.h/2, (SDL_Color){255,255,255,255});
    draw_text_centered(ed->renderer, ed->font, "B", btnB.x+btnB.w/2, btnB.y+btnB.h/2, (SDL_Color){255,255,255,255});

    SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
    SDL_RenderDrawLine(ed->renderer, 10,105, LEFT_PANEL_W-10,105);

    if (!ed->tileset_loaded) {
        draw_text_centered(ed->renderer, ed->font, "No tileset", LEFT_PANEL_W/2, 250, (SDL_Color){255,100,100,255});
        return;
    }

    int visible = (WINDOW_H - PALETTE_START_Y) / (PALETTE_TILE_SIZE + 2);
    int total = (ed->tile_count + PALETTE_COLS - 1) / PALETTE_COLS;

    for (int row = ed->palette_scroll; row < ed->palette_scroll + visible && row < total; row++) {
        for (int col = 0; col < PALETTE_COLS; col++) {
            int idx = row * PALETTE_COLS + col;
            if (idx >= ed->tile_count) break;
            SDL_Rect dst = { PALETTE_START_X + col*(PALETTE_TILE_SIZE+2),
                             PALETTE_START_Y + (row-ed->palette_scroll)*(PALETTE_TILE_SIZE+2),
                             PALETTE_TILE_SIZE, PALETTE_TILE_SIZE };

            // Показываем оригинальный тайл (без прозрачности)
            SDL_Texture *tex = ed->tiles[idx];
            SDL_RenderCopy(ed->renderer, tex, NULL, &dst);

            // Рамка выбранного тайла
            SDL_SetRenderDrawColor(ed->renderer, 70,70,70,255);
            SDL_Rect frame = { dst.x-1, dst.y-1, dst.w+2, dst.h+2 };
            SDL_RenderDrawRect(ed->renderer, &frame);
            if (idx == ed->selected_tile) {
                SDL_SetRenderDrawColor(ed->renderer, 255,255,0,255);
                for (int t=0; t<2; t++) {
                    SDL_Rect r = { frame.x-t, frame.y-t, frame.w+2*t, frame.h+2*t };
                    SDL_RenderDrawRect(ed->renderer, &r);
                }
            }
            // Если есть замена, можно показать маленькую метку (например, звёздочку)
            if (ed->tile_modified[idx]) {
                SDL_SetRenderDrawColor(ed->renderer, 0, 255, 0, 255);
                SDL_Rect dot = { dst.x + dst.w - 6, dst.y + 2, 4, 4 };
                SDL_RenderFillRect(ed->renderer, &dot);
            }
        }
    }
}

void render_center(Editor *ed) {
    SDL_Rect bg = {CENTER_X, CENTER_Y, CENTER_W, CENTER_H};
    SDL_SetRenderDrawColor(ed->renderer, 40,40,40,255);
    SDL_RenderFillRect(ed->renderer, &bg);
    if (!ed->tileset_loaded) {
        draw_text_centered(ed->renderer, ed->font, "Load tileset first", CENTER_X+CENTER_W/2, CENTER_Y+CENTER_H/2, (SDL_Color){200,200,200,255});
        return;
    }

    const char *fname = strrchr(ed->tileset_fullpath, '/');
    if (!fname) fname = strrchr(ed->tileset_fullpath, '\\');
    if (!fname) fname = ed->tileset_fullpath; else fname++;
    draw_text_centered(ed->renderer, ed->font, fname, CENTER_X+CENTER_W/2, CENTER_Y+20, (SDL_Color){200,200,200,255});

    if (ed->selected_tile >= 0 && ed->selected_tile < ed->tile_count) {
        int w = TILE_SIZE * 2, h = TILE_SIZE * 2;
        SDL_Rect dst = { CENTER_X + (CENTER_W - w)/2, CENTER_Y + (CENTER_H - h)/2, w, h };

        // Определяем, какую текстуру показывать (анимация в режиме B)
        SDL_Texture *tex = NULL;
        if (ed->mode == MODE_B && ed->tile_modified[ed->selected_tile]) {
            // Анимация: переключаемся каждые anim_delay сек
            Uint32 now = SDL_GetTicks();
            if (now - ed->anim_timer >= (Uint32)(ed->anim_delay * 1000)) {
                ed->show_anim = !ed->show_anim;
                ed->anim_timer = now;
            }
            tex = ed->show_anim ? ed->custom_tiles[ed->selected_tile] : ed->tiles[ed->selected_tile];
        } else {
            tex = ed->tiles[ed->selected_tile];
        }

        SDL_RenderCopy(ed->renderer, tex, NULL, &dst);

        if (ed->mode == MODE_B) {
            draw_text_centered(ed->renderer, ed->font, "Click image to replace (48x48)", CENTER_X+CENTER_W/2, dst.y+dst.h+15, (SDL_Color){200,200,200,255});
        }
    }

    char mode_text[32];
    snprintf(mode_text, sizeof(mode_text), "Mode: %s", ed->mode == MODE_A ? "A - View" : "B - Animation");
    draw_text_centered(ed->renderer, ed->font, mode_text, CENTER_X+CENTER_W/2, CENTER_Y+CENTER_H-10, (SDL_Color){180,180,180,255});
}

void render_right_panel(Editor *ed) {
    int px = WINDOW_W - RIGHT_PANEL_W;
    SDL_Rect bg = {px,0,RIGHT_PANEL_W,WINDOW_H};
    SDL_SetRenderDrawColor(ed->renderer, 60,60,60,255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "SAVE", px+RIGHT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    if (ed->mode == MODE_B) {
        SDL_Rect save_btn = {px+10, 35, RIGHT_PANEL_W-20, 30};

        Uint32 now = SDL_GetTicks();
        bool flash = ed->save_anim_active && (now - ed->save_anim_timer < 200);
        SDL_Color btn_color = flash ? (SDL_Color){180,180,80,255} : (SDL_Color){80,80,180,255};
        SDL_SetRenderDrawColor(ed->renderer, btn_color.r, btn_color.g, btn_color.b, 255);
        SDL_RenderFillRect(ed->renderer, &save_btn);
        draw_text_centered(ed->renderer, ed->font, "Save Tileset", save_btn.x+save_btn.w/2, save_btn.y+save_btn.h/2, (SDL_Color){255,255,255,255});

        if (flash && now - ed->save_anim_timer >= 200) ed->save_anim_active = false;
    } else {
        draw_text_centered(ed->renderer, ed->font, "Switch to B mode\nto edit and save", px+RIGHT_PANEL_W/2, 100, (SDL_Color){200,200,200,255});
    }
}

// ─── Обработка событий ────────────────────────
void handle_input(Editor *ed, bool *run) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *run = false; return; }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_a: ed->mode = MODE_A; break;
                case SDLK_b: ed->mode = MODE_B; break;
                default: break;
            }
        }
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my; get_logical_mouse(ed, &mx, &my);
            if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y && ed->tileset_loaded) {
                ed->palette_scroll -= e.wheel.y;
                int max = ((ed->tile_count+PALETTE_COLS-1)/PALETTE_COLS) - ((WINDOW_H-PALETTE_START_Y)/(PALETTE_TILE_SIZE+2));
                if (ed->palette_scroll < 0) ed->palette_scroll = 0;
                if (max > 0 && ed->palette_scroll > max) ed->palette_scroll = max;
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx, my; get_logical_mouse(ed, &mx, &my);

            if (mx < LEFT_PANEL_W) {
                if (my >= 35 && my < 61) {
                    char path[256];
                    if (open_file_dialog(path, sizeof(path), "assets\\tilesets"))
                        load_tileset(ed, path);
                } else if (my >= 70 && my < 94) {
                    int half = (LEFT_PANEL_W-30)/2;
                    if (mx >= 10 && mx < 10+half) ed->mode = MODE_A;
                    else if (mx >= 10+half+10 && mx < 10+half+10+half) ed->mode = MODE_B;
                } else if (ed->tileset_loaded && my >= PALETTE_START_Y) {
                    int rx = mx - PALETTE_START_X, ry = my - PALETTE_START_Y;
                    int step = PALETTE_TILE_SIZE+2;
                    if (rx>=0 && ry>=0) {
                        int col = rx/step, row = ry/step + ed->palette_scroll;
                        if (col < PALETTE_COLS && (rx%step) < PALETTE_TILE_SIZE) {
                            int idx = row*PALETTE_COLS + col;
                            if (idx>=0 && idx<ed->tile_count) {
                                ed->selected_tile = idx;
                                // В режиме B НЕ заменяем при клике по палитре
                            }
                        }
                    }
                }
            }
            else if (mx >= CENTER_X && mx < CENTER_X+CENTER_W && my >= CENTER_Y && my < CENTER_Y+CENTER_H) {
                if (ed->mode == MODE_B && ed->tileset_loaded && ed->selected_tile >= 0) {
                    int w = TILE_SIZE*2, h = TILE_SIZE*2;
                    int ix = CENTER_X+(CENTER_W-w)/2, iy = CENTER_Y+(CENTER_H-h)/2;
                    if (mx>=ix && mx<ix+w && my>=iy && my<iy+h) {
                        // Замена только здесь
                        replace_tile_from_file(ed, ed->selected_tile);
                    }
                }
            }
            else if (mx >= WINDOW_W-RIGHT_PANEL_W) {
                if (ed->mode == MODE_B) {
                    int rx = mx-(WINDOW_W-RIGHT_PANEL_W), ry = my;
                    if (ry >= 35 && ry < 65 && rx >= 10 && rx <= RIGHT_PANEL_W-10) {
                        save_tileset(ed);
                    }
                }
            }
        }
    }
}

// ─── main ──────────────────────────────────────
int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    Editor ed; editor_init(&ed);
    ed.window = SDL_CreateWindow("Tileset Animation Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    ed.renderer = SDL_CreateRenderer(ed.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ed.renderer, WINDOW_W, WINDOW_H);

    ed.font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", FONT_SIZE);
    if (!ed.font) ed.font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", FONT_SIZE);
    if (!ed.font) return 1;

    bool running = true;
    while (running) {
        handle_input(&ed, &running);
        SDL_SetRenderDrawColor(ed.renderer, 30,30,30,255);
        SDL_RenderClear(ed.renderer);
        render_left_panel(&ed);
        render_center(&ed);
        render_right_panel(&ed);
        SDL_RenderPresent(ed.renderer);
        SDL_Delay(16);
    }

    free_tileset(&ed);
    TTF_CloseFont(ed.font);
    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}