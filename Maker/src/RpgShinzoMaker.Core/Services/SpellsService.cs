using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models.Database;

namespace RpgShinzoMaker.Core.Services;

public class SpellsService
{
    private static readonly JsonSerializerOptions _options = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public List<Spell> LoadSpells(string filePath)
    {
        if (!File.Exists(filePath)) return new List<Spell>();
        
        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var spellsNode = root?["spells"];
        if (spellsNode == null) return new List<Spell>();

        var spells = new List<Spell>();
        foreach (var node in spellsNode.AsArray())
        {
            var spell = node.Deserialize<Spell>(_options);
            if (spell != null)
            {
                var range = node["range"]?.AsArray();
                if (range != null && range.Count >= 2)
                {
                    spell.RangeMin = range[0]!.GetValue<int>();
                    spell.RangeMax = range[1]?.GetValue<int>() ?? 0;
                }
                spells.Add(spell);
            }
        }
        return spells;
    }

    public void SaveSpells(string filePath, List<Spell> spells)
    {
        var spellsArray = new JsonArray();
        foreach (var spell in spells)
        {
            var node = JsonSerializer.SerializeToNode(spell, _options)?.AsObject();
            if (node != null)
            {
                node.Remove("range_min");
                node.Remove("range_max");
                
                var rangeArr = new JsonArray { spell.RangeMin, spell.RangeMax };
                node["range"] = rangeArr;
                
                spellsArray.Add(node);
            }
        }

        var root = new JsonObject { ["spells"] = spellsArray };
        var formattedJson = CJsonFormatter.Format(root);
        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        File.WriteAllText(filePath, formattedJson);
    }
}