using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Чтение data/maps/entries.json — список карт с именами и музыкой.
/// </summary>
public static class EntriesService
{
    public static List<MapEntry> Load(string path)
    {
        var list = new List<MapEntry>();
        if (!File.Exists(path)) return list;

        var arr = JsonNode.Parse(File.ReadAllText(path))?.AsArray();
        if (arr == null) return list;

        foreach (var n in arr)
        {
            if (n is not JsonObject o) continue;
            list.Add(new MapEntry
            {
                Id          = o["id"]?.GetValue<int>() ?? 0,
                Folder      = o["folder"]?.GetValue<string>() ?? "",
                Name        = o["name"]?.GetValue<string>() ?? "",
                Music       = o["music"]?.GetValue<string>() ?? "",
                MusicVolume = o["music_volume"]?.GetValue<float>() ?? 0.8f,
                Areas       = o["areas"]?.GetValue<string>() ?? "",
            });
        }
        return list;
    }

    public static MapEntry? FindByFolder(IEnumerable<MapEntry> entries, string folder)
    {
        foreach (var e in entries)
            if (e.Folder == folder) return e;
        return null;
    }

    public static void Save(string path, IEnumerable<MapEntry> entries)
    {
        var sb = new StringBuilder();
        sb.Append("[");

        bool first = true;
        foreach (var e in entries)
        {
            if (!first) sb.Append(", ");
            first = false;

            sb.Append("{\n");
            sb.Append($"\t\t\"id\":\t{e.Id},\n");
            sb.Append($"\t\t\"folder\":\t\"{e.Folder}\",\n");
            sb.Append($"\t\t\"name\":\t\"{e.Name}\",\n");
            sb.Append($"\t\t\"music\":\t\"{e.Music}\",\n");
            
            string vol = e.MusicVolume.ToString(System.Globalization.CultureInfo.InvariantCulture);
            sb.Append($"\t\t\"music_volume\":\t{vol},\n");
            
            sb.Append($"\t\t\"areas\":\t\"{e.Areas}\"\n");
            sb.Append("\t}");
        }

        sb.Append("]");

        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        File.WriteAllText(path, sb.ToString());
    }
}