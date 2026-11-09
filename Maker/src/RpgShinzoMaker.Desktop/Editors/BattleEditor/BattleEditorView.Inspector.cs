using System;
using System.Linq;
using Avalonia.Controls;
using Avalonia.Interactivity;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private bool _suppressInspectorEvents = false;

    private void ShowInspector(BattleUnit unit)
    {
        _inspectorVisible = true;
        _inspectorPanel = unit.IsEnemy ? 1 : 0;
        _inspectorSlotIdx = unit.IsEnemy ? _enemies.IndexOf(unit) : _allies.IndexOf(unit);

        InspectorPanel.IsVisible = true;
        InspectorText.Text = $"ID: {unit.Id}   {unit.Name}";
        
        // ★ ВКЛЮЧАЕМ ПАНЕЛЬ УРОВНЯ ТОЛЬКО ДЛЯ ВРАГОВ ★
        InspectorLevelPanel.IsVisible = unit.IsEnemy;

        if (unit.IsEnemy)
        {
            _suppressInspectorEvents = true;
            InspectorLevelBox.Text = unit.Level.ToString();
            _suppressInspectorEvents = false;
        }
    }

    private void HideInspector()
    {
        _inspectorVisible = false;
        InspectorPanel.IsVisible = false;
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

            if (attempts > metaList.Count) return; 

            if (_inspectorPanel == 0)
            {
                if (!_allies.Any(a => a.Used && a.Id == newId && a != unit)) break;
            }
            else break; 

        } while (true);

        var meta = metaList[newId];
        unit.Id = newId;
        unit.Name = meta.Name;
        unit.Sprite = meta.Sprite;
        unit.Level = 1; 
        LoadUnitSprite(unit);

        ShowInspector(unit);
        RebuildOverlay();
    }

    private void OnInspectorIncLevelClick(object? sender, RoutedEventArgs e) => ChangeInspectorLevel(1);
    private void OnInspectorDecLevelClick(object? sender, RoutedEventArgs e) => ChangeInspectorLevel(-1);

    private void ChangeInspectorLevel(int delta)
    {
        if (!_inspectorVisible || _inspectorPanel == 0) return; // Защита: только для врагов
        var unit = _enemies[_inspectorSlotIdx];
        unit.Level = Math.Max(1, unit.Level + delta);
        ShowInspector(unit);
    }

    private void OnInspectorLevelChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressInspectorEvents || !_inspectorVisible || _inspectorPanel == 0) return; // Защита: только для врагов

        if (sender is TextBox tb)
        {
            string validText = System.Text.RegularExpressions.Regex.Replace(tb.Text ?? "", "[^0-9]", "");
            if (tb.Text != validText)
            {
                tb.Text = validText;
                tb.CaretIndex = validText.Length;
            }

            if (int.TryParse(tb.Text, out int val))
            {
                var unit = _enemies[_inspectorSlotIdx];
                unit.Level = Math.Max(1, val);
            }
        }
    }
}