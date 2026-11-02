using Avalonia.Interactivity;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private void OnZoneToggleClick(object? sender, RoutedEventArgs e)
    {
        _selectingZone = ZoneBtn.IsChecked == true;
    }
}