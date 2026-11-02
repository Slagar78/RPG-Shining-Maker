using System;
using System.Collections.Generic;
using System.Diagnostics;
using Avalonia.Controls;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

/// <summary>
/// Часть BattleEditorView — загрузка battles/entries.json, выбор карты по map_id,
/// загрузка layout.json, spriteset.json, terrain.json.
/// </summary>
public partial class BattleEditorView
{
    // ══════════════════════════════════════════════════════════
    //   ЗАГРУЗКА ВСЕГО
    // ══════════════════════════════════════════════════════════
    private void LoadAll()
    {
        try
        {
            var battlesPath = BattleEntriesService.GetEntriesPath();
            _battleEntries = BattleEntriesService.Load(battlesPath);
            Debug.WriteLine($"[BATTLE] Загружено битв: {_battleEntries.Count}");

            _mapEntries = EntriesService.Load(ProjectPaths.EntriesFile);
            Debug.WriteLine($"[BATTLE] Карт: {_mapEntries.Count}");

            if (BattlesList != null)
            {
                BattlesList.ItemsSource = null;
                BattlesList.ItemsSource = _battleEntries;
                if (_battleEntries.Count > 0)
                    BattlesList.SelectedIndex = 0;
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[BATTLE] LoadAll error: {ex.Message}");
            StatusChanged?.Invoke($"Ошибка загрузки: {ex.Message}");
        }
    }

    // ══════════════════════════════════════════════════════════
    //   СПИСОК БИТВ
    // ══════════════════════════════════════════════════════════
    private void OnBattleSelected(object? sender, SelectionChangedEventArgs e)
    {
        int idx = BattlesList.SelectedIndex;
        if (idx < 0 || idx >= _battleEntries.Count) return;
        if (idx == _currentBattleIndex) return;
        SwitchBattle(idx);
    }

    private void SwitchBattle(int index)
    {
        if (index < 0 || index >= _battleEntries.Count) return;
        _currentBattleIndex = index;
        LoadBattle(index);
    }

    // ══════════════════════════════════════════════════════════
    //   ЗАГРУЗКА ОДНОЙ БИТВЫ
    // ══════════════════════════════════════════════════════════
    private void LoadBattle(int index)
    {
        if (index < 0 || index >= _battleEntries.Count) return;
        var entry = _battleEntries[index];

        // 1. Резолв карты по map_id (число) → folder
        var mapEntry = _mapEntries.Find(m => m.Id == entry.MapId);
        if (mapEntry == null)
        {
            Debug.WriteLine($"[BATTLE] Карта с ID={entry.MapId} не найдена в maps/entries.json");
            StatusChanged?.Invoke($"Карта с ID={entry.MapId} не найдена");
            return;
        }

        // 2. Загрузка layout.json
        var layoutPath = ProjectPaths.LayoutFile(mapEntry.Folder);
        var map = MapJsonService.Load(layoutPath);
        if (map == null)
        {
            Debug.WriteLine($"[BATTLE] Не удалось загрузить {layoutPath}");
            StatusChanged?.Invoke($"Не удалось загрузить {mapEntry.Folder}/layout.json");
            return;
        }

        map.Name        = mapEntry.Name;
        map.Folder      = mapEntry.Folder;
        map.MusicFile   = mapEntry.Music;
        map.MusicVolume = mapEntry.MusicVolume;
        _currentMap = map;

        // 3. Нарезка тайлсета
        LoadTilesetFromMap();

        // 4. Отдать карту в MapCanvas
        if (MapCanvasControl != null)
            MapCanvasControl.SetMap(_currentMap, _tiles);

        // 5. Загрузка spriteset.json и terrain.json (метод в Units.cs)
        LoadSpritesetAndTerrain(entry.Folder);

        // 6. Обновление UI
        UpdateToolbarInfo();
        ApplyModeButtons();
        ApplyViewMode();
        RebuildOverlay();

        StatusChanged?.Invoke($"Battle #{index} загружена — {map.Name}");
    }

    // ══════════════════════════════════════════════════════════
    //   ИНФО В ТУЛБАРЕ + BOTTOM BAR
    // ══════════════════════════════════════════════════════════
    private void UpdateToolbarInfo()
    {
        if (_currentBattleIndex < 0 || _currentBattleIndex >= _battleEntries.Count) return;

        var entry = _battleEntries[_currentBattleIndex];
        var mapName = _currentMap?.Name ?? "—";

        if (ToolbarInfo != null)
            ToolbarInfo.Text = $"Battle #{_currentBattleIndex}  |  Map: {mapName}";

        if (BottomZoneText != null)
        {
            BottomZoneText.Text = entry.BattleWidth > 0
                ? $"Zone: {entry.BattleX},{entry.BattleY}  {entry.BattleWidth}×{entry.BattleHeight}"
                : "Zone: —";
        }
    }
}