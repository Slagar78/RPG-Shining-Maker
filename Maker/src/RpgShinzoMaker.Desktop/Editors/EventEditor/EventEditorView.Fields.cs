// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.Fields.cs
using System;
using System.Diagnostics;
using System.IO;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Services;

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

        // Tile ID — только отображение
        Roof_TileId.Text = r.TileId.ToString();

        // Пары X / Y
        SetPair(Roof_StartX, Roof_StartY, r.StartX,    r.StartY,    allowEmpty: false);
        SetPair(Roof_EndX,   Roof_EndY,   r.EndX,      r.EndY,      allowEmpty: false);
        SetPair(Roof_Trig1X, Roof_Trig1Y, r.TriggerX,  r.TriggerY,  allowEmpty: true);
        SetPair(Roof_Trig2X, Roof_Trig2Y, r.Trigger2X, r.Trigger2Y, allowEmpty: true);
        SetPair(Roof_Exit1X, Roof_Exit1Y, r.ExitX,     r.ExitY,     allowEmpty: true);
        SetPair(Roof_Exit2X, Roof_Exit2Y, r.Exit2X,    r.Exit2Y,    allowEmpty: true);

        UpdateRoofExit2Enabled();
    }

    private void ClearRoofFields()
    {
        Roof_TileId.Text = "0";
        Roof_StartX.Text = ""; Roof_StartY.Text = "";
        Roof_EndX.Text   = ""; Roof_EndY.Text   = "";
        Roof_Trig1X.Text = ""; Roof_Trig1Y.Text = "";
        Roof_Trig2X.Text = ""; Roof_Trig2Y.Text = "";
        Roof_Exit1X.Text = ""; Roof_Exit1Y.Text = "";
        Roof_Exit2X.Text = ""; Roof_Exit2Y.Text = "";
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

    /// <summary>Пересчитывает Tile ID по координатам Start из карты (L1 слой).</summary>
    private void UpdateRoofTileId()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;
        if (_currentMap == null) return;

        var r = _events.Roofs[_selectedIndex];
        if (r.StartX < 0 || r.StartY < 0) return;
        if (r.StartX >= _currentMap.Width || r.StartY >= _currentMap.Height) return;

        int idx = r.StartX * _currentMap.Height + r.StartY;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        int tileId = _currentMap.Tiles[idx];
        r.TileId = tileId;

        if (Roof_TileId != null)
            Roof_TileId.Text = tileId.ToString();
    }

    /// <summary>Exit2 активен только если Trig2 заполнен.</summary>
    private void UpdateRoofExit2Enabled()
    {
        if (Roof_Exit2X == null || Roof_Exit2Y == null) return;

        bool trig2Filled = !string.IsNullOrEmpty(Roof_Trig2X.Text)
                        || !string.IsNullOrEmpty(Roof_Trig2Y.Text);

        Roof_Exit2X.IsEnabled = trig2Filled;
        Roof_Exit2Y.IsEnabled = trig2Filled;

        if (!trig2Filled)
        {
            Roof_Exit2X.Text = "";
            Roof_Exit2Y.Text = "";
        }
    }

    // ─── ROOF — обработчики Start ───
    private void OnRoofStartXChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        _events.Roofs[_selectedIndex].StartX = ParseCoord(Roof_StartX.Text, fallback: 0);
        UpdateRoofTileId();
        OnFieldChanged();
    }

    private void OnRoofStartYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        _events.Roofs[_selectedIndex].StartY = ParseCoord(Roof_StartY.Text, fallback: 0);
        UpdateRoofTileId();
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

    // ─── ROOF — обработчики Trig1 ───
    private void OnRoofTrig1XChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        int oldX = r.TriggerX;
        r.TriggerX = ParseCoord(Roof_Trig1X.Text, fallback: -1);

        if (r.TriggerX < 0) r.TriggerY = -1;
        else if (oldX < 0 && r.TriggerY < 0) r.TriggerY = 0;

        OnFieldChanged();
    }

    private void OnRoofTrig1YChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        r.TriggerY = ParseCoord(Roof_Trig1Y.Text, fallback: -1);

        if (r.TriggerY < 0) r.TriggerX = -1;
        else if (r.TriggerX < 0) r.TriggerX = 0;

        OnFieldChanged();
    }

    // ─── ROOF — обработчики Trig2 ───
    private void OnRoofTrig2XChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        int oldX = r.Trigger2X;
        r.Trigger2X = ParseCoord(Roof_Trig2X.Text, fallback: -1);

        if (r.Trigger2X < 0) r.Trigger2Y = -1;
        else if (oldX < 0 && r.Trigger2Y < 0) r.Trigger2Y = 0;

        UpdateRoofExit2Enabled();
        OnFieldChanged();
    }

    private void OnRoofTrig2YChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        r.Trigger2Y = ParseCoord(Roof_Trig2Y.Text, fallback: -1);

        if (r.Trigger2Y < 0) r.Trigger2X = -1;
        else if (r.Trigger2X < 0) r.Trigger2X = 0;

        UpdateRoofExit2Enabled();
        OnFieldChanged();
    }

    // ─── ROOF — обработчики Exit1 ───
    private void OnRoofExit1XChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        int oldX = r.ExitX;
        r.ExitX = ParseCoord(Roof_Exit1X.Text, fallback: -1);

        if (r.ExitX < 0) r.ExitY = -1;
        else if (oldX < 0 && r.ExitY < 0) r.ExitY = 0;

        OnFieldChanged();
    }

    private void OnRoofExit1YChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        r.ExitY = ParseCoord(Roof_Exit1Y.Text, fallback: -1);

        if (r.ExitY < 0) r.ExitX = -1;
        else if (r.ExitX < 0) r.ExitX = 0;

        OnFieldChanged();
    }

    // ─── ROOF — обработчики Exit2 ───
    private void OnRoofExit2XChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        int oldX = r.Exit2X;
        r.Exit2X = ParseCoord(Roof_Exit2X.Text, fallback: -1);

        if (r.Exit2X < 0) r.Exit2Y = -1;
        else if (oldX < 0 && r.Exit2Y < 0) r.Exit2Y = 0;

        OnFieldChanged();
    }

    private void OnRoofExit2YChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        r.Exit2Y = ParseCoord(Roof_Exit2Y.Text, fallback: -1);

        if (r.Exit2Y < 0) r.Exit2X = -1;
        else if (r.Exit2X < 0) r.Exit2X = 0;

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
            St_StartXY.Text   = "0,0";
            St_EndXY.Text     = "1,1";
            St_Direction.Text = "0";
            return;
        }

        var st = _events.Stairs[_selectedIndex];
        St_StartXY.Text   = $"{st.StartX},{st.StartY}";
        St_EndXY.Text     = $"{st.EndX},{st.EndY}";
        St_Direction.Text = st.Direction.ToString();
    }

    // ══════════════════════════════════════════════════════════════
    //   WARP — загрузка в поля
    // ══════════════════════════════════════════════════════════════
    private void LoadWarp()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count)
        {
            W_TriggerXY.Text = "-";
            W_TargetMap.Text = "";
            W_TargetXY.Text  = "0,0";
            W_Facing.Text    = "0";
            return;
        }

        var w = _events.Warps[_selectedIndex];
        W_TriggerXY.Text = w.TriggerX >= 0 ? $"{w.TriggerX},{w.TriggerY}" : "-";
        W_TargetMap.Text = w.TargetMap;
        W_TargetXY.Text  = $"{w.TargetX},{w.TargetY}";
        W_Facing.Text    = w.Facing.ToString();
    }

    // ══════════════════════════════════════════════════════════════
    //   NPC — загрузка в поля
    // ══════════════════════════════════════════════════════════════
    private void LoadNpc()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count)
        {
            Npc_Id.Text        = "";
            Npc_XY.Text        = "0,0";
            Npc_Sprite.Text    = "";
            Npc_Behavior.SelectedIndex = 0;
            Npc_Direction.SelectedIndex = 0;
            Npc_TextId.Text    = "";
            Npc_HomeXY.Text    = "0,0";
            Npc_Radius.Text    = "3";
            Npc_SpritePreview.Source = null;
            return;
        }

        var n = _events.Npcs[_selectedIndex];
        Npc_Id.Text        = n.Id;
        Npc_XY.Text        = $"{n.X},{n.Y}";
        Npc_Sprite.Text    = n.Sprite;
        Npc_Behavior.SelectedIndex  = n.Behavior == "wander" ? 1 : 0;
        Npc_Direction.SelectedIndex = n.Direction switch
        {
            "down"  => 0,
            "left"  => 1,
            "right" => 2,
            "up"    => 3,
            _       => 0
        };
        Npc_TextId.Text = n.TextId;
        Npc_HomeXY.Text = $"{n.HomeX},{n.HomeY}";
        Npc_Radius.Text = n.Radius.ToString();
        UpdateNpcSpritePreview(n.Sprite);
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
    //   ОБРАБОТЧИКИ STAIR
    // ══════════════════════════════════════════════════════════════
    private void OnStStartXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count) return;

        var p = ParsePair(St_StartXY.Text ?? "");
        if (p.HasValue)
        {
            _events.Stairs[_selectedIndex].StartX = p.Value.x;
            _events.Stairs[_selectedIndex].StartY = p.Value.y;
            OnFieldChanged();
        }
    }

    private void OnStEndXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count) return;

        var p = ParsePair(St_EndXY.Text ?? "");
        if (p.HasValue)
        {
            _events.Stairs[_selectedIndex].EndX = p.Value.x;
            _events.Stairs[_selectedIndex].EndY = p.Value.y;
            OnFieldChanged();
        }
    }

    private void OnStDirectionChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Stairs.Count) return;

        var v = ParseInt(St_Direction.Text ?? "");
        if (v.HasValue && (v.Value == 0 || v.Value == 1))
            _events.Stairs[_selectedIndex].Direction = v.Value;
        OnFieldChanged();
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ WARP
    // ══════════════════════════════════════════════════════════════
    private void OnWTriggerXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        var w = _events.Warps[_selectedIndex];
        if (IsDash(W_TriggerXY.Text ?? ""))
        {
            w.TriggerX = w.TriggerY = -1;
        }
        else
        {
            var p = ParsePair(W_TriggerXY.Text ?? "");
            if (p.HasValue) { w.TriggerX = p.Value.x; w.TriggerY = p.Value.y; }
        }
        OnFieldChanged();
    }

    private void OnWTargetMapChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        _events.Warps[_selectedIndex].TargetMap = (W_TargetMap.Text ?? "").Trim();
    }

    private void OnWTargetXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        var p = ParsePair(W_TargetXY.Text ?? "");
        if (p.HasValue)
        {
            _events.Warps[_selectedIndex].TargetX = p.Value.x;
            _events.Warps[_selectedIndex].TargetY = p.Value.y;
        }
    }

    private void OnWFacingChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Warps.Count) return;

        var v = ParseInt(W_Facing.Text ?? "");
        if (v.HasValue && v.Value >= 0 && v.Value <= 3)
        {
            _events.Warps[_selectedIndex].Facing = v.Value;
            OnFieldChanged();
        }
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

        var p = ParsePair(Npc_XY.Text ?? "");
        if (p.HasValue)
        {
            _events.Npcs[_selectedIndex].X = p.Value.x;
            _events.Npcs[_selectedIndex].Y = p.Value.y;
            OnFieldChanged();
        }
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
        _events.Npcs[_selectedIndex].Behavior = value;
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

    private void OnNpcTextIdChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        _events.Npcs[_selectedIndex].TextId = (Npc_TextId.Text ?? "").Trim();
    }

    private void OnNpcHomeXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        var n = _events.Npcs[_selectedIndex];
        if (IsDash(Npc_HomeXY.Text ?? ""))
        {
            n.HomeX = n.HomeY = 0;
        }
        else
        {
            var p = ParsePair(Npc_HomeXY.Text ?? "");
            if (p.HasValue) { n.HomeX = p.Value.x; n.HomeY = p.Value.y; }
        }
    }

    private void OnNpcRadiusChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Npcs.Count) return;

        var v = ParseInt(Npc_Radius.Text ?? "");
        if (v.HasValue) _events.Npcs[_selectedIndex].Radius = v.Value;
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

        int keepIndex = _selectedIndex;
        RefreshEventList();
        EventList.SelectedIndex = keepIndex;
    }
}