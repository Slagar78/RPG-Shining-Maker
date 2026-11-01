using System;
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

        // 1. Загружаем items.json, чтобы знать ID всех предметов
        var itemNameToId = new Dictionary<string, int>();
        var itemsPath = Path.Combine(Path.GetDirectoryName(filePath)!, "..", "items", "items.json");
        
        if (File.Exists(itemsPath))
        {
            var itemsRoot = JsonNode.Parse(File.ReadAllText(itemsPath))?["items"]?.AsArray();
            if (itemsRoot != null)
            {
                foreach (var it in itemsRoot)
                {
                    var id = it?["id"]?.GetValue<int>() ?? -1;
                    var name = it?["name"]?.GetValue<string>();
                    if (!string.IsNullOrEmpty(name) && id != -1)
                    {
                        itemNameToId[name] = id;
                    }
                }
            }
        }

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

            // ── МИГРАЦИЯ ПРЕДМЕТОВ: Из строки (Name) в число (ID) ──
            var itemsNode = node["items"]?.AsArray();
            enemy.Items.Clear(); // Очищаем то, что могло загрузиться десериализатором

            if (itemsNode != null)
            {
                for (int i = 0; i < FixedSlotCount; i++)
                {
                    var itemId = -1;
                    var equipped = false;

                    if (i < itemsNode.Count)
                    {
                        var itemNode = itemsNode[i]?.AsObject();
                        if (itemNode != null)
                        {
                            // Новый формат (уже ID)
                            if (itemNode.ContainsKey("item_id"))
                            {
                                itemId = itemNode["item_id"]?.GetValue<int>() ?? -1;
                            }
                            // Старый формат (строка с именем)
                            else if (itemNode.ContainsKey("item"))
                            {
                                var oldName = itemNode["item"]?.GetValue<string>();
                                if (!string.IsNullOrEmpty(oldName) && oldName != "Nothing" && oldName != "NOTHING")
                                {
                                    if (itemNameToId.TryGetValue(oldName, out int foundId))
                                    {
                                        itemId = foundId;
                                    }
                                    else
                                    {
                                        // Предмета нет в базе (например, Laser Blade) — ставим пусто
                                        itemId = -1;
                                    }
                                }
                            }
                            
                            equipped = itemNode["equipped"]?.GetValue<bool>() ?? false;
                        }
                    }

                    enemy.Items.Add(new EnemyItem { ItemId = itemId, Equipped = equipped });
                }
            }
            else
            {
                // Если предметов вообще нет в JSON, создаем 4 пустых
                for (int i = 0; i < FixedSlotCount; i++)
                    enemy.Items.Add(new EnemyItem { ItemId = -1, Equipped = false });
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

            // SpellSlots — [JsonIgnore]. Собираем обратно.
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