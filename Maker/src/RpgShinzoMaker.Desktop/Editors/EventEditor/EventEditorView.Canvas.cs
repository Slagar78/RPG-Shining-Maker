// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.Canvas.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
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

    /// <summary>
    /// Перерисовывает канвас: карта + подсветки.
    /// </summary>
    private void RedrawCanvas()
    {
        if (CanvasControl == null) return;
        if (_currentMap == null) return;

        // 1. Нарезать тайлсет, если он сменился
        EnsureTilesetLoaded();

        // 2. Передать карту и тайлы
        CanvasControl.ShowLayer1 = _showLayer1;
        CanvasControl.ShowLayer2 = _showLayer2;
        CanvasControl.SetMap(_currentMap, _tileBitmaps);

        // 3. Передать события и активный раздел
        CanvasControl.SetEvents(_events, _currentSection, _selectedIndex);

        // 4. Зум
        double zoom = CurrentZoom();
        CanvasControl.SetZoom(zoom);
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
            return; // уже загружен

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
    //   КЛИК ПО КАНВАСУ — заполнить активное поле координатами
    // ══════════════════════════════════════════════════════════════
    private void OnCanvasTileClicked(int tx, int ty)
    {
        // Пока просто выводим в Debug. Позже — заполнить активное поле.
        System.Diagnostics.Debug.WriteLine($"[EVENTS] Click at ({tx},{ty})");
        PushStatusBar(tx, ty);
    }

    private void PushStatusBar(int tx, int ty)
    {
        string pos = (tx < 0 || ty < 0) ? "—" : $"{tx}, {ty}";
        StatusChanged?.Invoke($"Позиция: {pos}");
    }
}