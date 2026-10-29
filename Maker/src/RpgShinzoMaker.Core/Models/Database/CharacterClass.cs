using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models.Database;

public class CharacterClass
{
    [JsonPropertyName("id")]
    public int Id { get; set; }

    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    // Остальные поля (move, growth, resistance) добавим позже!
}