namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Одна запись из data/maps/entries.json.
/// </summary>
public class MapEntry
{
    public string Folder      { get; set; } = "";
    public string Name        { get; set; } = "";
    public string Music       { get; set; } = "";
    public float  MusicVolume { get; set; } = 0.8f;
    public string Areas       { get; set; } = "";
}