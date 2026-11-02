using System;

namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Запись о битве из data/battles/entries.json.
/// Связь с картой — по MapId (числу), а не по имени папки (правило 27/28).
/// </summary>
public class BattleEntry
{
    /// <summary>ID карты, на которой проходит битва (число).</summary>
    public int MapId { get; set; }

    /// <summary>Имя папки с данными битвы (data/battles/{Folder}/).</summary>
    public string Folder { get; set; } = "";

    /// <summary>Отображаемое имя (для списка в редакторе).</summary>
    public string Name { get; set; } = "";

    /// <summary>Путь к музыке боя (assets/sounds/...).</summary>
    public string Music { get; set; } = "";

    /// <summary>Громкость музыки 0..1.</summary>
    public float MusicVolume { get; set; } = 1.0f;

    // ── Боевая зона (прямоугольник на карте) ──
    // Если width/height = 0 — вся карта (совместимо с battle.rb).
    public int BattleX      { get; set; }
    public int BattleY      { get; set; }
    public int BattleWidth  { get; set; }
    public int BattleHeight { get; set; }

    // ── Доп. поля, которые сейчас НЕ редактируются, но сохраняются ──
    public string Background { get; set; } = "";
    public string Ground     { get; set; } = "";
}