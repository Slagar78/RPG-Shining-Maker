using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models.Database;

namespace RpgShinzoMaker.Core.Services;

public class ItemsService
{
    private static readonly JsonSerializerOptions _options = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public List<Item> LoadItems(string filePath)
    {
        if (!File.Exists(filePath)) return new List<Item>();
        
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var itemsNode = root?["items"];
        if (itemsNode == null) return new List<Item>();

        var items = new List<Item>();
        foreach (var node in itemsNode.AsArray())
        {
            var item = node.Deserialize<Item>(_options);
            if (item != null)
            {
                // Парсим массив range [min, max]
                var range = node["range"]?.AsArray();
                if (range != null && range.Count >= 2)
                {
                    item.RangeMin = range[0]?.GetValue<int>() ?? 0; // (если ругается на это, можно просто проигнорировать, это не сломает программу)
                    item.RangeMax = range[1]?.GetValue<int>() ?? 0;
                }
                items.Add(item);
            }
        }
        return items;
    }

    public void SaveItems(string filePath, List<Item> items)
    {
        var itemsArray = new JsonArray();
        foreach (var item in items)
        {
            var node = JsonSerializer.SerializeToNode(item, _options)?.AsObject();
            if (node != null)
            {
                // Удаляем Min/Max и создаем массив range
                node.Remove("range_min");
                node.Remove("range_max");
                
                var rangeArr = new JsonArray { item.RangeMin, item.RangeMax };
                node["range"] = rangeArr;
                
                itemsArray.Add(node);
            }
        }

        var root = new JsonObject { ["items"] = itemsArray };
        var formattedJson = CJsonFormatter.Format(root);
        File.WriteAllText(filePath, formattedJson);
    }

    public List<Spell> LoadSpells(string filePath)
    {
        if (!File.Exists(filePath)) return new List<Spell>();
        
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var spellsNode = root?["spells"];
        if (spellsNode == null) return new List<Spell>();

        return spellsNode.Deserialize<List<Spell>>(_options) ?? new List<Spell>();
    }
}