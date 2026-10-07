using System.Collections.Generic;

namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Контейнер всех событий одной карты.
/// Хранит 5 типов событий: крыши, изменения тайлов, лестницы, варпы, NPC.
/// </summary>
public class MapEvents
{
    public List<RoofEvent>        Roofs        { get; set; } = new();
    public List<TileChangeEvent>  TileChanges  { get; set; } = new();
    public List<StairEvent>       Stairs       { get; set; } = new();
    public List<WarpEvent>        Warps        { get; set; } = new();
    public List<NpcEvent>         Npcs         { get; set; } = new();
}