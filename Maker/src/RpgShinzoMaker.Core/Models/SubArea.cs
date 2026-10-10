namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Дополнительная область внутри карты (комната, второй этаж, подвал).
/// Игрок может ходить в main area ИЛИ в любую subArea.
/// Камера/маска в игре работают относительно той area, где сейчас игрок.
/// </summary>
public class SubArea
{
    public int StartX { get; set; } = 0;
    public int StartY { get; set; } = 0;
    public int EndX   { get; set; } = 1;
    public int EndY   { get; set; } = 1;

    public int Width  => EndX - StartX + 1;
    public int Height => EndY - StartY + 1;

    public SubArea() { }

    public SubArea(int sx, int sy, int ex, int ey)
    {
        StartX = sx;
        StartY = sy;
        EndX   = ex;
        EndY   = ey;
    }
}