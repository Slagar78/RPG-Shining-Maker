using System.Collections.Generic;
using System.IO;
using System.Text.Json;
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
        var arr = new JsonArray();
        foreach (var e in entries)
        {
            arr.Add(new JsonObject
            {
                ["folder"]       = e.Folder,
                ["name"]         = e.Name,
                ["music"]        = e.Music,
                ["music_volume"] = e.MusicVolume,
                ["areas"]        = e.Areas,
            });
        }

        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        File.WriteAllText(path, arr.ToJsonString(new JsonSerializerOptions
        {
            WriteIndented = true
        }));
    }
}