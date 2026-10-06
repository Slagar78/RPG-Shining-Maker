// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.Palette.cs
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — БЛОКСЕТ и ПАЛИТРА.
/// Здесь: загрузка тайлсета, режимы A/B, Grid Mode,
/// назначение типов тайлам палитры, обработка кликов по палитре.
/// </summary>
public partial class MapEditorView
{
    // ─── Константы палитры ───
    private const int PaletteCols = 8;
    private const int TileSize    = 48;

    // ─── Состояние тайлсета ───
    private Bitmap? _sourceTileset;
    private List<TileItem> _tiles = new();
    private int[] _tileTypes = Array.Empty<int>();

    // ─── Состояние палитры ───
    private bool _isModeB;
    private int  _currentTileType = 0;
    private int  _leftSelectedIndex  = -1;
    private int  _rightSelectedIndex = -1;

    // ══════════════════════════════════════════════════════════════
    //   ЗАГРУЗКА ТАЙЛСЕТА
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Нарезает тайлсет на кусочки 48×48 (strip-based, по 8 тайлов в строку),
    /// заполняет _tiles и _tileTypes, обновляет палитру в UI.
    /// </summary>
    private void LoadTileset(string path)
    {
        try
        {
            _sourceTileset?.Dispose();
            _sourceTileset = new Bitmap(path);

            int cols   = _sourceTileset.PixelSize.Width  / TileSize;
            int rows   = _sourceTileset.PixelSize.Height / TileSize;
            int strips = cols / PaletteCols;

            var newTiles = new List<TileItem>();
            int idx = 0;
            for (int strip = 0; strip < strips; strip++)
            {
                int startCol = strip * PaletteCols;
                int endCol   = startCol + PaletteCols;
                for (int r = 0; r < rows; r++)
                for (int c = startCol; c < endCol; c++)
                {
                    var crop = new CroppedBitmap(_sourceTileset,
                        new PixelRect(c * TileSize, r * TileSize, TileSize, TileSize));

                    newTiles.Add(new TileItem
                    {
                        Image    = crop,
                        Index    = idx,
                        TileType = 0,
                        ShowType = _isModeB
                    });
                    idx++;
                }
            }

            _tiles = newTiles;

            // Загрузить типы из data/tile_types/{tileset}.json
            _tileTypes = TileTypesService.Load(path, _tiles.Count);
            for (int i = 0; i < _tiles.Count; i++)
                _tiles[i].TileType = _tileTypes[i];

            // Отдать типы канвасу — для отрисовки оверлея Grid Mode
            MapCanvasControl.TileTypes = _tileTypes;

            ClearSelections();
            ClearPreviews();

            PaletteList.ItemsSource = null;
            PaletteList.ItemsSource = _tiles;

            const int expectedTiles = 1024;
            bool isValid = _tiles.Count == expectedTiles;

            PaletteStatus.Text = isValid ? "✓" : "✗";
            PaletteStatus.Foreground = isValid ? Brushes.LightGreen : Brushes.IndianRed;
            PaletteInfo.Text = $"Тайлов: {_tiles.Count}";
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[TILESET] Ошибка: {ex.Message}");
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   РЕЖИМЫ A / B
    // ══════════════════════════════════════════════════════════════
    private void OnModeAClick(object? sender, RoutedEventArgs e)
    {
        ModeA.IsChecked = true;
        ModeB.IsChecked = false;
        SetModeB(false);
    }

    private void OnModeBClick(object? sender, RoutedEventArgs e)
    {
        ModeA.IsChecked = false;
        ModeB.IsChecked = true;
        SetModeB(true);
    }

    private void SetModeB(bool isB)
    {
        _isModeB = isB;

        foreach (var tile in _tiles)
            tile.ShowType = isB;

        ClearSelections();
        ClearPreviews();
        UpdatePreviewVisibility();

        // Обновить стиль иконок типов (мигание только в B)
        UpdateTypeIconSelection();
    }

    private void UpdatePreviewVisibility()
    {
        ModeAPreviewGrid.IsVisible  = !_isModeB;
        ModeBPreviewPanel.IsVisible = _isModeB;
    }

    private void ClearSelections()
    {
        _leftSelectedIndex  = -1;
        _rightSelectedIndex = -1;
        foreach (var t in _tiles)
        {
            t.LeftSelected  = false;
            t.RightSelected = false;
        }
    }

    private void ClearPreviews()
    {
        LeftClickPreview.Source  = null;
        RightClickPreview.Source = null;
        LeftClickLabel.Text  = "—";
        RightClickLabel.Text = "—";
    }

    // ══════════════════════════════════════════════════════════════
    //   GRID MODE — маленький тумблер внутри блока BLOCKSET
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Маленький тумблер "Grid" рядом с иконками типов.
    /// Пока ничего не делает — переключение делает большая кнопка Grid Mode
    /// в панели TOOLS (OnGridModeToggle).
    /// </summary>
    private void OnGridModeClick(object? sender, RoutedEventArgs e) { }

    // ══════════════════════════════════════════════════════════════
    //   ИКОНКИ ТИПОВ (4 кружка)
    // ══════════════════════════════════════════════════════════════
    private void OnTileTypeClick(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Border border || border.Tag is not string tagStr) return;
        if (!int.TryParse(tagStr, out int type)) return;

        _currentTileType = type;
        UpdateTypeIconSelection();
        UpdateTypePreview();
    }

    private void UpdateTypeIconSelection()
    {
        var icons = new[] { TypeIcon0, TypeIcon1, TypeIcon2, TypeIcon3 };
        for (int i = 0; i < icons.Length; i++)
        {
            bool selected = i == _currentTileType;

            icons[i].BorderBrush = selected
                ? Brushes.White
                : new SolidColorBrush(Color.Parse("#555555"));
            icons[i].BorderThickness = new Thickness(2);

            // Мигание — только в Mode B
            icons[i].Classes.Set("selected", selected && _isModeB);
        }
    }

    private void UpdateTypePreview()
    {
        var (brush, label) = _currentTileType switch
        {
            0 => ((IBrush)new SolidColorBrush(Color.Parse("#4CAF50")), "Passable"),
            1 => (new SolidColorBrush(Color.Parse("#E74C3C")),         "Block"),
            2 => (new SolidColorBrush(Color.Parse("#3498DB")),         "Slow"),
            3 => (new SolidColorBrush(Color.Parse("#E67E22")),         "Under"),
            _ => (Brushes.Gray, "Unknown")
        };
        TypePreviewEllipse.Fill = brush;
        TypePreviewLabel.Text   = label;
    }

    // ══════════════════════════════════════════════════════════════
    //   КЛИКИ ПО ПАЛИТРЕ
    // ══════════════════════════════════════════════════════════════
    private void OnPaletteItemPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Grid grid || grid.Tag is not TileItem tile) return;

        var props = e.GetCurrentPoint(grid).Properties;

        // ═══ Mode B → назначаем текущий тип тайлу палитры ═══
        if (_isModeB)
        {
            if (props.IsLeftButtonPressed || props.IsRightButtonPressed)
            {
                AssignTypeToPaletteTile(tile);

                // Снять захват, иначе PointerMoved не пойдёт на соседние Grid
                e.Pointer.Capture(null);
            }
            return;
        }

        // ═══ Mode A → выбор тайла для рисования ═══
        if (props.IsLeftButtonPressed)
        {
            _leftSelectedIndex = tile.Index;
            foreach (var t in _tiles)
                t.LeftSelected = (t.Index == tile.Index);

            LeftClickPreview.Source = tile.Image;
            LeftClickLabel.Text     = $"Tile #{tile.Index}";
        }
        else if (props.IsRightButtonPressed)
        {
            _rightSelectedIndex = tile.Index;
            foreach (var t in _tiles)
                t.RightSelected = (t.Index == tile.Index);

            RightClickPreview.Source = tile.Image;
            RightClickLabel.Text     = $"Tile #{tile.Index}";
        }
    }

    private void OnPaletteItemPointerMoved(object? sender, PointerEventArgs e)
    {
        if (!_isModeB) return;
        if (sender is not Grid grid || grid.Tag is not TileItem tile) return;

        var props = e.GetCurrentPoint(grid).Properties;
        if (!props.IsLeftButtonPressed && !props.IsRightButtonPressed) return;

        AssignTypeToPaletteTile(tile);
    }

    private void OnPaletteSelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (!_isModeB) return;
        if (PaletteList.SelectedItem is not TileItem tile) return;

        if (tile.Index >= 0 && tile.Index < _tileTypes.Length)
            _tileTypes[tile.Index] = _currentTileType;

        tile.TileType = _currentTileType;
    }

    /// <summary>
    /// Назначает текущий тип (из _currentTileType) конкретному тайлу палитры.
    /// Обновляет _tileTypes, сам TileItem и оверлей Grid Mode на карте.
    /// </summary>
    private void AssignTypeToPaletteTile(TileItem tile)
    {
        if (tile.Index < 0 || tile.Index >= _tileTypes.Length) return;

        _tileTypes[tile.Index] = _currentTileType;
        tile.TileType = _currentTileType;

        // Обновить оверлей на карте, если Grid Mode включён
        MapCanvasControl.UpdateGridOverlay();
    }
}