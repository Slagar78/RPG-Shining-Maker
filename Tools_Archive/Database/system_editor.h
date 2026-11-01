#ifndef SYSTEM_EDITOR_H
#define SYSTEM_EDITOR_H

#include <SDL.h>
#include "../cJSON.h"

// Инициализация (загрузка global.json)
int system_init(void);

// Отрисовка панели редактирования
void system_draw_edit_panel(SDL_Renderer *renderer, int x, int y);

// Обработка ввода (мышь, клавиатура)
void system_handle_input(SDL_Event *event);

// Освобождение памяти
void system_free(void);

// Сброс выделения (при переключении вкладок)
void system_reset_selection(void);

// Проверка, активно ли поле ввода (чтобы не перехватывать глобальные события)
int system_is_edit_active(void);

// Обновление таймера (для мигания кнопки Save)
void system_update_timer(void);

#endif