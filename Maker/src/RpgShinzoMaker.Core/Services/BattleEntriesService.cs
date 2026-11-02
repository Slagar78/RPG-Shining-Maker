using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Чтение/запись data/battles/entries.json.
/// Формат — массив объектов (без корневого ключа), как в C-версии.
/// Стиль записи — тот же, что в EntriesService (табы, каждый объект с новой строки).
/// </summary>
public static class BattleEntriesService
{
    public static string GetEntriesPath() =>
        Path.Combine(ProjectPaths.DataDir, "battles", "entries.json");

    public static string GetBattleFolderPath(string folder) =>
        Path.Combine(ProjectPaths.DataDir, "battles", folder);

    // ─── LOAD ──────────────────────────────────────
    public static List<BattleEntry> Load(string path)
    {
        var list = new List<BattleEntry>();
        if (!File.Exists(path)) return list;

        var arr = JsonNode.Parse(File.ReadAllText(path))?.AsArray();
        if (arr == null) return list;

        foreach (var n in arr)
        {
            if (n is not JsonObject o) continue;
            list.Add(new BattleEntry
            {
                Folder       = o["folder"]?.GetValue<string>() ?? "",
                Name         = o["name"]?.GetValue<string>() ?? "",
                MapId        = o["map_id"]?.GetValue<int>() ?? 0,
                Music        = o["music"]?.GetValue<string>() ?? "",
                MusicVolume  = o["music_volume"]?.GetValue<float>() ?? 1.0f,
                BattleX      = o["battle_x"]?.GetValue<int>() ?? 0,
                BattleY      = o["battle_y"]?.GetValue<int>() ?? 0,
                BattleWidth  = o["battle_width"]?.GetValue<int>() ?? 0,
                BattleHeight = o["battle_height"]?.GetValue<int>() ?? 0,
                Background   = o["background"]?.GetValue<string>() ?? "",
                Ground       = o["ground"]?.GetValue<string>() ?? "",
            });
        }
        return list;
    }

    // ─── SAVE ──────────────────────────────────────
    public static void Save(string path, IEnumerable<BattleEntry> entries)
    {
        var sb = new StringBuilder();
        sb.Append("[");

        bool first = true;
        foreach (var e in entries)
        {
            if (!first) sb.Append(", ");
            first = false;

            sb.Append("{\n");
            sb.Append($"\t\t\"folder\":\t\"{Escape(e.Folder)}\",\n");
            sb.Append($"\t\t\"name\":\t\"{Escape(e.Name)}\",\n");
            sb.Append($"\t\t\"map_id\": {e.MapId},\n");
            sb.Append($"\t\t\"music\":\t\"{Escape(e.Music)}\",\n");

            string vol = e.MusicVolume.ToString(System.Globalization.CultureInfo.InvariantCulture);
            sb.Append($"\t\t\"music_volume\":\t{vol},\n");

            sb.Append($"\t\t\"battle_x\":\t{e.BattleX},\n");
            sb.Append($"\t\t\"battle_y\":\t{e.BattleY},\n");
            sb.Append($"\t\t\"battle_width\":\t{e.BattleWidth},\n");
            sb.Append($"\t\t\"battle_height\":\t{e.BattleHeight}");

            // Опциональные поля — только если заданы
            if (!string.IsNullOrEmpty(e.Background))
                sb.Append($",\n\t\t\"background\":\t\"{Escape(e.Background)}\"");
            if (!string.IsNullOrEmpty(e.Ground))
                sb.Append($",\n\t\t\"ground\":\t\"{Escape(e.Ground)}\"");

            sb.Append("\n\t}");
        }

        sb.Append("]");

        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        File.WriteAllText(path, sb.ToString());
    }

    private static string Escape(string s)
    {
        if (string.IsNullOrEmpty(s)) return s;
        return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }
}