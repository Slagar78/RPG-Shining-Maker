// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.Dialogs.cs
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — ДИАЛОГИ.
/// New Map / Rename Map / Delete Map / Resize Map / Areas.
/// </summary>
public partial class MapEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   NEW MAP
    // ══════════════════════════════════════════════════════════════
    private void OnNewMapClick(object? sender, RoutedEventArgs e)
    {
        // Автоподбор имени: map00, map01, ...
        int idx = 0;
        string candidate = "map00";
        while (_mapEntries.Any(m => m.Name == candidate))
        {
            idx++;
            candidate = $"map{idx:00}";
        }

        NewMapName.Text = candidate;
        NewMapWidth.Value  = 20;
        NewMapHeight.Value = 15;

        NewMapStatus.IsVisible = false;
        NewMapOkBtn.IsEnabled = true;
        NewMapCancelBtn.IsEnabled = true;
        NewMapOverlay.IsVisible = true;
    }

    private void OnNewMapCancelClick(object? sender, RoutedEventArgs e)
    {
        NewMapOverlay.IsVisible = false;
    }

    /// <summary>Фильтр ввода имени карты: только [A-Za-z0-9_-].</summary>
    private void OnNewMapNameTextInput(object? sender, TextInputEventArgs e)
    {
        if (string.IsNullOrEmpty(e.Text)) return;

        foreach (char c in e.Text)
        {
            bool ok = (c >= 'A' && c <= 'Z') ||
                      (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') ||
                       c == '_' || c == '-';

            if (!ok)
            {
                e.Handled = true;
                return;
            }
        }
    }

    private void OnNewMapOkClick(object? sender, RoutedEventArgs e)
    {
        string name = (NewMapName.Text ?? "").Trim();
        if (string.IsNullOrEmpty(name))
        {
            ShowNewMapStatus("Name cannot be empty", isError: true);
            return;
        }

        if (!Regex.IsMatch(name, @"^[A-Za-z0-9_\-]+$"))
        {
            ShowNewMapStatus("Only English letters, digits, '_' and '-' allowed", isError: true);
            return;
        }

        int mapW = (int)(NewMapWidth.Value ?? 20);
        int mapH = (int)(NewMapHeight.Value ?? 15);

        if (mapW < 4 || mapH < 4)
        {
            ShowNewMapStatus("Map size must be at least 4x4", isError: true);
            return;
        }
        if (mapW > 128 || mapH > 128)
        {
            ShowNewMapStatus("Map size cannot exceed 128x128", isError: true);
            return;
        }

        // Уникальный folder
        string folder = name;
        int suffix = 1;
        var mapsDir = ProjectPaths.MapsDir;
        while (Directory.Exists(Path.Combine(mapsDir, folder)) ||
               _mapEntries.Any(m => m.Folder == folder))
        {
            folder = $"{name}{suffix}";
            suffix++;
        }

        try
        {
            string defaultTileset = GetDefaultTilesetPath();
            string defaultMusic   = GetDefaultMusicPath();

            var map = new GameMap
            {
                Name        = name,
                Folder      = folder,
                Width       = mapW,
                Height      = mapH,
                TilesetPath = defaultTileset,
                MusicFile   = defaultMusic,
                MusicVolume = 0.8f,
                AreasPath   = $"data/maps/{folder}/areas.json",

                Tiles    = new int[mapW * mapH],
                Rot      = new int[mapW * mapH],
                MirrorX  = new bool[mapW * mapH],
                MirrorY  = new bool[mapW * mapH],

                Tiles2   = new int[mapW * mapH],
                Rot2     = new int[mapW * mapH],
                MirrorX2 = new bool[mapW * mapH],
                MirrorY2 = new bool[mapW * mapH],

                CellType = new int[mapW * mapH],
            };

            // Layer 1 = 0, Layer 2 = -1 (пусто)
            for (int i = 0; i < map.TotalCells; i++)
            {
                map.Tiles[i]  = 0;
                map.Tiles2[i] = -1;
            }

            // Сохранить layout.json
            var layoutPath = ProjectPaths.LayoutFile(folder);
            MapJsonService.Save(layoutPath, map);

            // Areas = вся карта
            var newArea = new Areas
            {
                StartX = 0,
                StartY = 0,
                EndX   = mapW - 1,
                EndY   = mapH - 1,
            };
            AreasService.Save(map, newArea);

            // Добавить в entries
            var entry = new MapEntry
            {
                Folder      = folder,
                Name        = name,
                Music       = defaultMusic,
                MusicVolume = 0.8f,
                Areas       = $"data/maps/{folder}/areas.json",
            };
            _mapEntries.Add(entry);

            EntriesService.Save(ProjectPaths.EntriesFile, _mapEntries);

            // Обновить селектор и выбрать новую карту
            MapSelector.ItemsSource = null;
            MapSelector.ItemsSource = _mapEntries.Select(m => m.Name).ToList();
            MapSelector.SelectedIndex = _mapEntries.Count - 1;

            Debug.WriteLine($"[NEW MAP] '{name}' ({mapW}x{mapH}), folder={folder}");

            // Success + закрыть через 1.2 сек
            ShowNewMapStatus($"Success! Map '{name}' created ({mapW}x{mapH})", isError: false);
            NewMapOkBtn.IsEnabled = false;
            NewMapCancelBtn.IsEnabled = false;

            Avalonia.Threading.Dispatcher.UIThread.Post(async () =>
            {
                await System.Threading.Tasks.Task.Delay(1200);
                NewMapOverlay.IsVisible = false;
            });
        }
        catch (Exception ex)
        {
            ShowNewMapStatus($"Error: {ex.Message}", isError: true);
            Debug.WriteLine($"[NEW MAP] Ошибка: {ex}");
        }
    }

    private void ShowNewMapStatus(string msg, bool isError)
    {
        NewMapStatus.Text = msg;
        NewMapStatus.Foreground = new SolidColorBrush(
            Color.Parse(isError ? "#E74C3C" : "#4CAF50"));
        NewMapStatus.IsVisible = true;
    }

    // ══════════════════════════════════════════════════════════════
    //   RENAME MAP
    // ══════════════════════════════════════════════════════════════
    private void OnRenameMapClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;
        if (MapSelector.SelectedIndex < 0) return;

        RenameMapCurrent.Text = $"Current: {_currentMap.Name}";
        RenameMapName.Text    = _currentMap.Name;

        RenameMapStatus.IsVisible = false;
        RenameMapOkBtn.IsEnabled = true;
        RenameMapCancelBtn.IsEnabled = true;
        RenameMapOverlay.IsVisible = true;
    }

    private void OnRenameMapCancelClick(object? sender, RoutedEventArgs e)
    {
        RenameMapOverlay.IsVisible = false;
    }

    private void OnRenameMapNameTextInput(object? sender, TextInputEventArgs e)
    {
        if (string.IsNullOrEmpty(e.Text)) return;

        foreach (char c in e.Text)
        {
            bool ok = (c >= 'A' && c <= 'Z') ||
                      (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') ||
                       c == '_' || c == '-';

            if (!ok)
            {
                e.Handled = true;
                return;
            }
        }
    }

    private void OnRenameMapOkClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;

        int idx = MapSelector.SelectedIndex;
        if (idx < 0 || idx >= _mapEntries.Count) return;

        string newName = (RenameMapName.Text ?? "").Trim();

        // Валидация
        if (string.IsNullOrEmpty(newName))
        {
            ShowRenameMapStatus("Name cannot be empty", isError: true);
            return;
        }
        if (newName.Length > 30)
        {
            ShowRenameMapStatus("Name cannot exceed 30 characters", isError: true);
            return;
        }
        if (!Regex.IsMatch(newName, @"^[A-Za-z0-9_\-]+$"))
        {
            ShowRenameMapStatus("Only English letters, digits, '_' and '-' allowed", isError: true);
            return;
        }
        if (newName == _currentMap.Name)
        {
            ShowRenameMapStatus("Name is the same as current", isError: true);
            return;
        }

        var mapsDir = ProjectPaths.MapsDir;
        string newFolder = newName;
        string oldFolder = _currentMap.Folder;

        if (newFolder != oldFolder &&
            Directory.Exists(Path.Combine(mapsDir, newFolder)))
        {
            ShowRenameMapStatus($"Folder '{newFolder}' already exists", isError: true);
            return;
        }

        string oldName = _currentMap.Name;

        try
        {
            var oldPath = Path.Combine(mapsDir, oldFolder);
            var newPath = Path.Combine(mapsDir, newFolder);

            // 1. Переименовать папку на диске
            if (Directory.Exists(oldPath))
                Directory.Move(oldPath, newPath);

            // 2. Обновить поля карты
            _currentMap.Folder    = newFolder;
            _currentMap.Name      = newName;
            _currentMap.AreasPath = $"data/maps/{newFolder}/areas.json";

            // 3. Обновить запись в entries
            _mapEntries[idx].Folder = newFolder;
            _mapEntries[idx].Name   = newName;
            _mapEntries[idx].Areas  = $"data/maps/{newFolder}/areas.json";

            EntriesService.Save(ProjectPaths.EntriesFile, _mapEntries);

            // 5. Сохранить layout.json в новую папку
            var layoutPath = ProjectPaths.LayoutFile(newFolder);
            MapJsonService.Save(layoutPath, _currentMap);
            AreasService.Save(_currentMap, _area);

            // 6. Обновить селектор
            MapSelector.ItemsSource = null;
            MapSelector.ItemsSource = _mapEntries.Select(m => m.Name).ToList();
            MapSelector.SelectedIndex = idx;

            Debug.WriteLine($"[RENAME MAP] '{oldName}' -> '{newName}' (folder: {oldFolder} -> {newFolder})");

            ShowRenameMapStatus($"Success! Renamed to '{newName}'", isError: false);
            RenameMapOkBtn.IsEnabled = false;
            RenameMapCancelBtn.IsEnabled = false;

            Avalonia.Threading.Dispatcher.UIThread.Post(async () =>
            {
                await System.Threading.Tasks.Task.Delay(1200);
                RenameMapOverlay.IsVisible = false;
            });
        }
        catch (Exception ex)
        {
            ShowRenameMapStatus($"Error: {ex.Message}", isError: true);
            Debug.WriteLine($"[RENAME MAP] Ошибка: {ex}");
        }
    }

    private void ShowRenameMapStatus(string msg, bool isError)
    {
        RenameMapStatus.Text = msg;
        RenameMapStatus.Foreground = new SolidColorBrush(
            Color.Parse(isError ? "#E74C3C" : "#4CAF50"));
        RenameMapStatus.IsVisible = true;
    }

    // ══════════════════════════════════════════════════════════════
    //   DELETE MAP
    // ══════════════════════════════════════════════════════════════
    private void OnDeleteMapClick(object? sender, RoutedEventArgs e)
    {
        if (_mapEntries.Count <= 1)
        {
            Debug.WriteLine("[DELETE MAP] Нельзя удалить последнюю карту");
            return;
        }

        int idx = MapSelector.SelectedIndex;
        if (idx < 0 || idx >= _mapEntries.Count) return;

        var entry = _mapEntries[idx];
        DeleteMapText.Text = $"Delete map \"{entry.Name}\" ({entry.Folder})?";
        DeleteMapOverlay.IsVisible = true;
    }

    private void OnDeleteMapNoClick(object? sender, RoutedEventArgs e)
    {
        DeleteMapOverlay.IsVisible = false;
    }

    private void OnDeleteMapYesClick(object? sender, RoutedEventArgs e)
    {
        int idx = MapSelector.SelectedIndex;
        if (idx < 0 || idx >= _mapEntries.Count) return;

        var entry = _mapEntries[idx];

        try
        {
            // 1. Удалить папку с файлами
            var folderPath = Path.Combine(ProjectPaths.MapsDir, entry.Folder);
            if (Directory.Exists(folderPath))
                Directory.Delete(folderPath, recursive: true);

            // 2. Убрать из списка
            _mapEntries.RemoveAt(idx);

            // 3. Сохранить entries.json
            EntriesService.Save(ProjectPaths.EntriesFile, _mapEntries);

            // 4. Обновить селектор
            MapSelector.ItemsSource = null;
            MapSelector.ItemsSource = _mapEntries.Select(m => m.Name).ToList();

            // 5. Выбрать предыдущую или первую
            int newIdx = Math.Min(idx, _mapEntries.Count - 1);
            MapSelector.SelectedIndex = newIdx;

            // 6. Загрузить выбранную карту
            LoadMapByEntry(_mapEntries[newIdx]);

            Debug.WriteLine($"[DELETE MAP] Удалено '{entry.Name}' ({entry.Folder})");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[DELETE MAP] Ошибка: {ex.Message}");
        }

        DeleteMapOverlay.IsVisible = false;
    }

    // ══════════════════════════════════════════════════════════════
    //   RESIZE MAP
    // ══════════════════════════════════════════════════════════════
    private void OnResizeMapClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;

        ResizeMapCurrent.Text = $"Current: {_currentMap.Width} x {_currentMap.Height}";
        ResizeMapWidth.Value  = _currentMap.Width;
        ResizeMapHeight.Value = _currentMap.Height;

        ResizeMapStatus.IsVisible = false;
        ResizeMapOkBtn.IsEnabled = true;
        ResizeMapCancelBtn.IsEnabled = true;
        ResizeMapOverlay.IsVisible = true;
    }

    private void OnResizeMapCancelClick(object? sender, RoutedEventArgs e)
    {
        ResizeMapOverlay.IsVisible = false;
    }

    private void OnResizeMapOkClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;

        int newW = (int)(ResizeMapWidth.Value ?? 4);
        int newH = (int)(ResizeMapHeight.Value ?? 4);

        if (newW < 4 || newH < 4)
        {
            ShowResizeMapStatus("Size must be at least 4x4", isError: true);
            return;
        }
        if (newW > 128 || newH > 128)
        {
            ShowResizeMapStatus("Size cannot exceed 128x128", isError: true);
            return;
        }
        if (newW == _currentMap.Width && newH == _currentMap.Height)
        {
            ShowResizeMapStatus("Size is the same as current", isError: true);
            return;
        }

        try
        {
            var oldMap = _currentMap;

            var newMap = new GameMap
            {
                Name        = oldMap.Name,
                Folder      = oldMap.Folder,
                Width       = newW,
                Height      = newH,
                TilesetPath = oldMap.TilesetPath,
                MusicFile   = oldMap.MusicFile,
                MusicVolume = oldMap.MusicVolume,
                AreasPath   = oldMap.AreasPath,

                Tiles    = new int[newW * newH],
                Rot      = new int[newW * newH],
                MirrorX  = new bool[newW * newH],
                MirrorY  = new bool[newW * newH],

                Tiles2   = new int[newW * newH],
                Rot2     = new int[newW * newH],
                MirrorX2 = new bool[newW * newH],
                MirrorY2 = new bool[newW * newH],

                CellType = new int[newW * newH],
            };

            for (int i = 0; i < newMap.TotalCells; i++)
            {
                newMap.Tiles[i]  = 0;
                newMap.Tiles2[i] = -1;
            }

            // Копируем пересекающуюся область
            int copyW = Math.Min(oldMap.Width,  newW);
            int copyH = Math.Min(oldMap.Height, newH);

            for (int x = 0; x < copyW; x++)
            for (int y = 0; y < copyH; y++)
            {
                int oldIdx = x * oldMap.Height + y;
                int newIdx = x * newMap.Height + y;

                newMap.Tiles[newIdx]   = oldMap.Tiles[oldIdx];
                newMap.Rot[newIdx]     = oldMap.Rot[oldIdx];
                newMap.MirrorX[newIdx] = oldMap.MirrorX[oldIdx];
                newMap.MirrorY[newIdx] = oldMap.MirrorY[oldIdx];

                newMap.Tiles2[newIdx]   = oldMap.Tiles2[oldIdx];
                newMap.Rot2[newIdx]     = oldMap.Rot2[oldIdx];
                newMap.MirrorX2[newIdx] = oldMap.MirrorX2[oldIdx];
                newMap.MirrorY2[newIdx] = oldMap.MirrorY2[oldIdx];

                newMap.CellType[newIdx] = oldMap.CellType[oldIdx];
            }

            // Обрезать areas, если не влезают
            if (_area.EndX > newW - 1) _area.EndX = newW - 1;
            if (_area.EndY > newH - 1) _area.EndY = newH - 1;
            if (_area.StartX > _area.EndX) _area.StartX = 0;
            if (_area.StartY > _area.EndY) _area.StartY = 0;

            // Сохранить на диск
            var layoutPath = ProjectPaths.LayoutFile(newMap.Folder);
            MapJsonService.Save(layoutPath, newMap);
            AreasService.Save(newMap, _area);

            _currentMap = newMap;

            MapSizeText.Text = $"{newMap.Width}×{newMap.Height}";

            // Перезагрузить тайлсет и канвас
            var tilesetName = Path.GetFileName(newMap.TilesetPath);
            var tilesetPath = Path.Combine(ProjectPaths.TilesetsDir, tilesetName);
            if (File.Exists(tilesetPath))
                LoadTileset(tilesetPath);

            MapCanvasControl.SetMap(newMap, _tiles.Select(t => t.Image).ToList());
            // Размеры изменились — индексы ячеек теперь другие, история неактуальна
            ClearHistory();

            Debug.WriteLine($"[RESIZE MAP] {newW}x{newH} для '{newMap.Name}'");

            ShowResizeMapStatus($"Success! Map is now {newW}x{newH}", isError: false);
            ResizeMapOkBtn.IsEnabled = false;
            ResizeMapCancelBtn.IsEnabled = false;

            Avalonia.Threading.Dispatcher.UIThread.Post(async () =>
            {
                await System.Threading.Tasks.Task.Delay(1200);
                ResizeMapOverlay.IsVisible = false;
            });
        }
        catch (Exception ex)
        {
            ShowResizeMapStatus($"Error: {ex.Message}", isError: true);
            Debug.WriteLine($"[RESIZE MAP] Ошибка: {ex}");
        }
    }

    private void ShowResizeMapStatus(string msg, bool isError)
    {
        ResizeMapStatus.Text = msg;
        ResizeMapStatus.Foreground = new SolidColorBrush(
            Color.Parse(isError ? "#E74C3C" : "#4CAF50"));
        ResizeMapStatus.IsVisible = true;
    }

    // ══════════════════════════════════════════════════════════════
    //   AREAS
    // ══════════════════════════════════════════════════════════════
    private void OnAreasClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;

        AreaStartX.Value = _area.StartX;
        AreaStartY.Value = _area.StartY;
        AreaWidth.Value  = _area.Width;
        AreaHeight.Value = _area.Height;

        AreaStartX.Maximum = _currentMap.Width  - 2;
        AreaStartY.Maximum = _currentMap.Height - 2;
        AreaWidth.Maximum  = _currentMap.Width;
        AreaHeight.Maximum = _currentMap.Height;

        AreaError.IsVisible = false;
        AreasOverlay.IsVisible = true;
    }

    private void OnAreasCancelClick(object? sender, RoutedEventArgs e)
    {
        AreasOverlay.IsVisible = false;
    }

    private void OnAreasOkClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;

        int sx = (int)(AreaStartX.Value ?? 0);
        int sy = (int)(AreaStartY.Value ?? 0);
        int w  = (int)(AreaWidth.Value  ?? 2);
        int h  = (int)(AreaHeight.Value ?? 2);

        string? error = null;

        if (sx < 0 || sy < 0)
            error = "Start X / Y must be >= 0";
        else if (w < 2 || h < 2)
            error = "Width / Height must be at least 2";
        else if (sx + w > _currentMap.Width)
            error = $"Start X + Width exceeds map width ({_currentMap.Width})";
        else if (sy + h > _currentMap.Height)
            error = $"Start Y + Height exceeds map height ({_currentMap.Height})";

        if (error != null)
        {
            AreaError.Text = error;
            AreaError.IsVisible = true;
            return;
        }

        _area.StartX = sx;
        _area.StartY = sy;
        _area.EndX   = sx + w - 1;
        _area.EndY   = sy + h - 1;

        Debug.WriteLine($"[AREAS] Установлено: start=({_area.StartX},{_area.StartY}) end=({_area.EndX},{_area.EndY})");

        AreasOverlay.IsVisible = false;
    }

    // ══════════════════════════════════════════════════════════════
    //   ВСПОМОГАТЕЛЬНЫЕ — пути по умолчанию для новой карты
    // ══════════════════════════════════════════════════════════════
    /// <summary>Первый .png в assets/tilesets — используется для новых карт.</summary>
    private string GetDefaultTilesetPath()
    {
        var dir = ProjectPaths.TilesetsDir;
        if (Directory.Exists(dir))
        {
            var files = Directory.GetFiles(dir, "*.png");
            if (files.Length > 0)
                return "assets/tilesets/" + Path.GetFileName(files[0]);
        }
        return "assets/tilesets/tileset01.png";
    }

    /// <summary>Первый .mp3 в assets/sounds — используется для новых карт.</summary>
    private string GetDefaultMusicPath()
    {
        if (_musicFiles.Count > 0)
            return "assets/sounds/" + _musicFiles[0];
        return "";
    }
}