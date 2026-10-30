using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models.Database;

public class Spell
{
    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;
    
    [JsonPropertyName("level")]
    public int Level { get; set; } = 1;
}