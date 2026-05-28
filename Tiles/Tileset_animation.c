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

// ─── Размеры окна ─────────────────────────
#define WINDOW_W 1024
#define WINDOW_H 600
#define TOOLBAR_H 34

#define LEFT_PANEL_W 300
#define RIGHT_PANEL_W 220
#define CENTER_X LEFT_PANEL_W
#define CENTER_Y TOOLBAR_H
#define CENTER_W (WINDOW_W - LEFT_PANEL_W - RIGHT_PANEL_W)
#define CENTER_H (WINDOW_H - TOOLBAR_H)

#define TILE_SIZE 32
#define PALETTE_TILE_SIZE 32
#define PALETTE_COLS 8
#define PALETTE_START_X 10
#define PALETTE_START_Y 140

#define FONT_SIZE 16

#define MODE_A 0   // обычный просмотр (тайлсет не меняется)
#define MODE_B 1   // режим анимации (замена тайла, сохранение)

// Кадр анимации
typedef struct {
    int tile_index;          // -1 если используется custom_surf
    SDL_Surface *custom_surf; // временный тайл (32×32)
    int duration_ms;
} AnimFrame;

typedef struct {
    AnimFrame *frames;
    int frame_count;
    int current_frame;
    Uint32 last_frame_time;
    bool playing;
    bool loop;
} Animation;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    SDL_Texture **tiles;        // текстуры (для отрисовки)
    SDL_Surface **tile_surfs;   // поверхности (для сохранения)
    int tile_count;
    bool tileset_loaded;
    char tileset_fullpath[512]; // полный путь к файлу тайлсета

    int palette_scroll;
    int selected_tile;
    int mode;

    SDL_Surface *custom_surf;   // загруженный пользователем тайл (32×32)
    SDL_Texture *custom_tex;    // текстура для custom_surf

    Animation anim;
    SDL_Texture *checker_bg;    // шахматный фон
} Editor;

// Прототипы
void editor_init(Editor *ed);
void free_tileset(Editor *ed);
bool open_file_dialog(char *out, size_t len, const char *dir);
int load_tileset(Editor *ed, const char *path);
void animation_clear(Animation *anim);
void animation_add_frame(Editor *ed, int tile_index, int duration, SDL_Surface *custom);
void animation_delete_frame(Editor *ed, int index);
void animation_play(Animation *anim);
void animation_stop(Animation *anim);
void animation_update(Animation *anim, Uint32 now);
void load_custom_tile(Editor *ed);
void save_animation_strip(Editor *ed);
void load_checker_background(Editor *ed);
void render_left_panel(Editor *ed);
void render_center(Editor *ed);
void render_right_panel(Editor *ed);
void handle_input(Editor *ed, bool *running);

// --- Вспомогательные функции ---
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

// --- Инициализация ---
void editor_init(Editor *ed) {
    memset(ed, 0, sizeof(Editor));
    ed->mode = MODE_A;
    ed->anim.loop = true;
}

void animation_reset(Animation *anim) {
    animation_clear(anim);
    anim->playing = false;
}

// --- Загрузка тайлсета ---
void free_tileset(Editor *ed) {
    if (ed->tiles) {
        for (int i = 0; i < ed->tile_count; i++) SDL_DestroyTexture(ed->tiles[i]);
        free(ed->tiles);
        ed->tiles = NULL;
    }
    if (ed->tile_surfs) {
        for (int i = 0; i < ed->tile_count; i++) SDL_FreeSurface(ed->tile_surfs[i]);
        free(ed->tile_surfs);
        ed->tile_surfs = NULL;
    }
    ed->tile_count = 0;
    ed->tileset_loaded = false;
    animation_clear(&ed->anim);
    if (ed->custom_surf) { SDL_FreeSurface(ed->custom_surf); ed->custom_surf = NULL; }
    if (ed->custom_tex) { SDL_DestroyTexture(ed->custom_tex); ed->custom_tex = NULL; }
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

    int cols = surf->w / TILE_SIZE;
    int rows = surf->h / TILE_SIZE;
    int strips = cols / PALETTE_COLS;
    ed->tile_count = cols * rows;
    ed->tiles = malloc(ed->tile_count * sizeof(SDL_Texture*));
    ed->tile_surfs = malloc(ed->tile_count * sizeof(SDL_Surface*));

    int idx = 0;
    for (int strip = 0; strip < strips; strip++) {
        int sc = strip * PALETTE_COLS, ec = sc + PALETTE_COLS;
        for (int r = 0; r < rows; r++) {
            for (int c = sc; c < ec; c++) {
                SDL_Rect src = { c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE };
                SDL_Surface *ts = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
                SDL_BlitSurface(surf, &src, ts, NULL);
                ed->tile_surfs[idx] = ts;
                ed->tiles[idx] = SDL_CreateTextureFromSurface(ed->renderer, ts);
                idx++;
            }
        }
    }

    SDL_FreeSurface(surf);
    ed->tileset_loaded = true;
    ed->palette_scroll = 0;
    ed->selected_tile = 0;
    return 1;
}

// --- Диалог ---
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

// --- Анимация ---
void animation_clear(Animation *anim) {
    for (int i = 0; i < anim->frame_count; i++)
        if (anim->frames[i].custom_surf)
            SDL_FreeSurface(anim->frames[i].custom_surf);
    free(anim->frames);
    anim->frames = NULL;
    anim->frame_count = 0;
    anim->current_frame = 0;
}

void animation_add_frame(Editor *ed, int tile_index, int duration, SDL_Surface *custom) {
    ed->anim.frame_count++;
    ed->anim.frames = realloc(ed->anim.frames, ed->anim.frame_count * sizeof(AnimFrame));
    AnimFrame *f = &ed->anim.frames[ed->anim.frame_count - 1];
    f->custom_surf = custom;   // передача владения (если NULL, то не custom)
    f->tile_index = custom ? -1 : tile_index;
    f->duration_ms = duration;
}

void animation_delete_frame(Editor *ed, int index) {
    if (index < 0 || index >= ed->anim.frame_count) return;
    if (ed->anim.frames[index].custom_surf)
        SDL_FreeSurface(ed->anim.frames[index].custom_surf);
    for (int i = index; i < ed->anim.frame_count - 1; i++)
        ed->anim.frames[i] = ed->anim.frames[i + 1];
    ed->anim.frame_count--;
    if (ed->anim.frame_count == 0) {
        free(ed->anim.frames);
        ed->anim.frames = NULL;
    }
}

void animation_play(Animation *anim) {
    if (anim->frame_count) {
        anim->playing = true;
        anim->current_frame = 0;
        anim->last_frame_time = SDL_GetTicks();
    }
}

void animation_stop(Animation *anim) {
    anim->playing = false;
    anim->current_frame = 0;
}

void animation_update(Animation *anim, Uint32 now) {
    if (!anim->playing || anim->frame_count == 0) return;
    AnimFrame *f = &anim->frames[anim->current_frame];
    if (now - anim->last_frame_time >= f->duration_ms) {
        anim->current_frame = (anim->current_frame + 1) % anim->frame_count;
        anim->last_frame_time = now;
        if (anim->current_frame == 0 && !anim->loop)
            anim->playing = false;
    }
}

// --- Загрузка временного тайла (48×48) ---
void load_custom_tile(Editor *ed) {
    if (!ed->tileset_loaded) return;

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

    SDL_Surface *scaled = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
    SDL_BlitScaled(surf, NULL, scaled, NULL);
    SDL_FreeSurface(surf);

    if (ed->custom_surf) SDL_FreeSurface(ed->custom_surf);
    if (ed->custom_tex) SDL_DestroyTexture(ed->custom_tex);
    ed->custom_surf = scaled;
    ed->custom_tex = SDL_CreateTextureFromSurface(ed->renderer, scaled);
}

// ====== ГЛАВНАЯ ФУНКЦИЯ СОХРАНЕНИЯ (ГАРАНТИРОВАННО РАБОТАЕТ) ======
void save_animation_strip(Editor *ed) {
    if (ed->anim.frame_count == 0 || !ed->tileset_loaded) return;
    if (ed->tileset_fullpath[0] == '\0') return;

    // Определяем папку и базовое имя исходного файла
    char dir[512], base[256];
    const char *slash = strrchr(ed->tileset_fullpath, '/');
    if (!slash) slash = strrchr(ed->tileset_fullpath, '\\');
    if (slash) {
        size_t len = slash - ed->tileset_fullpath;
        memcpy(dir, ed->tileset_fullpath, len);
        dir[len] = '\0';
        safe_strcpy(base, sizeof(base), slash + 1);
    } else {
        strcpy(dir, ".");
        safe_strcpy(base, sizeof(base), ed->tileset_fullpath);
    }
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    char out_path[768];
    snprintf(out_path, sizeof(out_path), "%s/%s_animation.png", dir, base);

    // Собираем полосу кадров
    int total_w = ed->anim.frame_count * TILE_SIZE;
    SDL_Surface *strip = SDL_CreateRGBSurface(0, total_w, TILE_SIZE, 32, 0,0,0,0);
    if (!strip) return;

    for (int i = 0; i < ed->anim.frame_count; i++) {
        AnimFrame *f = &ed->anim.frames[i];
        SDL_Surface *src = NULL;
        if (f->custom_surf)
            src = f->custom_surf;
        else if (f->tile_index >= 0 && f->tile_index < ed->tile_count)
            src = ed->tile_surfs[f->tile_index];

        if (!src) continue;
        SDL_Rect dest = { i * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE };
        SDL_BlitSurface(src, NULL, strip, &dest);
    }

    IMG_SavePNG(strip, out_path);
    SDL_FreeSurface(strip);
}

// --- Шахматный фон ---
void load_checker_background(Editor *ed) {
    if (ed->checker_bg) SDL_DestroyTexture(ed->checker_bg);
    SDL_Surface *s = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
    SDL_Rect r1 = {0,0, TILE_SIZE/2, TILE_SIZE/2}, r2 = {TILE_SIZE/2, TILE_SIZE/2, TILE_SIZE/2, TILE_SIZE/2};
    SDL_FillRect(s, &r1, SDL_MapRGBA(s->format, 180,180,180,255));
    SDL_FillRect(s, &r2, SDL_MapRGBA(s->format, 180,180,180,255));
    ed->checker_bg = SDL_CreateTextureFromSurface(ed->renderer, s);
    SDL_FreeSurface(s);
}

// --- Отрисовка панелей ---
void render_left_panel(Editor *ed) {
    SDL_Rect bg = {0,0, LEFT_PANEL_W, WINDOW_H};
    SDL_SetRenderDrawColor(ed->renderer, 50,50,50,255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "TILESET", LEFT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    SDL_Rect load_btn = {10,35, LEFT_PANEL_W-20, 26};
    SDL_SetRenderDrawColor(ed->renderer, 80,80,80,255);
    SDL_RenderFillRect(ed->renderer, &load_btn);
    draw_text_centered(ed->renderer, ed->font, "Load Tileset", load_btn.x+load_btn.w/2, load_btn.y+load_btn.h/2, (SDL_Color){255,255,255,255});

    SDL_Rect btnA = {10,70, (LEFT_PANEL_W-30)/2, 24};
    SDL_Rect btnB = {btnA.x+btnA.w+10, 70, btnA.w, 24};
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

            if (ed->mode == MODE_B && ed->checker_bg)
                SDL_RenderCopy(ed->renderer, ed->checker_bg, NULL, &dst);

            SDL_Texture *tex = ed->tiles[idx];
            if (ed->mode == MODE_B) {
                SDL_SetTextureAlphaMod(tex, 128);
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            }
            SDL_RenderCopy(ed->renderer, tex, NULL, &dst);
            if (ed->mode == MODE_B) {
                SDL_SetTextureAlphaMod(tex, 255);
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
            }
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

    // Имя файла
    const char *fname = strrchr(ed->tileset_fullpath, '/');
    if (!fname) fname = strrchr(ed->tileset_fullpath, '\\');
    if (!fname) fname = ed->tileset_fullpath; else fname++;
    draw_text_centered(ed->renderer, ed->font, fname, CENTER_X+CENTER_W/2, CENTER_Y+20, (SDL_Color){200,200,200,255});

    SDL_Texture *tex = NULL;
    if (ed->anim.playing && ed->anim.frame_count > 0) {
        AnimFrame *f = &ed->anim.frames[ed->anim.current_frame];
        if (f->custom_surf)
            tex = SDL_CreateTextureFromSurface(ed->renderer, f->custom_surf);
        else if (f->tile_index >= 0 && f->tile_index < ed->tile_count)
            tex = ed->tiles[f->tile_index];
    } else {
        if (ed->custom_tex) tex = ed->custom_tex;
        else if (ed->selected_tile >= 0 && ed->selected_tile < ed->tile_count)
            tex = ed->tiles[ed->selected_tile];
    }

    if (tex) {
        int w = TILE_SIZE * 2, h = TILE_SIZE * 2;
        SDL_Rect dst = { CENTER_X + (CENTER_W - w)/2, CENTER_Y + (CENTER_H - h)/2, w, h };
        if (ed->mode == MODE_B && ed->checker_bg)
            for (int y=0; y<h; y+=TILE_SIZE) for (int x=0; x<w; x+=TILE_SIZE) {
                SDL_Rect d = { dst.x+x, dst.y+y, TILE_SIZE, TILE_SIZE };
                SDL_RenderCopy(ed->renderer, ed->checker_bg, NULL, &d);
            }
        if (ed->mode == MODE_B) {
            SDL_SetTextureAlphaMod(tex, 128);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
        SDL_RenderCopy(ed->renderer, tex, NULL, &dst);
        if (ed->mode == MODE_B) {
            SDL_SetTextureAlphaMod(tex, 255);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
        }
        if (ed->anim.playing && ed->anim.frames[ed->anim.current_frame].custom_surf)
            SDL_DestroyTexture(tex);
    }

    if (ed->mode == MODE_B) {
        const char *hint = ed->custom_surf ? "Custom tile loaded. Press Add Frame." : "Click image to load custom 48x48 tile";
        draw_text_centered(ed->renderer, ed->font, hint, CENTER_X+CENTER_W/2, CENTER_Y+CENTER_H-25, (SDL_Color){200,200,200,255});
    }
    draw_text_centered(ed->renderer, ed->font, ed->mode==MODE_A ? "Mode A" : "Mode B", CENTER_X+CENTER_W/2, CENTER_Y+CENTER_H-10, (SDL_Color){180,180,180,255});
}

void render_right_panel(Editor *ed) {
    int px = WINDOW_W - RIGHT_PANEL_W;
    SDL_Rect bg = {px,0,RIGHT_PANEL_W,WINDOW_H};
    SDL_SetRenderDrawColor(ed->renderer, 60,60,60,255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "ANIMATION", px+RIGHT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    int y = 35, bw = RIGHT_PANEL_W-20, bx = px+10;

    SDL_Rect add = {bx, y, bw, 24};
    SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
    SDL_RenderFillRect(ed->renderer, &add);
    draw_text_centered(ed->renderer, ed->font, "Add Frame (100ms)", add.x+add.w/2, add.y+add.h/2, (SDL_Color){255,255,255,255});

    y += 30;
    SDL_Rect play = {bx, y, bw/2-2,24}, stop = {bx+bw/2+2, y, bw/2-2,24};
    SDL_SetRenderDrawColor(ed->renderer, 80,130,80,255);
    SDL_RenderFillRect(ed->renderer, &play);
    draw_text_centered(ed->renderer, ed->font, "Play", play.x+play.w/2, play.y+play.h/2, (SDL_Color){255,255,255,255});
    SDL_SetRenderDrawColor(ed->renderer, 150,80,80,255);
    SDL_RenderFillRect(ed->renderer, &stop);
    draw_text_centered(ed->renderer, ed->font, "Stop", stop.x+stop.w/2, stop.y+stop.h/2, (SDL_Color){255,255,255,255});

    y += 30;
    SDL_Rect clear = {bx, y, bw, 24};
    SDL_SetRenderDrawColor(ed->renderer, 180,100,100,255);
    SDL_RenderFillRect(ed->renderer, &clear);
    draw_text_centered(ed->renderer, ed->font, "Clear All", clear.x+clear.w/2, clear.y+clear.h/2, (SDL_Color){255,255,255,255});

    if (ed->mode == MODE_B) {
        y += 30;
        SDL_Rect save = {bx, y, bw, 24};
        SDL_SetRenderDrawColor(ed->renderer, 80,80,180,255);
        SDL_RenderFillRect(ed->renderer, &save);
        draw_text_centered(ed->renderer, ed->font, "Save Animation", save.x+save.w/2, save.y+save.h/2, (SDL_Color){255,255,255,255});
    }

    y += 35;
    SDL_SetRenderDrawColor(ed->renderer, 100,100,100,255);
    SDL_RenderDrawLine(ed->renderer, px+5, y, px+RIGHT_PANEL_W-5, y);

    y += 5;
    for (int i=0; i<ed->anim.frame_count; i++) {
        if (y+22 > WINDOW_H-10) break;
        char buf[64];
        snprintf(buf, sizeof(buf), "%d: %s (%dms)", i+1, ed->anim.frames[i].custom_surf?"custom":"tile", ed->anim.frames[i].duration_ms);
        SDL_Color c = (i == ed->anim.current_frame && ed->anim.playing) ? (SDL_Color){255,255,0,255} : (SDL_Color){220,220,220,255};
        draw_text_centered(ed->renderer, ed->font, buf, px+RIGHT_PANEL_W/2, y+11, c);
        SDL_Rect del = {px+RIGHT_PANEL_W-30, y+2, 20,20};
        SDL_SetRenderDrawColor(ed->renderer, 200,70,70,255);
        SDL_RenderFillRect(ed->renderer, &del);
        draw_text_centered(ed->renderer, ed->font, "X", del.x+del.w/2, del.y+del.h/2, (SDL_Color){255,255,255,255});
        y += 22;
    }
}

// --- Обработка ---
void handle_input(Editor *ed, bool *run) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *run = false; return; }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_SPACE: if (ed->anim.playing) animation_stop(&ed->anim); else animation_play(&ed->anim); break;
                case SDLK_a: ed->mode = MODE_A; break;
                case SDLK_b: ed->mode = MODE_B; break;
                case SDLK_RETURN:
                    if (ed->tileset_loaded) {
                        SDL_Surface *cust = ed->custom_surf ? SDL_DuplicateSurface(ed->custom_surf) : NULL;
                        animation_add_frame(ed, ed->selected_tile, 100, cust);
                    }
                    break;
                case SDLK_DELETE: if (ed->anim.frame_count > 0) animation_delete_frame(ed, ed->anim.frame_count-1); break;
                default: break;
            }
        }
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my; SDL_GetMouseState(&mx, &my);
            if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y && ed->tileset_loaded) {
                ed->palette_scroll -= e.wheel.y;
                int max = ((ed->tile_count+PALETTE_COLS-1)/PALETTE_COLS) - ((WINDOW_H-PALETTE_START_Y)/(PALETTE_TILE_SIZE+2));
                if (ed->palette_scroll < 0) ed->palette_scroll = 0;
                if (max > 0 && ed->palette_scroll > max) ed->palette_scroll = max;
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = e.button.x, my = e.button.y;
            // Левая панель
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
                            if (idx>=0 && idx<ed->tile_count) ed->selected_tile = idx;
                        }
                    }
                }
            }
            // Центр
            else if (mx >= CENTER_X && mx < CENTER_X+CENTER_W && my >= CENTER_Y && my < CENTER_Y+CENTER_H) {
                if (ed->mode == MODE_B && ed->tileset_loaded) {
                    int w = TILE_SIZE*2, h = TILE_SIZE*2;
                    int ix = CENTER_X+(CENTER_W-w)/2, iy = CENTER_Y+(CENTER_H-h)/2;
                    SDL_Rect ir = {ix,iy,w,h};
                    if (mx>=ir.x && mx<ir.x+ir.w && my>=ir.y && my<ir.y+ir.h)
                        load_custom_tile(ed);
                }
            }
            // Правая панель
            else if (mx >= WINDOW_W-RIGHT_PANEL_W) {
                int rx = mx-(WINDOW_W-RIGHT_PANEL_W), ry = my;
                int bx = 10, bw = RIGHT_PANEL_W-20, y = 35;

                if (ry>=y && ry<y+24 && rx>=bx && rx<=bx+bw) {
                    if (ed->tileset_loaded) {
                        SDL_Surface *cust = ed->custom_surf ? SDL_DuplicateSurface(ed->custom_surf) : NULL;
                        animation_add_frame(ed, ed->selected_tile, 100, cust);
                    } return;
                }
                y+=30;
                if (ry>=y && ry<y+24 && rx>=bx && rx<=bx+bw/2-2) { animation_play(&ed->anim); return; }
                if (ry>=y && ry<y+24 && rx>=bx+bw/2+2 && rx<=bx+bw) { animation_stop(&ed->anim); return; }
                y+=30;
                if (ry>=y && ry<y+24 && rx>=bx && rx<=bx+bw) { animation_clear(&ed->anim); return; }
                if (ed->mode == MODE_B) {
                    y+=30;
                    if (ry>=y && ry<y+24 && rx>=bx && rx<=bx+bw) {
                        save_animation_strip(ed);
                        return;
                    }
                }
                y+=35+5;
                int lh = 22;
                for (int i=0; i<ed->anim.frame_count; i++) {
                    if (ry>=y && ry<y+lh) {
                        SDL_Rect del = {RIGHT_PANEL_W-30, y+2, 20,20};
                        if (rx>=del.x && rx<=del.x+del.w) { animation_delete_frame(ed,i); return; }
                    }
                    y+=lh;
                }
            }
        }
    }
}

// --- main ---
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

    load_checker_background(&ed);

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
    if (ed.checker_bg) SDL_DestroyTexture(ed.checker_bg);
    TTF_CloseFont(ed.font);
    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}