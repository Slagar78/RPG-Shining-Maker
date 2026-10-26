namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Warp Event — телепорт. Два режима:
///   Mode == ""     → одиночный тайл (trigger_x, trigger_y)
///   Mode == "line" → прямоугольная область (trigger_x/y → end_x/y)
/// Facing: 0=Down, 1=Left, 2=Right, 3=Up.
/// </summary>
public class WarpEvent
{
    public string Mode { get; set; } = "";   // "" | "line"

    public int TriggerX { get; set; } = -1;
    public int TriggerY { get; set; } = -1;

    // Только для Mode == "line"
    public int EndX { get; set; } = -1;
    public int EndY { get; set; } = -1;

    public string TargetMap { get; set; } = "";
    public int TargetX { get; set; } = 0;
    public int TargetY { get; set; } = 0;

    public int Facing { get; set; } = 0;
}