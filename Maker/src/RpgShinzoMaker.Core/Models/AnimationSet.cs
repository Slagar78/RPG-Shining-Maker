using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Корневой объект animation.json. Содержит три фазы и состояние для редактора.
/// </summary>
public class AnimationSet
{
    [JsonPropertyName("idle")]
    public AnimPhase Idle { get; set; } = new() { Name = "idle" };

    [JsonPropertyName("attack")]
    public AnimPhase Attack { get; set; } = new() { Name = "attack" };

    [JsonPropertyName("dodge")]
    public AnimPhase Dodge { get; set; } = new() { Name = "dodge" };

    // ─── Runtime-поля (не сохраняются в JSON) ───
    
    [JsonIgnore]
    public bool Loaded { get; set; } = false;

    [JsonIgnore]
    public string JsonPath { get; set; } = "";

    [JsonIgnore]
    public int CurrentPhase { get; set; } = 0; // 0=idle, 1=attack, 2=dodge

    [JsonIgnore]
    public int[] CurrentFrame { get; set; } = new int[3]; // Текущий кадр для каждой из 3 фаз

    [JsonIgnore]
    public float AnimTimer { get; set; } = 0f;

    /// <summary>
    /// Удобный доступ к фазе по индексу (0, 1, 2).
    /// </summary>
    public AnimPhase GetPhase(int index)
    {
        return index switch
        {
            0 => Idle,
            1 => Attack,
            2 => Dodge,
            _ => Idle
        };
    }
}