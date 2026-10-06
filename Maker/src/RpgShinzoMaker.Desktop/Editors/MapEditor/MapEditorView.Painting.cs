// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.Painting.cs
using System.Diagnostics;
using Avalonia.Controls;
using Avalonia.Media.Imaging;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — РИСОВАНИЕ И КЛИКИ ПО КАРТЕ.
/// Здесь: реакция на клик/drag/hover по канвасу,
/// запись тайла в слой, применение трансформаций.
/// </summary>
public partial class MapEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   КЛИК ПО КАРТЕ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Логика одного клика по клетке карты.
    /// Режимы приоритетно: Select → Grid → рисование тайлом → Tile Editor.
    /// </summary>
    private void OnMapTileClicked(int tx, int ty, int tileId, bool isLeftButton)
    {
        if (_currentMap == null) return;

        int idx = tx * _currentMap.Height + ty;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        // ═══ Select Mode ═══
        if (_selectMode)
        {
            if (!isLeftButton)
            {
                // ПКМ — сброс clipboard
                _hasClipboard = false;
                MapCanvasControl.HideSelectionRect();
                MapCanvasControl.HidePasteRect();
                return;
            }

            if (_hasClipboard)
            {
                // ЛКМ при clipboard — вставка
                PasteClipboard(tx, ty);
                return;
            }

            // Начать выделение
            _selecting = true;
            _selStartX = _selEndX = tx;
            _selStartY = _selEndY = ty;
            MapCanvasControl.ShowSelectionRect(tx, ty, tx, ty);
            return;
        }

        // ═══ Grid Mode → назначаем тип тайла под курсором ═══
        if (_gridMode)
        {
            AssignTypeToMapTile(idx);
            return;
        }

        // ═══ Tile Editor ВЫКЛЮЧЕН → рисуем тайлом из палитры ═══
        if (!_tileEditorMode)
        {
            if (_isModeB) return;  // в режиме типов по карте не рисуем

            int paintTile = isLeftButton ? _leftSelectedIndex : _rightSelectedIndex;
            if (paintTile < 0) return;

            PaintTile(idx, paintTile);
            MapCanvasControl.RedrawTile(tx, ty);
            return;
        }

        // ═══ Tile Editor ВКЛЮЧЁН → трансформации ═══
        if (_transformMode == 0) return;

        ApplyTransform(tx, ty);

        int newTileId = (_currentLayer == 0) ? _currentMap.Tiles[idx]  : _currentMap.Tiles2[idx];
        int rot       = (_currentLayer == 0) ? _currentMap.Rot[idx]    : _currentMap.Rot2[idx];
        bool mx       = (_currentLayer == 0) ? _currentMap.MirrorX[idx] : _currentMap.MirrorX2[idx];
        bool my       = (_currentLayer == 0) ? _currentMap.MirrorY[idx] : _currentMap.MirrorY2[idx];

        if (_transformMode != 4)
        {
            var img = RenderTileWithTransform(newTileId, rot, mx, my);
            if (isLeftButton)
            {
                MapLeftPreview.Source = img;
                MapLeftLabel.Text     = newTileId >= 0 ? $"#{newTileId}" : "—";
            }
            else
            {
                MapRightPreview.Source = img;
                MapRightLabel.Text     = newTileId >= 0 ? $"#{newTileId}" : "—";
            }
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   DRAG ПО КАРТЕ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Непрерывное движение мыши с зажатой кнопкой по карте.
    /// Select: расширяет рамку выделения.
    /// Grid:   назначает типы тайлов по пути.
    /// Рисование: записывает тайл в каждую клетку под курсором.
    /// TileEditor + Delete: удаляет тайлы по пути.
    /// </summary>
    private void OnMapTileDragged(int tx, int ty, int tileId, bool isLeftButton)
    {
        if (_currentMap == null) return;

        int idx = tx * _currentMap.Height + ty;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        // ═══ Select Mode ═══
        if (_selectMode)
        {
            if (!isLeftButton) return;
            if (!_selecting) return;

            _selEndX = tx;
            _selEndY = ty;
            MapCanvasControl.ShowSelectionRect(_selStartX, _selStartY, _selEndX, _selEndY);
            return;
        }

        // ═══ Grid Mode → непрерывно назначаем тип ═══
        if (_gridMode)
        {
            AssignTypeToMapTile(idx);
            return;
        }

        // ═══ Tile Editor ВЫКЛЮЧЕН → непрерывное рисование ═══
        if (!_tileEditorMode)
        {
            if (_isModeB) return;

            int paintTile = isLeftButton ? _leftSelectedIndex : _rightSelectedIndex;
            if (paintTile < 0) return;

            // Уже такой тайл — не перерисовываем (оптимизация)
            int existing = (_currentLayer == 0)
                ? _currentMap.Tiles[idx]
                : _currentMap.Tiles2[idx];
            if (existing == paintTile) return;

            PaintTile(idx, paintTile);
            MapCanvasControl.RedrawTile(tx, ty);
            return;
        }

        // ═══ Tile Editor ВКЛЮЧЁН → drag для Delete ═══
        if (_transformMode != 4) return;

        int currentTile = (_currentLayer == 0)
            ? _currentMap.Tiles[idx]
            : _currentMap.Tiles2[idx];
        if (currentTile < 0) return;

        ApplyTransform(tx, ty);
    }

    // ══════════════════════════════════════════════════════════════
    //   ОТПУСКАНИЕ КНОПКИ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Отпускание ЛКМ — завершает выделение и копирует его в буфер.
    /// </summary>
    private void OnMapTileReleased(int tx, int ty, bool isLeftButton)
    {
        if (_currentMap == null) return;
        if (!_selectMode) return;
        if (!isLeftButton) return;
        if (!_selecting) return;

        _selecting = false;
        CopySelectionToClipboard();
    }

    // ══════════════════════════════════════════════════════════════
    //   HOVER (мышь без нажатой кнопки)
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Наведение мыши — обновляет позицию в статус-баре MainWindow
    /// и рисует призрачную рамку вставки в Select Mode.
    /// </summary>
    private void OnMapTileHover(int tx, int ty)
    {
        // Запомнить последнюю позицию и отправить в статус-бар
        _lastHoverX = tx;
        _lastHoverY = ty;
        PushStatusBar();

        if (!_selectMode || !_hasClipboard)
        {
            MapCanvasControl.HidePasteRect();
            return;
        }
        MapCanvasControl.ShowPasteRect(tx, ty, _clipboardW, _clipboardH);
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБНОВЛЕНИЕ СТАТУС-БАРА
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Формирует строку "Позиция: X, Y    Зум: Nx" и отправляет
    /// её в событие StatusChanged (подписан MainWindow).
    /// Если курсор вне карты — позиция выводится как "—".
    /// </summary>
    private void PushStatusBar()
    {
        string pos = (_lastHoverX < 0 || _lastHoverY < 0)
            ? "—"
            : $"{_lastHoverX}, {_lastHoverY}";

        string zoom = CurrentZoomLabel();

        StatusChanged?.Invoke($"Позиция: {pos}    Зум: {zoom}");
    }

    /// <summary>
    /// Возвращает текст текущего зума из комбо ZoomSelector
    /// (например "1x", "2x"). Если контрол ещё не готов — "1x".
    /// </summary>
    private string CurrentZoomLabel()
    {
        if (ZoomSelector?.SelectedItem is ComboBoxItem item &&
            item.Content is string text)
            return text;

        return "1x";
    }

    // ══════════════════════════════════════════════════════════════
    //   ЗАПИСЬ ТАЙЛА В СЛОЙ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Записывает тайл (tileId) в активный слой по индексу idx.
    /// Сбрасывает rot/mirror у записанной клетки.
    /// </summary>
    private void PaintTile(int idx, int tileId)
    {
        if (_currentMap == null) return;

        if (_currentLayer == 0)
        {
            _currentMap.Tiles[idx]   = tileId;
            _currentMap.Rot[idx]     = 0;
            _currentMap.MirrorX[idx] = false;
            _currentMap.MirrorY[idx] = false;
        }
        else
        {
            _currentMap.Tiles2[idx]   = tileId;
            _currentMap.Rot2[idx]     = 0;
            _currentMap.MirrorX2[idx] = false;
            _currentMap.MirrorY2[idx] = false;
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   ТРАНСФОРМАЦИИ (Tile Editor)
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Применяет активную трансформацию (_transformMode) к клетке (tx, ty).
    ///   1 = rotate +90°
    ///   2 = flip horizontal
    ///   3 = flip vertical
    ///   4 = delete (тайл становится -1)
    /// </summary>
    private void ApplyTransform(int tx, int ty)
    {
        var map = _currentMap;
        if (map == null) return;

        int idx = tx * map.Height + ty;
        if (idx < 0 || idx >= map.TotalCells) return;

        switch (_transformMode)
        {
            case 1: // Rotate
                if (_currentLayer == 0)
                    map.Rot[idx]  = (map.Rot[idx]  + 1) % 4;
                else
                    map.Rot2[idx] = (map.Rot2[idx] + 1) % 4;
                break;

            case 2: // Flip H
                if (_currentLayer == 0) map.MirrorX[idx]  = !map.MirrorX[idx];
                else                    map.MirrorX2[idx] = !map.MirrorX2[idx];
                break;

            case 3: // Flip V
                if (_currentLayer == 0) map.MirrorY[idx]  = !map.MirrorY[idx];
                else                    map.MirrorY2[idx] = !map.MirrorY2[idx];
                break;

            case 4: // Delete
                if (_currentLayer == 0) map.Tiles[idx]  = -1;
                else                    map.Tiles2[idx] = -1;
                break;
        }

        MapCanvasControl.RedrawTile(tx, ty);
    }

    // ══════════════════════════════════════════════════════════════
    //   GRID MODE — назначение типа тайлу, который лежит на карте
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Меняет тип тайла, который находится в клетке idx на карте.
    /// Обновляет палитру (квадратик типа) и оверлей Grid Mode на канвасе.
    /// </summary>
    private void AssignTypeToMapTile(int idx)
    {
        if (_currentMap == null) return;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        int tileId = (_currentLayer == 0)
            ? _currentMap.Tiles[idx]
            : _currentMap.Tiles2[idx];

        if (tileId < 0 || tileId >= _tileTypes.Length) return;

        // Если тип не меняется — ничего не делаем
        if (_tileTypes[tileId] == _currentTileType) return;

        _tileTypes[tileId] = _currentTileType;

        // Обновить квадратик в палитре
        if (tileId < _tiles.Count)
            _tiles[tileId].TileType = _currentTileType;

        // Обновить точки Grid Mode на карте
        MapCanvasControl.UpdateGridOverlay();
    }
}
