namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Tile Change Event — при заходе на триггер тайл меняется на другой.
/// Используется для открывающихся сундуков, дверей, разрушаемых стен.
/// </summary>
public class TileChangeEvent
{
    public int TriggerX { get; set; } = -1;
    public int TriggerY { get; set; } = -1;

    public int NewTileId { get; set; } = 0;

    // Для sample (откуда взять стиль) и close (обратный триггер) — необязательны
    public int SampleX { get; set; } = -1;
    public int SampleY { get; set; } = -1;

    public int CloseX { get; set; } = -1;
    public int CloseY { get; set; } = -1;
}