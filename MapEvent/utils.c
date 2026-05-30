#include "editor.h"   // для WINDOW_W, WINDOW_H
#include "utils.h"
#include <string.h>

void get_logical_mouse(SDL_Window *window, int *mx, int *my) {
    SDL_GetMouseState(mx, my);
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);
    if (win_w != WINDOW_W || win_h != WINDOW_H) {
        *mx = (int)((float)*mx * WINDOW_W / win_w + 0.5f);
        *my = (int)((float)*my * WINDOW_H / win_h + 0.5f);
    }
}

void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text,
                        int cx, int cy, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst = { cx - surf->w/2, cy - surf->h/2, surf->w, surf->h };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}