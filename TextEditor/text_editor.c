#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <windows.h>
#include <commdlg.h>
#include "../cJSON.h"

// ---------- константы интерфейса ----------
#define WIN_W  900
#define WIN_H  640

#define MAX_VISIBLE_LINES 8
#define MAX_LINE_BYTES    1024
#define PREVIEW_LINES     3
#define PANEL_WIDTH       620
#define PANEL_HEIGHT      140
#define PANEL_X           ((WIN_W - PANEL_WIDTH) / 2)
#define PANEL_Y           (WIN_H - PANEL_HEIGHT - 20)

#define MARGIN_LEFT       20
#define MARGIN_RIGHT      20
#define MARGIN_TOP        10
#define LINE_SPACING      4

#define NOTE_X 20
#define NOTE_Y 150
#define NOTE_W (WIN_W - 40)
#define NOTE_H 240

// ---------- структура строки (с ID) ----------
typedef struct {
    char id[8];
    char text[MAX_LINE_BYTES];
} Line;

typedef struct {
    Line *data;
    int count;
    int capacity;
} LineArray;

typedef struct { int id; char name[64]; } Actor;
typedef struct { Actor *actors; int count; } ActorDB;

// ---------- UTF-8 helpers ----------
static int utf8_char_len(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc < 0x80) return 1;
    if (uc < 0xC0) return 1;
    if (uc < 0xE0) return 2;
    if (uc < 0xF0) return 3;
    return 4;
}

static int utf8_backspace(char *buf, int byte_len) {
    if (byte_len <= 0) return 0;
    int pos = byte_len - 1;
    while (pos > 0 && (buf[pos] & 0xC0) == 0x80) pos--;
    buf[pos] = '\0';
    return pos;
}

// ---------- File dialog ----------
static bool open_file_dialog(char *out_path, size_t out_len) {
    OPENFILENAMEA ofn; char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files\0*.TXT\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) {
        if (out_len > 0) { strncpy(out_path, ofn.lpstrFile, out_len-1); out_path[out_len-1] = '\0'; }
        return true;
    }
    return false;
}

static bool save_file_dialog(char *out_path, size_t out_len) {
    OPENFILENAMEA ofn; char szFile[260] = "message.txt";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files\0*.TXT\0All Files\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) {
        if (out_len > 0) { strncpy(out_path, ofn.lpstrFile, out_len-1); out_path[out_len-1] = '\0'; }
        return true;
    }
    return false;
}

// ---------- LineArray с ID ----------
static void la_init(LineArray *la) {
    la->data = NULL; la->count = la->capacity = 0;
}

static void la_add_empty(LineArray *la) {
    if (la->count >= la->capacity) {
        la->capacity = la->capacity ? la->capacity * 2 : 4;
        la->data = (Line*)realloc(la->data, la->capacity * sizeof(Line));
    }
    memset(&la->data[la->count], 0, sizeof(Line));
    la->data[la->count].id[0] = '\0';
    la->data[la->count].text[0] = '\0';
    la->count++;
}

static void la_free(LineArray *la) {
    free(la->data);
    la->data = NULL; la->count = la->capacity = 0;
}

// Проверяет, есть ли уже строка с таким ID
static bool la_id_exists(LineArray *la, const char *id) {
    if (id[0] == '\0') return false;
    for (int i = 0; i < la->count; i++) {
        if (strcmp(la->data[i].id, id) == 0) return true;
    }
    return false;
}

// Генерирует следующий уникальный ID
static void generate_unique_id(LineArray *la, char *out) {
    // Находим максимальный существующий ID
    unsigned int max_num = 0;
    bool has_ids = false;
    for (int i = 0; i < la->count; i++) {
        if (la->data[i].id[0] != '\0') {
            unsigned int num;
            if (sscanf(la->data[i].id, "%x", &num) == 1) {
                if (!has_ids || num > max_num) max_num = num;
                has_ids = true;
            }
        }
    }
    
    unsigned int candidate = has_ids ? max_num + 1 : 0;
    char try_id[8];
    while (1) {
        sprintf(try_id, "%04X", candidate);
        if (!la_id_exists(la, try_id)) {
            strcpy(out, try_id);
            return;
        }
        candidate++;
    }
}

// ---------- Кнопки тегов ----------
typedef struct { const char *label; const char *tag; SDL_Rect rect; bool is_actors_menu; } TagButton;
#define TAG_BTN_W  130
#define TAG_BTN_H  28
#define TAG_COLS   5
#define TAG_GAP_X  8
#define TAG_GAP_Y  8

static TagButton tag_btns[] = {
    {"Newline",    "{N}",     {0}, false},
    {"Clear",      "{CLEAR}", {0}, false},
    {"Wait-close", "{W1}",    {0}, false},
    {"Wait-newl",  "{W2}",    {0}, false},
    {"Delay-s",    "{D1}",    {0}, false},
    {"Delay-m",    "{D2}",    {0}, false},
    {"Delay-l",    "{D3}",    {0}, false},
    {"Leader",     "{LEADER}",{0}, false},
    {"Actors",     "",        {0}, true},
    {NULL, NULL, {0}, false}
};
static int num_tag_btns = sizeof(tag_btns)/sizeof(tag_btns[0]) - 1;

// ---------- загрузка actors.json ----------
void load_actors(ActorDB *db) {
    db->actors=NULL; db->count=0;
    FILE *f = fopen("../data/actors/actors.json","r"); if(!f)return;
    fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
    char *data=(char*)malloc(len+1); fread(data,1,len,f); fclose(f); data[len]='\0';
    cJSON *root = cJSON_Parse(data); free(data);
    if(!root)return;
    cJSON *actors_arr = cJSON_GetObjectItem(root,"actors");
    if(!actors_arr){cJSON_Delete(root);return;}
    int n = cJSON_GetArraySize(actors_arr);
    db->actors = (Actor*)calloc(n,sizeof(Actor));
    for(int i=0;i<n;i++){
        cJSON *item = cJSON_GetArrayItem(actors_arr,i);
        cJSON *id = cJSON_GetObjectItem(item,"id");
        cJSON *name = cJSON_GetObjectItem(item,"name");
        if(id && name){
            db->actors[i].id = id->valueint;
            strncpy(db->actors[i].name, name->valuestring, sizeof(db->actors[i].name)-1);
            db->actors[i].name[sizeof(db->actors[i].name)-1] = '\0';
        }
    }
    db->count = n;
    cJSON_Delete(root);
}
const char* get_actor_name(ActorDB *db, int id){
    for(int i=0;i<db->count;i++) if(db->actors[i].id==id) return db->actors[i].name;
    return "???";
}

// ---------- Предпросмотр ----------
typedef struct { char text[MAX_LINE_BYTES]; } PreviewLine;
PreviewLine preview_lines[PREVIEW_LINES];
int preview_line_count = 0;
void clear_preview(){ for(int i=0;i<PREVIEW_LINES;i++) preview_lines[i].text[0]='\0'; preview_line_count=0; }
void add_preview_line(const char *str){
    if(preview_line_count >= PREVIEW_LINES) return;
    snprintf(preview_lines[preview_line_count].text, MAX_LINE_BYTES, "%s", str ? str : "");
    preview_line_count++;
}
void generate_preview(LineArray *lines, ActorDB *actors, int current_idx){
    clear_preview();
    
    if (current_idx < 0 || current_idx >= lines->count) return;
    
    const char *p = lines->data[current_idx].text;
    char cur[MAX_LINE_BYTES] = {0};
    int cur_len = 0;

    while(*p){
        if(*p == '{'){
            const char *end = strchr(p, '}');
            if(!end){
                cur[cur_len++] = *p++;
                continue;
            }

            int cmd_len = end - p - 1;
            char cmd[64] = {0};
            strncpy(cmd, p+1, cmd_len);

            if(strcmp(cmd, "N") == 0){
                if(cur_len > 0 || preview_line_count == 0){
                    add_preview_line(cur);
                }
                cur[0] = '\0';
                cur_len = 0;
            }
            else if(strcmp(cmd, "CLEAR") == 0){
                clear_preview();
                cur[0] = '\0';
                cur_len = 0;
            }
            else if(strcmp(cmd, "W1") == 0 || strcmp(cmd, "W2") == 0 ||
                    strcmp(cmd, "D1") == 0 || strcmp(cmd, "D2") == 0 || strcmp(cmd, "D3") == 0){
                // не выводим
            }
            else if(strcmp(cmd, "LEADER") == 0){
                const char* name = get_actor_name(actors, 0);
                strcat(cur, name);
                cur_len += strlen(name);
            }
            else if(strncmp(cmd, "NAME;", 5) == 0){
                int id = atoi(cmd + 5);
                const char* name = get_actor_name(actors, id);
                strcat(cur, name);
                cur_len += strlen(name);
            }
            else {
                strncat(cur, p, end - p + 1);
                cur_len += end - p + 1;
            }
            p = end + 1;
        } 
        else {
            cur[cur_len++] = *p++;
            cur[cur_len] = '\0';
        }
    }

    if(cur_len > 0){
        add_preview_line(cur);
    }
}

// ---------- Сравнение ID для сортировки ----------
static int compare_by_id(const void *a, const void *b) {
    const Line *lineA = (const Line *)a;
    const Line *lineB = (const Line *)b;
    // строки без id уходят в конец
    if (lineA->id[0] == '\0' && lineB->id[0] == '\0') return 0;
    if (lineA->id[0] == '\0') return 1;
    if (lineB->id[0] == '\0') return -1;
    unsigned int idA, idB;
    sscanf(lineA->id, "%x", &idA);
    sscanf(lineB->id, "%x", &idB);
    return (idA > idB) - (idA < idB);
}

// ---------- Отрисовка кнопок тегов ----------
void draw_tag_buttons(SDL_Renderer *r, TTF_Font *font) {
    int start_x = NOTE_X, start_y = 50;
    for(int i=0;i<num_tag_btns;i++){
        int col = i%TAG_COLS, row = i/TAG_COLS;
        tag_btns[i].rect.x = start_x + col*(TAG_BTN_W+TAG_GAP_X);
        tag_btns[i].rect.y = start_y + row*(TAG_BTN_H+TAG_GAP_Y);
        tag_btns[i].rect.w = TAG_BTN_W; tag_btns[i].rect.h = TAG_BTN_H;
        SDL_SetRenderDrawColor(r,80,80,80,255); SDL_RenderFillRect(r,&tag_btns[i].rect);
        SDL_Surface *s = TTF_RenderUTF8_Blended(font, tag_btns[i].label, (SDL_Color){255,255,255,255});
        if(s){
            SDL_Texture *t = SDL_CreateTextureFromSurface(r,s);
            SDL_Rect d = { tag_btns[i].rect.x+4, tag_btns[i].rect.y+2, s->w, s->h };
            SDL_RenderCopy(r,t,NULL,&d); SDL_DestroyTexture(t); SDL_FreeSurface(s);
        }
    }
}

// ---------- Отрисовка ID панели ----------
void draw_id_panel(SDL_Renderer *r, TTF_Font *font, int current_idx, int total, const char *current_id, Uint32 new_flash_time, Uint32 del_flash_time) {
    SDL_Rect bg = { NOTE_X, NOTE_Y-35, NOTE_W, 30 };
    SDL_SetRenderDrawColor(r, 50,50,50,255); SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawColor(r, 120,120,120,255); SDL_RenderDrawRect(r, &bg);

    char info[64];
    snprintf(info, sizeof(info), "ID: %s (%d/%d)", current_id ? current_id : "----", current_idx+1, total);
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, info, (SDL_Color){255,255,255,255});
    int info_w = s ? s->w : 0;
    if(s){
        SDL_Texture *t = SDL_CreateTextureFromSurface(r,s);
        SDL_Rect d = { NOTE_X+5, bg.y+4, s->w, s->h };
        SDL_RenderCopy(r, t, NULL, &d);
        SDL_DestroyTexture(t); SDL_FreeSurface(s);
    }

    int left_offset = NOTE_X + 5 + info_w + 15;
    int btn_h = 26;
    int y_btn = bg.y + 2;
    int btn_new_w = 52;
    int btn_del_w = 52;
    SDL_Rect btn_new = { left_offset, y_btn, btn_new_w, btn_h };
    SDL_Rect btn_del = { left_offset + btn_new_w + 8, y_btn, btn_del_w, btn_h };

    SDL_Color new_col = {70,70,70,255}, del_col = {70,70,70,255};
    if (SDL_GetTicks() - new_flash_time < 500) new_col = (SDL_Color){0,200,0,255};
    if (SDL_GetTicks() - del_flash_time < 500) del_col = (SDL_Color){200,0,0,255};
    SDL_SetRenderDrawColor(r, new_col.r, new_col.g, new_col.b, 255);
    SDL_RenderFillRect(r, &btn_new);
    SDL_SetRenderDrawColor(r, del_col.r, del_col.g, del_col.b, 255);
    SDL_RenderFillRect(r, &btn_del);

    SDL_Surface *snew = TTF_RenderUTF8_Blended(font, "New", (SDL_Color){255,255,255,255});
    if(snew){ SDL_Texture *tnew = SDL_CreateTextureFromSurface(r,snew); SDL_Rect dnew = {btn_new.x+4, btn_new.y+4, snew->w, snew->h}; SDL_RenderCopy(r,tnew,NULL,&dnew); SDL_DestroyTexture(tnew); SDL_FreeSurface(snew); }
    SDL_Surface *sdel = TTF_RenderUTF8_Blended(font, "Del", (SDL_Color){255,255,255,255});
    if(sdel){ SDL_Texture *tdel = SDL_CreateTextureFromSurface(r,sdel); SDL_Rect ddel = {btn_del.x+4, btn_del.y+4, sdel->w, sdel->h}; SDL_RenderCopy(r,tdel,NULL,&ddel); SDL_DestroyTexture(tdel); SDL_FreeSurface(sdel); }

    int arrow_btn_w = 26;
    int right_x = bg.x + bg.w - 10;
    SDL_Rect btn_prev = { right_x - 60, y_btn, arrow_btn_w, btn_h };
    SDL_Rect btn_next = { right_x - 30, y_btn, arrow_btn_w, btn_h };
    SDL_SetRenderDrawColor(r, 70,70,70,255); SDL_RenderFillRect(r, &btn_prev);
    SDL_SetRenderDrawColor(r, 70,70,70,255); SDL_RenderFillRect(r, &btn_next);

    SDL_Surface *sp = TTF_RenderUTF8_Blended(font, "<", (SDL_Color){255,255,255,255});
    if(sp){ SDL_Texture *tp = SDL_CreateTextureFromSurface(r,sp); SDL_Rect dp = {btn_prev.x+6,btn_prev.y+4,sp->w,sp->h}; SDL_RenderCopy(r,tp,NULL,&dp); SDL_DestroyTexture(tp); SDL_FreeSurface(sp); }
    SDL_Surface *sn = TTF_RenderUTF8_Blended(font, ">", (SDL_Color){255,255,255,255});
    if(sn){ SDL_Texture *tn = SDL_CreateTextureFromSurface(r,sn); SDL_Rect dn = {btn_next.x+6,btn_next.y+4,sn->w,sn->h}; SDL_RenderCopy(r,tn,NULL,&dn); SDL_DestroyTexture(tn); SDL_FreeSurface(sn); }
}

// ---------- Меню актеров ----------
void draw_actors_menu(SDL_Renderer *r, TTF_Font *font, ActorDB *actors, bool menu_open, int menu_count, SDL_Rect menu_rects[], int mx, int my) {
    if (!menu_open) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0,0,0,180);
    SDL_Rect full = { 0,0,WIN_W,WIN_H };
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    SDL_Rect menu_bg = { tag_btns[8].rect.x, tag_btns[8].rect.y + TAG_BTN_H + 2, 180, menu_count * 24 + 4 };
    SDL_SetRenderDrawColor(r, 60,60,60,255); SDL_RenderFillRect(r, &menu_bg);
    SDL_SetRenderDrawColor(r, 120,120,120,255); SDL_RenderDrawRect(r, &menu_bg);

    for (int i = 0; i < menu_count; i++) {
        SDL_Rect item = menu_rects[i];
        if (mx >= item.x && mx < item.x+item.w && my >= item.y && my < item.y+item.h)
            SDL_SetRenderDrawColor(r, 80,80,80,255);
        else
            SDL_SetRenderDrawColor(r, 70,70,70,255);
        SDL_RenderFillRect(r, &item);

        char label[128];
        snprintf(label, sizeof(label), "%s (id=%d)", actors->actors[i].name, actors->actors[i].id);
        SDL_Surface *s = TTF_RenderUTF8_Blended(font, label, (SDL_Color){255,255,255,255});
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
            SDL_Rect d = { item.x+2, item.y+2, s->w, s->h };
            SDL_RenderCopy(r, t, NULL, &d);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
}

// ---------- main ----------
int main(int argc, char* argv[]){
    SDL_Init(SDL_INIT_VIDEO); IMG_Init(IMG_INIT_PNG); TTF_Init();
    SDL_Window* window = SDL_CreateWindow("Shinzo Text Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* panelTexture = IMG_LoadTexture(renderer, "../assets/ui/message_panel.png");
    TTF_Font* font = TTF_OpenFont("../assets/ui/fonts/main.ttf", 20);
    TTF_Font* preview_font = TTF_OpenFont("../assets/ui/fonts/main.ttf", 30);

    ActorDB actors; load_actors(&actors);
    LineArray lines; la_init(&lines); la_add_empty(&lines);

    int current_idx = 0;
    int first_visible = 0;
    bool show_cursor = true; Uint32 blink_timer = SDL_GetTicks();
    SDL_Rect import_btn = {10,10,90,28}, export_btn = {110,10,90,28};

    bool actors_menu_open = false;
    SDL_Rect menu_rects[actors.count];
    int menu_count = actors.count;

    Uint32 new_flash_time = 0;
    Uint32 del_flash_time = 0;

    SDL_StartTextInput();
    int running = 1; SDL_Event event;
    while(running){
        while(SDL_PollEvent(&event)){
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            if(event.type==SDL_QUIT) running=0;
            else if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT){
                if (actors_menu_open) {
                    for (int i = 0; i < menu_count; i++) {
                        if (mx >= menu_rects[i].x && mx < menu_rects[i].x+menu_rects[i].w &&
                            my >= menu_rects[i].y && my < menu_rects[i].y+menu_rects[i].h) {
                            char tag[32];
                            snprintf(tag, sizeof(tag), "{NAME;%d}", actors.actors[i].id);
                            if (lines.count == 0) la_add_empty(&lines);
                            strncat(lines.data[current_idx].text, tag, MAX_LINE_BYTES - strlen(lines.data[current_idx].text) - 1);
                            actors_menu_open = false;
                            break;
                        }
                    }
                    if (actors_menu_open) {
                        SDL_Rect menu_bg = { tag_btns[8].rect.x, tag_btns[8].rect.y + TAG_BTN_H + 2, 180, menu_count * 24 + 4 };
                        if (!(mx >= menu_bg.x && mx < menu_bg.x+menu_bg.w && my >= menu_bg.y && my < menu_bg.y+menu_bg.h))
                            actors_menu_open = false;
                    }
                    continue;
                }

                if(mx>=import_btn.x&&mx<import_btn.x+import_btn.w && my>=import_btn.y&&my<import_btn.y+import_btn.h){
                    char path[260]; if(open_file_dialog(path,sizeof(path))){
                        FILE *f=fopen(path,"r"); if(f){ la_free(&lines); la_init(&lines);
                            char buf[MAX_LINE_BYTES]; while(fgets(buf,sizeof(buf),f)){
                                size_t l=strlen(buf); if(l>0&&buf[l-1]=='\n') buf[l-1]='\0';
                                char *eq = strchr(buf, '=');
                                if(eq && (eq - buf) <= 4 && (eq - buf) > 0) {
                                    int id_len = eq - buf;
                                    if (id_len > 7) id_len = 7;
                                    char id_str[8] = {0};
                                    strncpy(id_str, buf, id_len); id_str[7]='\0';
                                    la_add_empty(&lines);
                                    strncpy(lines.data[lines.count-1].id, id_str, sizeof(lines.data[lines.count-1].id)-1);
                                    lines.data[lines.count-1].id[sizeof(lines.data[lines.count-1].id)-1]='\0';
                                    strncpy(lines.data[lines.count-1].text, eq+1, MAX_LINE_BYTES-1);
                                    lines.data[lines.count-1].text[MAX_LINE_BYTES-1]='\0';
                                } else {
                                    la_add_empty(&lines);
                                    strncpy(lines.data[lines.count-1].text, buf, MAX_LINE_BYTES-1);
                                    lines.data[lines.count-1].text[MAX_LINE_BYTES-1]='\0';
                                }
                            }
                            fclose(f); if(lines.count==0) la_add_empty(&lines);
                            current_idx = 0; first_visible = 0;
                        }
                    }
                }else if(mx>=export_btn.x&&mx<export_btn.x+export_btn.w && my>=export_btn.y&&my<export_btn.y+export_btn.h){
                    char path[260]; if(save_file_dialog(path,sizeof(path))){
                        FILE *f=fopen(path,"w"); if(f){
                            // Сортируем копию массива перед записью
                            Line *sorted = (Line*)malloc(lines.count * sizeof(Line));
                            memcpy(sorted, lines.data, lines.count * sizeof(Line));
                            qsort(sorted, lines.count, sizeof(Line), compare_by_id);
                            for(int i=0;i<lines.count;i++){
                                if(sorted[i].id[0])
                                    fprintf(f, "%s=%s\n", sorted[i].id, sorted[i].text);
                                else
                                    fprintf(f, "%s\n", sorted[i].text);
                            }
                            free(sorted);
                            fclose(f);
                        }
                    }
                }else if(mx>=tag_btns[8].rect.x && mx<tag_btns[8].rect.x+tag_btns[8].rect.w && my>=tag_btns[8].rect.y && my<tag_btns[8].rect.y+tag_btns[8].rect.h){
                    actors_menu_open = !actors_menu_open;
                }else for(int i=0;i<8;i++){
                    if(mx>=tag_btns[i].rect.x&&mx<tag_btns[i].rect.x+tag_btns[i].rect.w && my>=tag_btns[i].rect.y&&my<tag_btns[i].rect.y+tag_btns[i].rect.h){
                        if(lines.count==0) la_add_empty(&lines);
                        char *last = lines.data[current_idx].text;
                        int cur = strlen(last), add = strlen(tag_btns[i].tag);
                        if(cur+add < MAX_LINE_BYTES-2) strcat(last, tag_btns[i].tag);
                    }
                }

                int panel_y = NOTE_Y - 35;
                if(my >= panel_y && my <= panel_y+30 && mx >= NOTE_X && mx <= NOTE_X+NOTE_W) {
                    int y_btn = panel_y+2;
                    char tmp_info[64];
                    snprintf(tmp_info, sizeof(tmp_info), "ID: %s (%d/%d)", lines.data[current_idx].id, current_idx+1, lines.count);
                    SDL_Surface *tmp_s = TTF_RenderUTF8_Blended(font, tmp_info, (SDL_Color){255,255,255,255});
                    int info_w = tmp_s ? tmp_s->w : 0;
                    if(tmp_s) SDL_FreeSurface(tmp_s);
                    int left_offset = NOTE_X + 5 + info_w + 15;
                    SDL_Rect btn_new = { left_offset, y_btn, 52, 26 };
                    SDL_Rect btn_del = { left_offset + 52 + 8, y_btn, 52, 26 };
                    SDL_Rect btn_prev = { NOTE_X + NOTE_W - 10 - 60, y_btn, 26, 26 };
                    SDL_Rect btn_next = { NOTE_X + NOTE_W - 10 - 30, y_btn, 26, 26 };

                    if(mx >= btn_prev.x && mx < btn_prev.x+btn_prev.w && my >= btn_prev.y && my < btn_prev.y+btn_prev.h){
                        if(current_idx > 0) { current_idx--; first_visible = current_idx; }
                    }else if(mx >= btn_next.x && mx < btn_next.x+btn_next.w && my >= btn_next.y && my < btn_next.y+btn_next.h){
                        if(current_idx < lines.count-1) { current_idx++; first_visible = current_idx - MAX_VISIBLE_LINES + 1; }
                    }else if(mx >= btn_new.x && mx < btn_new.x+btn_new.w && my >= btn_new.y && my < btn_new.y+btn_new.h){
                        la_add_empty(&lines);
                        generate_unique_id(&lines, lines.data[lines.count-1].id);
                        current_idx = lines.count-1;
                        first_visible = current_idx - MAX_VISIBLE_LINES + 1;
                        new_flash_time = SDL_GetTicks();
                    }else if(mx >= btn_del.x && mx < btn_del.x+btn_del.w && my >= btn_del.y && my < btn_del.y+btn_del.h){
                        if(lines.count > 1){
                            for(int i=current_idx; i<lines.count-1; i++) lines.data[i] = lines.data[i+1];
                            lines.count--;
                            if(current_idx >= lines.count) current_idx = lines.count-1;
                            first_visible = current_idx - MAX_VISIBLE_LINES + 1;
                            del_flash_time = SDL_GetTicks();
                        }
                    }
                }
            }else if(event.type==SDL_TEXTINPUT){
                if(lines.count==0) la_add_empty(&lines);
                char *last = lines.data[current_idx].text;
                int cur_len = strlen(last);
                if(cur_len + strlen(event.text.text) < MAX_LINE_BYTES-2) strcat(last, event.text.text);
            }else if(event.type==SDL_KEYDOWN){
                if(event.key.keysym.sym==SDLK_BACKSPACE){
                    if(lines.count==0) la_add_empty(&lines);
                    char *last = lines.data[current_idx].text;
                    int len = strlen(last);
                    if(len>0) utf8_backspace(last,len);
                    else if(lines.count>1 && current_idx>0){
                        // Удаляем пустую строку и переходим к предыдущей
                        for(int i=current_idx; i<lines.count-1; i++) lines.data[i] = lines.data[i+1];
                        lines.count--;
                        current_idx--;
                    }
                }else if(event.key.keysym.sym==SDLK_DELETE){
                    if(lines.count==0) la_add_empty(&lines);
                    Line *cur_line = &lines.data[current_idx];
                    int len = strlen(cur_line->text);
                    if(len > 0){
                        // Удаляем первый символ (как в чате)
                        memmove(cur_line->text, cur_line->text + utf8_char_len(cur_line->text[0]), len - utf8_char_len(cur_line->text[0]) + 1);
                    }else if(lines.count > 1 && current_idx < lines.count-1){
                        // Пустая строка: удаляем её и объединяем со следующей
                        for(int i=current_idx; i<lines.count-1; i++) lines.data[i] = lines.data[i+1];
                        lines.count--;
                    }
                }else if(event.key.keysym.sym==SDLK_RETURN){
                    la_add_empty(&lines);
                    generate_unique_id(&lines, lines.data[lines.count-1].id);
                    current_idx = lines.count-1;
                    first_visible = current_idx - MAX_VISIBLE_LINES + 1;
                }else if(event.key.keysym.sym==SDLK_ESCAPE) running=0;
                else if(event.key.keysym.sym==SDLK_UP && current_idx>0){ current_idx--; first_visible = current_idx; }
                else if(event.key.keysym.sym==SDLK_DOWN && current_idx<lines.count-1){ current_idx++; first_visible = current_idx - MAX_VISIBLE_LINES + 1; }
            }
        }

        if(first_visible < 0) first_visible = 0;
        if(first_visible + MAX_VISIBLE_LINES > lines.count) first_visible = lines.count - MAX_VISIBLE_LINES;
        if(first_visible < 0) first_visible = 0;

        if(SDL_GetTicks()-blink_timer>=530){ show_cursor=!show_cursor; blink_timer=SDL_GetTicks(); }
        generate_preview(&lines, &actors, current_idx);

        SDL_SetRenderDrawColor(renderer, 10,10,40,255); SDL_RenderClear(renderer);
        int mx,my; SDL_GetMouseState(&mx,&my);
        SDL_SetRenderDrawColor(renderer, (mx>=import_btn.x&&mx<import_btn.x+import_btn.w&&my>=import_btn.y&&my<import_btn.y+import_btn.h)?150:100,100,100,255); SDL_RenderFillRect(renderer,&import_btn);
        SDL_SetRenderDrawColor(renderer, (mx>=export_btn.x&&mx<export_btn.x+export_btn.w&&my>=export_btn.y&&my<export_btn.y+export_btn.h)?150:100,100,100,255); SDL_RenderFillRect(renderer,&export_btn);
        SDL_Surface *s = TTF_RenderUTF8_Blended(font,"Import",(SDL_Color){255,255,255,255}); if(s){ SDL_Texture *t=SDL_CreateTextureFromSurface(renderer,s); SDL_Rect r={import_btn.x+5,import_btn.y+2,s->w,s->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_DestroyTexture(t); SDL_FreeSurface(s); }
        s = TTF_RenderUTF8_Blended(font,"Export",(SDL_Color){255,255,255,255}); if(s){ SDL_Texture *t=SDL_CreateTextureFromSurface(renderer,s); SDL_Rect r={export_btn.x+5,export_btn.y+2,s->w,s->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_DestroyTexture(t); SDL_FreeSurface(s); }

        draw_tag_buttons(renderer, font);
        draw_id_panel(renderer, font, current_idx, lines.count, lines.data[current_idx].id, new_flash_time, del_flash_time);

        // Блокнот
        SDL_SetRenderDrawColor(renderer, 30,30,30,255);
        SDL_Rect note_rect = { NOTE_X-4, NOTE_Y-4, NOTE_W+8, NOTE_H+8 };
        SDL_RenderFillRect(renderer, &note_rect);
        SDL_SetRenderDrawColor(renderer, 80,80,80,255);
        SDL_RenderDrawRect(renderer, &note_rect);

        SDL_Rect clip = { NOTE_X, NOTE_Y, NOTE_W, NOTE_H };
        SDL_RenderSetClipRect(renderer, &clip);
        int line_y = NOTE_Y;
        int start = first_visible;
        int visible = lines.count - start;
        if(visible > MAX_VISIBLE_LINES) visible = MAX_VISIBLE_LINES;
        for(int i=start; i<start+visible; i++){
            if(i == current_idx) {
                SDL_Rect sel = { NOTE_X, line_y, NOTE_W, TTF_FontHeight(font)+2 };
                SDL_SetRenderDrawColor(renderer, 60,60,80,255);
                SDL_RenderFillRect(renderer, &sel);
            }
            char display_line[MAX_LINE_BYTES+16];
            snprintf(display_line, sizeof(display_line), "%s: %s", lines.data[i].id, lines.data[i].text);
            SDL_Surface *ls = TTF_RenderUTF8_Blended_Wrapped(font, display_line, (SDL_Color){255,255,200,255}, NOTE_W - 8);
            if(ls){
                SDL_Texture *lt = SDL_CreateTextureFromSurface(renderer, ls);
                SDL_Rect r = { NOTE_X+4, line_y, ls->w, ls->h };
                SDL_RenderCopy(renderer, lt, NULL, &r);
                SDL_DestroyTexture(lt);
                SDL_FreeSurface(ls);
                line_y += ls->h + LINE_SPACING;
            }else{
                line_y += TTF_FontHeight(font) + LINE_SPACING;
            }
        }
        if(show_cursor && lines.count > 0 && current_idx >= start && current_idx < start+visible) {
            int cursor_line_y = NOTE_Y + (current_idx - start)*(TTF_FontHeight(font)+LINE_SPACING);
            char prefix[16];
            snprintf(prefix, sizeof(prefix), "%s: ", lines.data[current_idx].id);
            int prefix_w; TTF_SizeUTF8(font, prefix, &prefix_w, NULL);
            int text_w; TTF_SizeUTF8(font, lines.data[current_idx].text, &text_w, NULL);
            int cx = NOTE_X+4 + prefix_w + text_w;
            if(cx > NOTE_X + NOTE_W - 5) cx = NOTE_X + NOTE_W - 5;
            SDL_SetRenderDrawColor(renderer, 255,255,255,255);
            SDL_RenderDrawLine(renderer, cx, cursor_line_y, cx, cursor_line_y + TTF_FontHeight(font));
        }
        SDL_RenderSetClipRect(renderer, NULL);

        if (actors_menu_open) {
            int mx_pos = tag_btns[8].rect.x;
            int my_pos = tag_btns[8].rect.y + TAG_BTN_H + 2;
            for (int i = 0; i < menu_count; i++) {
                menu_rects[i].x = mx_pos;
                menu_rects[i].y = my_pos + i * 24;
                menu_rects[i].w = 180;
                menu_rects[i].h = 24;
            }
            draw_actors_menu(renderer, font, &actors, true, menu_count, menu_rects, mx, my);
        }

        if(panelTexture){
            SDL_Rect r = { PANEL_X, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT };
            SDL_RenderCopy(renderer, panelTexture, NULL, &r);
            int ly = PANEL_Y + MARGIN_TOP;
            for(int i=0;i<preview_line_count;i++){
                SDL_Surface *ps = TTF_RenderUTF8_Blended(preview_font, preview_lines[i].text, (SDL_Color){255,255,255,255});
                if(ps){
                    SDL_Texture *pt = SDL_CreateTextureFromSurface(renderer, ps);
                    SDL_Rect d = { PANEL_X+MARGIN_LEFT, ly, ps->w, ps->h };
                    SDL_RenderCopy(renderer, pt, NULL, &d);
                    SDL_DestroyTexture(pt); SDL_FreeSurface(ps);
                }
                ly += TTF_FontHeight(preview_font) + LINE_SPACING;
            }
        }

        SDL_RenderPresent(renderer); SDL_Delay(16);
    }

    SDL_StopTextInput();
    if(panelTexture) SDL_DestroyTexture(panelTexture);
    TTF_CloseFont(font);
    TTF_CloseFont(preview_font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    la_free(&lines);
    free(actors.actors);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}