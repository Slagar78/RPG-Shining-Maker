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

// ─── Константы окна ──────────────────────────
#define WINDOW_W 1024
#define WINDOW_H 600
#define TOOLBAR_H 34

#define LEFT_PANEL_W 250
#define RIGHT_PANEL_W 220
#define CENTER_X LEFT_PANEL_W
#define CENTER_Y TOOLBAR_H
#define CENTER_W (WINDOW_W - LEFT_PANEL_W - RIGHT_PANEL_W)
#define CENTER_H (WINDOW_H - TOOLBAR_H)

#define TILE_SIZE 32
#define PALETTE_TILE_SIZE 32
#define PALETTE_COLS 6
#define PALETTE_START_X 10
#define PALETTE_START_Y 140

#define FONT_SIZE 16

// Режимы
#define MODE_A 0   // обычный просмотр
#define MODE_B 1   // полупрозрачный (50%) + замена тайлов (клик по центру)

typedef struct {
    int tile_index;
    int duration_ms;
} AnimFrame;

typedef struct {
    AnimFrame *frames;
    int        frame_count;
    int        current_frame;
    Uint32     last_frame_time;
    bool       playing;
    bool       loop;
} Animation;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    SDL_Texture **tiles;        // массив тайлов
    int           tile_count;
    int           tileset_cols, tileset_rows;
    bool          tileset_loaded;
    char          tileset_name[256];

    int palette_scroll;
    int selected_tile;          // текущий выбранный тайл
    int mode;

    Animation    anim;

    SDL_Texture *checker_bg;    // шахматный фон для прозрачности
} Editor;

// ─── Прототипы ────────────────────────────────
void editor_init(Editor *ed);
void free_tileset(Editor *ed);
bool open_file_dialog(char *out_path, size_t out_len, const char *initial_dir);
int  load_tileset(Editor *ed, const char *path);
void animation_reset(Animation *anim);
void animation_add_frame(Editor *ed, int tile_index, int duration);
void animation_delete_frame(Editor *ed, int index);
void animation_clear(Animation *anim);
void animation_play(Animation *anim);
void animation_stop(Animation *anim);
void animation_update(Animation *anim, Uint32 now);
bool replace_tile_from_file(Editor *ed, int index);
void save_animation_strip(Editor *ed);
void load_checker_background(Editor *ed);
void render_left_panel(Editor *ed);
void render_center(Editor *ed);
void render_right_panel(Editor *ed);
void handle_input(Editor *ed, bool *running);

void safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (dest_size > 0) snprintf(dest, dest_size, "%s", src);
}

SDL_Texture* create_text_texture(SDL_Renderer *ren, TTF_Font *font, const char *text, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return NULL;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, SDL_Color color) {
    SDL_Texture *tex = create_text_texture(ren, font, text, color);
    if (!tex) return;
    int tw, th;
    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
    SDL_Rect dst = { cx - tw/2, cy - th/2, tw, th };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

// ─── Инициализация ────────────────────────────
void editor_init(Editor *ed) {
    memset(ed, 0, sizeof(Editor));
    ed->mode = MODE_A;
    ed->palette_scroll = 0;
    ed->selected_tile = 0;
    animation_reset(&ed->anim);
    ed->tileset_name[0] = '\0';
}

void animation_reset(Animation *anim) {
    anim->frame_count = 0;
    anim->current_frame = 0;
    anim->last_frame_time = 0;
    anim->playing = false;
    anim->loop = true;
    free(anim->frames);
    anim->frames = NULL;
}

// ─── Загрузка тайлсета ────────────────────────
void free_tileset(Editor *ed) {
    if (ed->tiles) {
        for (int i = 0; i < ed->tile_count; i++)
            SDL_DestroyTexture(ed->tiles[i]);
        free(ed->tiles);
        ed->tiles = NULL;
    }
    ed->tile_count = 0;
    ed->tileset_loaded = false;
    animation_clear(&ed->anim);
}

int load_tileset(Editor *ed, const char *path) {
    free_tileset(ed);
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) return 0;

    int cols = surface->w / TILE_SIZE;
    int rows = surface->h / TILE_SIZE;
    if (cols == 0 || rows == 0) {
        SDL_FreeSurface(surface);
        return 0;
    }

    ed->tileset_cols = cols;
    ed->tileset_rows = rows;
    ed->tile_count = cols * rows;
    ed->tiles = (SDL_Texture**)malloc(ed->tile_count * sizeof(SDL_Texture*));

    int idx = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            SDL_Rect src = { c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            SDL_Surface *tile_surf = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
            SDL_BlitSurface(surface, &src, tile_surf, NULL);
            ed->tiles[idx++] = SDL_CreateTextureFromSurface(ed->renderer, tile_surf);
            SDL_FreeSurface(tile_surf);
        }
    }
    SDL_FreeSurface(surface);

    const char *name = strrchr(path, '/');
    if (!name) name = strrchr(path, '\\');
    if (name) name++; else name = path;
    safe_strcpy(ed->tileset_name, sizeof(ed->tileset_name), name);
    char *dot = strrchr(ed->tileset_name, '.');
    if (dot) *dot = '\0';

    ed->tileset_loaded = true;
    ed->palette_scroll = 0;
    ed->selected_tile = 0;
    return 1;
}

// ─── Диалог открытия файла ────────────────────
bool open_file_dialog(char *out_path, size_t out_len, const char *initial_dir) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Files\0*.png\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = initial_dir;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) {
        safe_strcpy(out_path, out_len, ofn.lpstrFile);
        return true;
    }
    return false;
}

// ─── Управление анимацией ─────────────────────
void animation_clear(Animation *anim) {
    free(anim->frames);
    anim->frames = NULL;
    anim->frame_count = 0;
    anim->current_frame = 0;
    anim->playing = false;
}

void animation_add_frame(Editor *ed, int tile_index, int duration) {
    if (tile_index < 0 || tile_index >= ed->tile_count) return;
    ed->anim.frame_count++;
    ed->anim.frames = (AnimFrame*)realloc(ed->anim.frames, ed->anim.frame_count * sizeof(AnimFrame));
    ed->anim.frames[ed->anim.frame_count - 1].tile_index = tile_index;
    ed->anim.frames[ed->anim.frame_count - 1].duration_ms = duration;
}

void animation_delete_frame(Editor *ed, int index) {
    if (index < 0 || index >= ed->anim.frame_count) return;
    for (int i = index; i < ed->anim.frame_count - 1; i++)
        ed->anim.frames[i] = ed->anim.frames[i + 1];
    ed->anim.frame_count--;
    if (ed->anim.frame_count == 0) {
        free(ed->anim.frames);
        ed->anim.frames = NULL;
    } else {
        ed->anim.frames = (AnimFrame*)realloc(ed->anim.frames, ed->anim.frame_count * sizeof(AnimFrame));
    }
    if (ed->anim.current_frame >= ed->anim.frame_count)
        ed->anim.current_frame = ed->anim.frame_count - 1;
}

void animation_play(Animation *anim) {
    if (anim->frame_count > 0) {
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
    AnimFrame *frame = &anim->frames[anim->current_frame];
    if (now - anim->last_frame_time >= frame->duration_ms) {
        anim->current_frame = (anim->current_frame + 1) % anim->frame_count;
        anim->last_frame_time = now;
        if (anim->current_frame == 0 && !anim->loop)
            anim->playing = false;
    }
}

// ─── Замена тайла (только 48x48) ─────────────
bool replace_tile_from_file(Editor *ed, int index) {
    if (!ed->tileset_loaded || index < 0 || index >= ed->tile_count) return false;

    char path[256];
    if (!open_file_dialog(path, sizeof(path), NULL)) return false;

    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        MessageBoxA(NULL, "Failed to load image.", "Error", MB_ICONERROR);
        return false;
    }

    if (surf->w != 48 || surf->h != 48) {
        SDL_FreeSurface(surf);
        MessageBoxA(NULL, "Image must be exactly 48x48 pixels.", "Wrong Size", MB_ICONWARNING);
        return false;
    }

    SDL_Surface *scaled = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
    SDL_BlitScaled(surf, NULL, scaled, NULL);
    SDL_FreeSurface(surf);

    if (ed->tiles[index]) SDL_DestroyTexture(ed->tiles[index]);
    ed->tiles[index] = SDL_CreateTextureFromSurface(ed->renderer, scaled);
    SDL_FreeSurface(scaled);
    return true;
}

// ─── Сохранение анимации в PNG ────────────────
void save_animation_strip(Editor *ed) {
    if (ed->anim.frame_count == 0) return;

    int total_w = ed->anim.frame_count * TILE_SIZE;
    int total_h = TILE_SIZE;

    SDL_Surface *strip = SDL_CreateRGBSurface(0, total_w, total_h, 32, 0,0,0,0);
    if (!strip) return;

    for (int i = 0; i < ed->anim.frame_count; i++) {
        int idx = ed->anim.frames[i].tile_index;
        if (idx < 0 || idx >= ed->tile_count) continue;

        SDL_Texture *tex = ed->tiles[idx];
        SDL_Texture *target = SDL_CreateTexture(ed->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, TILE_SIZE, TILE_SIZE);
        SDL_SetRenderTarget(ed->renderer, target);
        SDL_SetRenderDrawColor(ed->renderer, 0,0,0,0);
        SDL_RenderClear(ed->renderer);
        SDL_RenderCopy(ed->renderer, tex, NULL, NULL);
        SDL_Surface *frame_surf = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
        SDL_RenderReadPixels(ed->renderer, NULL, SDL_PIXELFORMAT_RGBA8888, frame_surf->pixels, frame_surf->pitch);
        SDL_SetRenderTarget(ed->renderer, NULL);
        SDL_DestroyTexture(target);

        SDL_Rect dest = { i * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE };
        SDL_BlitSurface(frame_surf, NULL, strip, &dest);
        SDL_FreeSurface(frame_surf);
    }

    CreateDirectoryA("exports", NULL);
    char filename[512];
    snprintf(filename, sizeof(filename), "exports/%s_animation.png", ed->tileset_name[0] ? ed->tileset_name : "untitled");
    IMG_SavePNG(strip, filename);
    SDL_FreeSurface(strip);
}

// ─── Шахматный фон ────────────────────────────
void load_checker_background(Editor *ed) {
    if (ed->checker_bg) SDL_DestroyTexture(ed->checker_bg);
    SDL_Surface *surf = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
    SDL_Rect half1 = {0,0, TILE_SIZE/2, TILE_SIZE/2};
    SDL_Rect half2 = {TILE_SIZE/2, TILE_SIZE/2, TILE_SIZE/2, TILE_SIZE/2};
    SDL_FillRect(surf, &half1, SDL_MapRGBA(surf->format, 180,180,180,255));
    SDL_FillRect(surf, &half2, SDL_MapRGBA(surf->format, 180,180,180,255));
    ed->checker_bg = SDL_CreateTextureFromSurface(ed->renderer, surf);
    SDL_FreeSurface(surf);
}

// ─── Отрисовка ────────────────────────────────
void render_left_panel(Editor *ed) {
    SDL_Rect bg = { 0, 0, LEFT_PANEL_W, WINDOW_H };
    SDL_SetRenderDrawColor(ed->renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "TILESET", LEFT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    SDL_Rect load_btn = { 10, 35, LEFT_PANEL_W - 20, 26 };
    SDL_SetRenderDrawColor(ed->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(ed->renderer, &load_btn);
    draw_text_centered(ed->renderer, ed->font, "Load Tileset", load_btn.x + load_btn.w/2, load_btn.y + load_btn.h/2, (SDL_Color){255,255,255,255});

    SDL_Rect btnA = { 10, 70, (LEFT_PANEL_W - 30)/2, 24 };
    SDL_Rect btnB = { btnA.x + btnA.w + 10, 70, btnA.w, 24 };
    if (ed->mode == MODE_A) {
        SDL_SetRenderDrawColor(ed->renderer, 100, 160, 100, 255);
        SDL_RenderFillRect(ed->renderer, &btnA);
        SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
        SDL_RenderFillRect(ed->renderer, &btnB);
    } else {
        SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
        SDL_RenderFillRect(ed->renderer, &btnA);
        SDL_SetRenderDrawColor(ed->renderer, 100, 160, 100, 255);
        SDL_RenderFillRect(ed->renderer, &btnB);
    }
    draw_text_centered(ed->renderer, ed->font, "A", btnA.x + btnA.w/2, btnA.y + btnA.h/2, (SDL_Color){255,255,255,255});
    draw_text_centered(ed->renderer, ed->font, "B", btnB.x + btnB.w/2, btnB.y + btnB.h/2, (SDL_Color){255,255,255,255});

    SDL_SetRenderDrawColor(ed->renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(ed->renderer, 10, 105, LEFT_PANEL_W-10, 105);

    if (!ed->tileset_loaded) {
        draw_text_centered(ed->renderer, ed->font, "No tileset", LEFT_PANEL_W/2, 250, (SDL_Color){255,100,100,255});
        return;
    }

    int visible_rows = (WINDOW_H - PALETTE_START_Y) / (PALETTE_TILE_SIZE + 2);
    int total_rows = (ed->tile_count + PALETTE_COLS - 1) / PALETTE_COLS;

    for (int row = ed->palette_scroll; row < ed->palette_scroll + visible_rows && row < total_rows; row++) {
        for (int col = 0; col < PALETTE_COLS; col++) {
            int idx = row * PALETTE_COLS + col;
            if (idx >= ed->tile_count) break;

            SDL_Rect dst = {
                PALETTE_START_X + col * (PALETTE_TILE_SIZE + 2),
                PALETTE_START_Y + (row - ed->palette_scroll) * (PALETTE_TILE_SIZE + 2),
                PALETTE_TILE_SIZE, PALETTE_TILE_SIZE
            };

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

            SDL_SetRenderDrawColor(ed->renderer, 70, 70, 70, 255);
            SDL_Rect frame = { dst.x - 1, dst.y - 1, PALETTE_TILE_SIZE + 2, PALETTE_TILE_SIZE + 2 };
            SDL_RenderDrawRect(ed->renderer, &frame);

            // Жёлтая рамка для выбранного тайла
            if (idx == ed->selected_tile) {
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                for (int t = 0; t < 2; t++) {
                    SDL_Rect r = { frame.x - t, frame.y - t, frame.w + 2*t, frame.h + 2*t };
                    SDL_RenderDrawRect(ed->renderer, &r);
                }
            }
        }
    }
}

void render_center(Editor *ed) {
    SDL_Rect bg = { CENTER_X, CENTER_Y, CENTER_W, CENTER_H };
    SDL_SetRenderDrawColor(ed->renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(ed->renderer, &bg);

    if (!ed->tileset_loaded) {
        draw_text_centered(ed->renderer, ed->font, "Load tileset first", CENTER_X + CENTER_W/2, CENTER_Y + CENTER_H/2, (SDL_Color){200,200,200,255});
        return;
    }

    int tile_to_show = ed->selected_tile;
    if (ed->anim.frame_count > 0 && ed->anim.playing) {
        animation_update(&ed->anim, SDL_GetTicks());
        tile_to_show = ed->anim.frames[ed->anim.current_frame].tile_index;
    }

    if (tile_to_show >= 0 && tile_to_show < ed->tile_count) {
        int draw_w = TILE_SIZE * 4;
        int draw_h = TILE_SIZE * 4;
        SDL_Rect dst = { CENTER_X + (CENTER_W - draw_w)/2, CENTER_Y + (CENTER_H - draw_h)/2, draw_w, draw_h };

        if (ed->mode == MODE_B && ed->checker_bg) {
            for (int y = 0; y < draw_h; y += TILE_SIZE)
                for (int x = 0; x < draw_w; x += TILE_SIZE) {
                    SDL_Rect d = { dst.x + x, dst.y + y, TILE_SIZE, TILE_SIZE };
                    SDL_RenderCopy(ed->renderer, ed->checker_bg, NULL, &d);
                }
        }

        SDL_Texture *tex = ed->tiles[tile_to_show];
        if (ed->mode == MODE_B) {
            SDL_SetTextureAlphaMod(tex, 128);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
        SDL_RenderCopy(ed->renderer, tex, NULL, &dst);
        if (ed->mode == MODE_B) {
            SDL_SetTextureAlphaMod(tex, 255);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
        }

        // Подсказка: в режиме B клик по центру заменяет тайл
        if (ed->mode == MODE_B) {
            draw_text_centered(ed->renderer, ed->font, "Click on tile to replace (48x48)", CENTER_X + CENTER_W/2, dst.y + dst.h + 15, (SDL_Color){200,200,200,255});
        }
    }

    char mode_text[64];
    snprintf(mode_text, sizeof(mode_text), "Mode: %s", ed->mode == MODE_A ? "A - Normal" : "B - Transparent / Replace");
    draw_text_centered(ed->renderer, ed->font, mode_text, CENTER_X + CENTER_W/2, CENTER_Y + CENTER_H - 10, (SDL_Color){180,180,180,255});
}

void render_right_panel(Editor *ed) {
    int px = WINDOW_W - RIGHT_PANEL_W;
    SDL_Rect bg = { px, 0, RIGHT_PANEL_W, WINDOW_H };
    SDL_SetRenderDrawColor(ed->renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "ANIMATION", px + RIGHT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    int y = 35;
    int btn_w = RIGHT_PANEL_W - 20;
    int btn_x = px + 10;

    SDL_Rect add_btn = { btn_x, y, btn_w, 24 };
    SDL_SetRenderDrawColor(ed->renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(ed->renderer, &add_btn);
    draw_text_centered(ed->renderer, ed->font, "Add Frame (100ms)", add_btn.x + add_btn.w/2, add_btn.y + add_btn.h/2, (SDL_Color){255,255,255,255});

    y += 30;
    SDL_Rect play_btn = { btn_x, y, btn_w/2-2, 24 };
    SDL_Rect stop_btn = { btn_x + btn_w/2+2, y, btn_w/2-2, 24 };
    SDL_SetRenderDrawColor(ed->renderer, 80, 130, 80, 255);
    SDL_RenderFillRect(ed->renderer, &play_btn);
    draw_text_centered(ed->renderer, ed->font, "Play", play_btn.x + play_btn.w/2, play_btn.y + play_btn.h/2, (SDL_Color){255,255,255,255});
    SDL_SetRenderDrawColor(ed->renderer, 150, 80, 80, 255);
    SDL_RenderFillRect(ed->renderer, &stop_btn);
    draw_text_centered(ed->renderer, ed->font, "Stop", stop_btn.x + stop_btn.w/2, stop_btn.y + stop_btn.h/2, (SDL_Color){255,255,255,255});

    y += 30;
    SDL_Rect clear_btn = { btn_x, y, btn_w, 24 };
    SDL_SetRenderDrawColor(ed->renderer, 180, 100, 100, 255);
    SDL_RenderFillRect(ed->renderer, &clear_btn);
    draw_text_centered(ed->renderer, ed->font, "Clear All", clear_btn.x + clear_btn.w/2, clear_btn.y + clear_btn.h/2, (SDL_Color){255,255,255,255});

    if (ed->mode == MODE_B) {
        y += 30;
        SDL_Rect save_btn = { btn_x, y, btn_w, 24 };
        SDL_SetRenderDrawColor(ed->renderer, 80, 80, 180, 255);
        SDL_RenderFillRect(ed->renderer, &save_btn);
        draw_text_centered(ed->renderer, ed->font, "Save Animation", save_btn.x + save_btn.w/2, save_btn.y + save_btn.h/2, (SDL_Color){255,255,255,255});
    }

    y += 35;
    SDL_SetRenderDrawColor(ed->renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(ed->renderer, px + 5, y, px + RIGHT_PANEL_W - 5, y);

    y += 5;
    for (int i = 0; i < ed->anim.frame_count; i++) {
        if (y + 22 > WINDOW_H - 10) break;
        char buf[64];
        snprintf(buf, sizeof(buf), "%d: tile %d (%dms)", i+1, ed->anim.frames[i].tile_index, ed->anim.frames[i].duration_ms);
        SDL_Color col = (i == ed->anim.current_frame && ed->anim.playing) ? (SDL_Color){255,255,0,255} : (SDL_Color){220,220,220,255};
        draw_text_centered(ed->renderer, ed->font, buf, px + RIGHT_PANEL_W/2, y + 11, col);

        SDL_Rect del_rect = { px + RIGHT_PANEL_W - 30, y + 2, 20, 20 };
        SDL_SetRenderDrawColor(ed->renderer, 200, 70, 70, 255);
        SDL_RenderFillRect(ed->renderer, &del_rect);
        draw_text_centered(ed->renderer, ed->font, "X", del_rect.x + del_rect.w/2, del_rect.y + del_rect.h/2, (SDL_Color){255,255,255,255});
        y += 22;
    }
}

// ─── Обработка событий ────────────────────────
void handle_input(Editor *ed, bool *running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *running = false; return; }

        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_SPACE:
                    if (ed->anim.playing) animation_stop(&ed->anim);
                    else animation_play(&ed->anim);
                    break;
                case SDLK_a: ed->mode = MODE_A; break;
                case SDLK_b: ed->mode = MODE_B; break;
                case SDLK_RETURN:
                    if (ed->tileset_loaded) animation_add_frame(ed, ed->selected_tile, 100);
                    break;
                case SDLK_DELETE:
                    if (ed->anim.frame_count > 0) animation_delete_frame(ed, ed->anim.frame_count - 1);
                    break;
                default: break;
            }
        }

        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y && ed->tileset_loaded) {
                ed->palette_scroll -= e.wheel.y;
                int max_scroll = ((ed->tile_count + PALETTE_COLS - 1) / PALETTE_COLS) -
                                 ((WINDOW_H - PALETTE_START_Y) / (PALETTE_TILE_SIZE + 2));
                if (ed->palette_scroll < 0) ed->palette_scroll = 0;
                if (max_scroll > 0 && ed->palette_scroll > max_scroll) ed->palette_scroll = max_scroll;
            }
        }

        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = e.button.x, my = e.button.y;

            // Левая панель (палитра)
            if (mx < LEFT_PANEL_W) {
                if (my >= 35 && my < 61) {
                    char path[256];
                    if (open_file_dialog(path, sizeof(path), "assets\\tilesets"))
                        load_tileset(ed, path);
                }
                else if (my >= 70 && my < 94) {
                    int half = (LEFT_PANEL_W - 30) / 2;
                    if (mx >= 10 && mx < 10+half) ed->mode = MODE_A;
                    else if (mx >= 10+half+10 && mx < 10+half+10+half) ed->mode = MODE_B;
                }
                else if (ed->tileset_loaded && my >= PALETTE_START_Y) {
                    int rx = mx - PALETTE_START_X, ry = my - PALETTE_START_Y;
                    int step = PALETTE_TILE_SIZE + 2;
                    if (rx >= 0 && ry >= 0) {
                        int col = rx / step, row = ry / step + ed->palette_scroll;
                        if (col < PALETTE_COLS && (rx % step) < PALETTE_TILE_SIZE) {
                            int idx = row * PALETTE_COLS + col;
                            if (idx >= 0 && idx < ed->tile_count)
                                ed->selected_tile = idx;   // просто выбираем, без замены
                        }
                    }
                }
            }
            // Центральная область: замена тайла в режиме B по клику на увеличенное изображение
            else if (mx >= CENTER_X && mx < CENTER_X + CENTER_W && my >= CENTER_Y && my < CENTER_Y + CENTER_H) {
                if (ed->mode == MODE_B && ed->tileset_loaded && ed->selected_tile >= 0) {
                    // Определяем, попали ли в область большого тайла (она центрирована)
                    int draw_w = TILE_SIZE * 4;
                    int draw_h = TILE_SIZE * 4;
                    int img_x = CENTER_X + (CENTER_W - draw_w)/2;
                    int img_y = CENTER_Y + (CENTER_H - draw_h)/2;
                    SDL_Rect img_rect = { img_x, img_y, draw_w, draw_h };
                    if (mx >= img_rect.x && mx < img_rect.x + img_rect.w &&
                        my >= img_rect.y && my < img_rect.y + img_rect.h) {
                        replace_tile_from_file(ed, ed->selected_tile);
                    }
                }
            }
            // Правая панель (анимация)
            else if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                int rel_x = mx - (WINDOW_W - RIGHT_PANEL_W);
                int rel_y = my;
                int btn_x = 10, btn_w = RIGHT_PANEL_W - 20;
                int y = 35;

                if (rel_y >= y && rel_y < y+24 && rel_x >= btn_x && rel_x <= btn_x+btn_w) {
                    if (ed->tileset_loaded) animation_add_frame(ed, ed->selected_tile, 100);
                    return;
                }
                y += 30;
                if (rel_y >= y && rel_y < y+24 && rel_x >= btn_x && rel_x <= btn_x+btn_w/2-2) {
                    animation_play(&ed->anim); return;
                }
                if (rel_y >= y && rel_y < y+24 && rel_x >= btn_x+btn_w/2+2 && rel_x <= btn_x+btn_w) {
                    animation_stop(&ed->anim); return;
                }
                y += 30;
                if (rel_y >= y && rel_y < y+24 && rel_x >= btn_x && rel_x <= btn_x+btn_w) {
                    animation_clear(&ed->anim); return;
                }
                if (ed->mode == MODE_B) {
                    y += 30;
                    if (rel_y >= y && rel_y < y+24 && rel_x >= btn_x && rel_x <= btn_x+btn_w) {
                        save_animation_strip(ed); return;
                    }
                }

                y += 35 + 5;
                int line_h = 22;
                for (int i = 0; i < ed->anim.frame_count; i++) {
                    if (rel_y >= y && rel_y < y + line_h) {
                        SDL_Rect del_btn = { RIGHT_PANEL_W - 30, y + 2, 20, 20 };
                        if (rel_x >= del_btn.x && rel_x <= del_btn.x + del_btn.w) {
                            animation_delete_frame(ed, i);
                            return;
                        }
                    }
                    y += line_h;
                }
            }
        }
    }
}

// ─── main ──────────────────────────────────────
int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    Editor ed;
    editor_init(&ed);

    ed.window = SDL_CreateWindow("Tileset Animation Editor",
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 WINDOW_W, WINDOW_H,
                                 SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    ed.renderer = SDL_CreateRenderer(ed.window, -1,
                                     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ed.renderer, WINDOW_W, WINDOW_H);

    ed.font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", FONT_SIZE);
    if (!ed.font) ed.font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", FONT_SIZE);
    if (!ed.font) {
        SDL_DestroyRenderer(ed.renderer);
        SDL_DestroyWindow(ed.window);
        TTF_Quit(); IMG_Quit(); SDL_Quit();
        return 1;
    }

    load_checker_background(&ed);

    bool running = true;
    while (running) {
        handle_input(&ed, &running);

        SDL_SetRenderDrawColor(ed.renderer, 30, 30, 30, 255);
        SDL_RenderClear(ed.renderer);

        render_left_panel(&ed);
        render_center(&ed);
        render_right_panel(&ed);

        SDL_RenderPresent(ed.renderer);
        SDL_Delay(16);
    }

    free_tileset(&ed);
    animation_clear(&ed.anim);
    if (ed.checker_bg) SDL_DestroyTexture(ed.checker_bg);
    TTF_CloseFont(ed.font);
    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}