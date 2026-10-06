// Maker/src/RpgShinzoMaker.Core/Services/AreasService.cs
using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Границы основной области карты (mainLayerStart / mainLayerEnd).
/// </summary>
public class Areas
{
    public int StartX { get; set; }
    public int StartY { get; set; }
    public int EndX   { get; set; }
    public int EndY   { get; set; }

    public int Width  => EndX - StartX + 1;
    public int Height => EndY - StartY + 1;
}

public static class AreasService
{
    public static Areas Load(GameMap map)
    {
        var areas = new Areas
        {
            StartX = 0,
            StartY = 0,
            EndX   = map.Width  - 1,
            EndY   = map.Height - 1,
        };

        if (string.IsNullOrEmpty(map.Folder)) return areas;

        var file = ProjectPaths.AreasFile(map.Folder);
        if (!File.Exists(file)) return areas;

        try
        {
            var arr = JsonNode.Parse(File.ReadAllText(file))?.AsArray();
            if (arr == null || arr.Count == 0) return areas;

            var first = arr[0]?.AsObject();
            if (first == null) return areas;

            var start = first["mainLayerStart"]?.AsArray();
            var end   = first["mainLayerEnd"]?.AsArray();

            if (start != null && start.Count >= 2)
            {
                areas.StartX = start[0]?.GetValue<int>() ?? 0;
                areas.StartY = start[1]?.GetValue<int>() ?? 0;
            }
            if (end != null && end.Count >= 2)
            {
                areas.EndX = end[0]?.GetValue<int>() ?? (map.Width  - 1);
                areas.EndY = end[1]?.GetValue<int>() ?? (map.Height - 1);
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[AREAS] Load error: {ex.Message}");
        }
        return areas;
    }

    public static void Save(GameMap map, Areas areas)
    {
        if (string.IsNullOrEmpty(map.Folder)) return;

        var file = ProjectPaths.AreasFile(map.Folder);
        try
        {
            var dir = Path.GetDirectoryName(file);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

            var json = new JsonArray
            {
                new JsonObject
                {
                    ["mainLayerStart"] = new JsonArray { areas.StartX, areas.StartY },
                    ["mainLayerEnd"]   = new JsonArray { areas.EndX,   areas.EndY   },
                }
            };

            File.WriteAllText(file, json.ToJsonString(new JsonSerializerOptions
            {
                WriteIndented = false
            }));
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[AREAS] Save error: {ex.Message}");
        }
    }
}