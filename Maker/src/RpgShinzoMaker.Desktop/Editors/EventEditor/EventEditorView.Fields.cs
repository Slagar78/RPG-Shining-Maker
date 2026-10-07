// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.Fields.cs
using System;
using System.Diagnostics;
using System.IO;
using Avalonia.Controls;
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
    /// <summary>
    /// Копирует данные выбранного события в текстовые поля.
    /// Блокирует TextChanged, чтобы не было рекурсии.
    /// </summary>
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

    // ─── Roof ───
    private void LoadRoof()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count)
        {
            ClearRoofFields();
            return;
        }

        var r = _events.Roofs[_selectedIndex];
        Roof_TileId.Text   = r.TileId.ToString();
        Roof_StartXY.Text  = $"{r.StartX},{r.StartY}";
        Roof_EndXY.Text    = $"{r.EndX},{r.EndY}";
        Roof_Trig1XY.Text  = r.TriggerX  >= 0 ? $"{r.TriggerX},{r.TriggerY}"   : "-";
        Roof_Trig2XY.Text  = r.Trigger2X >= 0 ? $"{r.Trigger2X},{r.Trigger2Y}" : "-";
        Roof_Exit1XY.Text  = r.ExitX     >= 0 ? $"{r.ExitX},{r.ExitY}"         : "-";
        Roof_Exit2XY.Text  = r.Exit2X    >= 0 ? $"{r.Exit2X},{r.Exit2Y}"       : "-";
    }

    private void ClearRoofFields()
    {
        Roof_TileId.Text  = "0";
        Roof_StartXY.Text = "0,0";
        Roof_EndXY.Text   = "1,1";
        Roof_Trig1XY.Text = "-";
        Roof_Trig2XY.Text = "-";
        Roof_Exit1XY.Text = "-";
        Roof_Exit2XY.Text = "-";
    }

    // ─── Tile Change ───
    private void LoadTileChange()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count)
        {
            TC_TriggerXY.Text = "-";
            TC_NewTile.Text   = "0";
            TC_CloseXY.Text   = "-";
            return;
        }

        var tc = _events.TileChanges[_selectedIndex];
        TC_TriggerXY.Text = tc.TriggerX >= 0 ? $"{tc.TriggerX},{tc.TriggerY}" : "-";
        TC_NewTile.Text   = tc.NewTileId.ToString();
        TC_CloseXY.Text   = tc.CloseX   >= 0 ? $"{tc.CloseX},{tc.CloseY}"     : "-";
    }

    // ─── Stair ───
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

    // ─── Warp ───
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

    // ─── NPC ───
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
    //   ПАРСЕРЫ
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

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ ROOF
    // ══════════════════════════════════════════════════════════════
    private void OnRoofTileIdChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var v = ParseInt(Roof_TileId.Text ?? "");
        if (v.HasValue) _events.Roofs[_selectedIndex].TileId = v.Value;
    }

    private void OnRoofStartXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var p = ParsePair(Roof_StartXY.Text ?? "");
        if (p.HasValue)
        {
            _events.Roofs[_selectedIndex].StartX = p.Value.x;
            _events.Roofs[_selectedIndex].StartY = p.Value.y;
            OnFieldChanged();
        }
    }

    private void OnRoofEndXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var p = ParsePair(Roof_EndXY.Text ?? "");
        if (p.HasValue)
        {
            _events.Roofs[_selectedIndex].EndX = p.Value.x;
            _events.Roofs[_selectedIndex].EndY = p.Value.y;
            OnFieldChanged();
        }
    }

    private void OnRoofTrig1XYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        if (IsDash(Roof_Trig1XY.Text ?? ""))
        {
            r.TriggerX = r.TriggerY = -1;
        }
        else
        {
            var p = ParsePair(Roof_Trig1XY.Text ?? "");
            if (p.HasValue) { r.TriggerX = p.Value.x; r.TriggerY = p.Value.y; }
        }
        OnFieldChanged();
    }

    private void OnRoofTrig2XYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        if (IsDash(Roof_Trig2XY.Text ?? ""))
        {
            r.Trigger2X = r.Trigger2Y = -1;
        }
        else
        {
            var p = ParsePair(Roof_Trig2XY.Text ?? "");
            if (p.HasValue) { r.Trigger2X = p.Value.x; r.Trigger2Y = p.Value.y; }
        }
        OnFieldChanged();
    }

    private void OnRoofExit1XYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        if (IsDash(Roof_Exit1XY.Text ?? ""))
        {
            r.ExitX = r.ExitY = -1;
        }
        else
        {
            var p = ParsePair(Roof_Exit1XY.Text ?? "");
            if (p.HasValue) { r.ExitX = p.Value.x; r.ExitY = p.Value.y; }
        }
        OnFieldChanged();
    }

    private void OnRoofExit2XYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.Roofs.Count) return;

        var r = _events.Roofs[_selectedIndex];
        if (IsDash(Roof_Exit2XY.Text ?? ""))
        {
            r.Exit2X = r.Exit2Y = -1;
        }
        else
        {
            var p = ParsePair(Roof_Exit2XY.Text ?? "");
            if (p.HasValue) { r.Exit2X = p.Value.x; r.Exit2Y = p.Value.y; }
        }
        OnFieldChanged();
    }

    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ TILE CHANGE
    // ══════════════════════════════════════════════════════════════
    private void OnTCTriggerXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count) return;

        var tc = _events.TileChanges[_selectedIndex];
        if (IsDash(TC_TriggerXY.Text ?? ""))
        {
            tc.TriggerX = tc.TriggerY = -1;
        }
        else
        {
            var p = ParsePair(TC_TriggerXY.Text ?? "");
            if (p.HasValue) { tc.TriggerX = p.Value.x; tc.TriggerY = p.Value.y; }
        }
        OnFieldChanged();
    }

    private void OnTCNewTileChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count) return;

        var v = ParseInt(TC_NewTile.Text ?? "");
        if (v.HasValue) _events.TileChanges[_selectedIndex].NewTileId = v.Value;
    }

    private void OnTCCloseXYChanged(object? sender, TextChangedEventArgs e)
    {
        if (_suppressFieldEvents) return;
        if (_selectedIndex < 0 || _selectedIndex >= _events.TileChanges.Count) return;

        var tc = _events.TileChanges[_selectedIndex];
        if (IsDash(TC_CloseXY.Text ?? ""))
        {
            tc.CloseX = tc.CloseY = -1;
        }
        else
        {
            var p = ParsePair(TC_CloseXY.Text ?? "");
            if (p.HasValue) { tc.CloseX = p.Value.x; tc.CloseY = p.Value.y; }
        }
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

        // Обновляем текстовое поле (это триггерит TextChanged → OnNpcSpriteChanged)
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

            // Лист 96×144 = 2×3 клетки 48×48. Берём нижний левый кадр.
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
                // На всякий случай — просто целиком
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
    //   ОБЩИЙ ХУК — ПЕРЕРИСОВКА КАНВАСА И СПИСКА ПОСЛЕ ПРАВКИ ПОЛЯ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Вызывается из любого TextChanged, когда изменение координат/типа
    /// влияет на вид на карте. Обновляет подсветки и заголовок в списке.
    /// </summary>
    private void OnFieldChanged()
    {
        // Обновить подсветки на канвасе
        if (CanvasControl != null && _currentMap != null)
        {
            CanvasControl.SetEvents(_events, _currentSection, _selectedIndex);
        }

        // Обновить строку в списке (без сброса выделения)
        int keepIndex = _selectedIndex;
        RefreshEventList();
        EventList.SelectedIndex = keepIndex;
    }
}