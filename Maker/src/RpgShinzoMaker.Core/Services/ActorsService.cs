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
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public List<Actor> LoadActors(string filePath)
    {
        if (!File.Exists(filePath)) return new List<Actor>();
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var actorsNode = root?["actors"]?.AsArray();
        if (actorsNode == null) return new List<Actor>();

        var result = new List<Actor>();
        Actor? currentActor = null;

        foreach (var node in actorsNode)
        {
            if (node?["id"] != null)
            {
                // Это базовый персонаж
                currentActor = node.Deserialize<Actor>(_options);
                if (currentActor != null) result.Add(currentActor);
            }
            else if (node?["forClass"] != null && currentActor != null)
            {
                // Это блок промоушена текущего персонажа
                var promo = node.Deserialize<ActorPromotion>(_options);
                if (promo != null) currentActor.Promotions.Add(promo);
            }
        }
        return result;
    }

    public void SaveActors(string filePath, List<Actor> actors)
    {
        var array = new JsonArray();
        foreach (var actor in actors)
        {
            // 1. Сохраняем базовый блок
            var baseNode = JsonSerializer.SerializeToNode(actor, _options)?.AsObject();
            if (baseNode != null)
            {
                baseNode.Remove("promotions");
                baseNode.Remove("displayName");
                baseNode.Remove("useFirstSpellList"); // В базовом блоке этого поля нет
                array.Add(baseNode);
            }
            
            // 2. Сохраняем промоушены
            foreach (var promo in actor.Promotions)
            {
                var promoNode = JsonSerializer.SerializeToNode(promo, _options)?.AsObject();
                if (promoNode != null)
                {
                    promoNode.Remove("displayName");
                    array.Add(promoNode);
                }
            }
        }

        var root = new JsonObject { ["actors"] = array };
        var formattedJson = CJsonFormatter.Format(root);
        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        File.WriteAllText(filePath, formattedJson);
    }

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
        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        File.WriteAllText(filePath, formattedJson);
    }
}