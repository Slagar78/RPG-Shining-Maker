using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models.Database;

namespace RpgShinzoMaker.Core.Services;

public class ClassesService
{
    private static readonly JsonSerializerOptions _options = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public List<CharacterClass> LoadClasses(string filePath)
    {
        if (!File.Exists(filePath)) return new List<CharacterClass>();
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var classesNode = root?["classes"];
        if (classesNode == null) return new List<CharacterClass>();
        return classesNode.Deserialize<List<CharacterClass>>(_options) ?? new List<CharacterClass>();
    }

    public void SaveClasses(string filePath, List<CharacterClass> classes)
    {
        var root = new JsonObject
        {
            ["classes"] = JsonSerializer.SerializeToNode(classes, _options)
        };
        var formattedJson = CJsonFormatter.Format(root);
        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        File.WriteAllText(filePath, formattedJson);
    }
}