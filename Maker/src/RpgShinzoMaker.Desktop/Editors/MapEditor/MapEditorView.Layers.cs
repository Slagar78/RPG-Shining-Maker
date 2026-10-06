// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.Layers.cs
using Avalonia.Interactivity;
using Avalonia.Media;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — СЛОИ КАРТЫ.
/// Здесь: показать/скрыть L1 и L2, переключить активный слой.
/// </summary>
public partial class MapEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   ПОКАЗАТЬ / СКРЫТЬ L1
    // ══════════════════════════════════════════════════════════════
    private void OnLayer1ToggleClick(object? sender, RoutedEventArgs e)
    {
        _showLayer1 = Layer1Toggle.IsChecked == true;

        Layer1Toggle.Background = new SolidColorBrush(
            Color.Parse(_showLayer1 ? "#4CAF50" : "#555555"));

        UpdateCanvasLayers();
    }

    // ══════════════════════════════════════════════════════════════
    //   ПОКАЗАТЬ / СКРЫТЬ L2
    // ══════════════════════════════════════════════════════════════
    private void OnLayer2ToggleClick(object? sender, RoutedEventArgs e)
    {
        _showLayer2 = Layer2Toggle.IsChecked == true;

        Layer2Toggle.Background = new SolidColorBrush(
            Color.Parse(_showLayer2 ? "#4CAF50" : "#555555"));

        UpdateCanvasLayers();
    }

    // ══════════════════════════════════════════════════════════════
    //   АКТИВНЫЙ СЛОЙ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Переключает активный слой (0 = L1, 1 = L2) и обновляет
    /// кнопку Active Layer и оверлей Grid Mode.
    /// </summary>
    private void OnActiveLayerClick(object? sender, RoutedEventArgs e)
    {
        _currentLayer = 1 - _currentLayer;

        ActiveLayerButton.Content = (_currentLayer + 1).ToString();
        ActiveLayerButton.Background = new SolidColorBrush(
            Color.Parse(_currentLayer == 0 ? "#6497C8" : "#4169E1"));

        MapCanvasControl.CurrentLayer = _currentLayer;
        MapCanvasControl.UpdateGridOverlay();
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБНОВЛЕНИЕ ВИДИМОСТИ СЛОЁВ НА КАНВАСЕ
    // ══════════════════════════════════════════════════════════════
    private void UpdateCanvasLayers()
    {
        MapCanvasControl.ShowLayer1 = _showLayer1;
        MapCanvasControl.ShowLayer2 = _showLayer2;
        MapCanvasControl.Redraw();
    }
}