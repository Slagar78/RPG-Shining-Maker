// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.Sections.cs
using System.Collections.Generic;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.Media;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Editors.EventEditor;

/// <summary>
/// Часть EventEditorView — РАЗДЕЛЫ.
/// Переключение активного раздела (Roof / TileChange / Stair / Warp / NPC),
/// обновление списка событий и видимости панели полей.
/// </summary>
public partial class EventEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   ОБРАБОТЧИКИ 5 КНОПОК РАЗДЕЛОВ
    // ══════════════════════════════════════════════════════════════
    private void OnSectionRoofClick(object? sender, RoutedEventArgs e)       => SelectSection("roof");
    private void OnSectionTileChangeClick(object? sender, RoutedEventArgs e) => SelectSection("tile_change");
    private void OnSectionStairClick(object? sender, RoutedEventArgs e)      => SelectSection("stair");
    private void OnSectionWarpClick(object? sender, RoutedEventArgs e)       => SelectSection("warp");
    private void OnSectionNpcClick(object? sender, RoutedEventArgs e)        => SelectSection("npc");

    /// <summary>
    /// Переключает активный раздел.
    /// Сбрасывает выделение, обновляет кнопки, список событий и оверлей.
    /// </summary>
    private void SelectSection(string section)
    {
        if (_currentSection == section)
        {
            // Повторный клик — просто синхронизируем UI (на случай если что-то разъехалось)
            UpdateSectionButtons();
            return;
        }

        _currentSection = section;
        _selectedIndex = -1;

        UpdateSectionButtons();
        UpdatePanelVisibility();
        RefreshEventList();
        RedrawCanvas();
    }

    // ══════════════════════════════════════════════════════════════
    //   ПОДСВЕТКА АКТИВНОЙ КНОПКИ РАЗДЕЛА
    // ══════════════════════════════════════════════════════════════
    private void UpdateSectionButtons()
    {
        SetToggleActive(SectionRoofBtn,       _currentSection == "roof");
        SetToggleActive(SectionTileChangeBtn, _currentSection == "tile_change");
        SetToggleActive(SectionStairBtn,      _currentSection == "stair");
        SetToggleActive(SectionWarpBtn,       _currentSection == "warp");
        SetToggleActive(SectionNpcBtn,        _currentSection == "npc");
    }

    private static void SetToggleActive(ToggleButton btn, bool active)
    {
        if (btn == null) return;

        btn.IsChecked = active;
        btn.Background = new SolidColorBrush(
            Color.Parse(active ? "#3E5F8A" : "#3E3E42"));
        btn.Foreground = new SolidColorBrush(
            Color.Parse(active ? "#FFFFFF" : "#CCCCCC"));
    }

    // ══════════════════════════════════════════════════════════════
    //   ВИДИМОСТЬ ПАНЕЛИ ПОЛЕЙ
    // ══════════════════════════════════════════════════════════════
    private void UpdatePanelVisibility()
    {
        if (RoofPanel       == null) return;
        if (TileChangePanel == null) return;
        if (StairPanel      == null) return;
        if (WarpPanel       == null) return;
        if (NpcPanel        == null) return;

        RoofPanel.IsVisible       = _currentSection == "roof";
        TileChangePanel.IsVisible = _currentSection == "tile_change";
        StairPanel.IsVisible      = _currentSection == "stair";
        WarpPanel.IsVisible       = _currentSection == "warp";
        NpcPanel.IsVisible        = _currentSection == "npc";
    }

    // ══════════════════════════════════════════════════════════════
    //   СПИСОК СОБЫТИЙ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Перестраивает список событий текущего раздела.
    /// Ставит активным элементом _selectedIndex (если >= 0).
    /// </summary>
    private void RefreshEventList()
    {
        if (EventList == null) return;

        var summaries = BuildEventSummaries();

        // Защита от рекурсии — блокируем SelectionChanged на время заполнения
        EventList.SelectionChanged -= OnEventListSelectionChanged;

        EventList.ItemsSource = summaries;
        EventList.SelectedIndex = _selectedIndex;

        EventList.SelectionChanged += OnEventListSelectionChanged;
    }

    private List<string> BuildEventSummaries()
    {
        var result = new List<string>();

        switch (_currentSection)
        {
            case "roof":
                foreach (var r in _events.Roofs)
                    result.Add($"Tile {r.TileId} ({r.StartX},{r.StartY})-({r.EndX},{r.EndY})");
                break;

            case "tile_change":
                foreach (var tc in _events.TileChanges)
                    result.Add($"({tc.TriggerX},{tc.TriggerY}) → {tc.NewTileId}");
                break;

            case "stair":
                foreach (var st in _events.Stairs)
                    result.Add($"({st.StartX},{st.StartY})→({st.EndX},{st.EndY}) dir={st.Direction}");
                break;

            case "warp":
                foreach (var w in _events.Warps)
                    result.Add($"({w.TriggerX},{w.TriggerY}) → {w.TargetMap}");
                break;

            case "npc":
                foreach (var n in _events.Npcs)
                    result.Add($"{n.Id} ({n.X},{n.Y}) {n.Behavior} [text:{n.TextId}]");
                break;
        }

        return result;
    }

    // ══════════════════════════════════════════════════════════════
    //   ВЫБОР СОБЫТИЯ В СПИСКЕ
    // ══════════════════════════════════════════════════════════════
    private void OnEventListSelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        _selectedIndex = EventList.SelectedIndex;

        // Заполнить поля (Fields.cs)
        LoadSelectedIntoFields();

        // Обновить подсветку на карте
        RedrawCanvas();
    }
}