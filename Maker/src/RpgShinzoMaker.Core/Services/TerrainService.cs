using System;
using System.IO;
using System.Text;
using System.Text.Json.Nodes;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Чтение/запись data/battles/{folder}/terrain.json.
/// Формат — матрица int[height][width], значения:
///   -1 = blocked, 0 = fly only, 1 = walkable.
/// </summary>
public static class TerrainService
{
    public const int Blocked  = -1;
    public const int FlyOnly  = 0;
    public const int Walkable = 1;

    public static string GetPath(string folder) =>
        Path.Combine(ProjectPaths.DataDir, "battles", folder, "terrain.json");

    // ─── LOAD ──────────────────────────────────────
    /// <summary>
    /// Читает terrain.json. Если файла нет или он битый — возвращает
    /// матрицу [height][width] заполненную Walkable (по умолчанию всё проходимо).
    /// </summary>
    public static int[][] Load(string folder, int width, int height)
    {
        var result = CreateDefault(width, height);

        var path = GetPath(folder);
        if (!File.Exists(path)) return result;

        var arr = JsonNode.Parse(File.ReadAllText(path))?.AsArray();
        if (arr == null) return result;

        for (int y = 0; y < height && y < arr.Count; y++)
        {
            var row = arr[y]?.AsArray();
            if (row == null) continue;
            for (int x = 0; x < width && x < row.Count; x++)
            {
                result[y][x] = row[x]?.GetValue<int>() ?? Walkable;
            }
        }
        return result;
    }

    public static int[][] CreateDefault(int width, int height)
    {
        var m = new int[height][];
        for (int y = 0; y < height; y++)
        {
            m[y] = new int[width];
            for (int x = 0; x < width; x++)
                m[y][x] = Walkable;
        }
        return m;
    }

    // ─── SAVE ──────────────────────────────────────
    public static void Save(string folder, int[][] terrain)
    {
        var root = new JsonArray();
        foreach (var row in terrain)
        {
            var jrow = new JsonArray();
            foreach (var v in row) jrow.Add(v);
            root.Add(jrow);
        }

        var sb = new StringBuilder();
        WriteCompact(sb, root);

        var path = GetPath(folder);
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        File.WriteAllText(path, sb.ToString());
    }

    // ─── Компактный писатель (массивы в одну строку) ───
    private static void WriteCompact(StringBuilder sb, JsonNode? node)
    {
        if (node == null) { sb.Append("null"); return; }
        if (node is JsonObject o)
        {
            sb.Append("{");
            int i = 0;
            foreach (var kvp in o)
            {
                if (i > 0) sb.Append(",");
                sb.Append($"\"{kvp.Key}\":");
                WriteCompact(sb, kvp.Value);
                i++;
            }
            sb.Append("}");
            return;
        }
        if (node is JsonArray arr)
        {
            sb.Append("[");
            for (int i = 0; i < arr.Count; i++)
            {
                if (i > 0) sb.Append(",");
                WriteCompact(sb, arr[i]);
            }
            sb.Append("]");
            return;
        }
        if (node is JsonValue v)
        {
            if (v.TryGetValue<string>(out var s))
                sb.Append($"\"{s.Replace("\\", "\\\\").Replace("\"", "\\\"")}\"");
            else
                sb.Append(v.ToJsonString());
        }
    }
}