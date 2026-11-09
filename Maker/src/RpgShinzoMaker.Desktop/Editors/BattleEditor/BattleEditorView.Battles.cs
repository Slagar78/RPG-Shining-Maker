using System;
using System.Collections.Generic;
using System.Diagnostics;
using Avalonia.Controls;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
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

    private void LoadBattle(int index)
    {
        if (index < 0 || index >= _battleEntries.Count) return;
        var entry = _battleEntries[index];

        var mapEntry = _mapEntries.Find(m => m.Id == entry.MapId);
        if (mapEntry == null)
        {
            Debug.WriteLine($"[BATTLE] Карта с ID={entry.MapId} не найдена в maps/entries.json");
            StatusChanged?.Invoke($"Карта с ID={entry.MapId} не найдена");
            return;
        }

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

        LoadTilesetFromMap();

        if (MapCanvasControl != null)
            MapCanvasControl.SetMap(_currentMap, _tiles);

        LoadSpritesetAndTerrain(entry.Folder);

        UpdateToolbarInfo();
        ApplyModeButtons();
        ApplyViewMode();
        RebuildOverlay();

        StatusChanged?.Invoke($"Battle #{index} загружена — {map.Name}");
    }

    private void UpdateToolbarInfo()
    {
        if (_currentBattleIndex < 0 || _currentBattleIndex >= _battleEntries.Count) return;

        var entry = _battleEntries[_currentBattleIndex];

        // Блокируем события, чтобы не было рекурсии при обновлении TextBox.Text
        _suppressZoneEvents = true;
        
        if (BattlePosX != null) BattlePosX.Text = entry.BattleX.ToString();
        if (BattlePosY != null) BattlePosY.Text = entry.BattleY.ToString();
        if (BattleSizeX != null) BattleSizeX.Text = entry.BattleWidth.ToString();
        if (BattleSizeY != null) BattleSizeY.Text = entry.BattleHeight.ToString();
        
        _suppressZoneEvents = false;
    }
}