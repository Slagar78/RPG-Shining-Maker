namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Warp Event — телепорт на другую карту при заходе на триггер.
/// Facing: 0=Down, 1=Left, 2=Right, 3=Up.
/// </summary>
public class WarpEvent
{
    public int TriggerX { get; set; } = -1;
    public int TriggerY { get; set; } = -1;

    public string TargetMap { get; set; } = "";
    public int TargetX { get; set; } = 0;
    public int TargetY { get; set; } = 0;

    public int Facing { get; set; } = 0; // 0..3
}