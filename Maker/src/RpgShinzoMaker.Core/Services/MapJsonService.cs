using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Чтение/запись data/maps/{folder}/layout.json.
/// Формат идентичен C-версии: массивы [x][y], column-major.
/// </summary>
public static class MapJsonService
{
    // ─── LOAD ──────────────────────────────────────
    public static GameMap? Load(string filePath)
    {
        if (!File.Exists(filePath)) return null;

        var root = JsonNode.Parse(File.ReadAllText(filePath))?.AsObject();
        if (root == null) return null;

        int w = root["width"]?.GetValue<int>() ?? 0;
        int h = root["height"]?.GetValue<int>() ?? 0;
        if (w < GameMap.MinSize || w > GameMap.MaxSize) return null;
        if (h < GameMap.MinSize || h > GameMap.MaxSize) return null;

        int sz = w * h;
        var map = new GameMap
        {
            Width = w,
            Height = h,
            TilesetPath = root["tileset"]?.GetValue<string>() ?? "",

            Tiles    = new int[sz],
            Rot      = new int[sz],
            MirrorX  = new bool[sz],
            MirrorY  = new bool[sz],

            Tiles2   = new int[sz],
            Rot2     = new int[sz],
            MirrorX2 = new bool[sz],
            MirrorY2 = new bool[sz],

            CellType = new int[sz],
        };

        // Второй слой по умолчанию пуст
        for (int i = 0; i < sz; i++) map.Tiles2[i] = -1;

        // Слой 1
        LoadLayer(root, "tiles", "rot", "mirror_x", "mirror_y",
                  map.Tiles, map.Rot, map.MirrorX, map.MirrorY, w, h);

        // Слой 2 (если есть)
        if (root["tiles2"] is JsonArray)
            LoadLayer(root, "tiles2", "rot2", "mirror_x2", "mirror_y2",
                      map.Tiles2, map.Rot2, map.MirrorX2, map.MirrorY2, w, h);

        // Collision
        LoadCellTypes(root, map.CellType, w, h);

        // Music (опционально)
        if (root["music"] is JsonObject music)
        {
            map.MusicFile   = music["file"]?.GetValue<string>() ?? "";
            map.MusicVolume = music["volume"]?.GetValue<float>() ?? 0.8f;
        }

        return map;
    }

    private static void LoadLayer(JsonObject root,
        string tKey, string rKey, string mxKey, string myKey,
        int[] tiles, int[] rot, bool[] mx, bool[] my, int w, int h)
    {
        var tA  = root[tKey]?.AsArray();
        var rA  = root[rKey]?.AsArray();
        var mxA = root[mxKey]?.AsArray();
        var myA = root[myKey]?.AsArray();

        for (int x = 0; x < w; x++)
        {
            var colT  = tA?[x]?.AsArray();
            var colR  = rA?[x]?.AsArray();
            var colMx = mxA?[x]?.AsArray();
            var colMy = myA?[x]?.AsArray();

            for (int y = 0; y < h; y++)
            {
                int idx = x * h + y;
                tiles[idx] = colT?[y]?.GetValue<int>() ?? 0;
                rot  [idx] = colR?[y]?.GetValue<int>() ?? 0;
                mx   [idx] = colMx?[y]?.GetValue<bool>() ?? false;
                my   [idx] = colMy?[y]?.GetValue<bool>() ?? false;
            }
        }
    }

    private static void LoadCellTypes(JsonObject root, int[] cell, int w, int h)
    {
        var arr = root["collision"]?.AsArray();
        if (arr == null) return;

        for (int x = 0; x < w; x++)
        {
            var col = arr[x]?.AsArray();
            for (int y = 0; y < h; y++)
                cell[x * h + y] = col?[y]?.GetValue<int>() ?? 0;
        }
    }

    // ─── SAVE ──────────────────────────────────────
    public static void Save(string filePath, GameMap map)
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("{");
        sb.AppendLine($"\t\"width\":\t{map.Width},");
        sb.AppendLine($"\t\"height\":\t{map.Height},");
        sb.AppendLine($"\t\"tileset\":\t\"{map.TilesetPath}\",");

        // Слой 1
        sb.AppendLine("\t\"tiles\":\t" + BuildIntArray(map.Tiles, map.Width, map.Height) + ",");
        sb.AppendLine("\t\"rot\":\t" + BuildIntArray(map.Rot, map.Width, map.Height) + ",");
        sb.AppendLine("\t\"mirror_x\":\t" + BuildBoolArray(map.MirrorX, map.Width, map.Height) + ",");
        sb.AppendLine("\t\"mirror_y\":\t" + BuildBoolArray(map.MirrorY, map.Width, map.Height) + ",");

        // Слой 2 — только если не пусто
        if (map.HasLayer2())
        {
            sb.AppendLine("\t\"tiles2\":\t" + BuildIntArray(map.Tiles2, map.Width, map.Height) + ",");
            sb.AppendLine("\t\"rot2\":\t" + BuildIntArray(map.Rot2, map.Width, map.Height) + ",");
            sb.AppendLine("\t\"mirror_x2\":\t" + BuildBoolArray(map.MirrorX2, map.Width, map.Height) + ",");
            sb.AppendLine("\t\"mirror_y2\":\t" + BuildBoolArray(map.MirrorY2, map.Width, map.Height) + ",");
        }

        // Музыка — если задана
        if (!string.IsNullOrEmpty(map.MusicFile))
        {
            sb.AppendLine("\t\"music\":\t{");
            sb.AppendLine($"\t\t\"file\":\t\"{map.MusicFile}\",");
            sb.AppendLine($"\t\t\"volume\":\t{map.MusicVolume.ToString(System.Globalization.CultureInfo.InvariantCulture)}");
            sb.AppendLine("\t},");
        }

        // Коллизия (без запятой в конце, так как это последний элемент)
        sb.AppendLine("\t\"collision\":\t" + BuildIntArray(map.CellType, map.Width, map.Height));
        
        sb.AppendLine("}");

        var dir = Path.GetDirectoryName(filePath);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(filePath, sb.ToString());
    }

    // ─── ХЕЛПЕРЫ ДЛЯ КРАСИВОГО JSON ──────────────────

    private static string BuildIntArray(int[] data, int w, int h)
    {
        var sb = new System.Text.StringBuilder();
        sb.Append("[\n\t\t");
        for (int x = 0; x < w; x++)
        {
            if (x > 0) sb.Append(",\n\t\t");
            sb.Append('[');
            for (int y = 0; y < h; y++)
            {
                if (y > 0) sb.Append(',');
                sb.Append(data[x * h + y]);
            }
            sb.Append(']');
        }
        sb.Append("\n\t]");
        return sb.ToString();
    }

    private static string BuildBoolArray(bool[] data, int w, int h)
    {
        var sb = new System.Text.StringBuilder();
        sb.Append("[\n\t\t");
        for (int x = 0; x < w; x++)
        {
            if (x > 0) sb.Append(",\n\t\t");
            sb.Append('[');
            for (int y = 0; y < h; y++)
            {
                if (y > 0) sb.Append(',');
                sb.Append(data[x * h + y] ? "true" : "false");
            }
            sb.Append(']');
        }
        sb.Append("\n\t]");
        return sb.ToString();
    }
}