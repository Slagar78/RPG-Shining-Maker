// Maker/src/RpgShinzoMaker.Core/Services/AreasService.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Границы основной области карты (mainLayerStart / mainLayerEnd)
/// + дополнительные sub-areas внутри карты.
/// </summary>
public class Areas
{
    public int StartX { get; set; }
    public int StartY { get; set; }
    public int EndX   { get; set; }
    public int EndY   { get; set; }

    public int Width  => EndX - StartX + 1;
    public int Height => EndY - StartY + 1;

    /// <summary>
    /// Дополнительные области внутри карты (до 10 штук).
    /// Могут быть пустыми — тогда карта без sub-areas.
    /// </summary>
    public List<SubArea> SubAreas { get; set; } = new();
}

public static class AreasService
{
    // ══════════════════════════════════════════════════════════════
    //   LOAD
    // ══════════════════════════════════════════════════════════════
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

            // ─── main area ───
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

            // ─── subAreas ───
            var subArr = first["subAreas"]?.AsArray();
            if (subArr != null)
            {
                foreach (var item in subArr)
                {
                    var subObj = item?.AsObject();
                    if (subObj == null) continue;

                    var subStart = subObj["start"]?.AsArray();
                    var subEnd   = subObj["end"]?.AsArray();

                    if (subStart == null || subStart.Count < 2) continue;
                    if (subEnd   == null || subEnd.Count   < 2) continue;

                    var sub = new SubArea
                    {
                        StartX = subStart[0]?.GetValue<int>() ?? 0,
                        StartY = subStart[1]?.GetValue<int>() ?? 0,
                        EndX   = subEnd[0]?.GetValue<int>()   ?? 1,
                        EndY   = subEnd[1]?.GetValue<int>()   ?? 1,
                    };

                    areas.SubAreas.Add(sub);

                    // максимум 10 sub-areas
                    if (areas.SubAreas.Count >= 10) break;
                }
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

            var sb = new System.Text.StringBuilder();

            sb.Append("[{\n");

            // main
            sb.Append("  \"mainLayerStart\": [")
                .Append(areas.StartX).Append(',').Append(areas.StartY).Append("],\n");
            sb.Append("  \"mainLayerEnd\": [")
                .Append(areas.EndX).Append(',').Append(areas.EndY).Append("]");

            // subAreas — только если есть
            if (areas.SubAreas.Count > 0)
            {
                sb.Append(",\n  \"subAreas\": [\n");
                for (int i = 0; i < areas.SubAreas.Count; i++)
                {
                    var s = areas.SubAreas[i];
                    sb.Append("    { \"start\": [")
                        .Append(s.StartX).Append(',').Append(s.StartY)
                        .Append("], \"end\": [")
                        .Append(s.EndX).Append(',').Append(s.EndY)
                        .Append("] }");

                    if (i < areas.SubAreas.Count - 1) sb.Append(',');
                    sb.Append('\n');
                }
                sb.Append("  ]");
            }

            sb.Append("\n}]\n");

            File.WriteAllText(file, sb.ToString());
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[AREAS] Save error: {ex.Message}");
        }
    }
}    