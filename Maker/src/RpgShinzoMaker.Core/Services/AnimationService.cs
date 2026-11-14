using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Core.Services;

public class AnimationService
{
    private const int MaxAnimFrames = 32;

    public AnimationSet Load(string jsonPath)
    {
        var set = new AnimationSet();
        if (!File.Exists(jsonPath)) return set;

        try
        {
            var json = File.ReadAllText(jsonPath);
            var root = JsonNode.Parse(json);
            if (root == null) return set;

            set.JsonPath = jsonPath;
            set.Loaded = true;

            set.Idle = ParsePhase(root["idle"], "idle");
            set.Attack = ParsePhase(root["attack"], "attack");
            set.Dodge = ParsePhase(root["dodge"], "dodge");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[AnimationService] Ошибка загрузки {jsonPath}: {ex.Message}");
        }

        return set;
    }

    private AnimPhase ParsePhase(JsonNode? node, string phaseName)
    {
        var phase = new AnimPhase { Name = phaseName };
        if (node == null) return phase;

        // В C-коде offset_x/y могли быть на уровне фазы (как дефолтные)
        int defaultOx = node["offset_x"]?.GetValue<int>() ?? 0;
        int defaultOy = node["offset_y"]?.GetValue<int>() ?? 0;
        phase.OffsetX = defaultOx;
        phase.OffsetY = defaultOy;

        var framesNode = node["frames"]?.AsArray();
        if (framesNode != null)
        {
            foreach (var frameNode in framesNode)
            {
                var frame = new AnimFrame
                {
                    File = frameNode?["file"]?.GetValue<string>() ?? "",
                    Duration = frameNode?["duration"]?.GetValue<float>() ?? 0.3f,
                    // Если на уровне кадра оффсета нет, берем дефолтный из фазы
                    OffsetX = frameNode?["offset_x"]?.GetValue<int>() ?? defaultOx,
                    OffsetY = frameNode?["offset_y"]?.GetValue<int>() ?? defaultOy
                };
                phase.Frames.Add(frame);
            }
        }
        return phase;
    }

    public void Save(AnimationSet set)
    {
        if (!set.Loaded || string.IsNullOrEmpty(set.JsonPath)) return;

        try
        {
            // Читаем существующий JSON, чтобы не затереть чужие ключи (как в C-коде)
            JsonNode? root = null;
            if (File.Exists(set.JsonPath))
            {
                var existingJson = File.ReadAllText(set.JsonPath);
                root = JsonNode.Parse(existingJson);
            }
            root ??= new JsonObject();

            UpdatePhaseInJson(root, "idle", set.Idle);
            UpdatePhaseInJson(root, "attack", set.Attack);
            UpdatePhaseInJson(root, "dodge", set.Dodge);

            // Используем твой форматер (Правило №10)
            var formatted = CJsonFormatter.Format(root);
            File.WriteAllText(set.JsonPath, formatted);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[AnimationService] Ошибка сохранения {set.JsonPath}: {ex.Message}");
        }
    }

    private void UpdatePhaseInJson(JsonNode root, string key, AnimPhase phase)
    {
        if (phase.Frames.Count == 0) return;

        var phaseNode = root[key] as JsonObject;
        if (phaseNode == null)
        {
            phaseNode = new JsonObject();
            root[key] = phaseNode;
        }

        var framesNode = phaseNode["frames"] as JsonArray;
        if (framesNode != null)
        {
            for (int i = 0; i < framesNode.Count && i < phase.Frames.Count; i++)
            {
                if (framesNode[i] is JsonObject frameNode)
                {
                    var f = phase.Frames[i];
                    // Округляем до 2 знаков, как в C-коде (round(dur * 100.0) / 100.0)
                    frameNode["duration"] = Math.Round(f.Duration, 2);
                    frameNode["offset_x"] = f.OffsetX;
                    frameNode["offset_y"] = f.OffsetY;
                }
            }
        }
    }

    /// <summary>
    /// Аналог rescan_frames из C: ищет idle_1.png, frame_attack_1.png, frame_dodge_1.png
    /// </summary>
    public void RescanFrames(AnimationSet set)
    {
        if (!set.Loaded || string.IsNullOrEmpty(set.JsonPath)) return;

        string folder = Path.GetDirectoryName(set.JsonPath) ?? "";
        string[] prefixes = { "idle", "frame_attack", "frame_dodge" };

        for (int p = 0; p < 3; p++)
        {
            var phase = set.GetPhase(p);
            phase.Frames.Clear();

            int maxFrames = 0;
            // Ищем файлы от 1 до 32, пока не найдем первый отсутствующий
            for (int n = 1; n <= MaxAnimFrames; n++)
            {
                string filename = Path.Combine(folder, $"{prefixes[p]}_{n}.png");
                if (!File.Exists(filename)) break;
                maxFrames = n;
            }

            for (int i = 1; i <= maxFrames; i++)
            {
                phase.Frames.Add(new AnimFrame
                {
                    File = $"{prefixes[p]}_{i}.png",
                    Duration = 0.3f,
                    OffsetX = 0,
                    OffsetY = 0
                });
            }
        }
    }
}