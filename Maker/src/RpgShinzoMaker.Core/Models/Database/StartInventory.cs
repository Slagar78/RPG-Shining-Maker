using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models.Database;

public class StartInventory
{
    [JsonPropertyName("actor_id")]
    public int ActorId { get; set; }

    [JsonPropertyName("start_level")]
    public int StartLevel { get; set; } = 1;

    [JsonPropertyName("items")]
    public List<StartItem> Items { get; set; } = new();
}

public class StartItem
{
    [JsonPropertyName("item")]
    public string Item { get; set; } = "NOTHING";

    [JsonPropertyName("equipped")]
    public bool Equipped { get; set; } = false;
}