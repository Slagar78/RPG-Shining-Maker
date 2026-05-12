// Constructor/constructor.c – стильный конструктор с запуском редакторов
// (адаптивный под любой экран через логический размер)
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>

#define LOGICAL_W 1024
#define LOGICAL_H 768

static const SDL_Color BG_COLOR          = { 20,  20,  30, 255 };
static const SDL_Color MENU_BG           = { 30,  30,  45, 255 };
static const SDL_Color TOOLBAR_BG        = { 40,  40,  60, 255 };
static const SDL_Color BUTTON_IDLE       = { 70,  50, 100, 255 };
static const SDL_Color BUTTON_HOVER      = { 110, 70, 160, 255 };
static const SDL_Color BUTTON_ACTIVE     = { 150, 90, 200, 255 };
static const SDL_Color TEXT_PRIMARY      = { 220, 220, 240, 255 };
static const SDL_Color SEPARATOR         = { 80,  80, 110, 255 };

typedef enum {
    MODE_MAP_EDITOR,
    MODE_MAP_CREATOR,
    MODE_BATTLE_EDITOR,
    MODE_TEXT_EDITOR,
    MODE_DATABASE,
    MODE_COUNT
} EditorMode;

typedef struct {
    SDL_Rect    rect;
    EditorMode  mode;
    SDL_Texture *icon;
    float       hover_t;
    const char *exe_path;
} ToolButton;

void draw_rounded_button(SDL_Renderer *ren, SDL_Rect *r, SDL_Color color) {
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    SDL_Rect inner = { r->x + 1, r->y + 1, r->w - 2, r->h - 2 };
    SDL_RenderFillRect(ren, &inner);
    SDL_SetRenderDrawColor(ren, 200, 200, 255, 60);
    SDL_RenderDrawRect(ren, &inner);
    SDL_SetRenderDrawColor(ren, TOOLBAR_BG.r, TOOLBAR_BG.g, TOOLBAR_BG.b, 255);
    SDL_RenderDrawPoint(ren, r->x, r->y);
    SDL_RenderDrawPoint(ren, r->x + r->w - 1, r->y);
    SDL_RenderDrawPoint(ren, r->x, r->y + r->h - 1);
    SDL_RenderDrawPoint(ren, r->x + r->w - 1, r->y + r->h - 1);
}

bool file_exists(const char *path) {
    DWORD attr = GetFileAttributes(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

void run_program_with_dll(const char *exe_name) {
    if (!exe_name) return;
    if (!file_exists(exe_name)) {
        printf("File not found: %s\n", exe_name);
        return;
    }

    char *base = SDL_GetBasePath();
    char dllDir[512];
    snprintf(dllDir, sizeof(dllDir), "%sdll", base);
    SDL_free(base);

    char oldPath[32767];
    GetEnvironmentVariable("PATH", oldPath, sizeof(oldPath));
    char newPath[65536];
    snprintf(newPath, sizeof(newPath), "%s;%s", dllDir, oldPath);
    SetEnvironmentVariable("PATH", newPath);

    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s", exe_name);
    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        printf("Failed to start %s\n", exe_name);
    }

    SetEnvironmentVariable("PATH", oldPath);
}

int main(int argc, char *argv[]) {
    // Unicode-поиск папки dll
    wchar_t baseW[MAX_PATH];
    GetModuleFileNameW(NULL, baseW, MAX_PATH);
    wchar_t *last = wcsrchr(baseW, L'\\');
    if (last) *(last + 1) = L'\0';
    wcscat(baseW, L"dll");
    SetDllDirectoryW(baseW);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    // === Адаптация под размер экрана (как в map_editor.c) ===
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int screenW = workArea.right - workArea.left;
    int screenH = workArea.bottom - workArea.top;

    int winW = LOGICAL_W;
    int winH = LOGICAL_H;

    if (screenW < LOGICAL_W || screenH < LOGICAL_H) {
        float scale = fminf((float)screenW / LOGICAL_W, (float)screenH / LOGICAL_H) * 0.92f;
        winW = (int)(LOGICAL_W * scale);
        winH = (int)(LOGICAL_H * scale);
    }

    SDL_Window *win = SDL_CreateWindow("RPG Shinzo Constructor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) return 1;

    SDL_RenderSetLogicalSize(ren, LOGICAL_W, LOGICAL_H);
    SDL_SetWindowMinimumSize(win, 800, 600);

    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    char *lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';

    TTF_Font *font = TTF_OpenFont("Font/NotoSans-Regular.ttf", 14);
    if (!font) font = TTF_OpenFont("Font/main.ttf", 14);
    if (!font) font = TTF_OpenFont("Font/arial.ttf", 14);
    if (!font) font = TTF_OpenFont("arial.ttf", 14);

    TTF_Font *font_bold = TTF_OpenFont("Font/NotoSans-Regular.ttf", 20);
    if (!font_bold) font_bold = TTF_OpenFont("Font/main.ttf", 20);
    if (!font_bold) font_bold = TTF_OpenFont("Font/arial.ttf", 20);
    if (!font_bold) font_bold = TTF_OpenFont("arial.ttf", 20);

    if (!font || !font_bold) { printf("Font error\n"); return 1; }

    const char *iconFilenames[MODE_COUNT] = {
        "map.png", "terrain.png", "battle.png", "text.png", "database.png"
    };
    SDL_Texture *icons[MODE_COUNT] = { NULL };

    for (int i = 0; i < MODE_COUNT; i++) {
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%sicons/%s", exePath, iconFilenames[i]);
        SDL_Surface *loaded = IMG_Load(fullPath);
        if (loaded) {
            icons[i] = SDL_CreateTextureFromSurface(ren, loaded);
            SDL_SetTextureScaleMode(icons[i], SDL_ScaleModeNearest);
            SDL_FreeSurface(loaded);
        } else {
            SDL_Surface *fb = SDL_CreateRGBSurface(0, 24, 24, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
            SDL_FillRect(fb, NULL, SDL_MapRGBA(fb->format, 120, 80, 180, 255));
            icons[i] = SDL_CreateTextureFromSurface(ren, fb);
            SDL_SetTextureScaleMode(icons[i], SDL_ScaleModeNearest);
            SDL_FreeSurface(fb);
        }
    }

    ToolButton buttons[MODE_COUNT] = {
        { {20, 35, 48, 48}, MODE_MAP_EDITOR,    icons[0], 0.0f, "Maker/MapEditor.exe" },
        { {80, 35, 48, 48}, MODE_MAP_CREATOR,   icons[1], 0.0f, "Maker/MapCreator.exe" },
        { {140,35, 48, 48}, MODE_BATTLE_EDITOR, icons[2], 0.0f, "Maker/BattleEditor.exe" },
        { {200,35, 48, 48}, MODE_TEXT_EDITOR,   icons[3], 0.0f, "Maker/text_editor.exe" },
        { {260,35, 48, 48}, MODE_DATABASE,      icons[4], 0.0f, "Maker/Database.exe" },
    };

    EditorMode currentMode = MODE_MAP_EDITOR;
    bool running = true;
    SDL_Event e;

    while (running) {
        int raw_mx, raw_my;
        SDL_GetMouseState(&raw_mx, &raw_my);
        int win_w, win_h;
        SDL_GetWindowSize(win, &win_w, &win_h);
        int mx = (int)((float)raw_mx * LOGICAL_W / win_w);
        int my = (int)((float)raw_my * LOGICAL_H / win_h);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                for (int i = 0; i < MODE_COUNT; i++) {
                    SDL_Point pt = {mx, my};
                    if (SDL_PointInRect(&pt, &buttons[i].rect)) {
                        currentMode = buttons[i].mode;
                        run_program_with_dll(buttons[i].exe_path);
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MODE_COUNT; i++) {
            SDL_Point pt = {mx, my};
            int target = SDL_PointInRect(&pt, &buttons[i].rect) ? 1 : 0;
            buttons[i].hover_t += (target - buttons[i].hover_t) * 0.15f;
        }

        SDL_SetRenderDrawColor(ren, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, 255);
        SDL_RenderClear(ren);

        SDL_Rect menuBar = {0, 0, LOGICAL_W, 28};
        SDL_SetRenderDrawColor(ren, MENU_BG.r, MENU_BG.g, MENU_BG.b, 255);
        SDL_RenderFillRect(ren, &menuBar);
        SDL_SetRenderDrawColor(ren, 0,0,0,40);
        SDL_RenderDrawLine(ren, 0, 28, LOGICAL_W, 28);

        const char *menuLabels[] = {"File", "Edit", "View", "Tools", "Help"};
        int xOff = 10;
        for (int i = 0; i < 5; i++) {
            SDL_Surface *s = TTF_RenderUTF8_Blended(font, menuLabels[i], TEXT_PRIMARY);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
                SDL_Rect dst = {xOff, 6, s->w, s->h};
                SDL_RenderCopy(ren, t, NULL, &dst);
                xOff += s->w + 24;
                SDL_FreeSurface(s);
                SDL_DestroyTexture(t);
            }
        }

        SDL_Rect toolbar = {0, 29, LOGICAL_W, 62};
        SDL_SetRenderDrawColor(ren, TOOLBAR_BG.r, TOOLBAR_BG.g, TOOLBAR_BG.b, 255);
        SDL_RenderFillRect(ren, &toolbar);
        SDL_SetRenderDrawColor(ren, SEPARATOR.r, SEPARATOR.g, SEPARATOR.b, 255);
        SDL_RenderDrawLine(ren, 0, 91, LOGICAL_W, 91);

        for (int i = 0; i < MODE_COUNT; i++) {
            SDL_Rect r = buttons[i].rect;
            float t = buttons[i].hover_t;
            SDL_Color col;
            if (currentMode == buttons[i].mode)
                col = BUTTON_ACTIVE;
            else if (t > 0.01f) {
                col.r = BUTTON_IDLE.r + (BUTTON_HOVER.r - BUTTON_IDLE.r) * t;
                col.g = BUTTON_IDLE.g + (BUTTON_HOVER.g - BUTTON_IDLE.g) * t;
                col.b = BUTTON_IDLE.b + (BUTTON_HOVER.b - BUTTON_IDLE.b) * t;
                col.a = 255;
            } else
                col = BUTTON_IDLE;

            SDL_SetRenderDrawColor(ren, 255,255,255,20);
            SDL_Rect shadow = {r.x+1, r.y+1, r.w, r.h};
            SDL_RenderFillRect(ren, &shadow);

            draw_rounded_button(ren, &r, col);

            if (icons[i]) {
                SDL_Rect iconRect = { r.x + 12, r.y + 12, 24, 24 };
                SDL_RenderCopy(ren, icons[i], NULL, &iconRect);
            }
        }

        SDL_Rect content = {0, 92, LOGICAL_W, LOGICAL_H - 92};
        SDL_SetRenderDrawColor(ren, 25,25,38,255);
        SDL_RenderFillRect(ren, &content);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    for (int i = 0; i < MODE_COUNT; i++) SDL_DestroyTexture(icons[i]);
    TTF_CloseFont(font);
    TTF_CloseFont(font_bold);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}