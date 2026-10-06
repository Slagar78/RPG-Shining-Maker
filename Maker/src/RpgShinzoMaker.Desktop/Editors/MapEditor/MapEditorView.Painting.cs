// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.Painting.cs
using System.Diagnostics;
using Avalonia.Controls;
using Avalonia.Media.Imaging;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — РИСОВАНИЕ И КЛИКИ ПО КАРТЕ.
/// Здесь: реакция на клик/drag/hover по канвасу,
/// запись тайла в слой, применение трансформаций, запись undo.
/// </summary>
public partial class MapEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   КЛИК ПО КАРТЕ
    // ══════════════════════════════════════════════════════════════
    private void OnMapTileClicked(int tx, int ty, int tileId, bool isLeftButton)
    {
        if (_currentMap == null) return;

        int idx = tx * _currentMap.Height + ty;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        // Новая операция — завершаем предыдущую (если пользователь кликнул
        // после drag, который закончился за пределами карты)
        FinalizeBatch();

        // ═══ Select Mode ═══
        if (_selectMode)
        {
            if (!isLeftButton)
            {
                _hasClipboard = false;
                MapCanvasControl.HideSelectionRect();
                MapCanvasControl.HidePasteRect();
                return;
            }

            if (_hasClipboard)
            {
                PasteClipboard(tx, ty);
                return;
            }

            _selecting = true;
            _selStartX = _selEndX = tx;
            _selStartY = _selEndY = ty;
            MapCanvasControl.ShowSelectionRect(tx, ty, tx, ty);
            return;
        }

        // ═══ Grid Mode ═══
        if (_gridMode)
        {
            AssignTypeToMapTile(idx);
            return;
        }

        // ═══ Рисование тайлом ═══
        if (!_tileEditorMode)
        {
            if (_isModeB) return;

            int paintTile = isLeftButton ? _leftSelectedIndex : _rightSelectedIndex;
            if (paintTile < 0) return;

            PaintTile(idx, paintTile);
            MapCanvasControl.RedrawTile(tx, ty);
            return;
        }

        // ═══ Tile Editor ═══
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
    private void OnMapTileDragged(int tx, int ty, int tileId, bool isLeftButton)
    {
        if (_currentMap == null) return;

        int idx = tx * _currentMap.Height + ty;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        if (_selectMode)
        {
            if (!isLeftButton) return;
            if (!_selecting) return;

            _selEndX = tx;
            _selEndY = ty;
            MapCanvasControl.ShowSelectionRect(_selStartX, _selStartY, _selEndX, _selEndY);
            return;
        }

        if (_gridMode)
        {
            AssignTypeToMapTile(idx);
            return;
        }

        if (!_tileEditorMode)
        {
            if (_isModeB) return;

            int paintTile = isLeftButton ? _leftSelectedIndex : _rightSelectedIndex;
            if (paintTile < 0) return;

            int existing = (_currentLayer == 0)
                ? _currentMap.Tiles[idx]
                : _currentMap.Tiles2[idx];
            if (existing == paintTile) return;

            PaintTile(idx, paintTile);
            MapCanvasControl.RedrawTile(tx, ty);
            return;
        }

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
    private void OnMapTileReleased(int tx, int ty, bool isLeftButton)
    {
        if (_currentMap == null) return;

        // Завершить текущий undo-batch: одна линия drag = одна отмена
        FinalizeBatch();

        if (!_selectMode) return;
        if (!isLeftButton) return;
        if (!_selecting) return;

        _selecting = false;
        CopySelectionToClipboard();
    }

    // ══════════════════════════════════════════════════════════════
    //   HOVER
    // ══════════════════════════════════════════════════════════════
    private void OnMapTileHover(int tx, int ty)
    {
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
    //   СТАТУС-БАР
    // ══════════════════════════════════════════════════════════════
    private void PushStatusBar()
    {
        string pos = (_lastHoverX < 0 || _lastHoverY < 0)
            ? "—"
            : $"{_lastHoverX}, {_lastHoverY}";

        string zoom = CurrentZoomLabel();

        StatusChanged?.Invoke($"Позиция: {pos}    Зум: {zoom}");
    }

    private string CurrentZoomLabel()
    {
        if (ZoomSelector?.SelectedItem is ComboBoxItem item &&
            item.Content is string text)
            return text;

        return "1x";
    }

    // ══════════════════════════════════════════════════════════════
    //   ЗАПИСЬ ТАЙЛА В СЛОЙ (с записью в undo)
    // ══════════════════════════════════════════════════════════════
    private void PaintTile(int idx, int tileId)
    {
        if (_currentMap == null) return;

        // Снимок "до"
        var oldSnap = SnapshotCell(idx, _currentLayer);

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

        // Снимок "после"
        var newSnap = SnapshotCell(idx, _currentLayer);

        // Записываем только если реально изменилось
        if (!SnapshotsEqual(oldSnap, newSnap))
            RecordChange(idx, _currentLayer, oldSnap, newSnap);
    }

    // ══════════════════════════════════════════════════════════════
    //   ТРАНСФОРМАЦИИ (с записью в undo)
    // ══════════════════════════════════════════════════════════════
    private void ApplyTransform(int tx, int ty)
    {
        var map = _currentMap;
        if (map == null) return;

        int idx = tx * map.Height + ty;
        if (idx < 0 || idx >= map.TotalCells) return;

        var oldSnap = SnapshotCell(idx, _currentLayer);

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

        var newSnap = SnapshotCell(idx, _currentLayer);
        if (!SnapshotsEqual(oldSnap, newSnap))
            RecordChange(idx, _currentLayer, oldSnap, newSnap);

        MapCanvasControl.RedrawTile(tx, ty);
    }

    // ══════════════════════════════════════════════════════════════
    //   GRID MODE — назначение типа
    // ══════════════════════════════════════════════════════════════
    private void AssignTypeToMapTile(int idx)
    {
        if (_currentMap == null) return;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        int tileId = (_currentLayer == 0)
            ? _currentMap.Tiles[idx]
            : _currentMap.Tiles2[idx];

        if (tileId < 0 || tileId >= _tileTypes.Length) return;

        if (_tileTypes[tileId] == _currentTileType) return;

        _tileTypes[tileId] = _currentTileType;

        if (tileId < _tiles.Count)
            _tiles[tileId].TileType = _currentTileType;

        MapCanvasControl.UpdateGridOverlay();
    }
}
