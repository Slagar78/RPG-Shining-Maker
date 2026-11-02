using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

/// <summary>
/// Часть BattleEditorView — тайлсет, зум, режимы Map/Battle/Terrain.
/// </summary>
public partial class BattleEditorView
{
    private List<CroppedBitmap> _tiles = new();
    private Bitmap? _sourceTileset;

    private const int TileSize    = 48;
    private const int PaletteCols = 8;

    // Размер тайла на экране = 48 * Zoom
    private int Ts => (int)(GameMap.TileSize * MapCanvasControl.Zoom);

    // ══════════════════════════════════════════════════════════
    //   НАРЕЗКА ТАЙЛСЕТА
    // ══════════════════════════════════════════════════════════
    /// <summary>
    /// Режет PNG-тайлсет на CroppedBitmap 48×48. Палитра — 8 тайлов в ряд.
    /// Тот же алгоритм, что в MapEditorView.Palette.LoadTileset.
    /// </summary>
    private void LoadTilesetFromMap()
    {
        _tiles = new List<CroppedBitmap>();

        if (_currentMap == null) return;
        if (string.IsNullOrEmpty(_currentMap.TilesetPath)) return;

        var tilesetName = Path.GetFileName(_currentMap.TilesetPath);
        var tilesetPath = Path.Combine(ProjectPaths.TilesetsDir, tilesetName);
        if (!File.Exists(tilesetPath))
        {
            Debug.WriteLine($"[BATTLE] Tileset not found: {tilesetPath}");
            return;
        }

        _sourceTileset?.Dispose();
        _sourceTileset = new Bitmap(tilesetPath);

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
                    new Avalonia.PixelRect(c * TileSize, r * TileSize, TileSize, TileSize));
                _tiles.Add(crop);
            }
        }

        Debug.WriteLine($"[BATTLE] Tileset: {_tiles.Count} тайлов");
    }

    // ══════════════════════════════════════════════════════════
    //   ЗУМ
    // ══════════════════════════════════════════════════════════
    private void OnZoomChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (MapCanvasControl == null) return;
        if (ZoomSelector?.SelectedItem is not ComboBoxItem item) return;
        if (item.Content is not string text) return;

        double zoom = text switch
        {
            "1/4x" => 0.25,
            "1/2x" => 0.5,
            "1x"   => 1.0,
            "2x"   => 2.0,
            "4x"   => 4.0,
            _      => 1.0
        };

        MapCanvasControl.SetZoom(zoom);
        RebuildOverlay();
    }

    // ══════════════════════════════════════════════════════════
    //   РЕЖИМЫ
    // ══════════════════════════════════════════════════════════
    private void OnModeMapClick(object? sender, RoutedEventArgs e)
    {
        _viewMode = 0;
        ApplyModeButtons();
        ApplyViewMode();
    }

    private void OnModeBattleClick(object? sender, RoutedEventArgs e)
    {
        _viewMode = 1;
        ApplyModeButtons();
        ApplyViewMode();
    }

    private void OnModeTerrainClick(object? sender, RoutedEventArgs e)
    {
        _viewMode = 2;
        ApplyModeButtons();
        ApplyViewMode();
    }

    private void ApplyModeButtons()
    {
        if (ModeMapBtn     != null) ModeMapBtn.IsChecked     = _viewMode == 0;
        if (ModeBattleBtn  != null) ModeBattleBtn.IsChecked  = _viewMode == 1;
        if (ModeTerrainBtn != null) ModeTerrainBtn.IsChecked = _viewMode == 2;

        // показываем кисти только в режиме Terrain
        if (TerrainBrushesPanel != null) TerrainBrushesPanel.IsVisible = _viewMode == 2;

        if (BottomModeText != null)
            BottomModeText.Text = _viewMode switch
            {
                0 => "GLOBAL MAP",
                1 => "BATTLE ZONE",
                2 => "TERRAIN EDIT",
                _ => "—"
            };
    }

    /// <summary>
    /// В режиме Battle — ставит зум/скролл так, чтобы зона была видна.
    /// В остальных — сбрасывает на 1x и в начало карты.
    /// </summary>
    private void ApplyViewMode()
    {
        if (_currentMap == null || _currentBattleIndex < 0) return;
        if (MapCanvasControl == null) return;

        var entry = _battleEntries[_currentBattleIndex];

        if (_viewMode == 1 && entry.BattleWidth > 0 && entry.BattleHeight > 0)
        {
            // Подбираем зум так, чтобы зона влезла (не больше 1x)
            var scrollW = _mapScrollViewer?.Viewport.Width  ?? 800;
            var scrollH = _mapScrollViewer?.Viewport.Height ?? 600;
            double zw = scrollW / (entry.BattleWidth  * (double)GameMap.TileSize);
            double zh = scrollH / (entry.BattleHeight * (double)GameMap.TileSize);
            double zoom = Math.Min(1.0, Math.Min(zw, zh));
            if (zoom < 0.25) zoom = 0.25;

            MapCanvasControl.SetZoom(zoom);
            SetScrollTo(entry.BattleX * Ts, entry.BattleY * Ts);
        }
        else
        {
            MapCanvasControl.SetZoom(1.0);
            SetScrollTo(0, 0);
        }

        RebuildOverlay();
    }

    private void SetScrollTo(double x, double y)
    {
        if (_mapScrollViewer == null) return;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        _mapScrollViewer.Offset = new Avalonia.Vector(x, y);
    }
}