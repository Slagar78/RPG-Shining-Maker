#include "system_editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_ttf.h>

extern TTF_Font *g_font;
extern int g_font_ok;

static cJSON *root = NULL, *obj = NULL;
static struct {
    char t[64];
    int c, a;
    const char *k;
    int n;
    SDL_Rect r;
} f[4];

static int act = -1, timer = 0, px = 0, py = 0;

static int gi(cJSON *o, const char *k, int d) {
    cJSON *i = cJSON_GetObjectItem(o, k);
    return (i && cJSON_IsNumber(i)) ? i->valueint : d;
}

static const char* gs(cJSON *o, const char *k, const char *d) {
    cJSON *i = cJSON_GetObjectItem(o, k);
    return (i && cJSON_IsString(i)) ? i->valuestring : d;
}

int system_init(void) {
    // Создаём объект вручную, без чтения файла
    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "start_map", "Granseal");
    cJSON_AddNumberToObject(root, "start_x", 12);
    cJSON_AddNumberToObject(root, "start_y", 14);
    cJSON_AddNumberToObject(root, "gold", 500);
    obj = root;

    snprintf(f[0].t, 64, "%s", gs(obj, "start_map", ""));
    f[0].c = strlen(f[0].t);
    f[0].a = 0;
    f[0].k = "start_map";
    f[0].n = 0;

    snprintf(f[1].t, 64, "%d", gi(obj, "start_x", 0));
    f[1].c = strlen(f[1].t);
    f[1].a = 0;
    f[1].k = "start_x";
    f[1].n = 1;

    snprintf(f[2].t, 64, "%d", gi(obj, "start_y", 0));
    f[2].c = strlen(f[2].t);
    f[2].a = 0;
    f[2].k = "start_y";
    f[2].n = 1;

    snprintf(f[3].t, 64, "%d", gi(obj, "gold", 0));
    f[3].c = strlen(f[3].t);
    f[3].a = 0;
    f[3].k = "gold";
    f[3].n = 1;

    act = -1;
    return 1;
}

void system_free(void) {
    if (root) {
        cJSON_Delete(root);
        root = NULL;
    }
    obj = NULL;
}

void system_reset_selection(void) {
    act = -1;
    for (int i = 0; i < 4; i++) f[i].a = 0;
}

int system_is_edit_active(void) {
    return act >= 0;
}

void system_update_timer(void) {
    if (timer > 0) timer--;
}

void system_draw_edit_panel(SDL_Renderer *R, int x, int y) {
    px = x;
    py = y;

    SDL_Color w = {255, 255, 255};
    SDL_Color b = {0, 0, 0};
    SDL_Color g = {100, 100, 100};

    SDL_Rect p = {x, y, 580, 250};
    SDL_SetRenderDrawColor(R, 60, 60, 60, 255);
    SDL_RenderFillRect(R, &p);
    SDL_SetRenderDrawColor(R, 255, 255, 255, 255);
    SDL_RenderDrawRect(R, &p);

    if (obj) {
        const char *lb[] = {"Start Map:", "Start X:", "Start Y:", "Gold:"};
        int by = y + 20, lh = 35;

        for (int i = 0; i < 4; i++) {
            int fy = by + i * lh;

            // Метка
            SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, lb[i], w);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(R, s);
                SDL_Rect d = {x + 20, fy + 5, s->w, s->h};
                SDL_RenderCopy(R, t, NULL, &d);
                SDL_FreeSurface(s);
                SDL_DestroyTexture(t);
            }

            // Поле ввода
            int fx = x + 160, fw = 250, fh = 24;
            f[i].r = (SDL_Rect){fx, fy, fw, fh};
            SDL_SetRenderDrawColor(R, g.r, g.g, g.b, 255);
            SDL_RenderFillRect(R, &f[i].r);
            SDL_SetRenderDrawColor(R, w.r, w.g, w.b, 255);
            SDL_RenderDrawRect(R, &f[i].r);

            // Текст в поле
            s = TTF_RenderUTF8_Solid(g_font, f[i].t, b);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(R, s);
                SDL_Rect d = {fx + 5, fy + 3, s->w, s->h};
                SDL_RenderCopy(R, t, NULL, &d);
                SDL_FreeSurface(s);
                SDL_DestroyTexture(t);
            }

            // Курсор, если поле активно
            if (f[i].a) {
                char bf[64] = {0};
                if (f[i].c > 0) {
                    strncpy(bf, f[i].t, f[i].c);
                    bf[f[i].c] = 0;
                }
                s = TTF_RenderUTF8_Solid(g_font, bf, b);
                int cx = fx + 5 + (s ? s->w : 0);
                if (s) SDL_FreeSurface(s);
                SDL_SetRenderDrawColor(R, 0, 0, 0, 255);
                SDL_RenderDrawLine(R, cx, fy + 3, cx, fy + fh - 3);
            }
        }

        // Кнопка Save
        int btn_y = by + 4 * lh + 10;
        SDL_Rect sb = {x + 200, btn_y, 120, 30};
        SDL_Color sc = (timer > 0) ? (SDL_Color){0, 255, 0, 255} : (SDL_Color){255, 255, 0, 255};
        SDL_SetRenderDrawColor(R, sc.r, sc.g, sc.b, 255);
        SDL_RenderFillRect(R, &sb);
        SDL_SetRenderDrawColor(R, 0, 0, 0, 255);
        SDL_RenderDrawRect(R, &sb);
        SDL_Surface *ss = TTF_RenderUTF8_Solid(g_font, "Save", b);
        if (ss) {
            SDL_Texture *st = SDL_CreateTextureFromSurface(R, ss);
            SDL_Rect sd = {sb.x + 30, sb.y + 5, ss->w, ss->h};
            SDL_RenderCopy(R, st, NULL, &sd);
            SDL_FreeSurface(ss);
            SDL_DestroyTexture(st);
        }
    } else {
        // Данных нет – рисуем красный крест
        SDL_SetRenderDrawColor(R, 255, 0, 0, 255);
        SDL_RenderDrawLine(R, x + 10, y + 10, x + 570, y + 240);
        SDL_RenderDrawLine(R, x + 570, y + 10, x + 10, y + 240);
    }
}

static void save(void) {
    if (!obj) return;

    for (int i = 0; i < 4; i++) {
        cJSON *it = cJSON_GetObjectItem(obj, f[i].k);
        if (f[i].n) {
            int v = atoi(f[i].t);
            if (it && cJSON_IsNumber(it)) {
                it->valueint = v;
                it->valuedouble = v;
            } else {
                cJSON_ReplaceItemInObject(obj, f[i].k, cJSON_CreateNumber(v));
            }
        } else {
            if (it && cJSON_IsString(it))
                cJSON_SetValuestring(it, f[i].t);
            else
                cJSON_ReplaceItemInObject(obj, f[i].k, cJSON_CreateString(f[i].t));
        }
    }

    char *js = cJSON_PrintBuffered(root, 0, 1);
    FILE *fp = fopen("../data/global.json", "w");
    if (fp) {
        fputs(js, fp);
        fclose(fp);
        timer = 60;
    }
    free(js);
}

void system_handle_input(SDL_Event *ev) {
    if (!obj) return;

    // Клик мышью
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        int mx = ev->button.x, my = ev->button.y;

        // Поля ввода
        for (int i = 0; i < 4; i++) {
            SDL_Rect *r = &f[i].r;
            if (mx >= r->x && mx < r->x + r->w && my >= r->y && my < r->y + r->h) {
                if (act >= 0) f[act].a = 0;
                f[i].a = 1;
                f[i].c = strlen(f[i].t);
                act = i;
                return;
            }
        }

        // Кнопка Save
        int bx = px + 200, by = py + 20 + 4 * 35 + 10;
        if (mx >= bx && mx < bx + 120 && my >= by && my < by + 30) {
            save();
            return;
        }
        return;
    }

    if (act < 0) return;

    // Клавиатура
    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_BACKSPACE:
                if (f[act].c > 0) {
                    memmove(f[act].t + f[act].c - 1,
                            f[act].t + f[act].c,
                            strlen(f[act].t) - f[act].c + 1);
                    f[act].c--;
                }
                break;
            case SDLK_LEFT:
                if (f[act].c > 0) f[act].c--;
                break;
            case SDLK_RIGHT:
                if (f[act].c < strlen(f[act].t)) f[act].c++;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                f[act].a = 0;
                act = -1;
                break;
        }
    }
    // Текстовый ввод
    else if (ev->type == SDL_TEXTINPUT) {
        char ch = ev->text.text[0];

        if (f[act].n) {   // числовое поле
            if (isdigit(ch) || (ch == '-' && f[act].c == 0 && f[act].t[0] == '\0')) {
                int max_chars = 63;
                if (act == 1 || act == 2) max_chars = 8;   // start_x, start_y
                else if (act == 3) max_chars = 15;         // gold

                if (strlen(f[act].t) < max_chars) {
                    memmove(f[act].t + f[act].c + 1,
                            f[act].t + f[act].c,
                            strlen(f[act].t) - f[act].c + 1);
                    f[act].t[f[act].c++] = ch;
                }
            }
        } else {           // текстовое поле (start_map)
            int max_chars = 20;
            if (ch >= 32 && ch <= 126 && strlen(f[act].t) < max_chars) {
                memmove(f[act].t + f[act].c + 1,
                        f[act].t + f[act].c,
                        strlen(f[act].t) - f[act].c + 1);
                f[act].t[f[act].c++] = ch;
            }
        }
    }
}