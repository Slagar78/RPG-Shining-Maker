// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.TileEditor.cs
using System;
using Avalonia;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — TILE EDITOR.
/// Включение режима трансформаций, выбор активной трансформации,
/// подсветка кнопок и рендер превью тайла с учётом rot/mirror.
/// </summary>
public partial class MapEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   ВКЛЮЧЕНИЕ / ВЫКЛЮЧЕНИЕ TILE EDITOR
    // ══════════════════════════════════════════════════════════════
    private void OnTileEditorToggle(object? sender, RoutedEventArgs e)
    {
        _tileEditorMode = TileEditorToggle.IsChecked == true;

        // ═══ Взаимоисключение с Select Mode ═══
        if (_tileEditorMode && _selectMode)
        {
            SelectButton.IsChecked = false;
            _selectMode = false;
            SelectButton.Background = new SolidColorBrush(Color.Parse("#3E3E42"));
            ClearSelectState();
        }

        // ═══ Взаимоисключение с Grid Mode ═══
        if (_tileEditorMode && _gridMode)
        {
            GridModeButton.IsChecked = false;
            _gridMode = false;

            GridModeButton.Background = new SolidColorBrush(Color.Parse("#3E3E42"));
            GridModeToggle.Content = "OFF";

            MapCanvasControl.ShowGridMode = false;
            MapCanvasControl.UpdateGridOverlay();
        }

        // Галочка / крестик
        TileEditorIcon.Text = _tileEditorMode ? "✓" : "✗";
        TileEditorIcon.Foreground = new SolidColorBrush(
            Color.Parse(_tileEditorMode ? "#4CAF50" : "#E74C3C"));

        // 4 кнопки трансформации — активны только в режиме
        RotateBtn.IsEnabled = _tileEditorMode;
        FlipHBtn.IsEnabled  = _tileEditorMode;
        FlipVBtn.IsEnabled  = _tileEditorMode;
        DeleteBtn.IsEnabled = _tileEditorMode;

        if (_tileEditorMode)
        {
            // Автовыбор первой трансформации — Rotate
            _transformMode = 1;
            UpdateTransformHighlight();
        }
        else
        {
            // Сброс превью при выключении
            _transformMode = 0;
            UpdateTransformHighlight();

            MapLeftPreview.Source  = null;
            MapRightPreview.Source = null;
            MapLeftLabel.Text  = "—";
            MapRightLabel.Text = "—";
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   ВЫБОР ТРАНСФОРМАЦИИ
    // ══════════════════════════════════════════════════════════════
    private void OnRotateClick(object? sender, RoutedEventArgs e)
    {
        _transformMode = 1;
        UpdateTransformHighlight();
    }

    private void OnFlipHClick(object? sender, RoutedEventArgs e)
    {
        _transformMode = 2;
        UpdateTransformHighlight();
    }

    private void OnFlipVClick(object? sender, RoutedEventArgs e)
    {
        _transformMode = 3;
        UpdateTransformHighlight();
    }

    private void OnDeleteClick(object? sender, RoutedEventArgs e)
    {
        _transformMode = 4;
        UpdateTransformHighlight();

        // Delete не использует превью — сбрасываем его
        MapLeftPreview.Source  = null;
        MapRightPreview.Source = null;
        MapLeftLabel.Text  = "—";
        MapRightLabel.Text = "—";
    }

    // ══════════════════════════════════════════════════════════════
    //   ПОДСВЕТКА АКТИВНОЙ ТРАНСФОРМАЦИИ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Красит жёлтым рамку у активной кнопки трансформации,
    /// у остальных — обычный цвет. Вызывается также из
    /// OnGridModeToggle и OnSelectToggle при взаимоисключении.
    /// </summary>
    private void UpdateTransformHighlight()
    {
        var yellow = new SolidColorBrush(Color.Parse("#FFD700"));
        var normal = new SolidColorBrush(Color.Parse("#3E3E42"));

        RotateBtn.BorderBrush = (_transformMode == 1) ? yellow : normal;
        FlipHBtn.BorderBrush  = (_transformMode == 2) ? yellow : normal;
        FlipVBtn.BorderBrush  = (_transformMode == 3) ? yellow : normal;
        DeleteBtn.BorderBrush = (_transformMode == 4) ? yellow : normal;
    }

    // ══════════════════════════════════════════════════════════════
    //   РЕНДЕР ПРЕВЬЮ ТАЙЛА С ТРАНСФОРМАЦИЕЙ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Собирает картинку тайла с учётом поворота и отражений.
    /// Используется для превью Left/Right в панели TOOLS.
    /// Возвращает null, если tileId вне диапазона.
    /// </summary>
    private Bitmap? RenderTileWithTransform(int tileId, int rot, bool mx, bool my)
    {
        if (tileId < 0 || tileId >= _tiles.Count) return null;

        var src = _tiles[tileId].Image;
        int ts  = TileSize;

        var rtb = new RenderTargetBitmap(new PixelSize(ts, ts), new Vector(96, 96));

        using (var ctx = rtb.CreateDrawingContext())
        {
            double cx = ts / 2.0;
            double cy = ts / 2.0;

            // Порядок: сместить в центр → отразить → повернуть → вернуть обратно
            var matrix = Matrix.Identity;
            matrix = matrix * Matrix.CreateTranslation(-cx, -cy);
            if (mx) matrix = matrix * Matrix.CreateScale(-1, 1);
            if (my) matrix = matrix * Matrix.CreateScale(1, -1);
            matrix = matrix * Matrix.CreateRotation(rot * Math.PI / 2);
            matrix = matrix * Matrix.CreateTranslation(cx, cy);

            using (ctx.PushTransform(matrix))
            {
                ctx.DrawImage(src, new Rect(0, 0, ts, ts));
            }
        }

        return rtb;
    }
}