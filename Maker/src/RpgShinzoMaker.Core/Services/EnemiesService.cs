using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models.Database;

namespace RpgShinzoMaker.Core.Services;

public class EnemiesService
{
    private const int FixedSlotCount = 4;

    private static readonly JsonSerializerOptions _options = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public List<Enemy> LoadEnemies(string filePath)
    {
        if (!File.Exists(filePath)) return new List<Enemy>();

        var json = File.ReadAllText(filePath);
        var root = JsonNode.Parse(json);
        var enemiesNode = root?["enemies"]?.AsArray();
        if (enemiesNode == null) return new List<Enemy>();

        var result = new List<Enemy>();

        foreach (var node in enemiesNode)
        {
            if (node == null) continue;

            var enemy = node.Deserialize<Enemy>(_options);
            if (enemy == null) continue;

            // ── Items: гарантируем ровно 4 слота ──
            var itemsNode = node["items"]?.AsArray();
            enemy.Items.Clear();
            for (int i = 0; i < FixedSlotCount; i++)
            {
                if (itemsNode != null && i < itemsNode.Count)
                {
                    var item = itemsNode[i].Deserialize<EnemyItem>(_options);
                    enemy.Items.Add(item ?? new EnemyItem { ItemId = -1, Equipped = false });
                }
                else
                {
                    enemy.Items.Add(new EnemyItem { ItemId = -1, Equipped = false });
                }
            }

            // ── SpellSlots: из двух параллельных массивов ──
            var spellsNode = node["spells"]?.AsArray();
            var levelsNode = node["spell_levels"]?.AsArray();

            for (int i = 0; i < FixedSlotCount; i++)
            {
                int spellId = -1;
                int lvl = 1;

                if (spellsNode != null && i < spellsNode.Count)
                    spellId = spellsNode[i]?.GetValue<int>() ?? -1;

                if (levelsNode != null && i < levelsNode.Count)
                    lvl = levelsNode[i]?.GetValue<int>() ?? 1;

                enemy.SpellSlots.Add(new EnemySpellSlot { SpellId = spellId, Level = lvl });
            }

            result.Add(enemy);
        }

        return result;
    }

    public void SaveEnemies(string filePath, List<Enemy> enemies)
    {
        var array = new JsonArray();

        foreach (var enemy in enemies)
        {
            var node = JsonSerializer.SerializeToNode(enemy, _options)?.AsObject();
            if (node == null) continue;

            // SpellSlots — [JsonIgnore], собираем обратно в два массива.
            var spellsArr = new JsonArray();
            var levelsArr = new JsonArray();

            for (int i = 0; i < FixedSlotCount; i++)
            {
                if (i < enemy.SpellSlots.Count)
                {
                    spellsArr.Add(enemy.SpellSlots[i].SpellId);
                    levelsArr.Add(enemy.SpellSlots[i].Level);
                }
                else
                {
                    spellsArr.Add(-1);
                    levelsArr.Add(1);
                }
            }

            node["spells"] = spellsArr;
            node["spell_levels"] = levelsArr;

            // Гарантируем ровно 4 предмета
            var itemsArr = node["items"]?.AsArray() ?? new JsonArray();
            while (itemsArr.Count < FixedSlotCount)
                itemsArr.Add(JsonSerializer.SerializeToNode(new EnemyItem { ItemId = -1 }, _options));
            node["items"] = itemsArr;

            array.Add(node);
        }

        var root = new JsonObject { ["enemies"] = array };
        var formattedJson = CJsonFormatter.Format(root);

        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        File.WriteAllText(filePath, formattedJson);
    }
}