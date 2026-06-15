#include "events.h"
#include "editor.h"          // чтобы видеть поля Editor (roof_events, map_list и т.д.)
#include <stdio.h>
#include <string.h>
#include <windows.h>         // CreateDirectoryA
#include "cJSON.h"

void load_events_from_json(Editor *ed, const char *folder) {
    // Сбрасываем все события
    ed->roof_event_count = 0;
    ed->tile_change_count = 0;
    ed->stair_event_count = 0;
    ed->warp_event_count = 0;
    ed->selected_roof_event = -1;
    ed->selected_tile_change = -1;
    ed->selected_stair = -1;
    ed->selected_warp = -1;
    ed->edit_field = -1;
    ed->tc_edit_field = -1;
    ed->stair_edit_field = -1;
    ed->warp_edit_field = -1;
    ed->input_buf[0] = '\0';
    ed->tc_input_buf[0] = '\0';
    ed->stair_input_buf[0] = '\0';
    ed->warp_input_buf[0] = '\0';

    char path[512];
    snprintf(path, sizeof(path), "../data/maps/%s/events.json", folder);
    FILE *f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(len + 1);
    if (!data) { fclose(f); return; }
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *arr = cJSON_Parse(data);
    free(data);
    if (!arr || !cJSON_IsArray(arr)) {
        if (arr) cJSON_Delete(arr);
        return;
    }

    int count = cJSON_GetArraySize(arr);
    for (int i = 0; i < count; i++) {
        cJSON *ev = cJSON_GetArrayItem(arr, i);
        if (!ev) continue;

        cJSON *type_json = cJSON_GetObjectItem(ev, "type");
        const char *type = type_json ? type_json->valuestring : "roof";

        if (strcmp(type, "roof") == 0) {
            if (ed->roof_event_count < MAX_ROOF_EVENTS) {
                cJSON *tid = cJSON_GetObjectItem(ev, "tile_id");
                cJSON *sx  = cJSON_GetObjectItem(ev, "start_x");
                cJSON *sy  = cJSON_GetObjectItem(ev, "start_y");
                cJSON *ex  = cJSON_GetObjectItem(ev, "end_x");
                cJSON *ey  = cJSON_GetObjectItem(ev, "end_y");

                if (tid && sx && sy && ex && ey) {
                    RoofEvent *re = &ed->roof_events[ed->roof_event_count++];
                    re->tile_id = tid->valueint;
                    re->start_x = sx->valueint;
                    re->start_y = sy->valueint;
                    re->end_x = ex->valueint;
                    re->end_y = ey->valueint;

                    // Чтение triggers (массив или одиночные поля)
                    cJSON *triggers_arr = cJSON_GetObjectItem(ev, "triggers");
                    if (triggers_arr && cJSON_IsArray(triggers_arr)) {
                        int n = cJSON_GetArraySize(triggers_arr);
                        if (n > 0) {
                            cJSON *t0 = cJSON_GetArrayItem(triggers_arr, 0);
                            if (t0 && cJSON_IsArray(t0) && cJSON_GetArraySize(t0) == 2) {
                                re->trigger_x = cJSON_GetArrayItem(t0, 0)->valueint;
                                re->trigger_y = cJSON_GetArrayItem(t0, 1)->valueint;
                            } else {
                                re->trigger_x = re->trigger_y = -1;
                            }
                            if (n > 1) {
                                cJSON *t1 = cJSON_GetArrayItem(triggers_arr, 1);
                                if (t1 && cJSON_IsArray(t1) && cJSON_GetArraySize(t1) == 2) {
                                    re->trigger2_x = cJSON_GetArrayItem(t1, 0)->valueint;
                                    re->trigger2_y = cJSON_GetArrayItem(t1, 1)->valueint;
                                } else {
                                    re->trigger2_x = re->trigger2_y = -1;
                                }
                            } else {
                                re->trigger2_x = re->trigger2_y = -1;
                            }
                        }
                    } else {
                        // Обратная совместимость
                        cJSON *tx = cJSON_GetObjectItem(ev, "trigger_x");
                        cJSON *ty = cJSON_GetObjectItem(ev, "trigger_y");
                        re->trigger_x = (tx && cJSON_IsNumber(tx)) ? tx->valueint : -1;
                        re->trigger_y = (ty && cJSON_IsNumber(ty)) ? ty->valueint : -1;
                        cJSON *t2x = cJSON_GetObjectItem(ev, "trigger2_x");
                        cJSON *t2y = cJSON_GetObjectItem(ev, "trigger2_y");
                        re->trigger2_x = (t2x && cJSON_IsNumber(t2x)) ? t2x->valueint : -1;
                        re->trigger2_y = (t2y && cJSON_IsNumber(t2y)) ? t2y->valueint : -1;
                    }

                    // Чтение exits (массив или одиночные поля)
                    cJSON *exits_arr = cJSON_GetObjectItem(ev, "exits");
                    if (exits_arr && cJSON_IsArray(exits_arr)) {
                        int n = cJSON_GetArraySize(exits_arr);
                        if (n > 0) {
                            cJSON *e0 = cJSON_GetArrayItem(exits_arr, 0);
                            if (e0 && cJSON_IsArray(e0) && cJSON_GetArraySize(e0) == 2) {
                                re->exit_x = cJSON_GetArrayItem(e0, 0)->valueint;
                                re->exit_y = cJSON_GetArrayItem(e0, 1)->valueint;
                            } else {
                                re->exit_x = re->exit_y = -1;
                            }
                            if (n > 1) {
                                cJSON *e1 = cJSON_GetArrayItem(exits_arr, 1);
                                if (e1 && cJSON_IsArray(e1) && cJSON_GetArraySize(e1) == 2) {
                                    re->exit2_x = cJSON_GetArrayItem(e1, 0)->valueint;
                                    re->exit2_y = cJSON_GetArrayItem(e1, 1)->valueint;
                                } else {
                                    re->exit2_x = re->exit2_y = -1;
                                }
                            } else {
                                re->exit2_x = re->exit2_y = -1;
                            }
                        }
                    } else {
                        cJSON *ex_x = cJSON_GetObjectItem(ev, "exit_x");
                        cJSON *ex_y = cJSON_GetObjectItem(ev, "exit_y");
                        re->exit_x = (ex_x && cJSON_IsNumber(ex_x)) ? ex_x->valueint : -1;
                        re->exit_y = (ex_y && cJSON_IsNumber(ex_y)) ? ex_y->valueint : -1;
                        cJSON *ex2x = cJSON_GetObjectItem(ev, "exit2_x");
                        cJSON *ex2y = cJSON_GetObjectItem(ev, "exit2_y");
                        re->exit2_x = (ex2x && cJSON_IsNumber(ex2x)) ? ex2x->valueint : -1;
                        re->exit2_y = (ex2y && cJSON_IsNumber(ex2y)) ? ex2y->valueint : -1;
                    }
                }
            }
        }
        else if (strcmp(type, "tile_change") == 0) {
            if (ed->tile_change_count < MAX_TILE_CHANGES) {
                cJSON *tx = cJSON_GetObjectItem(ev, "trigger_x");
                cJSON *ty = cJSON_GetObjectItem(ev, "trigger_y");
                cJSON *ntid = cJSON_GetObjectItem(ev, "new_tile_id");
                if (tx && ty && ntid) {
                    TileChangeEvent *tc = &ed->tile_changes[ed->tile_change_count++];
                    tc->trigger_x = tx->valueint;
                    tc->trigger_y = ty->valueint;
                    tc->new_tile_id = ntid->valueint;
                    cJSON *sx = cJSON_GetObjectItem(ev, "sample_x");
                    cJSON *sy = cJSON_GetObjectItem(ev, "sample_y");
                    tc->sample_x = (sx && cJSON_IsNumber(sx)) ? sx->valueint : -1;
                    tc->sample_y = (sy && cJSON_IsNumber(sy)) ? sy->valueint : -1;
                    cJSON *closex = cJSON_GetObjectItem(ev, "close_x");
                    cJSON *closey = cJSON_GetObjectItem(ev, "close_y");
                    tc->close_x = (closex && cJSON_IsNumber(closex)) ? closex->valueint : -1;
                    tc->close_y = (closey && cJSON_IsNumber(closey)) ? closey->valueint : -1;
                }
            }
        }
        else if (strcmp(type, "stairs") == 0) {
            if (ed->stair_event_count < MAX_STAIRS) {
                cJSON *sx = cJSON_GetObjectItem(ev, "start_x");
                cJSON *sy = cJSON_GetObjectItem(ev, "start_y");
                cJSON *ex = cJSON_GetObjectItem(ev, "end_x");
                cJSON *ey = cJSON_GetObjectItem(ev, "end_y");
                if (sx && sy && ex && ey) {
                    StairEvent *se = &ed->stair_events[ed->stair_event_count++];
                    se->start_x = sx->valueint;
                    se->start_y = sy->valueint;
                    se->end_x = ex->valueint;
                    se->end_y = ey->valueint;
                    cJSON *dir = cJSON_GetObjectItem(ev, "direction");
                    se->direction = (dir && cJSON_IsNumber(dir)) ? dir->valueint : 0;
                }
            }
        }
        else if (strcmp(type, "warp") == 0) {
            if (ed->warp_event_count < MAX_WARPS) {
                cJSON *tx = cJSON_GetObjectItem(ev, "trigger_x");
                cJSON *ty = cJSON_GetObjectItem(ev, "trigger_y");
                cJSON *tmap = cJSON_GetObjectItem(ev, "target_map");
                cJSON *tox = cJSON_GetObjectItem(ev, "target_x");
                cJSON *toy = cJSON_GetObjectItem(ev, "target_y");
                cJSON *facing = cJSON_GetObjectItem(ev, "facing");
                if (tx && ty && tmap && tox && toy && facing) {
                    WarpEvent *we = &ed->warp_events[ed->warp_event_count++];
                    we->trigger_x = tx->valueint;
                    we->trigger_y = ty->valueint;
                    snprintf(we->target_map, sizeof(we->target_map), "%s", tmap->valuestring);
                    we->target_map[63] = '\0';
                    we->target_x = tox->valueint;
                    we->target_y = toy->valueint;
                    we->facing = facing->valueint;
                    // Конвертация старых значений (2,4,6,8) в новые 0..3
                    if (we->facing == 2) we->facing = 0;       // Down
                    else if (we->facing == 4) we->facing = 1;  // Left
                    else if (we->facing == 6) we->facing = 2;  // Right
                    else if (we->facing == 8) we->facing = 3;  // Up
                    // Если уже 0..3 — оставляем как есть
                }
            }
        }
    }
    cJSON_Delete(arr);
}

void save_events_to_json(Editor *ed, const char *folder) {
    char dir[512];
    snprintf(dir, sizeof(dir), "../data/maps/%s", folder);
    CreateDirectoryA(dir, NULL);
    char filename[768];
    snprintf(filename, sizeof(filename), "%s/events.json", dir);

    cJSON *root = cJSON_CreateArray();

    // Сохраняем крыши
    for (int i = 0; i < ed->roof_event_count; i++) {
        RoofEvent *re = &ed->roof_events[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "roof");
        cJSON_AddNumberToObject(ev, "tile_id", re->tile_id);
        cJSON_AddNumberToObject(ev, "start_x", re->start_x);
        cJSON_AddNumberToObject(ev, "start_y", re->start_y);
        cJSON_AddNumberToObject(ev, "end_x", re->end_x);
        cJSON_AddNumberToObject(ev, "end_y", re->end_y);

        // triggers
        cJSON *triggers = cJSON_CreateArray();
        if (re->trigger_x != -1 || re->trigger_y != -1) {
            cJSON *t1 = cJSON_CreateArray();
            cJSON_AddItemToArray(t1, cJSON_CreateNumber(re->trigger_x));
            cJSON_AddItemToArray(t1, cJSON_CreateNumber(re->trigger_y));
            cJSON_AddItemToArray(triggers, t1);
        }
        if (re->trigger2_x != -1 || re->trigger2_y != -1) {
            cJSON *t2 = cJSON_CreateArray();
            cJSON_AddItemToArray(t2, cJSON_CreateNumber(re->trigger2_x));
            cJSON_AddItemToArray(t2, cJSON_CreateNumber(re->trigger2_y));
            cJSON_AddItemToArray(triggers, t2);
        }
        cJSON_AddItemToObject(ev, "triggers", triggers);

        // exits
        cJSON *exits = cJSON_CreateArray();
        if (re->exit_x != -1 || re->exit_y != -1) {
            cJSON *e1 = cJSON_CreateArray();
            cJSON_AddItemToArray(e1, cJSON_CreateNumber(re->exit_x));
            cJSON_AddItemToArray(e1, cJSON_CreateNumber(re->exit_y));
            cJSON_AddItemToArray(exits, e1);
        }
        if (re->exit2_x != -1 || re->exit2_y != -1) {
            cJSON *e2 = cJSON_CreateArray();
            cJSON_AddItemToArray(e2, cJSON_CreateNumber(re->exit2_x));
            cJSON_AddItemToArray(e2, cJSON_CreateNumber(re->exit2_y));
            cJSON_AddItemToArray(exits, e2);
        }
        cJSON_AddItemToObject(ev, "exits", exits);

        cJSON_AddItemToArray(root, ev);
    }

    // Сохраняем замены тайлов
    for (int i = 0; i < ed->tile_change_count; i++) {
        TileChangeEvent *tc = &ed->tile_changes[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "tile_change");
        cJSON_AddNumberToObject(ev, "trigger_x", tc->trigger_x);
        cJSON_AddNumberToObject(ev, "trigger_y", tc->trigger_y);
        cJSON_AddNumberToObject(ev, "new_tile_id", tc->new_tile_id);
        cJSON_AddNumberToObject(ev, "sample_x", tc->sample_x);
        cJSON_AddNumberToObject(ev, "sample_y", tc->sample_y);
        cJSON_AddNumberToObject(ev, "close_x", tc->close_x);
        cJSON_AddNumberToObject(ev, "close_y", tc->close_y);
        cJSON_AddItemToArray(root, ev);
    }

    // Сохраняем лестницы
    for (int i = 0; i < ed->stair_event_count; i++) {
        StairEvent *se = &ed->stair_events[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "stairs");
        cJSON_AddNumberToObject(ev, "start_x", se->start_x);
        cJSON_AddNumberToObject(ev, "start_y", se->start_y);
        cJSON_AddNumberToObject(ev, "end_x", se->end_x);
        cJSON_AddNumberToObject(ev, "end_y", se->end_y);
        cJSON_AddNumberToObject(ev, "direction", se->direction);
        cJSON_AddItemToArray(root, ev);
    }

    // Сохраняем варпы
    for (int i = 0; i < ed->warp_event_count; i++) {
        WarpEvent *we = &ed->warp_events[i];
        cJSON *ev = cJSON_CreateObject();
        cJSON_AddStringToObject(ev, "type", "warp");
        cJSON_AddNumberToObject(ev, "trigger_x", we->trigger_x);
        cJSON_AddNumberToObject(ev, "trigger_y", we->trigger_y);
        cJSON_AddStringToObject(ev, "target_map", we->target_map);
        cJSON_AddNumberToObject(ev, "target_x", we->target_x);
        cJSON_AddNumberToObject(ev, "target_y", we->target_y);
        cJSON_AddNumberToObject(ev, "facing", we->facing);
        cJSON_AddItemToArray(root, ev);
    }

    char *str = cJSON_Print(root);
    FILE *f = fopen(filename, "w");
    if (f) {
        fputs(str, f);
        fclose(f);
    }
    cJSON_Delete(root);
    free(str);
}