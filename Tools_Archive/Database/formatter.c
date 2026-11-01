// formatter.c
#include "formatter.h"
#include "../cJSON.h"
#include <stdio.h>
#include <stdlib.h>

static void minify_json_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("Не удалось открыть файл: %s\n", filename);
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        return;
    }

    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        printf("Ошибка парсинга JSON: %s\n", filename);
        return;
    }

    // === ИСПРАВЛЕННЫЙ ВАРИАНТ ===        
    char *js = cJSON_PrintBuffered(root, 0, 1);

    f = fopen(filename, "w");
    if (f) {
        fputs(js, f);
        fclose(f);
        printf("Отформатировано: %s\n", filename);
    } else {
        printf("Не удалось записать файл: %s\n", filename);
    }

    free(js);
    cJSON_Delete(root);
}

void format_all_json(void) {
    minify_json_file("../data/spells/spells.json");
    minify_json_file("../data/items/items.json");
    minify_json_file("../data/actors/actors.json");
    minify_json_file("../data/actors/classes.json");
}