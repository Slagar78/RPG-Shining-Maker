using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Core.Services;

public class GlobalService
{
    private static readonly JsonSerializerOptions _options = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public GlobalSettings LoadGlobal(string filePath)
    {
        if (!File.Exists(filePath)) return new GlobalSettings();
        
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        
        if (root == null) return new GlobalSettings();

        // Обратная совместимость: если вдруг остался старый "start_map" (строка)
        if (root["start_map"] is JsonValue legacyMap && root["start_map_id"] == null)
        {
            // Тут можно было бы искать ID по имени, но раз мы правим файл, 
            // просто удалим старое поле при сохранении. Пока ставим 0.
            root["start_map_id"] = 0; 
        }

        return root.Deserialize<GlobalSettings>(_options) ?? new GlobalSettings();
    }

    public void SaveGlobal(string filePath, GlobalSettings settings)
    {
        var root = JsonSerializer.SerializeToNode(settings, _options)?.AsObject();
        if (root != null)
        {
            // Удаляем старое строковое поле, если оно там завалялось
            root.Remove("start_map");
        }

        var formattedJson = CJsonFormatter.Format(root);
        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        File.WriteAllText(filePath, formattedJson);
    }
}