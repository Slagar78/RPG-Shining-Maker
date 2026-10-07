namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// NPC Event — персонаж на карте.
/// Behavior: "static" (стоит) или "wander" (ходит в радиусе radius от home).
/// Direction: "down", "left", "right", "up".
/// TextId — ID диалоговых текстов через запятую (runtime, не пишется в NPC_events.json).
/// </summary>
public class NpcEvent
{
    public string Id { get; set; } = "";

    public int X { get; set; } = 0;
    public int Y { get; set; } = 0;

    public string Sprite    { get; set; } = "";
    public string Behavior  { get; set; } = "static";
    public string Direction { get; set; } = "down";

    public int HomeX  { get; set; } = 0;
    public int HomeY  { get; set; } = 0;
    public int Radius { get; set; } = 3;

    /// <summary>ID диалогов через запятую. В JSON не сохраняется — только runtime.</summary>
    public string TextId { get; set; } = "";
}