// RpgShinzoMaker.Core/Services/EventsService.cs
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Чтение и запись событий карты.
/// Работает с тремя файлами в папке data/maps/&lt;folder&gt;/:
///   • events.json       — крыши, tile changes, лестницы, варпы
///   • NPC_events.json   — NPC (без text_id)
///   • NPC_script.json   — диалоговые скрипты NPC (генерируется из TextId)
/// </summary>
public static class EventsService
{
    // ══════════════════════════════════════════════════════════════
    //   ПУТИ
    // ══════════════════════════════════════════════════════════════
    private static string GetMapDir(string folder)
        => Path.Combine(ProjectPaths.MapsDir, folder);

    private static string GetEventsFile(string folder)
        => Path.Combine(GetMapDir(folder), "events.json");

    private static string GetNpcEventsFile(string folder)
        => Path.Combine(GetMapDir(folder), "NPC_events.json");

    private static string GetNpcScriptFile(string folder)
        => Path.Combine(GetMapDir(folder), "NPC_script.json");

    // ══════════════════════════════════════════════════════════════
    //   LOAD
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Читает events.json, NPC_events.json и NPC_script.json.
    /// Если какого-то файла нет — возвращает пустой список для этого типа.
    /// </summary>
    public static MapEvents Load(string folder)
    {
        var events = new MapEvents();

        if (string.IsNullOrEmpty(folder)) return events;

        // ─── events.json ───
        var eventsFile = GetEventsFile(folder);
        if (File.Exists(eventsFile))
        {
            try
            {
                var arr = JsonNode.Parse(File.ReadAllText(eventsFile))?.AsArray();
                if (arr != null)
                {
                    foreach (var item in arr)
                    {
                        var obj = item?.AsObject();
                        if (obj == null) continue;

                        string type = obj["type"]?.GetValue<string>() ?? "";
                        switch (type)
                        {
                            case "roof":        ParseRoof(obj, events);        break;
                            case "tile_change": ParseTileChange(obj, events);  break;
                            case "stairs":      ParseStairs(obj, events);      break;
                            case "warp":        ParseWarp(obj, events);        break;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[EVENTS] events.json error: {ex.Message}");
            }
        }

        // ─── NPC_events.json ───
        var npcFile = GetNpcEventsFile(folder);
        if (File.Exists(npcFile))
        {
            try
            {
                var arr = JsonNode.Parse(File.ReadAllText(npcFile))?.AsArray();
                if (arr != null)
                {
                    foreach (var item in arr)
                    {
                        var obj = item?.AsObject();
                        if (obj == null) continue;

                        events.Npcs.Add(new NpcEvent
                        {
                            Id        = obj["id"]?.GetValue<string>()        ?? "",
                            X         = obj["x"]?.GetValue<int>()            ?? 0,
                            Y         = obj["y"]?.GetValue<int>()            ?? 0,
                            Sprite    = obj["sprite"]?.GetValue<string>()    ?? "",
                            Behavior  = obj["behavior"]?.GetValue<string>()  ?? "static",
                            Direction = obj["direction"]?.GetValue<string>() ?? "down",
                            HomeX     = obj["home_x"]?.GetValue<int>()       ?? 0,
                            HomeY     = obj["home_y"]?.GetValue<int>()       ?? 0,
                            Radius    = obj["radius"]?.GetValue<int>()       ?? 3,
                            TextId    = ""
                        });
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[EVENTS] NPC_events.json error: {ex.Message}");
            }
        }

        // ─── NPC_script.json — заполняем TextId ───
        var scriptFile = GetNpcScriptFile(folder);
        if (File.Exists(scriptFile))
        {
            try
            {
                var arr = JsonNode.Parse(File.ReadAllText(scriptFile))?.AsArray();
                if (arr != null)
                {
                    foreach (var item in arr)
                    {
                        var obj = item?.AsObject();
                        if (obj == null) continue;

                        string npcId = obj["npc_id"]?.GetValue<string>() ?? "";
                        if (string.IsNullOrEmpty(npcId)) continue;

                        var npc = events.Npcs.FirstOrDefault(n => n.Id == npcId);
                        if (npc == null) continue;

                        var scriptArr = obj["script"]?.AsArray();
                        if (scriptArr == null) continue;

                        var sayIds = new List<string>();
                        foreach (var cmd in scriptArr)
                        {
                            var cmdObj = cmd?.AsObject();
                            if (cmdObj == null) continue;

                            string cmdType = cmdObj["type"]?.GetValue<string>() ?? "";
                            if (cmdType == "say")
                            {
                                string tid = cmdObj["text_id"]?.GetValue<string>() ?? "";
                                if (!string.IsNullOrEmpty(tid)) sayIds.Add(tid);
                            }
                        }

                        if (sayIds.Count > 0)
                            npc.TextId = string.Join(",", sayIds);
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[EVENTS] NPC_script.json error: {ex.Message}");
            }
        }

        return events;
    }

    // ══════════════════════════════════════════════════════════════
    //   ПАРСЕРЫ ОТДЕЛЬНЫХ ТИПОВ
    // ══════════════════════════════════════════════════════════════
    private static void ParseRoof(JsonObject obj, MapEvents events)
    {
        var roof = new RoofEvent
        {
            TileId = obj["tile_id"]?.GetValue<int>() ?? 0,
            StartX = obj["start_x"]?.GetValue<int>() ?? 0,
            StartY = obj["start_y"]?.GetValue<int>() ?? 0,
            EndX   = obj["end_x"]?.GetValue<int>()   ?? 1,
            EndY   = obj["end_y"]?.GetValue<int>()   ?? 1,
        };

        // triggers — массив массивов: [[x, y], [x2, y2]]
        var triggers = obj["triggers"]?.AsArray();
        if (triggers != null && triggers.Count > 0)
        {
            var t0 = triggers[0]?.AsArray();
            if (t0 != null && t0.Count >= 2)
            {
                roof.TriggerX = t0[0]?.GetValue<int>() ?? -1;
                roof.TriggerY = t0[1]?.GetValue<int>() ?? -1;
            }
        }
        if (triggers != null && triggers.Count > 1)
        {
            var t1 = triggers[1]?.AsArray();
            if (t1 != null && t1.Count >= 2)
            {
                roof.Trigger2X = t1[0]?.GetValue<int>() ?? -1;
                roof.Trigger2Y = t1[1]?.GetValue<int>() ?? -1;
            }
        }

        // exits — массив массивов: [[x, y], [x2, y2]]
        var exits = obj["exits"]?.AsArray();
        if (exits != null && exits.Count > 0)
        {
            var e0 = exits[0]?.AsArray();
            if (e0 != null && e0.Count >= 2)
            {
                roof.ExitX = e0[0]?.GetValue<int>() ?? -1;
                roof.ExitY = e0[1]?.GetValue<int>() ?? -1;
            }
        }
        if (exits != null && exits.Count > 1)
        {
            var e1 = exits[1]?.AsArray();
            if (e1 != null && e1.Count >= 2)
            {
                roof.Exit2X = e1[0]?.GetValue<int>() ?? -1;
                roof.Exit2Y = e1[1]?.GetValue<int>() ?? -1;
            }
        }

        events.Roofs.Add(roof);
    }

    private static void ParseTileChange(JsonObject obj, MapEvents events)
    {
        events.TileChanges.Add(new TileChangeEvent
        {
            TriggerX  = obj["trigger_x"]?.GetValue<int>()   ?? -1,
            TriggerY  = obj["trigger_y"]?.GetValue<int>()   ?? -1,
            NewTileId = obj["new_tile_id"]?.GetValue<int>() ?? 0,
            SampleX   = obj["sample_x"]?.GetValue<int>()    ?? -1,
            SampleY   = obj["sample_y"]?.GetValue<int>()    ?? -1,
            CloseX    = obj["close_x"]?.GetValue<int>()     ?? -1,
            CloseY    = obj["close_y"]?.GetValue<int>()     ?? -1,
        });
    }

    private static void ParseStairs(JsonObject obj, MapEvents events)
    {
        events.Stairs.Add(new StairEvent
        {
            StartX    = obj["start_x"]?.GetValue<int>()   ?? 0,
            StartY    = obj["start_y"]?.GetValue<int>()   ?? 0,
            EndX      = obj["end_x"]?.GetValue<int>()     ?? 1,
            EndY      = obj["end_y"]?.GetValue<int>()     ?? 1,
            Direction = obj["direction"]?.GetValue<int>() ?? 0,
        });
    }

    private static void ParseWarp(JsonObject obj, MapEvents events)
    {
        // Старые данные хранят facing как 2/4/6/8 — конвертируем в 0..3
        int facing = obj["facing"]?.GetValue<int>() ?? 0;
        facing = facing switch
        {
            2 => 0,
            4 => 1,
            6 => 2,
            8 => 3,
            _ => facing
        };

        events.Warps.Add(new WarpEvent
        {
            TriggerX  = obj["trigger_x"]?.GetValue<int>()   ?? -1,
            TriggerY  = obj["trigger_y"]?.GetValue<int>()   ?? -1,
            TargetMap = obj["target_map"]?.GetValue<string>() ?? "",
            TargetX   = obj["target_x"]?.GetValue<int>()     ?? 0,
            TargetY   = obj["target_y"]?.GetValue<int>()     ?? 0,
            Facing    = facing,
        });
    }

    // ══════════════════════════════════════════════════════════════
    //   SAVE
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Пишет три файла: events.json, NPC_events.json, NPC_script.json.
    /// Каталог создаётся, если его нет.
    /// </summary>
    public static void Save(string folder, MapEvents events)
    {
        if (string.IsNullOrEmpty(folder)) return;

        var dir = GetMapDir(folder);
        try { Directory.CreateDirectory(dir); }
        catch (Exception ex)
        {
            Debug.WriteLine($"[EVENTS] Can't create dir: {ex.Message}");
            return;
        }

        SaveEventsFile(folder, events);
        SaveNpcEventsFile(folder, events);
        SaveNpcScriptFile(folder, events);
    }

    private static void SaveEventsFile(string folder, MapEvents events)
    {
        var jsonOptions = new JsonSerializerOptions { WriteIndented = true };
        var root = new JsonArray();

        // Roofs
        foreach (var r in events.Roofs)
        {
            var triggers = new JsonArray();
            if (r.TriggerX  != -1 || r.TriggerY  != -1)
                triggers.Add(new JsonArray { r.TriggerX, r.TriggerY });
            if (r.Trigger2X != -1 || r.Trigger2Y != -1)
                triggers.Add(new JsonArray { r.Trigger2X, r.Trigger2Y });

            var exits = new JsonArray();
            if (r.ExitX  != -1 || r.ExitY  != -1)
                exits.Add(new JsonArray { r.ExitX, r.ExitY });
            if (r.Exit2X != -1 || r.Exit2Y != -1)
                exits.Add(new JsonArray { r.Exit2X, r.Exit2Y });

            root.Add(new JsonObject
            {
                ["type"]     = "roof",
                ["tile_id"]  = r.TileId,
                ["start_x"]  = r.StartX,
                ["start_y"]  = r.StartY,
                ["end_x"]    = r.EndX,
                ["end_y"]    = r.EndY,
                ["triggers"] = triggers,
                ["exits"]    = exits,
            });
        }

        // Tile changes
        foreach (var t in events.TileChanges)
        {
            root.Add(new JsonObject
            {
                ["type"]        = "tile_change",
                ["trigger_x"]   = t.TriggerX,
                ["trigger_y"]   = t.TriggerY,
                ["new_tile_id"] = t.NewTileId,
                ["sample_x"]    = t.SampleX,
                ["sample_y"]    = t.SampleY,
                ["close_x"]     = t.CloseX,
                ["close_y"]     = t.CloseY,
            });
        }

        // Stairs
        foreach (var s in events.Stairs)
        {
            root.Add(new JsonObject
            {
                ["type"]      = "stairs",
                ["start_x"]   = s.StartX,
                ["start_y"]   = s.StartY,
                ["end_x"]     = s.EndX,
                ["end_y"]     = s.EndY,
                ["direction"] = s.Direction,
            });
        }

        // Warps
        foreach (var w in events.Warps)
        {
            root.Add(new JsonObject
            {
                ["type"]       = "warp",
                ["trigger_x"]  = w.TriggerX,
                ["trigger_y"]  = w.TriggerY,
                ["target_map"] = w.TargetMap,
                ["target_x"]   = w.TargetX,
                ["target_y"]   = w.TargetY,
                ["facing"]     = w.Facing,
            });
        }

        try
        {
            File.WriteAllText(GetEventsFile(folder), root.ToJsonString(jsonOptions));
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[EVENTS] events.json write error: {ex.Message}");
        }
    }

    private static void SaveNpcEventsFile(string folder, MapEvents events)
    {
        var jsonOptions = new JsonSerializerOptions { WriteIndented = true };
        var root = new JsonArray();

        foreach (var npc in events.Npcs)
        {
            var obj = new JsonObject
            {
                ["id"]        = npc.Id,
                ["x"]         = npc.X,
                ["y"]         = npc.Y,
                ["sprite"]    = npc.Sprite,
                ["behavior"]  = npc.Behavior,
                ["direction"] = npc.Direction,
            };

            if (npc.Behavior == "wander")
            {
                obj["home_x"] = npc.HomeX;
                obj["home_y"] = npc.HomeY;
                obj["radius"] = npc.Radius;
            }

            root.Add(obj);
        }

        try
        {
            File.WriteAllText(GetNpcEventsFile(folder), root.ToJsonString(jsonOptions));
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[EVENTS] NPC_events.json write error: {ex.Message}");
        }
    }

    private static void SaveNpcScriptFile(string folder, MapEvents events)
    {
        var jsonOptions = new JsonSerializerOptions { WriteIndented = true };
        var root = new JsonArray();

        foreach (var npc in events.Npcs)
        {
            if (string.IsNullOrWhiteSpace(npc.TextId)) continue;

            var ids = npc.TextId
                .Split(',')
                .Select(s => s.Trim())
                .Where(s => !string.IsNullOrEmpty(s))
                .ToList();

            if (ids.Count == 0) continue;

            var script = new JsonArray
            {
                new JsonObject { ["type"] = "turn_to_player" },
                new JsonObject { ["type"] = "turn_player_to_npc" },
            };

            foreach (var tid in ids)
            {
                script.Add(new JsonObject
                {
                    ["type"]    = "say",
                    ["text_id"] = tid,
                });
            }

            script.Add(new JsonObject
            {
                ["type"]           = "end",
                ["wait_for_input"] = true,
            });

            root.Add(new JsonObject
            {
                ["npc_id"] = npc.Id,
                ["script"] = script,
            });
        }

        try
        {
            File.WriteAllText(GetNpcScriptFile(folder), root.ToJsonString(jsonOptions));
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[EVENTS] NPC_script.json write error: {ex.Message}");
        }
    }
}