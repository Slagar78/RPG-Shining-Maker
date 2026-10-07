// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.Canvas.cs
using System;
using System.Collections.Generic;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media.Imaging;

namespace RpgShinzoMaker.Desktop.Editors.EventEditor;

/// <summary>
/// Часть EventEditorView — РАБОТА С КАНВАСОМ.
/// Связывает данные редактора с EventCanvas.
/// </summary>
public partial class EventEditorView
{
    // ─── Кэш нарезанного тайлсета ───
    private List<CroppedBitmap> _tileBitmaps = new();
    private Bitmap? _sourceTileset;
    private string? _loadedTilesetPath;

    // ══════════════════════════════════════════════════════════════
    //   ПОЛНАЯ ПЕРЕРИСОВКА — при загрузке новой карты / смене тайлсета
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Полная перерисовка: пересоздаёт все Image-контролы карты.
    /// Тяжёлая операция — вызывать ТОЛЬКО при смене карты или тайлсета.
    /// </summary>
    private void FullRedraw()
    {
        if (CanvasControl == null) return;
        if (_currentMap == null) return;

        EnsureTilesetLoaded();

        CanvasControl.ShowLayer1 = _showLayer1;
        CanvasControl.ShowLayer2 = _showLayer2;
        CanvasControl.SetMap(_currentMap, _tileBitmaps);
        CanvasControl.SetEvents(_events, _currentSection, _selectedIndex);
        CanvasControl.SetZoom(CurrentZoom());
    }

    // ══════════════════════════════════════════════════════════════
    //   ЛЁГКАЯ ПЕРЕРИСОВКА — при смене раздела, выделения, слоёв
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Быстрая перерисовка: обновляет слои и оверлей, но НЕ пересоздаёт
    /// Image-контролы карты. Можно звать часто (клик по разделу, выделение).
    /// </summary>
    private void RedrawCanvas()
    {
        if (CanvasControl == null) return;
        if (_currentMap == null) return;

        // Если канвас вообще не показывал никакой карты — нужен полный ремонт
        if (CanvasControl.CurrentMap != _currentMap)
        {
            FullRedraw();
            return;
        }

        CanvasControl.ShowLayer1 = _showLayer1;
        CanvasControl.ShowLayer2 = _showLayer2;
        CanvasControl.SetEvents(_events, _currentSection, _selectedIndex);
        CanvasControl.Redraw();
    }

    // ══════════════════════════════════════════════════════════════
    //   ЗАГРУЗКА ТАЙЛСЕТА
    // ══════════════════════════════════════════════════════════════
    private void EnsureTilesetLoaded()
    {
        if (_currentMap == null) return;

        string tilesetName = Path.GetFileName(_currentMap.TilesetPath);
        string tilesetAbs  = Path.Combine(Core.Services.ProjectPaths.TilesetsDir, tilesetName);

        if (_loadedTilesetPath == tilesetAbs && _tileBitmaps.Count > 0)
            return;

        _tileBitmaps.Clear();
        _sourceTileset?.Dispose();
        _sourceTileset = null;
        _loadedTilesetPath = null;

        if (!File.Exists(tilesetAbs))
        {
            System.Diagnostics.Debug.WriteLine($"[EVENTS] Tileset not found: {tilesetAbs}");
            return;
        }

        try
        {
            _sourceTileset = new Bitmap(tilesetAbs);

            const int TileSize    = 48;
            const int PaletteCols = 8;

            int cols   = _sourceTileset.PixelSize.Width  / TileSize;
            int rows   = _sourceTileset.PixelSize.Height / TileSize;
            int strips = cols / PaletteCols;

            for (int strip = 0; strip < strips; strip++)
            {
                int startCol = strip * PaletteCols;
                int endCol   = startCol + PaletteCols;
                for (int r = 0; r < rows; r++)
                for (int c = startCol; c < endCol; c++)
                {
                    var crop = new CroppedBitmap(_sourceTileset,
                        new PixelRect(c * TileSize, r * TileSize, TileSize, TileSize));
                    _tileBitmaps.Add(crop);
                }
            }

            _loadedTilesetPath = tilesetAbs;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[EVENTS] LoadTileset error: {ex.Message}");
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   ТЕКУЩИЙ ЗУМ
    // ══════════════════════════════════════════════════════════════
    private double CurrentZoom()
    {
        if (ZoomSelector?.SelectedItem is ComboBoxItem item &&
            item.Content is string text)
        {
            return text switch
            {
                "1/4x" => 0.25,
                "1/2x" => 0.5,
                "1x"   => 1.0,
                "2x"   => 2.0,
                "4x"   => 4.0,
                _      => 1.0
            };
        }
        return 1.0;
    }

    // ══════════════════════════════════════════════════════════════
    //   КЛИК ПО КАНВАСУ
    // ══════════════════════════════════════════════════════════════
    private void OnCanvasTileClicked(int tx, int ty)
    {
        System.Diagnostics.Debug.WriteLine($"[EVENTS] Click at ({tx},{ty})");
        PushStatusBar(tx, ty);
    }

    private void PushStatusBar(int tx, int ty)
    {
        string pos = (tx < 0 || ty < 0) ? "—" : $"{tx}, {ty}";
        StatusChanged?.Invoke($"Позиция: {pos}");
    }
}
