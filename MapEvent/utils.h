#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void get_logical_mouse(SDL_Window *window, int *mx, int *my);
void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text,
                        int cx, int cy, SDL_Color color);