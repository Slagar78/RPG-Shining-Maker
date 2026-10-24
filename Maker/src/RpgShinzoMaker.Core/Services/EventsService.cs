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
                var sayIds = new List<string>();
                foreach (var cmd in scriptArr)
                {
                    if (cmd?["type"]?.GetValue<string>() == "say")
                    {
                        string tid = cmd["text_id"]?.GetValue<string>() ?? "";
                        if (!string.IsNullOrEmpty(tid)) sayIds.Add(tid);
                    }
                }
                if (sayIds.Count > 0) npc.TextId = string.Join(",", sayIds);
            }
        }
        catch (Exception ex) { Debug.WriteLine($"[EVENTS] Script error: {ex.Message}"); }
    }

    // ─── Парсеры Roof / TileChange / Stairs / Warp (оставь свои, они не меняются) ───
    private static void ParseRoof(JsonObject obj, MapEvents events) { /* Твой код */ }
    private static void ParseTileChange(JsonObject obj, MapEvents events) { /* Твой код */ }
    private static void ParseStairs(JsonObject obj, MapEvents events) { /* Твой код */ }
    private static void ParseWarp(JsonObject obj, MapEvents events) { /* Твой код */ }

    // ══════════════════════════════════════════════════════════════
    //   SAVE
    // ══════════════════════════════════════════════════════════════
    public static void Save(string folder, string versionId, MapEvents events)
    {
        if (string.IsNullOrEmpty(folder)) return;
        string mapDir = GetMapDir(folder);
        Directory.CreateDirectory(mapDir);

        // Ищем, куда сохранять (в какую версию)
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

    private static void SaveEventsFileTo(string file, MapEvents events) {
        // Твой старый код SaveEventsFile, только вместо GetEventsFile(folder) пишем в file
        // Не забудь Directory.CreateDirectory(Path.GetDirectoryName(file))
    }
    private static void SaveNpcEventsFileTo(string file, MapEvents events) {
        // Твой старый код SaveNpcEventsFile, пишем в file
    }
    private static void SaveNpcScriptFileTo(string file, MapEvents events) {
        // Твой старый код SaveNpcScriptFile, пишем в file
    }
}
