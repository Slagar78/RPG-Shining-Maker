// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.Map.cs
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json; // Добавлено для чтения mapsetups.json
using Avalonia.Controls;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.EventEditor;

/// <summary>
/// Часть EventEditorView — ЖИЗНЕННЫЙ ЦИКЛ КАРТЫ.
/// Загрузка проекта, комбо карт, слои, зум, сохранение событий.
/// </summary>
public partial class EventEditorView
{
    // ★ ДОБАВЛЕНО: выбранная версия (по умолчанию v00)
    private string _selectedVersionId = "v00";

    // ══════════════════════════════════════════════════════════════
    //   ЗАГРУЗКА ПРОЕКТА
    // ══════════════════════════════════════════════════════════════
    private void LoadProject()
    {
        try
        {
            var root = FindProjectRoot();
            Debug.WriteLine($"[EVENTS] root = {root ?? "NULL"}");
            if (root == null) return;

            ProjectPaths.Root = root;

            _mapEntries = EntriesService.Load(ProjectPaths.EntriesFile);
            Debug.WriteLine($"[EVENTS] entries: {_mapEntries.Count}");
            if (_mapEntries.Count == 0) return;

            MapSelector.ItemsSource = null;
            MapSelector.ItemsSource = _mapEntries.Select(e => e.Name).ToList();

            // Загрузить спрайты NPC заранее (для превью и валидации)
            LoadNpcSpriteNames();

            MapSelector.SelectedIndex = 0;
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[EVENTS] LoadProject error: {ex.Message}");
        }
    }

    private static string? FindProjectRoot()
    {
        var dir = new DirectoryInfo(Environment.CurrentDirectory);
        for (int i = 0; i < 10 && dir != null; i++)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "data", "maps")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return null;
    }

    // ══════════════════════════════════════════════════════════════
    //   СПИСОК СПРАЙТОВ NPC
    // ══════════════════════════════════════════════════════════════
    private void LoadNpcSpriteNames()
    {
        _npcSpriteNames.Clear();

        var dir = ProjectPaths.NpcSpritesDir;
        if (Directory.Exists(dir))
        {
            foreach (var f in Directory.GetFiles(dir, "*.png"))
                _npcSpriteNames.Add(Path.GetFileNameWithoutExtension(f));

            _npcSpriteNames.Sort(StringComparer.OrdinalIgnoreCase);
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   КОМБО КАРТ
    // ══════════════════════════════════════════════════════════════
    private void OnMapSelectorChanged(object? sender, SelectionChangedEventArgs e)
    {
        int idx = MapSelector.SelectedIndex;
        if (idx < 0 || idx >= _mapEntries.Count) return;

        _currentMapIndex = idx;
        LoadMapByEntry(_mapEntries[idx]);
    }

    // ══════════════════════════════════════════════════════════════
    //   ★ НОВОЕ: КОМБО ВЕРСИЙ (MAPSETUP)
    // ══════════════════════════════════════════════════════════════
    private void OnVersionSelectorChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (VersionSelector.SelectedItem is string ver && ver != _selectedVersionId)
        {
            _selectedVersionId = ver;
            if (_currentMapIndex >= 0)
                LoadMapByEntry(_mapEntries[_currentMapIndex]); // Перезагружаем карту с новой версией
        }
    }

    private void PopulateVersionSelector(string folder)
    {
        if (VersionSelector == null) return;

        // Отписываемся от события, чтобы не было рекурсии при заполнении
        VersionSelector.SelectionChanged -= OnVersionSelectorChanged;
        
        var versions = new List<string>();
        var setupFile = ProjectPaths.MapSetupsFile(folder);

        if (File.Exists(setupFile))
        {
            try
            {
                var setup = JsonSerializer.Deserialize<MapSetup>(File.ReadAllText(setupFile));
                if (setup?.Versions != null)
                {
                    foreach (var v in setup.Versions)
                        versions.Add(v.Id); // Берем только ID (например, "v00", "v01")
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[EVENTS] Ошибка чтения mapsetups.json: {ex.Message}");
            }
        }

        // Если файла нет или он пуст — показываем заглушку v00
        if (versions.Count == 0) 
            versions.Add("v00");

        VersionSelector.ItemsSource = versions;
        
        // Восстанавливаем выбранную версию (если она есть в новом списке)
        int idx = versions.IndexOf(_selectedVersionId);
        VersionSelector.SelectedIndex = idx >= 0 ? idx : 0;
        _selectedVersionId = versions[VersionSelector.SelectedIndex];
        
        // Подписываемся обратно
        VersionSelector.SelectionChanged += OnVersionSelectorChanged;
    }

    // ══════════════════════════════════════════════════════════════
    //   ЗАГРУЗКА КОНКРЕТНОЙ КАРТЫ
    // ══════════════════════════════════════════════════════════════
    private void LoadMapByEntry(MapEntry entry)
    {
        try
        {
            // 1. Карта (layout.json)
            var layoutPath = ProjectPaths.LayoutFile(entry.Folder);
            if (!File.Exists(layoutPath))
            {
                Debug.WriteLine($"[EVENTS] layout.json не найден: {layoutPath}");
                return;
            }

            var map = MapJsonService.Load(layoutPath);
            if (map == null) return;

            map.Name        = entry.Name;
            map.Folder      = entry.Folder;
            map.MusicFile   = entry.Music;
            map.MusicVolume = entry.MusicVolume;
            map.AreasPath   = entry.Areas;

            _currentMap = map;

            // Загружаем text.txt карты
            _localTexts.Clear();
            string textPath = Path.Combine(ProjectPaths.MapsDir, entry.Folder, "text.txt");
            if (File.Exists(textPath))
            {
                foreach (var line in File.ReadAllLines(textPath))
                {
                    var parts = line.Split('=', 2);
                    if (parts.Length == 2) _localTexts[parts[0].Trim()] = parts[1].Trim();
                }
            }
            
            // 2. Обновляем список версий для текущей карты
            PopulateVersionSelector(entry.Folder);

            // 3. Загружаем события с учетом выбранной версии
            _events = EventsService.Load(entry.Folder, _selectedVersionId);

            // 4. Сбросить выделение
            _selectedIndex = -1;
            _selectedRoofTriggerIndex = -1;
            _selectedRoofExitIndex = -1;

            // 5. Обновить список событий и панель полей (Sections.cs + Fields.cs)
            RefreshEventList();
            UpdateSectionButtons();
            UpdatePanelVisibility();

            // 6. Полная перерисовка — создаём все Image для новой карты
            FullRedraw();

            Debug.WriteLine($"[EVENTS] Карта '{entry.Name}' загружена (Версия: {_selectedVersionId}). " +
                            $"Roofs={_events.Roofs.Count}, " +
                            $"TileChanges={_events.TileChanges.Count}, " +
                            $"Stairs={_events.Stairs.Count}, " +
                            $"Warps={_events.Warps.Count}, " +
                            $"Npcs={_events.Npcs.Count}");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[EVENTS] LoadMapByEntry error: {ex.Message}");
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   СЛОИ
    // ══════════════════════════════════════════════════════════════
    private void OnLayer1ToggleClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        _showLayer1 = Layer1Toggle.IsChecked == true;
        Layer1Toggle.Background = new Avalonia.Media.SolidColorBrush(
            Avalonia.Media.Color.Parse(_showLayer1 ? "#4CAF50" : "#555555"));
        RedrawCanvas();
    }

    private void OnLayer2ToggleClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        _showLayer2 = Layer2Toggle.IsChecked == true;
        Layer2Toggle.Background = new Avalonia.Media.SolidColorBrush(
            Avalonia.Media.Color.Parse(_showLayer2 ? "#4CAF50" : "#555555"));
        RedrawCanvas();
    }

    // ══════════════════════════════════════════════════════════════
    //   ЗУМ
    // ══════════════════════════════════════════════════════════════
    private void OnZoomChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (CanvasControl == null) return;
        if (_currentMap == null) return;

        // Передать новый зум в канвас — он сам пересчитает все Image
        CanvasControl.SetZoom(CurrentZoom());

        // Обновить зум в статус-баре
        PushStatusBar();
    }

    // ══════════════════════════════════════════════════════════════
    //   СОХРАНЕНИЕ СОБЫТИЙ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Сохраняет events.json, NPC_events.json, NPC_script.json.
    /// Вызывается из IEditor.Save() по кнопке SAVE в MainWindow.
    /// </summary>
    private void SaveCurrentEvents()
    {
        if (_currentMap == null)
        {
            Debug.WriteLine("[EVENTS] Нет активной карты.");
            return;
        }

        try
        {
            // Передаем версию, чтобы сохранить в нужную папку mapsetups
            EventsService.Save(_currentMap.Folder, _selectedVersionId, _events);
            Debug.WriteLine($"[EVENTS] Сохранено: {_currentMap.Folder} (Версия: {_selectedVersionId})");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[EVENTS] Save error: {ex.Message}");
        }
    }
}
