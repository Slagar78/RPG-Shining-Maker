// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.Select.cs
using System;
using System.Diagnostics;
using Avalonia.Interactivity;
using Avalonia.Media;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — SELECT MODE И БУФЕР ОБМЕНА.
/// Здесь: включение/выключение режима выделения,
/// копирование выделенной области и вставка.
/// </summary>
public partial class MapEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   ВКЛЮЧЕНИЕ / ВЫКЛЮЧЕНИЕ SELECT MODE
    // ══════════════════════════════════════════════════════════════
    private void OnSelectToggle(object? sender, RoutedEventArgs e)
    {
        _selectMode = SelectButton.IsChecked == true;

        SelectButton.Background = new SolidColorBrush(
            Color.Parse(_selectMode ? "#C83232" : "#3E3E42"));

        if (_selectMode)
        {
            // ═══ Взаимоисключение с Tile Editor ═══
            if (_tileEditorMode)
            {
                TileEditorToggle.IsChecked = false;
                _tileEditorMode = false;
                _transformMode = 0;

                TileEditorIcon.Text = "✗";
                TileEditorIcon.Foreground = new SolidColorBrush(Color.Parse("#E74C3C"));

                RotateBtn.IsEnabled = false;
                FlipHBtn.IsEnabled  = false;
                FlipVBtn.IsEnabled  = false;
                DeleteBtn.IsEnabled = false;

                UpdateTransformHighlight();

                MapLeftPreview.Source  = null;
                MapRightPreview.Source = null;
                MapLeftLabel.Text  = "—";
                MapRightLabel.Text = "—";
            }

            // ═══ Взаимоисключение с Grid Mode ═══
            if (_gridMode)
            {
                GridModeButton.IsChecked = false;
                _gridMode = false;
                GridModeButton.Background = new SolidColorBrush(Color.Parse("#3E3E42"));
                GridModeToggle.Content = "OFF";
                MapCanvasControl.ShowGridMode = false;
                MapCanvasControl.UpdateGridOverlay();
            }
        }
        else
        {
            ClearSelectState();
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   СБРОС СОСТОЯНИЯ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Полный сброс состояния выделения и буфера обмена.
    /// Вызывается при выключении Select Mode, а также из
    /// OnGridModeToggle и OnTileEditorToggle (взаимоисключение).
    /// </summary>
    private void ClearSelectState()
    {
        _selecting    = false;
        _hasClipboard = false;

        _clipboardTiles  = null;
        _clipboardRot    = null;
        _clipboardMX     = null;
        _clipboardMY     = null;

        _clipboardTiles2 = null;
        _clipboardRot2   = null;
        _clipboardMX2    = null;
        _clipboardMY2    = null;

        MapCanvasControl.HideSelectionRect();
        MapCanvasControl.HidePasteRect();
    }

    // ══════════════════════════════════════════════════════════════
    //   КОПИРОВАНИЕ ВЫДЕЛЕНИЯ В БУФЕР
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Копирует прямоугольную область выделения в буфер обмена.
    /// Копируется ТОЛЬКО активный слой (L1 или L2).
    /// Вызывается из OnMapTileReleased после отпускания ЛКМ.
    /// </summary>
    private void CopySelectionToClipboard()
    {
        if (_currentMap == null) return;

        int x1 = Math.Min(_selStartX, _selEndX);
        int y1 = Math.Min(_selStartY, _selEndY);
        int x2 = Math.Max(_selStartX, _selEndX);
        int y2 = Math.Max(_selStartY, _selEndY);

        int w = x2 - x1 + 1;
        int h = y2 - y1 + 1;

        _clipboardW = w;
        _clipboardH = h;
        int sz = w * h;

        _clipboardTiles  = new int[sz];
        _clipboardRot    = new int[sz];
        _clipboardMX     = new bool[sz];
        _clipboardMY     = new bool[sz];
        _clipboardTiles2 = new int[sz];
        _clipboardRot2   = new int[sz];
        _clipboardMX2    = new bool[sz];
        _clipboardMY2    = new bool[sz];

        for (int dx = 0; dx < w; dx++)
        for (int dy = 0; dy < h; dy++)
        {
            int mapX = x1 + dx;
            int mapY = y1 + dy;
            int srcIdx = mapX * _currentMap.Height + mapY;
            int dstIdx = dx * h + dy;

            // Берём ТОЛЬКО активный слой
            if (_currentLayer == 0)
            {
                _clipboardTiles[dstIdx] = _currentMap.Tiles[srcIdx];
                _clipboardRot[dstIdx]   = _currentMap.Rot[srcIdx];
                _clipboardMX[dstIdx]    = _currentMap.MirrorX[srcIdx];
                _clipboardMY[dstIdx]    = _currentMap.MirrorY[srcIdx];
            }
            else
            {
                _clipboardTiles2[dstIdx] = _currentMap.Tiles2[srcIdx];
                _clipboardRot2[dstIdx]   = _currentMap.Rot2[srcIdx];
                _clipboardMX2[dstIdx]    = _currentMap.MirrorX2[srcIdx];
                _clipboardMY2[dstIdx]    = _currentMap.MirrorY2[srcIdx];
            }
        }

        _hasClipboard = true;
        Debug.WriteLine($"[SELECT] Скопировано {w}x{h} (from {x1},{y1})");
    }

    // ══════════════════════════════════════════════════════════════
    //   ВСТАВКА БУФЕРА
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Вставляет содержимое буфера в точку (destX, destY).
    /// Клетки за пределами карты обрезаются.
    /// Вызывается из OnMapTileClicked в Select Mode при наличии буфера.
    /// </summary>
    private void PasteClipboard(int destX, int destY)
    {
        if (_currentMap == null) return;
        if (!_hasClipboard) return;
        if (_clipboardTiles == null) return;

        for (int dx = 0; dx < _clipboardW; dx++)
        for (int dy = 0; dy < _clipboardH; dy++)
        {
            int tx = destX + dx;
            int ty = destY + dy;
            if (tx < 0 || tx >= _currentMap.Width)  continue;
            if (ty < 0 || ty >= _currentMap.Height) continue;

            int dstIdx = tx * _currentMap.Height + ty;
            int srcIdx = dx * _clipboardH + dy;

            if (_currentLayer == 0)
            {
                _currentMap.Tiles[dstIdx]   = _clipboardTiles![srcIdx];
                _currentMap.Rot[dstIdx]     = _clipboardRot![srcIdx];
                _currentMap.MirrorX[dstIdx] = _clipboardMX![srcIdx];
                _currentMap.MirrorY[dstIdx] = _clipboardMY![srcIdx];
            }
            else
            {
                _currentMap.Tiles2[dstIdx]   = _clipboardTiles2![srcIdx];
                _currentMap.Rot2[dstIdx]     = _clipboardRot2![srcIdx];
                _currentMap.MirrorX2[dstIdx] = _clipboardMX2![srcIdx];
                _currentMap.MirrorY2[dstIdx] = _clipboardMY2![srcIdx];
            }
        }

        MapCanvasControl.Redraw();
        Debug.WriteLine($"[SELECT] Вставлено в ({destX},{destY})");
    }
}