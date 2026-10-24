using System.Collections.Generic;

namespace RpgShinzoMaker.Core.Models;

public class NpcEvent
{
    public string Id { get; set; } = "";
    public int X { get; set; } = 0;
    public int Y { get; set; } = 0;
    public string Sprite { get; set; } = "";
    public string Behavior { get; set; } = "static";
    public string Direction { get; set; } = "down";
    public int HomeX { get; set; } = 0;
    public int HomeY { get; set; } = 0;
    public int Radius { get; set; } = 3;

    // Старое поле (оставляем для совместимости)
    public string TextId { get; set; } = "";

    // ★ НОВОЕ: Полноценный скрипт
    public List<ScriptCommand> Script { get; set; } = new();
}