using System.Collections.Generic;

namespace RpgShinzoMaker.Core.Models.Database;

/// <summary>
/// Каталог токенов поля "prowess" (общий для CharacterClass и Enemy).
/// Значения — "сырые" строки, как в JSON: "critical150_1in8", "double_1in16" и т.п.
/// </summary>
public static class ProwessCatalog
{
    public static IReadOnlyList<string> All { get; } = new List<string>
    {
        // ── Критические удары (обычный крит) ──
        "critical_1in8",
        "critical_1in16",
        "critical_1in32",
        "critical_1in64",

        // ── Усиленные криты (крит x1.5 или выше) ──
        "critical150_1in8",
        "critical150_1in16",
        "critical150_1in32",
        "critical150_1in64",

        // ── Криты с эффектом ──
        "critical_poison",
        "critical_sleep",
        "critical_stun",
        "critical_silence",

        // ── Двойная атака ──
        "double_1in8",
        "double_1in16",
        "double_1in32",
        "double_1in64",

        // ── Контратака ──
        "counter_1in8",
        "counter_1in16",
        "counter_1in32",
        "counter_1in64",
    };

    /// <summary>
    /// Пустой литерал-заглушка для слотов предметов/магии.
    /// В enemies.json — "Nothing", в actors.json (start_inventory) — "NOTHING".
    /// Не путать: в prowess этот литерал не используется.
    /// </summary>
    public const string EmptySlot = "Nothing";
}