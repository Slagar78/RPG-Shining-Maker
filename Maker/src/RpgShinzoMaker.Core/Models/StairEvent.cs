namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Stair Event — лестница от Start к End.
/// Direction: 0 = "\" (сверху-слева вниз-направо), 1 = "/" (снизу-слева вверх-направо).
/// </summary>
public class StairEvent
{
    public int StartX { get; set; } = 0;
    public int StartY { get; set; } = 0;
    public int EndX   { get; set; } = 1;
    public int EndY   { get; set; } = 1;

    public int Direction { get; set; } = 0; // 0 или 1
}