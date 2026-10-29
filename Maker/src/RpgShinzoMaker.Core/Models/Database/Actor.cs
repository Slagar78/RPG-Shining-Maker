using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models.Database;

public class Actor
{
    [JsonPropertyName("id")]
    public int Id { get; set; }

    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    [JsonPropertyName("class_id")]
    public int ClassId { get; set; }

    [JsonPropertyName("portrait")]
    public string Portrait { get; set; } = string.Empty;

    [JsonPropertyName("mapsprite")]
    public string Mapsprite { get; set; } = string.Empty;

    [JsonPropertyName("level")]
    public int Level { get; set; } = 1;

    [JsonPropertyName("exp")]
    public int Exp { get; set; } = 0;

    [JsonPropertyName("kills")]
    public int Kills { get; set; } = 0;

    [JsonPropertyName("defeats")]
    public int Defeats { get; set; } = 0;
}