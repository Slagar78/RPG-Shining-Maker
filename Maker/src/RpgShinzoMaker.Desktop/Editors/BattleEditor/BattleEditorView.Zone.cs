using System;
using Avalonia.Controls;
using Avalonia.Interactivity;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private bool _suppressZoneEvents;

    private void OnZoneToggleClick(object? sender, RoutedEventArgs e)
    {
        _selectingZone = ZoneBtn.IsChecked == true;
        if (_selectingZone)
        {
            MapCanvasControl.SetZoom(1.0);
            SetScrollTo(0, 0);
        }
        else
        {
            ApplyViewMode();
        }
    }

    // Ввод цифр с клавиатуры
    private void OnZoneTextChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressZoneEvents) return;
        
        if (sender is TextBox tb)
        {
            string validText = System.Text.RegularExpressions.Regex.Replace(tb.Text ?? "", "[^0-9]", "");
            if (tb.Text != validText)
            {
                tb.Text = validText;
                tb.CaretIndex = validText.Length;
            }
        }

        int x = int.TryParse(BattlePosX.Text, out var vx) ? vx : 0;
        int y = int.TryParse(BattlePosY.Text, out var vy) ? vy : 0;
        int w = int.TryParse(BattleSizeX.Text, out var vw) ? vw : 1;
        int h = int.TryParse(BattleSizeY.Text, out var vh) ? vh : 1;

        ApplyZoneChanges(x, y, w, h);
    }

    // Обработчики кликов для кнопок (RepeatButton сам будет их вызывать при зажатии)
    private void OnIncXClick(object? sender, RoutedEventArgs e) => ChangeX(1);
    private void OnDecXClick(object? sender, RoutedEventArgs e) => ChangeX(-1);
    private void OnIncYClick(object? sender, RoutedEventArgs e) => ChangeY(1);
    private void OnDecYClick(object? sender, RoutedEventArgs e) => ChangeY(-1);
    private void OnIncWClick(object? sender, RoutedEventArgs e) => ChangeW(1);
    private void OnDecWClick(object? sender, RoutedEventArgs e) => ChangeW(-1);
    private void OnIncHClick(object? sender, RoutedEventArgs e) => ChangeH(1);
    private void OnDecHClick(object? sender, RoutedEventArgs e) => ChangeH(-1);

    private void ChangeX(int delta)
    {
        if (_currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];
        ApplyZoneChanges(entry.BattleX + delta, entry.BattleY, entry.BattleWidth, entry.BattleHeight);
    }
    private void ChangeY(int delta)
    {
        if (_currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];
        ApplyZoneChanges(entry.BattleX, entry.BattleY + delta, entry.BattleWidth, entry.BattleHeight);
    }
    private void ChangeW(int delta)
    {
        if (_currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];
        ApplyZoneChanges(entry.BattleX, entry.BattleY, entry.BattleWidth + delta, entry.BattleHeight);
    }
    private void ChangeH(int delta)
    {
        if (_currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];
        ApplyZoneChanges(entry.BattleX, entry.BattleY, entry.BattleWidth, entry.BattleHeight + delta);
    }

    private void ApplyZoneChanges(int x, int y, int w, int h)
    {
        if (_currentBattleIndex < 0 || _currentMap == null) return;
        var entry = _battleEntries[_currentBattleIndex];

        x = Math.Clamp(x, 0, _currentMap.Width - 1);
        y = Math.Clamp(y, 0, _currentMap.Height - 1);
        w = Math.Clamp(w, 1, _currentMap.Width - x);
        h = Math.Clamp(h, 1, _currentMap.Height - y);

        if (entry.BattleX == x && entry.BattleY == y && entry.BattleWidth == w && entry.BattleHeight == h) return;

        entry.BattleX = x;
        entry.BattleY = y;
        entry.BattleWidth = w;
        entry.BattleHeight = h;

        UpdateToolbarInfo();
        RebuildOverlay();
    }
}