// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.Fields.cs
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;   // ← для SolidColorBrush / Color
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;
using System.Linq;

namespace RpgShinzoMaker.Desktop.Editors.EventEditor;

/// <summary>
/// Часть EventEditorView — ПОЛЯ ВЫБРАННОГО СОБЫТИЯ.
/// Заполнение полей из модели + обработчики TextChanged.
/// </summary>
public partial class EventEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   ЗАГРУЗКА ВЫБРАННОГО СОБЫТИЯ В ПОЛЯ
    // ══════════════════════════════════════════════════════════════
    private void LoadSelectedIntoFields()
    {
        _suppressFieldEvents = true;

        try
        {
            switch (_currentSection)
            {
                case "roof":        LoadRoof();        break;
                case "tile_change": LoadTileChange();  break;
                case "stair":       LoadStair();       break;
                case "warp":        LoadWarp();        break;
                case "npc":         LoadNpc();         break;
            }
        }
        finally
        {
            _suppressFieldEvents = false;
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБЩИЙ ФИЛЬТР ВВОДА — только цифры, макс 3 символа
    // ══════════════════════════════════════════════════════════════
    private void OnCoordTextInput(object? sender, TextInputEventArgs e)
    {
        if (string.IsNullOrEmpty(e.Text)) return;

        // Только цифры
        foreach (char c in e.Text)
        {
            if (!char.IsDigit(c))
            {
                e.Handled = true;
                return;
            }
        }

        // Ограничение длины по MaxLength TextBox
        if (sender is TextBox tb)
        {
            int maxLen = tb.MaxLength > 0 ? tb.MaxLength : 3;
            int currentLen = tb.Text?.Length ?? 0;
            int selLen = Math.Abs(tb.SelectionEnd - tb.SelectionStart);
            int effectiveLen = currentLen - selLen;

            if (effectiveLen + e.Text.Length > maxLen)
                e.Handled = true;
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   ROOF — загрузка в поля
    // ══════════════════════════════════════════════════════════════
    private void LoadRoof()
{
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count)
    {
        ClearRoofFields();
        return;
    }

    var r = _events.Roofs[_selectedIndex];

    SetPair(Roof_StartX, Roof_StartY, r.StartX, r.StartY, allowEmpty: false);
    SetPair(Roof_EndX,   Roof_EndY,   r.EndX,   r.EndY,   allowEmpty: false);

    // Сбросить выделения списков
    _selectedRoofTriggerIndex = r.Triggers.Count > 0 ? 0 : -1;
    _selectedRoofExitIndex    = r.Exits.Count    > 0 ? 0 : -1;

    RefreshRoofTriggerList();
    RefreshRoofExitList();
    UpdateRoofTriggerFields();
    UpdateRoofExitFields();
}

/// <summary>Пересобирает список триггеров, сохраняя выделение.</summary>
private void RefreshRoofTriggerList()
{
    if (Roof_TriggersList == null) return;
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

    var r = _events.Roofs[_selectedIndex];
    var labels = new List<string>();
    for (int i = 0; i < r.Triggers.Count; i++)
    {
        var t = r.Triggers[i];
        labels.Add($"#{i}   ({t.X},{t.Y})");
    }

    Roof_TriggersList.SelectionChanged -= OnRoofTriggerSelected;
    Roof_TriggersList.ItemsSource = labels;
    Roof_TriggersList.SelectedIndex = _selectedRoofTriggerIndex;
    Roof_TriggersList.SelectionChanged += OnRoofTriggerSelected;

    if (Roof_TriggersCount != null)
        Roof_TriggersCount.Text = $"{r.Triggers.Count} / 10";
}

/// <summary>Пересобирает список выходов, сохраняя выделение.</summary>
private void RefreshRoofExitList()
{
    if (Roof_ExitsList == null) return;
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

    var r = _events.Roofs[_selectedIndex];
    var labels = new List<string>();
    for (int i = 0; i < r.Exits.Count; i++)
    {
        var e = r.Exits[i];
        labels.Add($"#{i}   ({e.X},{e.Y})");
    }

    Roof_ExitsList.SelectionChanged -= OnRoofExitSelected;
    Roof_ExitsList.ItemsSource = labels;
    Roof_ExitsList.SelectedIndex = _selectedRoofExitIndex;
    Roof_ExitsList.SelectionChanged += OnRoofExitSelected;

    if (Roof_ExitsCount != null)
        Roof_ExitsCount.Text = $"{r.Exits.Count} / 10";
}

/// <summary>Заполняет X/Y триггера для выбранного элемента.</summary>
private void UpdateRoofTriggerFields()
{
    if (Roof_TriggerX == null || Roof_TriggerY == null) return;

    bool has = _selectedIndex >= 0 && _selectedIndex < _events.Roofs.Count
                                   && _selectedRoofTriggerIndex >= 0
                                   && _selectedRoofTriggerIndex < _events.Roofs[_selectedIndex].Triggers.Count;

    Roof_TriggerX.IsEnabled = has;
    Roof_TriggerY.IsEnabled = has;

    // ─── Save/Restore: если снаружи уже подавлено — оставляем подавленным ───
    bool saved = _suppressFieldEvents;
    _suppressFieldEvents = true;
    try
    {
        if (!has)
        {
            Roof_TriggerX.Text = "";
            Roof_TriggerY.Text = "";
        }
        else
        {
            var t = _events.Roofs[_selectedIndex].Triggers[_selectedRoofTriggerIndex];
            Roof_TriggerX.Text = t.X.ToString();
            Roof_TriggerY.Text = t.Y.ToString();
        }
    }
    finally
    {
        _suppressFieldEvents = saved;
    }
}

/// <summary>Заполняет X/Y выхода для выбранного элемента.</summary>
private void UpdateRoofExitFields()
{
    if (Roof_ExitX == null || Roof_ExitY == null) return;

    bool has = _selectedIndex >= 0 && _selectedIndex < _events.Roofs.Count
                                   && _selectedRoofExitIndex >= 0
                                   && _selectedRoofExitIndex < _events.Roofs[_selectedIndex].Exits.Count;

    Roof_ExitX.IsEnabled = has;
    Roof_ExitY.IsEnabled = has;

    // ─── Save/Restore: если снаружи уже подавлено — оставляем подавленным ───
    bool saved = _suppressFieldEvents;
    _suppressFieldEvents = true;
    try
    {
        if (!has)
        {
            Roof_ExitX.Text = "";
            Roof_ExitY.Text = "";
        }
        else
        {
            var e = _events.Roofs[_selectedIndex].Exits[_selectedRoofExitIndex];
            Roof_ExitX.Text = e.X.ToString();
            Roof_ExitY.Text = e.Y.ToString();
        }
    }
    finally
    {
        _suppressFieldEvents = saved;
    }
}

private void ClearRoofFields()
{
    _selectedRoofTriggerIndex = -1;
    _selectedRoofExitIndex = -1;

    Roof_StartX.Text = ""; Roof_StartY.Text = "";
    Roof_EndX.Text   = ""; Roof_EndY.Text   = "";

    if (Roof_TriggersList != null) Roof_TriggersList.ItemsSource = null;
    if (Roof_ExitsList    != null) Roof_ExitsList.ItemsSource = null;
    if (Roof_TriggersCount != null) Roof_TriggersCount.Text = "0 / 10";
    if (Roof_ExitsCount    != null) Roof_ExitsCount.Text = "0 / 10";

    if (Roof_TriggerX != null) { Roof_TriggerX.Text = ""; Roof_TriggerX.IsEnabled = false; }
    if (Roof_TriggerY != null) { Roof_TriggerY.Text = ""; Roof_TriggerY.IsEnabled = false; }
    if (Roof_ExitX    != null) { Roof_ExitX.Text    = ""; Roof_ExitX.IsEnabled    = false; }
    if (Roof_ExitY    != null) { Roof_ExitY.Text    = ""; Roof_ExitY.IsEnabled    = false; }
}

    private static void SetPair(TextBox xBox, TextBox yBox, int x, int y, bool allowEmpty)
    {
        if (allowEmpty && (x < 0 || y < 0))
        {
            xBox.Text = "";
            yBox.Text = "";
        }
        else
        {
            xBox.Text = x >= 0 ? x.ToString() : "";
            yBox.Text = y >= 0 ? y.ToString() : "";
        }
    }
    
    // ─── ROOF — обработчики Start ───
    private void OnRoofStartXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        _events.Roofs[_selectedIndex].StartX = ParseCoord(Roof_StartX.Text, fallback: 0);
        OnFieldChanged();
    }

    private void OnRoofStartYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        _events.Roofs[_selectedIndex].StartY = ParseCoord(Roof_StartY.Text, fallback: 0);
        OnFieldChanged();
    }

    // ─── ROOF — обработчики End ───
    private void OnRoofEndXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        _events.Roofs[_selectedIndex].EndX = ParseCoord(Roof_EndX.Text, fallback: 1);
        OnFieldChanged();
    }

    private void OnRoofEndYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        _events.Roofs[_selectedIndex].EndY = ParseCoord(Roof_EndY.Text, fallback: 1);
        OnFieldChanged();
    }
    // ══════════════════════════════════════════════════════════════
//   ROOF — выбранный триггер / выход
// ══════════════════════════════════════════════════════════════
private void OnRoofTriggerSelected(object? sender, SelectionChangedEventArgs e)
{
    if (_suppressFieldEvents) return;
    _selectedRoofTriggerIndex = Roof_TriggersList.SelectedIndex;
    UpdateRoofTriggerFields();
}

private void OnRoofExitSelected(object? sender, SelectionChangedEventArgs e)
{
    if (_suppressFieldEvents) return;
    _selectedRoofExitIndex = Roof_ExitsList.SelectedIndex;
    UpdateRoofExitFields();
}

// ══════════════════════════════════════════════════════════════
//   ROOF — Add / Delete триггеров и выходов
// ══════════════════════════════════════════════════════════════
private void OnRoofTriggerAdd(object? sender, RoutedEventArgs e)
{
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
    var r = _events.Roofs[_selectedIndex];
    if (r.Triggers.Count >= 10) return;

    r.Triggers.Add(new TriggerPoint(0, 0));
    _selectedRoofTriggerIndex = r.Triggers.Count - 1;
    RefreshRoofTriggerList();
    UpdateRoofTriggerFields();
    OnFieldChanged();
}

private void OnRoofTriggerDelete(object? sender, RoutedEventArgs e)
{
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
    var r = _events.Roofs[_selectedIndex];
    if (_selectedRoofTriggerIndex < 0 || _selectedRoofTriggerIndex >= r.Triggers.Count) return;

    r.Triggers.RemoveAt(_selectedRoofTriggerIndex);
    if (_selectedRoofTriggerIndex >= r.Triggers.Count)
        _selectedRoofTriggerIndex = r.Triggers.Count - 1;

    RefreshRoofTriggerList();
    UpdateRoofTriggerFields();
    OnFieldChanged();
}

private void OnRoofExitAdd(object? sender, RoutedEventArgs e)
{
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
    var r = _events.Roofs[_selectedIndex];
    if (r.Exits.Count >= 10) return;

    r.Exits.Add(new TriggerPoint(0, 0));
    _selectedRoofExitIndex = r.Exits.Count - 1;
    RefreshRoofExitList();
    UpdateRoofExitFields();
    OnFieldChanged();
}

private void OnRoofExitDelete(object? sender, RoutedEventArgs e)
{
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
    var r = _events.Roofs[_selectedIndex];
    if (_selectedRoofExitIndex < 0 || _selectedRoofExitIndex >= r.Exits.Count) return;

    r.Exits.RemoveAt(_selectedRoofExitIndex);
    if (_selectedRoofExitIndex >= r.Exits.Count)
        _selectedRoofExitIndex = r.Exits.Count - 1;

    RefreshRoofExitList();
    UpdateRoofExitFields();
    OnFieldChanged();
}

// ══════════════════════════════════════════════════════════════
//   ROOF — изменение X/Y выбранного триггера / выхода
// ══════════════════════════════════════════════════════════════
private void OnRoofTriggerXChanged(object? sender, TextChangedEventArgs e)
{
    if (_suppressFieldEvents) return;
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
    if (_selectedRoofTriggerIndex < 0) return;

    var r = _events.Roofs[_selectedIndex];
    if (_selectedRoofTriggerIndex >= r.Triggers.Count) return;

    r.Triggers[_selectedRoofTriggerIndex].X = ParseCoord(Roof_TriggerX.Text, fallback: 0);
    RefreshRoofTriggerList();
    OnFieldChanged();
}

private void OnRoofTriggerYChanged(object? sender, TextChangedEventArgs e)
{
    if (_suppressFieldEvents) return;
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
    if (_selectedRoofTriggerIndex < 0) return;

    var r = _events.Roofs[_selectedIndex];
    if (_selectedRoofTriggerIndex >= r.Triggers.Count) return;

    r.Triggers[_selectedRoofTriggerIndex].Y = ParseCoord(Roof_TriggerY.Text, fallback: 0);
    RefreshRoofTriggerList();
    OnFieldChanged();
}

private void OnRoofExitXChanged(object? sender, TextChangedEventArgs e)
{
    if (_suppressFieldEvents) return;
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
    if (_selectedRoofExitIndex < 0) return;

    var r = _events.Roofs[_selectedIndex];
    if (_selectedRoofExitIndex >= r.Exits.Count) return;

    r.Exits[_selectedRoofExitIndex].X = ParseCoord(Roof_ExitX.Text, fallback: 0);
    RefreshRoofExitList();
    OnFieldChanged();
}

private void OnRoofExitYChanged(object? sender, TextChangedEventArgs e)
{
    if (_suppressFieldEvents) return;
    if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
    if (_selectedRoofExitIndex < 0) return;

    var r = _events.Roofs[_selectedIndex];
    if (_selectedRoofExitIndex >= r.Exits.Count) return;

    r.Exits[_selectedRoofExitIndex].Y = ParseCoord(Roof_ExitY.Text, fallback: 0);
    RefreshRoofExitList();
    OnFieldChanged();
}

    // ══════════════════════════════════════════════════════════════
    //   TILE CHANGE — загрузка в поля
    // ══════════════════════════════════════════════════════════════
    private void LoadTileChange()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count)
        {
            TC_TriggerX.Text = "";
            TC_TriggerY.Text = "";
            TC_NewTile.Text  = "0";
            TC_CloseX.Text   = "";
            TC_CloseY.Text   = "";
            return;
        }

        var tc = _events.TileChanges[_selectedIndex];

        // Trigger
        if (tc.TriggerX >= 0 && tc.TriggerY >= 0)
        {
            TC_TriggerX.Text = tc.TriggerX.ToString();
            TC_TriggerY.Text = tc.TriggerY.ToString();
        }
        else
        {
            TC_TriggerX.Text = "";
            TC_TriggerY.Text = "";
        }

        TC_NewTile.Text = tc.NewTileId.ToString();

        // Close
        if (tc.CloseX >= 0 && tc.CloseY >= 0)
        {
            TC_CloseX.Text = tc.CloseX.ToString();
            TC_CloseY.Text = tc.CloseY.ToString();
        }
        else
        {
            TC_CloseX.Text = "";
            TC_CloseY.Text = "";
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   STAIR — загрузка в поля
    // ══════════════════════════════════════════════════════════════
    private void LoadStair()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count)
        {
            St_StartX.Text = "0";
            St_StartY.Text = "0";
            St_EndX.Text   = "1";
            St_EndY.Text   = "1";
            SetStairDirectionUI(0);
            return;
        }

        var st = _events.Stairs[_selectedIndex];
        St_StartX.Text = st.StartX.ToString();
        St_StartY.Text = st.StartY.ToString();
        St_EndX.Text   = st.EndX.ToString();
        St_EndY.Text   = st.EndY.ToString();
        SetStairDirectionUI(st.Direction);
    }

    // ══════════════════════════════════════════════════════════════
    //   WARP — загрузка в поля
    // ══════════════════════════════════════════════════════════════
    private void LoadWarp()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count)
        {
            W_TriggerX.Text = "";
            W_TriggerY.Text = "";
            PopulateTargetMapCombo(null);
            W_TargetX.Text  = "0";
            W_TargetY.Text  = "0";
            SetWarpFacingUI(0);
            return;
        }

        var w = _events.Warps[_selectedIndex];

        // Trigger (может быть пустой = нет триггера)
        if (w.TriggerX >= 0 && w.TriggerY >= 0)
        {
            W_TriggerX.Text = w.TriggerX.ToString();
            W_TriggerY.Text = w.TriggerY.ToString();
        }
        else
        {
            W_TriggerX.Text = "";
            W_TriggerY.Text = "";
        }

        PopulateTargetMapCombo(w.TargetMap);
        W_TargetX.Text = w.TargetX.ToString();
        W_TargetY.Text = w.TargetY.ToString();
        SetWarpFacingUI(w.Facing);
    }

    /// <summary>
    /// Заполняет ComboBox Target Map названиями карт из entries.json.
    /// Выбирает нужную (или -1, если current пустой / не найден).
    /// </summary>
    private void PopulateTargetMapCombo(string? current)
    {
        if (W_TargetMapCombo == null) return;

        // Временно снимаем подписку, чтобы не было рекурсии при заполнении
        W_TargetMapCombo.SelectionChanged -= OnWTargetMapChanged;

        var names = new List<string>();
        foreach (var e in _mapEntries) names.Add(e.Name);

        W_TargetMapCombo.ItemsSource = names;

        int idx = -1;
        if (!string.IsNullOrEmpty(current))
            idx = names.IndexOf(current);

        W_TargetMapCombo.SelectedIndex = idx;

        W_TargetMapCombo.SelectionChanged += OnWTargetMapChanged;
    }
    
    // ══════════════════════════════════════════════════════════════
    //   ХЕЛПЕРЫ
    // ══════════════════════════════════════════════════════════════
    private static int? ParseInt(string s)
    {
        return int.TryParse(s?.Trim(), out int v) ? v : (int?)null;
    }

    private static (int x, int y)? ParsePair(string s)
    {
        if (string.IsNullOrEmpty(s)) return null;
        var parts = s.Split(',');
        if (parts.Length != 2) return null;

        if (!int.TryParse(parts[0].Trim(), out int x)) return null;
        if (!int.TryParse(parts[1].Trim(), out int y)) return null;
        return (x, y);
    }

    private static bool IsDash(string s) =>
        string.IsNullOrEmpty(s) || s.Trim() == "-";

    /// <summary>Парсит текст как целое. Пустая строка = fallback.</summary>
    private static int ParseCoord(string? text, int fallback)
    {
        if (string.IsNullOrWhiteSpace(text)) return fallback;
        return int.TryParse(text.Trim(), out int v) ? v : fallback;
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ TILE CHANGE
    // ══════════════════════════════════════════════════════════════
        private void OnTCTriggerXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count) return;

        var tc = _events.TileChanges[_selectedIndex];
        int oldX = tc.TriggerX;
        tc.TriggerX = ParseCoord(TC_TriggerX.Text, fallback: -1);

        if (tc.TriggerX < 0) tc.TriggerY = -1;
        else if (oldX < 0 && tc.TriggerY < 0) tc.TriggerY = 0;

        OnFieldChanged();
    }

    private void OnTCTriggerYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count) return;

        var tc = _events.TileChanges[_selectedIndex];
        tc.TriggerY = ParseCoord(TC_TriggerY.Text, fallback: -1);

        if (tc.TriggerY < 0) tc.TriggerX = -1;
        else if (tc.TriggerX < 0) tc.TriggerX = 0;

        OnFieldChanged();
    }

    private void OnTCNewTileChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count) return;

        var v = ParseInt(TC_NewTile.Text ?? "");
        if (v.HasValue) _events.TileChanges[_selectedIndex].NewTileId = v.Value;
    }

    private void OnTCCloseXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count) return;

        var tc = _events.TileChanges[_selectedIndex];
        int oldX = tc.CloseX;
        tc.CloseX = ParseCoord(TC_CloseX.Text, fallback: -1);

        if (tc.CloseX < 0) tc.CloseY = -1;
        else if (oldX < 0 && tc.CloseY < 0) tc.CloseY = 0;

        OnFieldChanged();
    }

    private void OnTCCloseYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count) return;

        var tc = _events.TileChanges[_selectedIndex];
        tc.CloseY = ParseCoord(TC_CloseY.Text, fallback: -1);

        if (tc.CloseY < 0) tc.CloseX = -1;
        else if (tc.CloseX < 0) tc.CloseX = 0;

        OnFieldChanged();
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ STAIR — координаты
    // ══════════════════════════════════════════════════════════════
    private void OnStStartXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count) return;

        _events.Stairs[_selectedIndex].StartX = ParseCoord(St_StartX.Text, fallback: 0);
        OnFieldChanged();
    }

    private void OnStStartYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count) return;

        _events.Stairs[_selectedIndex].StartY = ParseCoord(St_StartY.Text, fallback: 0);
        OnFieldChanged();
    }

    private void OnStEndXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count) return;

        _events.Stairs[_selectedIndex].EndX = ParseCoord(St_EndX.Text, fallback: 1);
        OnFieldChanged();
    }

    private void OnStEndYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count) return;

        _events.Stairs[_selectedIndex].EndY = ParseCoord(St_EndY.Text, fallback: 1);
        OnFieldChanged();
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ STAIR — направление (0 = влево, 1 = вправо)
    // ══════════════════════════════════════════════════════════════
    private void OnStDirLeftClick(object? sender, RoutedEventArgs e)  => SetStairDirection(0);
    private void OnStDirRightClick(object? sender, RoutedEventArgs e) => SetStairDirection(1);

    private void SetStairDirection(int direction)
    {
        // Сначала обновляем вид — даже если события подавлены
        SetStairDirectionUI(direction);

        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count) return;

        _events.Stairs[_selectedIndex].Direction = direction;
        OnFieldChanged();
    }

    /// <summary>Обновляет подсветку кнопок ← → и подпись «влево» / «вправо».</summary>
    private void SetStairDirectionUI(int direction)
    {
        if (St_DirLeftBtn  == null) return;
        if (St_DirRightBtn == null) return;
        if (St_DirLabel    == null) return;

        bool isLeft = direction == 0;

        St_DirLeftBtn.IsChecked  = isLeft;
        St_DirRightBtn.IsChecked = !isLeft;

        St_DirLeftBtn.Background = new SolidColorBrush(
            Color.Parse(isLeft ? "#3E5F8A" : "#3E3E42"));
        St_DirRightBtn.Background = new SolidColorBrush(
            Color.Parse(!isLeft ? "#3E5F8A" : "#3E3E42"));

        St_DirLeftBtn.Foreground = new SolidColorBrush(
            Color.Parse(isLeft ? "#FFFFFF" : "#CCCCCC"));
        St_DirRightBtn.Foreground = new SolidColorBrush(
            Color.Parse(!isLeft ? "#FFFFFF" : "#CCCCCC"));

        St_DirLabel.Text = isLeft ? "left" : "right";
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ WARP — Trigger X/Y
    // ══════════════════════════════════════════════════════════════
    private void OnWTriggerXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        var w = _events.Warps[_selectedIndex];
        int oldX = w.TriggerX;
        w.TriggerX = ParseCoord(W_TriggerX.Text, fallback: -1);

        if (w.TriggerX < 0) w.TriggerY = -1;
        else if (oldX < 0 && w.TriggerY < 0) w.TriggerY = 0;

        OnFieldChanged();
    }

    private void OnWTriggerYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        var w = _events.Warps[_selectedIndex];
        w.TriggerY = ParseCoord(W_TriggerY.Text, fallback: -1);

        if (w.TriggerY < 0) w.TriggerX = -1;
        else if (w.TriggerX < 0) w.TriggerX = 0;

        OnFieldChanged();
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ WARP — Target Map (ComboBox)
    // ══════════════════════════════════════════════════════════════
    private void OnWTargetMapChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        string name = (W_TargetMapCombo?.SelectedItem as string) ?? "";
        _events.Warps[_selectedIndex].TargetMap = name;
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ WARP — Target X/Y
    // ══════════════════════════════════════════════════════════════
    private void OnWTargetXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        _events.Warps[_selectedIndex].TargetX = ParseCoord(W_TargetX.Text, fallback: 0);
    }

    private void OnWTargetYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        _events.Warps[_selectedIndex].TargetY = ParseCoord(W_TargetY.Text, fallback: 0);
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ WARP — Facing (0=down, 1=left, 2=right, 3=up)
    // ══════════════════════════════════════════════════════════════
    private void OnWFacingDownClick(object? sender, RoutedEventArgs e)  => SetWarpFacing(0);
    private void OnWFacingLeftClick(object? sender, RoutedEventArgs e)  => SetWarpFacing(1);
    private void OnWFacingRightClick(object? sender, RoutedEventArgs e) => SetWarpFacing(2);
    private void OnWFacingUpClick(object? sender, RoutedEventArgs e)    => SetWarpFacing(3);

    private void SetWarpFacing(int facing)
    {
        SetWarpFacingUI(facing);

        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        _events.Warps[_selectedIndex].Facing = facing;
        OnFieldChanged();
    }

    private void SetWarpFacingUI(int facing)
    {
        if (W_FacingDownBtn  == null) return;
        if (W_FacingLeftBtn  == null) return;
        if (W_FacingRightBtn == null) return;
        if (W_FacingUpBtn    == null) return;
        if (W_FacingLabel    == null) return;

        var activeBg = new SolidColorBrush(Color.Parse("#3E5F8A"));
        var idleBg   = new SolidColorBrush(Color.Parse("#3E3E42"));
        var activeFg = new SolidColorBrush(Color.Parse("#FFFFFF"));
        var idleFg   = new SolidColorBrush(Color.Parse("#CCCCCC"));

        // Сброс всех
        W_FacingDownBtn.IsChecked  = false;  W_FacingDownBtn.Background  = idleBg;   W_FacingDownBtn.Foreground  = idleFg;
        W_FacingLeftBtn.IsChecked  = false;  W_FacingLeftBtn.Background  = idleBg;   W_FacingLeftBtn.Foreground  = idleFg;
        W_FacingRightBtn.IsChecked = false;  W_FacingRightBtn.Background = idleBg;   W_FacingRightBtn.Foreground = idleFg;
        W_FacingUpBtn.IsChecked    = false;  W_FacingUpBtn.Background    = idleBg;   W_FacingUpBtn.Foreground    = idleFg;

        string label;
        ToggleButton? active;
        switch (facing)
        {
            case 1:  active = W_FacingLeftBtn;  label = "left";  break;
            case 2:  active = W_FacingRightBtn; label = "right"; break;
            case 3:  active = W_FacingUpBtn;    label = "up";    break;
            default: active = W_FacingDownBtn;  label = "down";  break;
        }

        active.IsChecked  = true;
        active.Background = activeBg;
        active.Foreground = activeFg;

        W_FacingLabel.Text = label;
    }

    // ══════════════════════════════════════════════════════════════
    //   NPC — загрузка в поля
    // ══════════════════════════════════════════════════════════════
    private void LoadNpc()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count)
        {
            Npc_Id.Text = ""; Npc_X.Text = "0"; Npc_Y.Text = "0";
            Npc_Sprite.Text = ""; Npc_Behavior.SelectedIndex = 0; Npc_Direction.SelectedIndex = 0;
            Npc_HomeX.Text = "0"; Npc_HomeY.Text = "0"; Npc_Radius.Text = "3";
            Npc_SpritePreview.Source = null;
            
            Npc_ScriptList.ItemsSource = null;
            _selectedScriptIndex = -1;
            return;
        }

        var n = _events.Npcs[_selectedIndex];
        Npc_Id.Text = n.Id; Npc_X.Text = n.X.ToString(); Npc_Y.Text = n.Y.ToString();
        Npc_Sprite.Text = n.Sprite;
        Npc_Behavior.SelectedIndex = n.Behavior == "wander" ? 1 : 0;
        Npc_Direction.SelectedIndex = n.Direction switch
        {
            "down" => 0, "left" => 1, "right" => 2, "up" => 3, _ => 0
        };
        Npc_HomeX.Text = n.HomeX.ToString(); Npc_HomeY.Text = n.HomeY.ToString();
        Npc_Radius.Text = n.Radius.ToString();
        Npc_WanderPanel.IsEnabled = (n.Behavior == "wander");
        UpdateNpcSpritePreview(n.Sprite);

        RefreshScriptList();
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ NPC
    // ══════════════════════════════════════════════════════════════
    private void OnNpcIdChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        _events.Npcs[_selectedIndex].Id = (Npc_Id.Text ?? "").Trim();
    }

    private void OnNpcXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        // ─── ОЧИСТКА ОТ БУКВ (для вставки) ───
        if (sender is TextBox tb)
        {
            string validText = System.Text.RegularExpressions.Regex.Replace(tb.Text ?? "", "[^0-9]", "");
            if (tb.Text != validText)
            {
                tb.Text = validText;
                tb.CaretIndex = validText.Length; // Ставим курсор в конец
            }
        }

        var n = _events.Npcs[_selectedIndex];
        n.X = ParseCoord(Npc_X.Text, fallback: 0);
        n.Y = ParseCoord(Npc_Y.Text, fallback: 0);
        OnFieldChanged();
    }

    private void OnNpcHomeXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        // ─── ОЧИСТКА ОТ БУКВ (для вставки) ───
        if (sender is TextBox tb)
        {
            string validText = System.Text.RegularExpressions.Regex.Replace(tb.Text ?? "", "[^0-9]", "");
            if (tb.Text != validText)
            {
                tb.Text = validText;
                tb.CaretIndex = validText.Length;
            }
        }

        var n = _events.Npcs[_selectedIndex];
        n.HomeX = ParseCoord(Npc_HomeX.Text, fallback: 0);
        n.HomeY = ParseCoord(Npc_HomeY.Text, fallback: 0);
        OnFieldChanged();
    }

    private void OnNpcSpriteChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        var name = (Npc_Sprite.Text ?? "").Trim();
        _events.Npcs[_selectedIndex].Sprite = name;
        UpdateNpcSpritePreview(name);
        OnFieldChanged();
    }

    private void OnNpcBehaviorChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        var item = Npc_Behavior.SelectedItem as ComboBoxItem;
        string value = item?.Content?.ToString() ?? "static";
        
        var npc = _events.Npcs[_selectedIndex];
        npc.Behavior = value;
        
        // Включаем/выключаем поля Home и Radius
        Npc_WanderPanel.IsEnabled = (value == "wander");

        // ★ ХИТРОСТЬ: Если включили wander, а Home 0,0 — ставим координаты NPC ★
        if (value == "wander" && npc.HomeX == 0 && npc.HomeY == 0)
        {
            npc.HomeX = npc.X;
            npc.HomeY = npc.Y;
            Npc_HomeX.Text = npc.X.ToString();
            Npc_HomeY.Text = npc.Y.ToString();
        }
    }

    private void OnNpcDirectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        var item = Npc_Direction.SelectedItem as ComboBoxItem;
        string value = item?.Content?.ToString() ?? "down";
        _events.Npcs[_selectedIndex].Direction = value;
        OnFieldChanged();
    }
    
    private void OnNpcRadiusChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        // ─── ОЧИСТКА ОТ БУКВ (для вставки) ───
        if (sender is TextBox tb)
        {
            string validText = System.Text.RegularExpressions.Regex.Replace(tb.Text ?? "", "[^0-9]", "");
            if (tb.Text != validText)
            {
                tb.Text = validText;
                tb.CaretIndex = validText.Length;
            }
        }

        var v = ParseInt(Npc_Radius.Text ?? "");
        if (v.HasValue) _events.Npcs[_selectedIndex].Radius = v.Value;
    }
    private void RefreshScriptList()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;
        var n = _events.Npcs[_selectedIndex];

        Npc_ScriptList.SelectionChanged -= OnNpcScriptSelected;
        var labels = n.Script.Select((c, i) => $"{i+1}. {c.Type} ({(c.Type == "say" ? c.TextId : c.Type == "open_shop" ? c.ShopId?.ToString() : "-")})").ToList();
        Npc_ScriptList.ItemsSource = labels;
        Npc_ScriptList.SelectedIndex = _selectedScriptIndex;
        Npc_ScriptList.SelectionChanged += OnNpcScriptSelected;
        
        LoadScriptStepIntoFields();
    }

       private void LoadScriptStepIntoFields()
    {
        _suppressFieldEvents = true;
        try
        {
            if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count || _selectedScriptIndex < 0 || _selectedScriptIndex >= _events.Npcs[_selectedIndex].Script.Count)
            {
                Npc_ScriptType.SelectedIndex = -1;
                Npc_ScriptParam.Text = "";
                Npc_ScriptParam.IsEnabled = false;
                Npc_ScriptTextCombo.IsVisible = false;
                return;
            }

            var cmd = _events.Npcs[_selectedIndex].Script[_selectedScriptIndex];
            
            // Выбираем тип в ComboBox
            for (int i = 0; i < Npc_ScriptType.Items.Count; i++)
            {
                if (Npc_ScriptType.Items[i] is ComboBoxItem item && item.Content?.ToString() == cmd.Type)
                {
                    Npc_ScriptType.SelectedIndex = i;
                    break;
                }
            }

            // Настраиваем поле параметра
            UpdateScriptParamUI(cmd.Type);
            
            if (cmd.Type == "say")
            {
                var items = _localTexts.Select(kvp => $"{kvp.Key}: {(kvp.Value.Length > 25 ? kvp.Value.Substring(0, 25) + "..." : kvp.Value)}").ToList();
                Npc_ScriptTextCombo.ItemsSource = items;
                
                // Сначала сбрасываем выделение и ставим чистый текст (чтобы запятые работали)
                Npc_ScriptTextCombo.SelectedIndex = -1;
                Npc_ScriptTextCombo.Text = cmd.TextId ?? "";
                
                // ★ Если ID один (без запятой) — выделяем его в выпадающем списке
                if (!string.IsNullOrEmpty(cmd.TextId) && !cmd.TextId.Contains(","))
                {
                    string firstId = cmd.TextId.Trim();
                    for (int i = 0; i < items.Count; i++)
                    {
                        if (items[i].StartsWith(firstId + ":"))
                        {
                            Npc_ScriptTextCombo.SelectedIndex = i;
                            break;
                        }
                    }
                }
            }
            else if (cmd.Type == "open_shop") Npc_ScriptParam.Text = cmd.ShopId?.ToString() ?? "0";
            else if (cmd.Type == "start_battle" || cmd.Type == "unlock_battle") Npc_ScriptParam.Text = cmd.BattleId?.ToString() ?? "0";
            else Npc_ScriptParam.Text = "";
        }
        finally { _suppressFieldEvents = false; }
    }
    
    private void UpdateScriptParamUI(string type)
    {
        if (type == "say") 
        { 
            Npc_ScriptParamLabel.Text = "Text ID:"; 
            Npc_ScriptParam.IsVisible = false; 
            Npc_ScriptTextCombo.IsVisible = true; 
        }
        else if (type == "open_shop") 
        { 
            Npc_ScriptParamLabel.Text = "Shop ID:"; 
            Npc_ScriptParam.IsVisible = true; 
            Npc_ScriptTextCombo.IsVisible = false; 
            Npc_ScriptParam.IsEnabled = true; 
        }
        else if (type == "start_battle" || type == "unlock_battle") 
        { 
            Npc_ScriptParamLabel.Text = "Battle ID:"; 
            Npc_ScriptParam.IsVisible = true; 
            Npc_ScriptTextCombo.IsVisible = false; 
            Npc_ScriptParam.IsEnabled = true; 
        }
        else 
        { 
            Npc_ScriptParamLabel.Text = "Param:"; 
            Npc_ScriptParam.IsVisible = true; 
            Npc_ScriptTextCombo.IsVisible = false; 
            Npc_ScriptParam.IsEnabled = false; 
        }
    }

    private void OnNpcScriptSelected(object? sender, SelectionChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        _selectedScriptIndex = Npc_ScriptList.SelectedIndex;
        LoadScriptStepIntoFields();
    }

    private void OnNpcScriptAdd(object? sender, RoutedEventArgs e)
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;
        var n = _events.Npcs[_selectedIndex];
        n.Script.Add(new ScriptCommand { Type = "say", TextId = "0001" });
        _selectedScriptIndex = n.Script.Count - 1;
        RefreshScriptList();
        OnFieldChanged();
    }

    private void OnNpcScriptRemove(object? sender, RoutedEventArgs e)
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count || _selectedScriptIndex < 0) return;
        var n = _events.Npcs[_selectedIndex];
        if (_selectedScriptIndex >= n.Script.Count) return;

        n.Script.RemoveAt(_selectedScriptIndex);
        if (_selectedScriptIndex >= n.Script.Count) _selectedScriptIndex = n.Script.Count - 1;
        
        RefreshScriptList();
        OnFieldChanged();
    }

    private void OnNpcScriptTypeChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count || _selectedScriptIndex < 0) return;
        
        var n = _events.Npcs[_selectedIndex];
        if (_selectedScriptIndex >= n.Script.Count) return;

        var type = (Npc_ScriptType.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "say";
        n.Script[_selectedScriptIndex].Type = type;
        
        UpdateScriptParamUI(type);
        RefreshScriptList();
        OnFieldChanged();
    }

    private void OnNpcScriptParamChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count || _selectedScriptIndex < 0) return;
        
        var n = _events.Npcs[_selectedIndex];
        if (_selectedScriptIndex >= n.Script.Count) return;

        var cmd = n.Script[_selectedScriptIndex];
        string text = Npc_ScriptParam.Text ?? "";

        if (cmd.Type == "say") cmd.TextId = text;
        else if (cmd.Type == "open_shop") cmd.ShopId = int.TryParse(text, out int s) ? s : 0;
        else if (cmd.Type == "start_battle" || cmd.Type == "unlock_battle") cmd.BattleId = int.TryParse(text, out int b) ? b : 0;

        // Обновляем отображение в списке
        RefreshScriptList(); 
        OnFieldChanged();
    }

    private void OnNpcScriptTextSelected(object? sender, SelectionChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count || _selectedScriptIndex < 0) return;

        var n = _events.Npcs[_selectedIndex];
        if (_selectedScriptIndex >= n.Script.Count) return;

        var cmd = n.Script[_selectedScriptIndex];
        
        if (Npc_ScriptTextCombo.SelectedItem is string selectedItem)
        {
            // Отрезаем текст после двоеточия (нам нужен только ID, например "0001")
            string id = selectedItem.Split(':')[0].Trim();
            cmd.TextId = id;
            // НЕ ставим Npc_ScriptTextCombo.Text = id, чтобы не было рекурсии. 
            // ComboBox сам поставит нужный текст.
        }
        
        RefreshScriptList();
        OnFieldChanged();
    }
    
    private void OnNpcScriptTextLostFocus(object? sender, RoutedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count || _selectedScriptIndex < 0) return;

        var n = _events.Npcs[_selectedIndex];
        if (_selectedScriptIndex >= n.Script.Count) return;

        var cmd = n.Script[_selectedScriptIndex];
        
        string text = Npc_ScriptTextCombo.Text ?? "";
        
        // ★ Если в тексте есть двоеточие (например "0001: Привет"), отрезаем его, оставляя только ID!
        if (text.Contains(":") && !text.Contains(","))
        {
            text = text.Split(':')[0].Trim();
            Npc_ScriptTextCombo.Text = text;
        }

        if (cmd.TextId != text)
        {
            cmd.TextId = text;
            RefreshScriptList();
            OnFieldChanged();
        }
    }
    
    // ══════════════════════════════════════════════════════════════
    //   СПРАЙТ NPC — PREVIEW И ПЕРЕКЛЮЧЕНИЕ
    // ══════════════════════════════════════════════════════════════
    private void OnNpcSpritePrevClick(object? sender, RoutedEventArgs e) => StepNpcSprite(-1);
    private void OnNpcSpriteNextClick(object? sender, RoutedEventArgs e) => StepNpcSprite(+1);

    private void StepNpcSprite(int delta)
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;
        if (_npcSpriteNames.Count == 0) return;

        var npc = _events.Npcs[_selectedIndex];
        int cur = _npcSpriteNames.IndexOf(npc.Sprite);
        if (cur < 0) cur = 0;

        int next = (cur + delta + _npcSpriteNames.Count) % _npcSpriteNames.Count;
        string newName = _npcSpriteNames[next];

        Npc_Sprite.Text = newName;
    }

    private void UpdateNpcSpritePreview(string spriteName)
    {
        if (Npc_SpritePreview == null) return;

        if (string.IsNullOrEmpty(spriteName))
        {
            Npc_SpritePreview.Source = null;
            return;
        }

        try
        {
            string path = Path.Combine(ProjectPaths.NpcSpritesDir, spriteName + ".png");
            if (!File.Exists(path))
            {
                Npc_SpritePreview.Source = null;
                return;
            }

            var full = new Bitmap(path);

            if (full.PixelSize.Width  >= 48 &&
                full.PixelSize.Height >= 144)
            {
                var cropped = new CroppedBitmap(full,
                    new Avalonia.PixelRect(0, 96, 48, 48));
                Npc_SpritePreview.Source = cropped;
            }
            else
            {
                Npc_SpritePreview.Source = full;
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[EVENTS] Sprite preview error: {ex.Message}");
            Npc_SpritePreview.Source = null;
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБЩИЙ ХУК
    // ══════════════════════════════════════════════════════════════
    private void OnFieldChanged()
    {
        if (CanvasControl != null && _currentMap != null)
        {
            CanvasControl.SetEvents(_events, _currentSection, _selectedIndex);
        }

        // Обновляем список, НО замораживаем обработчик выделения.
        // Иначе EventList.ItemsSource = ... сбросит SelectedIndex → вызовется
        // OnEventListSelectionChanged → LoadSelectedIntoFields() → перезапишет
        // все поля из модели, включая те, что пользователь сейчас не трогал.
        int keepIndex = _selectedIndex;

        _suppressListSelection = true;
        try
        {
            RefreshEventList();
            EventList.SelectedIndex = keepIndex;
        }
        finally
        {
            _suppressListSelection = false;
        }
    }
}