using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json.Nodes;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Размещение одного юнита на карте боя (в координатах карты, не зоны).
/// Id — это actor_id (для allies) или enemy_id (для enemies).
/// </summary>
public class UnitPlacement
{
    public int Id;
    public int X;
    public int Y;
    public int Level = 1;
}

/// <summary>
/// Чтение/запись data/battles/{folder}/spriteset.json.
/// При сохранении НЕ трогает прочие ключи (ai_points, ai_regions и т.п.),
/// а только перезаписывает массивы allies и enemies.
/// </summary>
public static class SpritesetService
{
    public static string GetPath(string folder) =>
        Path.Combine(ProjectPaths.DataDir, "battles", folder, "spriteset.json");

    // ─── LOAD ──────────────────────────────────────
    public static void Load(string folder,
        out List<UnitPlacement> allies,
        out List<UnitPlacement> enemies)
    {
        allies  = new List<UnitPlacement>();
        enemies = new List<UnitPlacement>();

        var path = GetPath(folder);
        if (!File.Exists(path)) return;

        var root = JsonNode.Parse(File.ReadAllText(path))?.AsObject();
        if (root == null) return;

        ReadSlots(root["allies"]?.AsArray(), "actor_id", allies);
        ReadSlots(root["enemies"]?.AsArray(), "enemy_id", enemies);
    }

    private static void ReadSlots(JsonArray? arr, string idKey, List<UnitPlacement> target)
    {
        if (arr == null) return;
        foreach (var n in arr)
        {
            if (n is not JsonObject o) continue;
            target.Add(new UnitPlacement
            {
                Id = o[idKey]?.GetValue<int>() ?? -1,
                X  = o["x"]?.GetValue<int>() ?? 0,
                Y  = o["y"]?.GetValue<int>() ?? 0,
                Level = o["level"]?.GetValue<int>() ?? 1,
            });
        }
    }

    // ─── SAVE ──────────────────────────────────────
    /// <summary>
    /// Перезаписывает allies и enemies, оставляя все прочие ключи
    /// (ai_points, ai_regions и т.п.) нетронутыми.
    /// </summary>
    public static void Save(string folder,
        IEnumerable<UnitPlacement> allies,
        IEnumerable<UnitPlacement> enemies)
    {
        var path = GetPath(folder);

        // Читаем существующий файл, если есть — чтобы сохранить прочие поля
        JsonObject root;
        if (File.Exists(path))
        {
            root = JsonNode.Parse(File.ReadAllText(path))?.AsObject() ?? new JsonObject();
        }
        else
        {
            root = new JsonObject();
        }

        root["allies"]  = BuildArray(allies,  "actor_id", writeLevel: false);
        root["enemies"] = BuildArray(enemies, "enemy_id", writeLevel: true);

        var sb = new StringBuilder();
        WriteObject(sb, root, 0);

        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        File.WriteAllText(path, sb.ToString());
    }

    private static JsonArray BuildArray(IEnumerable<UnitPlacement> items, string idKey, bool writeLevel = false)
    {
        var arr = new JsonArray();
        foreach (var u in items)
        {
            var obj = new JsonObject
            {
                [idKey] = u.Id,
                ["x"]   = u.X,
                ["y"]   = u.Y,
            };
            if (writeLevel)
                obj["level"] = u.Level;

            arr.Add(obj);
        }
        return arr;
    }

    // ─── Ручной писатель с табами в стиле C-редактора ───
    private static void WriteObject(StringBuilder sb, JsonObject obj, int depth)
    {
        sb.Append("{\n");
        int i = 0;
        foreach (var kvp in obj)
        {
            AppendTabs(sb, depth + 1);
            sb.Append($"\"{kvp.Key}\": ");
            WriteValue(sb, kvp.Value, depth + 1);
            if (i < obj.Count - 1) sb.Append(",");
            sb.Append("\n");
            i++;
        }
        AppendTabs(sb, depth);
        sb.Append("}");
    }

    private static void WriteValue(StringBuilder sb, JsonNode? node, int depth)
    {
        if (node == null) { sb.Append("null"); return; }
        if (node is JsonObject o) { WriteObject(sb, o, depth); return; }
        if (node is JsonArray arr)
        {
            // Массивы объектов (allies/enemies) пишем многострочно,
            // массивы чисел (x/y) — горизонтально.
            bool hasObjects = false;
            foreach (var item in arr)
                if (item is JsonObject) { hasObjects = true; break; }

            if (!hasObjects)
            {
                sb.Append("[");
                for (int i = 0; i < arr.Count; i++)
                {
                    if (i > 0) sb.Append(", ");
                    WriteValue(sb, arr[i], depth);
                }
                sb.Append("]");
                return;
            }

            // Многострочный массив объектов
            sb.Append("[\n");
            for (int i = 0; i < arr.Count; i++)
            {
                AppendTabs(sb, depth + 1);
                WriteValue(sb, arr[i], depth + 1);
                if (i < arr.Count - 1) sb.Append(",");
                sb.Append("\n");
            }
            AppendTabs(sb, depth);
            sb.Append("]");
            return;
        }
        if (node is JsonValue v)
        {
            if (v.TryGetValue<string>(out var s))
                sb.Append($"\"{Escape(s)}\"");
            else
                sb.Append(v.ToJsonString());
        }
    }

    private static void AppendTabs(StringBuilder sb, int count)
    {
        for (int i = 0; i < count; i++) sb.Append('\t');
    }

    private static string Escape(string s)
    {
        if (string.IsNullOrEmpty(s)) return s;
        return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }
}