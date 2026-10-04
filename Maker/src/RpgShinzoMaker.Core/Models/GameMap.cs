using System;

namespace RpgShinzoMaker.Core.Models;

/// <summary>
/// Прямой перенос struct Map из map.h.
/// Индексация: idx = x * Height + y  (column-major, как в C-версии).
/// </summary>
public class GameMap
{
    public const int TileSize = 48;
    public const int MinSize  = 2;
    public const int MaxSize  = 999;

    // ── Метаданные ──
    public string Name    { get; set; } = "";
    public string Folder  { get; set; } = "";
    public int    Width   { get; set; }
    public int    Height  { get; set; }

    public string TilesetPath { get; set; } = "";
    public string MusicFile   { get; set; } = "";
    public float  MusicVolume { get; set; } = 0.8f;
    public string AreasPath   { get; set; } = "";

    // ── Слой 1 ──
    public int[]  Tiles   { get; set; } = Array.Empty<int>();
    public int[]  Rot     { get; set; } = Array.Empty<int>();
    public bool[] MirrorX { get; set; } = Array.Empty<bool>();
    public bool[] MirrorY { get; set; } = Array.Empty<bool>();

    // ── Слой 2 ── (tiles2 = -1 означает "пусто")
    public int[]  Tiles2   { get; set; } = Array.Empty<int>();
    public int[]  Rot2     { get; set; } = Array.Empty<int>();
    public bool[] MirrorX2 { get; set; } = Array.Empty<bool>();
    public bool[] MirrorY2 { get; set; } = Array.Empty<bool>();

    // ── Коллизии ──
    public int[] CellType { get; set; } = Array.Empty<int>();

    // ── Утилиты ──
    public int TotalCells => Width * Height;

    /// <summary>
    /// Индекс в массивах. Column-major: x * Height + y.
    /// </summary>
    public int Index(int x, int y) => x * Height + y;

    /// <summary>
    /// Создать новую пустую карту.
    /// </summary>
    public static GameMap CreateNew(string name, int w, int h, string tileset)
    {
        w = Math.Clamp(w, MinSize, MaxSize);
        h = Math.Clamp(h, MinSize, MaxSize);
        int sz = w * h;

        var map = new GameMap
        {
            Name = name,
            Folder = name,
            Width = w,
            Height = h,
            TilesetPath = tileset,

            Tiles    = new int[sz],
            Rot      = new int[sz],
            MirrorX  = new bool[sz],
            MirrorY  = new bool[sz],

            Tiles2   = new int[sz],
            Rot2     = new int[sz],
            MirrorX2 = new bool[sz],
            MirrorY2 = new bool[sz],

            CellType = new int[sz],
        };

        // Второй слой по умолчанию пуст: tiles2 = -1
        for (int i = 0; i < sz; i++)
            map.Tiles2[i] = -1;

        return map;
    }

    /// <summary>
    /// Проверить, есть ли хоть один тайл на втором слое.
    /// </summary>
    public bool HasLayer2()
    {
        foreach (var t in Tiles2)
            if (t != -1) return true;
        return false;
    }
}