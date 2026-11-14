using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Описывает фазу анимации (idle, attack, dodge).
/// </summary>
public class AnimPhase
{
    [JsonPropertyName("frames")]
    public List<AnimFrame> Frames { get; set; } = new();

    // В C-коде offset_x и offset_y были на уровне фазы (как дефолтные значения)
    [JsonPropertyName("offset_x")]
    public int OffsetX { get; set; } = 0;

    [JsonPropertyName("offset_y")]
    public int OffsetY { get; set; } = 0;

    // ─── Runtime-поля (не сохраняются в JSON, нужны только для редактора) ───
    
    [JsonIgnore]
    public string Name { get; set; } = "";

    [JsonIgnore]
    public bool Animate { get; set; } = false;
}