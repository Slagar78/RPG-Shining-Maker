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

public static class EventsService
{
    private static string GetMapDir(string folder) => Path.Combine(ProjectPaths.MapsDir, folder);

    // ══════════════════════════════════════════════════════════════
    //   LOAD
    // ══════════════════════════════════════════════════════════════
    public static MapEvents Load(string folder, string versionId = "v00")
    {
        var events = new MapEvents();
        if (string.IsNullOrEmpty(folder)) return events;

        string mapDir = GetMapDir(folder);
        MapSetupVersion? chosen = null;

        // 1. Читаем mapsetups.json (если есть)
        var setupFile = ProjectPaths.MapSetupsFile(folder);
        if (File.Exists(setupFile))
        {
            try
            {
                var setup = JsonSerializer.Deserialize<MapSetup>(File.ReadAllText(setupFile));
                chosen = setup?.Versions.FirstOrDefault(v => v.Id == versionId);
            }
            catch (Exception ex) { Debug.WriteLine($"[EVENTS] mapsetups.json error: {ex.Message}"); }
        }

        // 2. EVENTS (с фолбэком на корень)
        string eventsPath = Path.Combine(mapDir, "events.json");
        if (chosen?.Files?.Events != null)
        {
            string customPath = Path.Combine(mapDir, "mapsetups", chosen.Files.Events);
            if (File.Exists(customPath)) eventsPath = customPath;
        }
        if (File.Exists(eventsPath)) ParseEventsFile(eventsPath, events);

        // 3. NPCs (БЕЗ фолбэка!)
        if (chosen?.Files?.Npcs != null)
        {
            string npcPath = Path.Combine(mapDir, "mapsetups", chosen.Files.Npcs);
            if (File.Exists(npcPath)) ParseNpcsFile(npcPath, events);
        }

        // 4. Scripts (БЕЗ фолбэка!)
        if (chosen?.Files?.Scripts != null)
        {
            string scriptPath = Path.Combine(mapDir, "mapsetups", chosen.Files.Scripts);
            if (File.Exists(scriptPath)) ParseScriptsFile(scriptPath, events);
        }

        return events;
    }

    private static void ParseEventsFile(string file, MapEvents events)
    {
        try
        {
            var arr = JsonNode.Parse(File.ReadAllText(file))?.AsArray();
            if (arr == null) return;
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
        catch (Exception ex) { Debug.WriteLine($"[EVENTS] events error: {ex.Message}"); }
    }

    private static void ParseNpcsFile(string file, MapEvents events)
    {
        try
        {
            var arr = JsonNode.Parse(File.ReadAllText(file))?.AsArray();
            if (arr == null) return;
            foreach (var item in arr)
            {
                var obj = item?.AsObject();
                if (obj == null) continue;
                events.Npcs.Add(new NpcEvent
                {
                    Id = obj["id"]?.GetValue<string>() ?? "",
                    X = obj["x"]?.GetValue<int>() ?? 0,
                    Y = obj["y"]?.GetValue<int>() ?? 0,
                    Sprite = obj["sprite"]?.GetValue<string>() ?? "",
                    Behavior = obj["behavior"]?.GetValue<string>() ?? "static",
                    Direction = obj["direction"]?.GetValue<string>() ?? "down",
                    HomeX = obj["home_x"]?.GetValue<int>() ?? 0,
                    HomeY = obj["home_y"]?.GetValue<int>() ?? 0,
                    Radius = obj["radius"]?.GetValue<int>() ?? 3,
                    TextId = ""
                });
            }
        }
        catch (Exception ex) { Debug.WriteLine($"[EVENTS] NPC error: {ex.Message}"); }
    }

    private static void ParseScriptsFile(string file, MapEvents events)
    {
        try
        {
            var arr = JsonNode.Parse(File.ReadAllText(file))?.AsArray();
            if (arr == null) return;
            
            foreach (var item in arr)
            {
                var obj = item?.AsObject();
                if (obj == null) continue;
                
                string npcId = obj["npc_id"]?.GetValue<string>() ?? "";
                var npc = events.Npcs.FirstOrDefault(n => n.Id == npcId);
                if (npc == null) continue;

                var scriptArr = obj["script"]?.AsArray();
                if (scriptArr == null) continue;
                
                npc.Script.Clear();
                
                foreach (var cmdNode in scriptArr)
                {
                    var cmdObj = cmdNode?.AsObject();
                    if (cmdObj == null) continue;

                    var cmd = new ScriptCommand
                    {
                        Type = cmdObj["type"]?.GetValue<string>() ?? "say",
                        WaitForInput = cmdObj["wait_for_input"]?.GetValue<bool>() ?? false
                    };

                    if (cmd.Type == "say")
                        cmd.TextId = cmdObj["text_id"]?.GetValue<string>();
                    else if (cmd.Type == "open_shop")
                        cmd.ShopId = cmdObj["shop_id"]?.GetValue<int>();
                    else if (cmd.Type == "start_battle" || cmd.Type == "unlock_battle")
                        cmd.BattleId = cmdObj["battle"]?.GetValue<int>();

                    npc.Script.Add(cmd);
                }
            }
        }
        catch (Exception ex) { Debug.WriteLine($"[EVENTS] Script error: {ex.Message}"); }
    }

    // ─── ПАРСЕРЫ (ВОТ ОНИ, ВСЁ НА МЕСТЕ) ───
    private static void ParseRoof(JsonObject obj, MapEvents events)
    {
        var roof = new RoofEvent
        {
            StartX = obj["start_x"]?.GetValue<int>() ?? 0,
            StartY = obj["start_y"]?.GetValue<int>() ?? 0,
            EndX   = obj["end_x"]?.GetValue<int>()   ?? 1,
            EndY   = obj["end_y"]?.GetValue<int>()   ?? 1,
        };

        var triggers = obj["triggers"]?.AsArray();
        if (triggers != null)
        {
            foreach (var item in triggers)
            {
                var pair = item?.AsArray();
                if (pair == null || pair.Count < 2) continue;
                roof.Triggers.Add(new TriggerPoint(pair[0]?.GetValue<int>() ?? 0, pair[1]?.GetValue<int>() ?? 0));
            }
        }

        var exits = obj["exits"]?.AsArray();
        if (exits != null)
        {
            foreach (var item in exits)
            {
                var pair = item?.AsArray();
                if (pair == null || pair.Count < 2) continue;
                roof.Exits.Add(new TriggerPoint(pair[0]?.GetValue<int>() ?? 0, pair[1]?.GetValue<int>() ?? 0));
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
        int facing = obj["facing"]?.GetValue<int>() ?? 0;
        facing = facing switch { 2 => 0, 4 => 1, 6 => 2, 8 => 3, _ => facing };

        var warp = new WarpEvent
        {
            Mode      = obj["mode"]?.GetValue<string>() ?? "",
            TriggerX  = obj["trigger_x"]?.GetValue<int>()   ?? -1,
            TriggerY  = obj["trigger_y"]?.GetValue<int>()   ?? -1,
            TargetMap = obj["target_map"]?.GetValue<string>() ?? "",
            TargetX   = obj["target_x"]?.GetValue<int>()     ?? 0,
            TargetY   = obj["target_y"]?.GetValue<int>()     ?? 0,
            Facing    = facing,
        };

        if (warp.Mode == "line")
        {
            warp.EndX = obj["end_x"]?.GetValue<int>() ?? warp.TriggerX;
            warp.EndY = obj["end_y"]?.GetValue<int>() ?? warp.TriggerY;
        }

        events.Warps.Add(warp);
    }

    // ══════════════════════════════════════════════════════════════
    //   SAVE
    // ══════════════════════════════════════════════════════════════
    public static void Save(string folder, string versionId, MapEvents events)
    {
        if (string.IsNullOrEmpty(folder)) return;
        string mapDir = GetMapDir(folder);
        Directory.CreateDirectory(mapDir);

        MapSetupVersion? chosen = null;
        var setupFile = ProjectPaths.MapSetupsFile(folder);
        if (File.Exists(setupFile))
        {
            var setup = JsonSerializer.Deserialize<MapSetup>(File.ReadAllText(setupFile));
            chosen = setup?.Versions.FirstOrDefault(v => v.Id == versionId);
        }

        // 1. EVENTS
        string eventsFile = Path.Combine(mapDir, "events.json");
        if (chosen?.Files?.Events != null)
            eventsFile = Path.Combine(mapDir, "mapsetups", chosen.Files.Events);
        SaveEventsFileTo(eventsFile, events);

        // 2. NPCs
        if (chosen?.Files?.Npcs != null)
        {
            string npcFile = Path.Combine(mapDir, "mapsetups", chosen.Files.Npcs);
            SaveNpcEventsFileTo(npcFile, events);
        }

        // 3. Scripts
        if (chosen?.Files?.Scripts != null)
        {
            string scriptFile = Path.Combine(mapDir, "mapsetups", chosen.Files.Scripts);
            SaveNpcScriptFileTo(scriptFile, events);
        }
    }

private static void SaveEventsFileTo(string file, MapEvents events)
{
    var root = new JsonArray();

    // ─── ROOFS ────────────────────────────────────────────────
    // Крыши — прямоугольник (start→end) + списки триггеров и выходов.
    // triggers/exits — массивы пар [x, y].
    foreach (var r in events.Roofs)
    {
        var triggers = new JsonArray();
        foreach (var t in r.Triggers) triggers.Add(new JsonArray { t.X, t.Y });

        var exits = new JsonArray();
        foreach (var e in r.Exits) exits.Add(new JsonArray { e.X, e.Y });

        root.Add(new JsonObject
        {
            ["type"]     = "roof",
            ["start_x"]  = r.StartX, ["start_y"] = r.StartY,
            ["end_x"]    = r.EndX,   ["end_y"]   = r.EndY,
            ["triggers"] = triggers,
            ["exits"]    = exits
        });
    }

    // ─── TILE CHANGES ─────────────────────────────────────────
    // Смена тайла при заходе на trigger_x/y.
    // sample_x/y — откуда взять стиль, close_x/y — обратный триггер.
    foreach (var t in events.TileChanges)
    {
        root.Add(new JsonObject
        {
            ["type"]        = "tile_change",
            ["trigger_x"]   = t.TriggerX, ["trigger_y"] = t.TriggerY,
            ["new_tile_id"] = t.NewTileId,
            ["sample_x"]    = t.SampleX,  ["sample_y"]  = t.SampleY,
            ["close_x"]     = t.CloseX,   ["close_y"]   = t.CloseY
        });
    }

    // ─── STAIRS ───────────────────────────────────────────────
    // Лестница от start к end. direction: 0 = "\", 1 = "/".
    foreach (var s in events.Stairs)
    {
        root.Add(new JsonObject
        {
            ["type"]      = "stairs",
            ["start_x"]   = s.StartX, ["start_y"] = s.StartY,
            ["end_x"]     = s.EndX,   ["end_y"]   = s.EndY,
            ["direction"] = s.Direction
        });
    }

    // ─── WARPS ────────────────────────────────────────────────
    // Два режима:
    //   single (Mode == "")     → только trigger_x/y
    //   line   (Mode == "line") → trigger_x/y + end_x/y (область)
    // "mode" пишем ТОЛЬКО для line — single остаётся обратно совместимым
    // со старыми JSON, где поля mode вообще нет.
    foreach (var w in events.Warps)
    {
        var obj = new JsonObject { ["type"] = "warp" };

        if (w.Mode == "line")
        {
            obj["mode"]      = "line";
            obj["trigger_x"] = w.TriggerX;
            obj["trigger_y"] = w.TriggerY;
            obj["end_x"]     = w.EndX;
            obj["end_y"]     = w.EndY;
        }
        else
        {
            obj["trigger_x"] = w.TriggerX;
            obj["trigger_y"] = w.TriggerY;
        }

        obj["target_map"] = w.TargetMap;
        obj["target_x"]   = w.TargetX;
        obj["target_y"]   = w.TargetY;
        obj["facing"]     = w.Facing;

        root.Add(obj);
    }

    // ─── ЗАПИСЬ НА ДИСК ───────────────────────────────────────
    var dir = Path.GetDirectoryName(file);
    if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

    File.WriteAllText(file, root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }).Replace("  ", "\t").Replace(": ", ":\t"));
}

    private static void SaveNpcEventsFileTo(string file, MapEvents events)
    {
        var root = new JsonArray();
        foreach (var npc in events.Npcs)
        {
            var obj = new JsonObject {
                ["id"] = npc.Id, ["x"] = npc.X, ["y"] = npc.Y,
                ["sprite"] = npc.Sprite, ["behavior"] = npc.Behavior, ["direction"] = npc.Direction
            };
            if (npc.Behavior == "wander")
            {
                obj["home_x"] = npc.HomeX;
                obj["home_y"] = npc.HomeY;
                obj["radius"] = npc.Radius;
            }
            root.Add(obj);
        }
        var dir = Path.GetDirectoryName(file);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(file, root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }).Replace("  ", "\t").Replace(": ", ":\t"));
    }

    private static void SaveNpcScriptFileTo(string file, MapEvents events)
    {
        var root = new JsonArray();

        foreach (var npc in events.Npcs)
        {
            if (npc.Script.Count == 0 && !string.IsNullOrWhiteSpace(npc.TextId))
            {
                npc.Script.Add(new ScriptCommand { Type = "turn_to_player" });
                npc.Script.Add(new ScriptCommand { Type = "turn_player_to_npc" });
                var ids = npc.TextId.Split(',').Select(s => s.Trim()).Where(s => !string.IsNullOrEmpty(s));
                foreach (var tid in ids)
                    npc.Script.Add(new ScriptCommand { Type = "say", TextId = tid });
                npc.Script.Add(new ScriptCommand { Type = "end", WaitForInput = true });
            }

            if (npc.Script.Count == 0) continue;

            var scriptArr = new JsonArray();
            foreach (var cmd in npc.Script)
            {
                var cmdObj = new JsonObject { ["type"] = cmd.Type };

                if (cmd.Type == "say" && !string.IsNullOrEmpty(cmd.TextId))
                    cmdObj["text_id"] = cmd.TextId;
                else if (cmd.Type == "open_shop" && cmd.ShopId.HasValue)
                    cmdObj["shop_id"] = cmd.ShopId.Value;
                else if ((cmd.Type == "start_battle" || cmd.Type == "unlock_battle") && cmd.BattleId.HasValue)
                    cmdObj["battle"] = cmd.BattleId.Value;
                else if (cmd.Type == "end")
                    cmdObj["wait_for_input"] = cmd.WaitForInput;

                scriptArr.Add(cmdObj);
            }

            root.Add(new JsonObject { ["npc_id"] = npc.Id, ["script"] = scriptArr });
        }

        var dir = Path.GetDirectoryName(file);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(file, root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }).Replace("  ", "\t").Replace(": ", ":\t"));
    }
}