using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Описывает один кадр анимации (соответствует элементу массива "frames" в JSON).
/// </summary>
public class AnimFrame
{
    [JsonPropertyName("file")]
    public string File { get; set; } = "";

    [JsonPropertyName("duration")]
    public float Duration { get; set; } = 0.3f;

    [JsonPropertyName("offset_x")]
    public int OffsetX { get; set; } = 0;

    [JsonPropertyName("offset_y")]
    public int OffsetY { get; set; } = 0;
}