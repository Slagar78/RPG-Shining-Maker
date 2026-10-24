using System.Collections.Generic;
using System.Text.Json.Serialization; // ← Обязательно добавь этот using

namespace RpgShinzoMaker.Core.Models;

public class MapSetup
{
    [JsonPropertyName("versions")]
    public List<MapSetupVersion> Versions { get; set; } = new();
}

public class MapSetupVersion
{
    [JsonPropertyName("id")]
    public string Id { get; set; } = "";

    [JsonPropertyName("when")]
    public List<string> When { get; set; } = new();

    [JsonPropertyName("desc")]
    public string Desc { get; set; } = "";

    [JsonPropertyName("files")]
    public MapSetupFiles Files { get; set; } = new();
}

public class MapSetupFiles
{
    [JsonPropertyName("events")]
    public string? Events { get; set; }

    [JsonPropertyName("npcs")]
    public string? Npcs { get; set; }

    [JsonPropertyName("scripts")]
    public string? Scripts { get; set; }
}