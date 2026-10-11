using System.Collections.Generic;

namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Клетка (X, Y) — используется для триггеров и выходов крыши.
/// </summary>
public class TriggerPoint
{
    public int X { get; set; }
    public int Y { get; set; }

    public TriggerPoint() { }
    public TriggerPoint(int x, int y) { X = x; Y = y; }
}

/// <summary>
/// Roof Event — прямоугольная крыша, которая перекрывает игрока,
/// когда он заходит за триггеры, и открывается, когда выходит за exits.
/// Картинка крыши берётся из слоя 2 карты по координатам Start-End.
/// </summary>
public class RoofEvent
{
    public int StartX { get; set; } = 0;
    public int StartY { get; set; } = 0;
    public int EndX   { get; set; } = 1;
    public int EndY   { get; set; } = 1;

    /// <summary>Клетки, где крыша скрывается (входы). До 10 штук.</summary>
    public List<TriggerPoint> Triggers { get; set; } = new();

    /// <summary>Клетки, где крыша показывается (выходы). До 10 штук.</summary>
    public List<TriggerPoint> Exits { get; set; } = new();
}
