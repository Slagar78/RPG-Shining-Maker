using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models.Database;

namespace RpgShinzoMaker.Core.Services;

public class ActorsService
{
    private static readonly JsonSerializerOptions _options = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.SnakeCaseLower // <--- ВОТ ЭТО
    };

    // --- ACTORS ---
    public List<Actor> LoadActors(string filePath)
    {
        if (!File.Exists(filePath)) return new List<Actor>();
        
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var actorsNode = root?["actors"];
        if (actorsNode == null) return new List<Actor>();

        return actorsNode.Deserialize<List<Actor>>(_options) ?? new List<Actor>();
    }

    public void SaveActors(string filePath, List<Actor> actors)
    {
        var root = new JsonObject
        {
            ["actors"] = JsonSerializer.SerializeToNode(actors, _options)
        };
        var formattedJson = CJsonFormatter.Format(root);
        File.WriteAllText(filePath, formattedJson);
    }

    // --- START INVENTORY ---
    public List<StartInventory> LoadStartInventory(string filePath)
    {
        if (!File.Exists(filePath)) return new List<StartInventory>();
        
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var invNode = root?["start_inventory"];
        if (invNode == null) return new List<StartInventory>();

        return invNode.Deserialize<List<StartInventory>>(_options) ?? new List<StartInventory>();
    }

    public void SaveStartInventory(string filePath, List<StartInventory> inventory)
    {
        var root = new JsonObject
        {
            ["start_inventory"] = JsonSerializer.SerializeToNode(inventory, _options)
        };
        var formattedJson = CJsonFormatter.Format(root);
        File.WriteAllText(filePath, formattedJson);
    }

    // --- CLASSES (только для выпадающего списка) ---
    public List<CharacterClass> LoadClasses(string filePath)
    {
        if (!File.Exists(filePath)) return new List<CharacterClass>();
        
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var classesNode = root?["classes"];
        if (classesNode == null) return new List<CharacterClass>();

        return classesNode.Deserialize<List<CharacterClass>>(_options) ?? new List<CharacterClass>();
    }
}