using System;
using System.Linq;
using Avalonia.Interactivity;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private void ShowInspector(BattleUnit unit)
    {
        _inspectorVisible = true;
        _inspectorPanel = unit.IsEnemy ? 1 : 0;
        _inspectorSlotIdx = unit.IsEnemy ? _enemies.IndexOf(unit) : _allies.IndexOf(unit);

        InspectorPanel.IsVisible = true;
        InspectorText.Text = $"ID: {unit.Id}   {unit.Name}";
    }

    private void OnInspectorPrevClick(object? sender, RoutedEventArgs e)
    {
        if (!_inspectorVisible) return;
        ChangeInspectorId(-1);
    }

    private void OnInspectorNextClick(object? sender, RoutedEventArgs e)
    {
        if (!_inspectorVisible) return;
        ChangeInspectorId(1);
    }

    private void ChangeInspectorId(int step)
    {
        var unit = _inspectorPanel == 0 ? _allies[_inspectorSlotIdx] : _enemies[_inspectorSlotIdx];
        var metaList = _inspectorPanel == 0 ? LoadActorsJson() : LoadEnemiesJson();
        if (metaList.Count == 0) return;

        int newId = unit.Id;
        int attempts = 0;
        do
        {
            newId = (newId + step) % metaList.Count;
            if (newId < 0) newId += metaList.Count;
            attempts++;

            if (attempts > metaList.Count) return; // Все проверено

            if (_inspectorPanel == 0)
            {
                // Для союзников — уникальность
                if (!_allies.Any(a => a.Used && a.Id == newId && a != unit)) break;
            }
            else break; // Врагам можно дублироваться

        } while (true);

        var meta = metaList[newId];
        unit.Id = newId;
        unit.Name = meta.Name;
        unit.Sprite = meta.Sprite;
        LoadUnitSprite(unit);

        InspectorText.Text = $"ID: {unit.Id}   {unit.Name}";
        RebuildOverlay();
    }
}