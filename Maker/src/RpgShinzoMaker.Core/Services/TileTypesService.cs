// Maker/src/RpgShinzoMaker.Core/Services/TileTypesService.cs
using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Чтение/запись data/tile_types/{tileset}.json.
/// Формат: массив int'ов — тип каждого тайла (0..3).
/// </summary>
public static class TileTypesService
{
    public static int[] Load(string tilesetAbsPath, int tileCount)
    {
        var result = new int[tileCount];  // по умолчанию — Passable
        string rel  = GetRelativePath(tilesetAbsPath);
        string file = ProjectPaths.TileTypesFile(rel);

        if (!File.Exists(file)) return result;

        try
        {
            var arr = JsonNode.Parse(File.ReadAllText(file))?.AsArray();
            if (arr == null) return result;

            int n = Math.Min(arr.Count, tileCount);
            for (int i = 0; i < n; i++)
                result[i] = arr[i]?.GetValue<int>() ?? 0;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[TILE_TYPES] Load error: {ex.Message}");
        }
        return result;
    }

    public static void Save(string tilesetAbsPath, int[] tileTypes)
    {
        if (tileTypes.Length == 0) return;

        string rel  = GetRelativePath(tilesetAbsPath);
        string file = ProjectPaths.TileTypesFile(rel);

        try
        {
            var dir = Path.GetDirectoryName(file);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

            var arr = new JsonArray();
            foreach (var t in tileTypes) arr.Add(t);

            File.WriteAllText(file, arr.ToJsonString(new JsonSerializerOptions
            {
                WriteIndented = false
            }));
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[TILE_TYPES] Save error: {ex.Message}");
        }
    }

    private static string GetRelativePath(string absPath)
    {
        var root = ProjectPaths.Root;
        if (!string.IsNullOrEmpty(root) &&
            absPath.StartsWith(root, StringComparison.OrdinalIgnoreCase))
        {
            var rel = absPath.Substring(root.Length).TrimStart('\\', '/');
            return rel.Replace('\\', '/');
        }
        return Path.GetFileName(absPath);
    }
}