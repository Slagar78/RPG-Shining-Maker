namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Типы тайлов (из C-версии: passable / block / slow / under).
/// </summary>
public enum TileType
{
    Passable = 0,  // зелёный — проходимый
    Block    = 1,  // красный — блок
    Slow     = 2,  // синий — медленный
    Under    = 3   // оранжевый — под персонажем
}