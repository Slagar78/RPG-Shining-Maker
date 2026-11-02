using Avalonia.Controls;
using Avalonia.Interactivity;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private void OnTerrainBrushClick(object? sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string tagStr && int.TryParse(tagStr, out int val))
        {
            _currentCollisionType = val;
            BottomTerrainText.Text = val switch { -1 => "Terrain: Block", 0 => "Terrain: Fly only", _ => "Terrain: Walkable" };
        }
    }

    private void PaintTerrain(int tx, int ty)
    {
        if (_terrain == null || _currentMap == null) return;
        
        var entry = _battleEntries[_currentBattleIndex];
        int mapX = tx;
        int mapY = ty;

        // Если мы в режиме Battle (видна только зона), tx,ty приходят относительно всей карты
        // Но мы можем ограничить покраску только зоной
        if (_viewMode == 1 && entry.BattleWidth > 0)
        {
            if (tx < entry.BattleX || tx >= entry.BattleX + entry.BattleWidth ||
                ty < entry.BattleY || ty >= entry.BattleY + entry.BattleHeight) return;
        }

        if (mapX >= 0 && mapX < _currentMap.Width && mapY >= 0 && mapY < _currentMap.Height)
        {
            if (_terrain[mapY][mapX] != _currentCollisionType)
            {
                _terrain[mapY][mapX] = _currentCollisionType;
                RebuildOverlay(); // Перерисовываем маркеры
            }
        }
    }
}