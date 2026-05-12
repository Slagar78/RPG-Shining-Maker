#include "editor.h"
#include <windows.h>
#include <math.h>

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    printf("=== Battle Editor ===\n");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) return 1;
    if (TTF_Init() != 0) return 1;

    // размер окна

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int screenW = workArea.right - workArea.left;
    int screenH = workArea.bottom - workArea.top;

    int winW = WINDOW_WIDTH;
    int winH = WINDOW_HEIGHT;

    if (screenW < WINDOW_WIDTH || screenH < WINDOW_HEIGHT) {
    float scale = fminf((float)screenW / WINDOW_WIDTH, (float)screenH / WINDOW_HEIGHT) * 0.92f;
    winW = (int)(WINDOW_WIDTH * scale);
    winH = (int)(WINDOW_HEIGHT * scale);
    }

    SDL_Window* win = SDL_CreateWindow("Battle Editor",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    winW, winH,
    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_SetWindowMinimumSize(win, 800, 600);
    BattleEditor ed;
    init_editor(&ed, ren);

    int run = 1; SDL_Event e;
    while (run) {
        while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) run = 0; handle_input(&ed, &e); }
        ed.anim_counter++;
        SDL_SetRenderDrawColor(ren, 50,50,50,255);
        SDL_RenderClear(ren);
        draw_ally_panel(&ed);
        draw_enemy_panel(&ed);
        draw_map_area(&ed);
        draw_inspector(&ed);   // +++ панель инспектора
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    cleanup_editor(&ed);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}