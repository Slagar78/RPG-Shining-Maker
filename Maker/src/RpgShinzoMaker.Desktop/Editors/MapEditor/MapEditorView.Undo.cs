// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.Undo.cs
using System.Collections.Generic;
using Avalonia.Interactivity;
using Avalonia.Media;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — UNDO / REDO.
/// Одна операция = одно действие пользователя (клик или вся линия при drag).
/// Снимок ячейки включает: tile, rot, mirrorX, mirrorY.
/// </summary>
public partial class MapEditorView
{
    // ─── Снимок одной ячейки ───
    private struct CellSnapshot
    {
        public int  Tile;
        public int  Rot;
        public bool MirrorX;
        public bool MirrorY;
    }

    // ─── Изменение одной ячейки ───
    private class CellChange
    {
        public int          Idx;
        public int          Layer;   // 0 = L1, 1 = L2
        public CellSnapshot Old;
        public CellSnapshot New;
    }

    // ─── Операция undo (может содержать много изменений) ───
    private class UndoRecord
    {
        public List<CellChange> Changes = new();
    }

    // ─── Стеки ───
    private readonly List<UndoRecord> _undoStack = new();
    private readonly List<UndoRecord> _redoStack = new();

    // ─── Текущий batch (для одной линии drag) ───
    private UndoRecord? _currentBatch;

    // ══════════════════════════════════════════════════════════════
    //   СНИМОК ЯЧЕЙКИ
    // ══════════════════════════════════════════════════════════════
    private CellSnapshot SnapshotCell(int idx, int layer)
    {
        if (_currentMap == null) return default;

        if (layer == 0)
        {
            return new CellSnapshot
            {
                Tile    = _currentMap.Tiles[idx],
                Rot     = _currentMap.Rot[idx],
                MirrorX = _currentMap.MirrorX[idx],
                MirrorY = _currentMap.MirrorY[idx],
            };
        }
        else
        {
            return new CellSnapshot
            {
                Tile    = _currentMap.Tiles2[idx],
                Rot     = _currentMap.Rot2[idx],
                MirrorX = _currentMap.MirrorX2[idx],
                MirrorY = _currentMap.MirrorY2[idx],
            };
        }
    }

    private static bool SnapshotsEqual(in CellSnapshot a, in CellSnapshot b)
    {
        return a.Tile == b.Tile
            && a.Rot == b.Rot
            && a.MirrorX == b.MirrorX
            && a.MirrorY == b.MirrorY;
    }

    // ══════════════════════════════════════════════════════════════
    //   ПРИМЕНИТЬ СНИМОК
    // ══════════════════════════════════════════════════════════════
    private void ApplySnapshot(int idx, int layer, in CellSnapshot snap)
    {
        if (_currentMap == null) return;

        if (layer == 0)
        {
            _currentMap.Tiles[idx]   = snap.Tile;
            _currentMap.Rot[idx]     = snap.Rot;
            _currentMap.MirrorX[idx] = snap.MirrorX;
            _currentMap.MirrorY[idx] = snap.MirrorY;
        }
        else
        {
            _currentMap.Tiles2[idx]   = snap.Tile;
            _currentMap.Rot2[idx]     = snap.Rot;
            _currentMap.MirrorX2[idx] = snap.MirrorX;
            _currentMap.MirrorY2[idx] = snap.MirrorY;
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   ЗАПИСЬ ИЗМЕНЕНИЯ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Добавляет изменение в текущий batch.
    /// Batch создаётся лениво — при первом изменении.
    /// Публикуется в undo-стек в FinalizeBatch().
    /// </summary>
    private void RecordChange(int idx, int layer, in CellSnapshot old, in CellSnapshot now)
    {
        if (_currentBatch == null)
            _currentBatch = new UndoRecord();

        _currentBatch.Changes.Add(new CellChange
        {
            Idx   = idx,
            Layer = layer,
            Old   = old,
            New   = now,
        });
    }

    // ══════════════════════════════════════════════════════════════
    //   ФИНАЛИЗАЦИЯ BATCH
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Публикует текущий batch в undo-стек и очищает redo.
    /// Вызывается: при отпускании мыши и при начале нового клика.
    /// </summary>
    private void FinalizeBatch()
    {
        if (_currentBatch == null) return;
        if (_currentBatch.Changes.Count == 0)
        {
            _currentBatch = null;
            return;
        }

        _undoStack.Add(_currentBatch);
        _redoStack.Clear();
        _currentBatch = null;

        UpdateUndoButtons();
    }

    // ══════════════════════════════════════════════════════════════
    //   UNDO
    // ══════════════════════════════════════════════════════════════
    private void OnUndoClick(object? sender, RoutedEventArgs e)
    {
        if (_undoStack.Count == 0) return;
        if (_currentMap == null) return;

        // Сначала закрываем незавершённый batch
        FinalizeBatch();

        var rec = _undoStack[^1];
        _undoStack.RemoveAt(_undoStack.Count - 1);

        foreach (var ch in rec.Changes)
            ApplySnapshot(ch.Idx, ch.Layer, ch.Old);

        _redoStack.Add(rec);

        MapCanvasControl.Redraw();
        UpdateUndoButtons();
    }

    // ══════════════════════════════════════════════════════════════
    //   REDO
    // ══════════════════════════════════════════════════════════════
    private void OnRedoClick(object? sender, RoutedEventArgs e)
    {
        if (_redoStack.Count == 0) return;
        if (_currentMap == null) return;

        FinalizeBatch();

        var rec = _redoStack[^1];
        _redoStack.RemoveAt(_redoStack.Count - 1);

        foreach (var ch in rec.Changes)
            ApplySnapshot(ch.Idx, ch.Layer, ch.New);

        _undoStack.Add(rec);

        MapCanvasControl.Redraw();
        UpdateUndoButtons();
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБНОВЛЕНИЕ СОСТОЯНИЯ КНОПОК
    // ══════════════════════════════════════════════════════════════
    private void UpdateUndoButtons()
    {
        if (UndoBtn == null || RedoBtn == null) return;

        UndoBtn.IsEnabled = _undoStack.Count > 0;
        RedoBtn.IsEnabled = _redoStack.Count > 0;

        // Визуально затемняем выключенные
        UndoBtn.Opacity = UndoBtn.IsEnabled ? 1.0 : 0.4;
        RedoBtn.Opacity = RedoBtn.IsEnabled ? 1.0 : 0.4;
    }

    // ══════════════════════════════════════════════════════════════
    //   ОЧИСТКА ИСТОРИИ (при смене карты, resize, new map)
    // ══════════════════════════════════════════════════════════════
    private void ClearHistory()
    {
        _undoStack.Clear();
        _redoStack.Clear();
        _currentBatch = null;
        UpdateUndoButtons();
    }
}