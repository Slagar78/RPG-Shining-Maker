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
        var spellsNode = root?["spells"]?.AsArray();
        if (spellsNode == null) return new List<Spell>();

        var spells = new List<Spell>();
        foreach (var node in spellsNode)
        {
            var spell = node.Deserialize<Spell>(_options);
            if (spell != null)
            {
                var levelsArray = node["levels"]?.AsArray();
                if (levelsArray != null)
                {
                    spell.Levels.Clear();
                    foreach (var lvlNode in levelsArray)
                    {
                        var lvl = lvlNode.Deserialize<SpellLevel>(_options);
                        if (lvl != null)
                        {
                            var range = lvlNode["range"]?.AsArray();
                            if (range != null && range.Count >= 2)
                            {
                                lvl.RangeMin = range[0]!.GetValue<int>();
                                lvl.RangeMax = range[1]?.GetValue<int>() ?? 0;
                            }
                            spell.Levels.Add(lvl);
                        }
                    }
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
                // Пересобираем массив levels с правильным форматом range
                var levelsArray = new JsonArray();
                foreach (var lvl in spell.Levels)
                {
                    var lvlNode = JsonSerializer.SerializeToNode(lvl, _options)?.AsObject();
                    if (lvlNode != null)
                    {
                        lvlNode.Remove("range_min");
                        lvlNode.Remove("range_max");
                        lvlNode["range"] = new JsonArray { lvl.RangeMin, lvl.RangeMax };
                        levelsArray.Add(lvlNode);
                    }
                }
                node["levels"] = levelsArray;
                spellsArray.Add(node);
            }
        }

        var root = new JsonObject { ["spells"] = spellsArray };
        var formattedJson = CJsonFormatter.Format(root);
        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        File.WriteAllText(filePath, formattedJson);
    }
}