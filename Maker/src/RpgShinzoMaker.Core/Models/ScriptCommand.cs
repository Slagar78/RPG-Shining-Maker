namespace RpgShinzoMaker.Core.Models;

public class ScriptCommand
{
    public string Type { get; set; } = "say"; // say, turn_to_player, open_shop, start_battle, end...
    
    // Параметры, которые могут понадобиться в зависимости от Type
    public string? TextId { get; set; }
    public int? ShopId { get; set; }
    public int? BattleId { get; set; }
    public bool WaitForInput { get; set; } = true;
}