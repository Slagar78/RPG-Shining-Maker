using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models.Database;

namespace RpgShinzoMaker.Core.Services;

public class EnemiesService
{
    // В enemies.json фиксированные слоты: items[4], spells[4], spell_levels[4]
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

            // ── Разворачиваем параллельные массивы в SpellSlots ──
            var spellsNode = node["spells"]?.AsArray();
            var levelsNode = node["spell_levels"]?.AsArray();

            for (int i = 0; i < FixedSlotCount; i++)
            {
                string name = ProwessCatalog.EmptySlot;
                int lvl = 1;

                if (spellsNode != null && i < spellsNode.Count)
                    name = spellsNode[i]?.GetValue<string>() ?? ProwessCatalog.EmptySlot;

                if (levelsNode != null && i < levelsNode.Count)
                    lvl = levelsNode[i]?.GetValue<int>() ?? 1;

                enemy.SpellSlots.Add(new EnemySpellSlot { Name = name, Level = lvl });
            }

            // ── Добиваем Items до 4 слотов ──
            while (enemy.Items.Count < FixedSlotCount)
                enemy.Items.Add(new EnemyItem { Item = ProwessCatalog.EmptySlot, Equipped = false });

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

            // SpellSlots — [JsonIgnore], в ноду не попали. Собираем обратно
            // в два параллельных массива строго по 4 элемента.
            var spellsArr = new JsonArray();
            var levelsArr = new JsonArray();

            for (int i = 0; i < FixedSlotCount; i++)
            {
                if (i < enemy.SpellSlots.Count)
                {
                    spellsArr.Add(enemy.SpellSlots[i].Name);
                    levelsArr.Add(enemy.SpellSlots[i].Level);
                }
                else
                {
                    spellsArr.Add(ProwessCatalog.EmptySlot);
                    levelsArr.Add(1);
                }
            }

            node["spells"] = spellsArr;
            node["spell_levels"] = levelsArr;

            // Гарантируем ровно 4 предмета (не трогая модель — правим только JSON)
            var itemsArr = node["items"]?.AsArray() ?? new JsonArray();
            while (itemsArr.Count < FixedSlotCount)
                itemsArr.Add(JsonSerializer.SerializeToNode(new EnemyItem(), _options));
            node["items"] = itemsArr;

            array.Add(node);
        }

        var root = new JsonObject { ["enemies"] = array };
        var formattedJson = CJsonFormatter.Format(root);

        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);
        File.WriteAllText(filePath, formattedJson);
    }
}