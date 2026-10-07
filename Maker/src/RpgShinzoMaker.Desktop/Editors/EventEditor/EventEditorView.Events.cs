// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.Events.cs
using System.Diagnostics;
using Avalonia.Interactivity;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Editors.EventEditor;

/// <summary>
/// Часть EventEditorView — ДОБАВЛЕНИЕ И УДАЛЕНИЕ СОБЫТИЙ.
/// Кнопки "+" и "−" справа от панели полей.
/// </summary>
public partial class EventEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   ADD
    // ══════════════════════════════════════════════════════════════
    private void OnAddClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;

        switch (_currentSection)
        {
            case "roof":        _events.Roofs.Add(new RoofEvent());              break;
            case "tile_change": _events.TileChanges.Add(new TileChangeEvent());  break;
            case "stair":       _events.Stairs.Add(new StairEvent());            break;
            case "warp":        _events.Warps.Add(new WarpEvent());              break;
            case "npc":         _events.Npcs.Add(new NpcEvent());                break;
        }

        int newIndex = GetCount(_currentSection) - 1;
        _selectedIndex = newIndex;

        RefreshEventList();
        EventList.SelectedIndex = newIndex;

        Debug.WriteLine($"[EVENTS] Added {_currentSection} #{newIndex}");
    }

    // ══════════════════════════════════════════════════════════════
    //   REMOVE
    // ══════════════════════════════════════════════════════════════
    private void OnRemoveClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;
        if (_selectedIndex < 0) return;

        bool removed = false;

        switch (_currentSection)
        {
            case "roof":
                if (_selectedIndex < _events.Roofs.Count)
                {
                    _events.Roofs.RemoveAt(_selectedIndex);
                    removed = true;
                }
                break;

            case "tile_change":
                if (_selectedIndex < _events.TileChanges.Count)
                {
                    _events.TileChanges.RemoveAt(_selectedIndex);
                    removed = true;
                }
                break;

            case "stair":
                if (_selectedIndex < _events.Stairs.Count)
                {
                    _events.Stairs.RemoveAt(_selectedIndex);
                    removed = true;
                }
                break;

            case "warp":
                if (_selectedIndex < _events.Warps.Count)
                {
                    _events.Warps.RemoveAt(_selectedIndex);
                    removed = true;
                }
                break;

            case "npc":
                if (_selectedIndex < _events.Npcs.Count)
                {
                    _events.Npcs.RemoveAt(_selectedIndex);
                    removed = true;
                }
                break;
        }

        if (!removed) return;

        Debug.WriteLine($"[EVENTS] Removed {_currentSection} #{_selectedIndex}");

        // Выбираем предыдущий элемент (или -1, если список пуст)
        int newIndex = _selectedIndex - 1;
        if (newIndex < 0 && GetCount(_currentSection) > 0)
            newIndex = 0;

        _selectedIndex = newIndex;

        RefreshEventList();
        EventList.SelectedIndex = newIndex;

        // Если ничего не выбрано — очищаем поля
        if (newIndex < 0)
            LoadSelectedIntoFields();

        RedrawCanvas();
    }

    // ══════════════════════════════════════════════════════════════
    //   ВСПОМОГАТЕЛЬНОЕ
    // ══════════════════════════════════════════════════════════════
    private int GetCount(string section) => section switch
    {
        "roof"        => _events.Roofs.Count,
        "tile_change" => _events.TileChanges.Count,
        "stair"       => _events.Stairs.Count,
        "warp"        => _events.Warps.Count,
        "npc"         => _events.Npcs.Count,
        _             => 0
    };
}