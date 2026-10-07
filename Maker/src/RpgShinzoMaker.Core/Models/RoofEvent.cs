namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Roof Event — прямоугольная крыша, которая перекрывает игрока,
/// когда он заходит за триггеры, и открывается, когда выходит за exits.
/// </summary>
public class RoofEvent
{
    public int TileId  { get; set; } = 0;

    public int StartX { get; set; } = 0;
    public int StartY { get; set; } = 0;
    public int EndX   { get; set; } = 1;
    public int EndY   { get; set; } = 1;

    // Триггеры: -1 = нет триггера
    public int TriggerX  { get; set; } = -1;
    public int TriggerY  { get; set; } = -1;
    public int Trigger2X { get; set; } = -1;
    public int Trigger2Y { get; set; } = -1;

    // Выходы: -1 = нет выхода
    public int ExitX  { get; set; } = -1;
    public int ExitY  { get; set; } = -1;
    public int Exit2X { get; set; } = -1;
    public int Exit2Y { get; set; } = -1;
}