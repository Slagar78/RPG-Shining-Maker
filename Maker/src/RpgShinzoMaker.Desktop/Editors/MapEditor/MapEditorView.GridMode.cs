// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.GridMode.cs
using Avalonia.Interactivity;
using Avalonia.Media;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — GRID MODE.
/// Большая кнопка в панели TOOLS: назначение типов тайлам прямо на карте.
/// Взаимоисключается с Select и TileEditor.
/// При включении автоматически переводит блоксет в Mode B.
/// </summary>
public partial class MapEditorView
{
    private void OnGridModeToggle(object? sender, RoutedEventArgs e)
    {
        _gridMode = GridModeButton.IsChecked == true;

        GridModeButton.Background = new SolidColorBrush(
            Color.Parse(_gridMode ? "#C83232" : "#3E3E42"));

        // Синхронизируем маленький тумблер в палитре
        GridModeToggle.Content = _gridMode ? "ON" : "OFF";

        // ═══ Взаимоисключение с Select Mode ═══
        if (_gridMode && _selectMode)
        {
            SelectButton.IsChecked = false;
            _selectMode = false;
            SelectButton.Background = new SolidColorBrush(Color.Parse("#3E3E42"));
            ClearSelectState();
        }

        // ═══ Взаимоисключение с Tile Editor ═══
        if (_gridMode && _tileEditorMode)
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

        // ═══ Авто-переключение блоксета A/B ═══
        if (_gridMode)
        {
            // Включаем Grid Mode → переводим блоксет в Mode B
            ModeA.IsChecked = false;
            ModeB.IsChecked = true;
            SetModeB(true);
        }
        else
        {
            // Выключаем Grid Mode → возвращаем блоксет в Mode A
            ModeA.IsChecked = true;
            ModeB.IsChecked = false;
            SetModeB(false);
        }

        MapCanvasControl.ShowGridMode = _gridMode;
        MapCanvasControl.UpdateGridOverlay();
    }
}