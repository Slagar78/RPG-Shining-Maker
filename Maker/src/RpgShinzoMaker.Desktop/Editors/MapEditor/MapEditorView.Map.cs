// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.Map.cs
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Часть MapEditorView — ЖИЗНЕННЫЙ ЦИКЛ КАРТЫ.
/// Здесь: загрузка проекта и карт, комбо Карта/Музыка, зум, сохранение.
/// </summary>
public partial class MapEditorView
{
    // ══════════════════════════════════════════════════════════════
    //   ЗАГРУЗКА ПРОЕКТА
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Ищет корень проекта, читает entries.json, наполняет список карт и музыки.
    /// </summary>
    private void LoadProject()
    {
        try
        {
            var root = FindProjectRoot();
            Debug.WriteLine($"[TEST] root = {root ?? "NULL"}");
            if (root == null) { Debug.WriteLine("Корень не найден"); return; }

            ProjectPaths.Root = root;

            _mapEntries = EntriesService.Load(ProjectPaths.EntriesFile);
            Debug.WriteLine($"[TEST] entries: {_mapEntries.Count}");
            if (_mapEntries.Count == 0) { Debug.WriteLine("entries пуст"); return; }

            MapSelector.ItemsSource = null;
            MapSelector.ItemsSource = _mapEntries.Select(e => e.Name).ToList();

            PopulateMusicList();

            MapSelector.SelectedIndex = 0;

            Debug.WriteLine("[TEST] Проект загружен");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[TEST] Ошибка: {ex.Message}\n{ex.StackTrace}");
        }
    }

    /// <summary>
    /// Ищет корень проекта — идёт вверх по дереву каталогов,
    /// пока не найдёт папку data/maps.
    /// </summary>
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
    //   ЗАГРУЗКА КОНКРЕТНОЙ КАРТЫ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Читает layout.json карты, нарезает её тайлсет,
    /// читает areas.json, обновляет канвас.
    /// </summary>
    private void LoadMapByEntry(MapEntry entry)
    {
        try
        {
            var layoutPath = ProjectPaths.LayoutFile(entry.Folder);
            if (!File.Exists(layoutPath)) return;

            var map = MapJsonService.Load(layoutPath);
            if (map == null) return;

            map.Name        = entry.Name;
            map.Folder      = entry.Folder;
            map.MusicFile   = entry.Music;
            map.MusicVolume = entry.MusicVolume;
            map.AreasPath   = entry.Areas;

            var tilesetName = Path.GetFileName(map.TilesetPath);
            var tilesetPath = Path.Combine(ProjectPaths.TilesetsDir, tilesetName);
            if (!File.Exists(tilesetPath)) return;

            // Нарезать тайлсет (метод из Palette.cs)
            LoadTileset(tilesetPath);

            MapCanvasControl.SetMap(map, _tiles.Select(t => t.Image).ToList());
            _currentMap = map;
            MapCanvasControl.CurrentLayer = _currentLayer;

            // Прочитать areas.json (границы основной области карты)
            _area = AreasService.Load(map);

            MapSizeText.Text    = $"{map.Width}×{map.Height}";
            MapTilesetText.Text = tilesetName;

            // Обновить комбо музыки без рекурсии
            _suppressMusicChange = true;
            var musicName = string.IsNullOrEmpty(entry.Music)
                ? null
                : Path.GetFileName(entry.Music);

            if (musicName != null && _musicFiles.Contains(musicName))
                MapMusicSelector.SelectedItem = musicName;
            else
                MapMusicSelector.SelectedIndex = -1;
            _suppressMusicChange = false;

            // Сбросить историю — карта новая
            ClearHistory();
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[MAP] {ex.Message}");
        }
    }

    /// <summary>
    /// Обработчик комбо «Карта» — загрузить выбранную карту.
    /// </summary>
    private void OnMapSelectorChanged(object? sender, SelectionChangedEventArgs e)
    {
        int idx = MapSelector.SelectedIndex;
        if (idx < 0 || idx >= _mapEntries.Count) return;

        LoadMapByEntry(_mapEntries[idx]);
    }

    // ══════════════════════════════════════════════════════════════
    //   СОХРАНЕНИЕ КАРТЫ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Сохраняет:
    ///   1) layout.json    — тайлы, повороты, коллизии
    ///   2) tile_types/... — типы тайлов текущего тайлсета
    ///   3) areas.json     — границы основной области
    ///   4) entries.json   — музыка, громкость, пути
    /// Вызывается из IEditor.Save() по кнопке SAVE в тулбаре MainWindow.
    /// </summary>
    private void SaveCurrentMap()
    {
        if (_currentMap == null)
        {
            Debug.WriteLine("[SAVE] Нет активной карты для сохранения.");
            return;
        }

        try
        {
            // 1. layout.json
            var layoutPath = ProjectPaths.LayoutFile(_currentMap.Folder);
            MapJsonService.Save(layoutPath, _currentMap);
            Debug.WriteLine($"[SAVE] Карта сохранена: {layoutPath}");

            // 2. tile_types/{tileset}.json
            if (!string.IsNullOrEmpty(_currentMap.TilesetPath))
            {
                var tilesetName = Path.GetFileName(_currentMap.TilesetPath);
                var tilesetAbs  = Path.Combine(ProjectPaths.TilesetsDir, tilesetName);
                TileTypesService.Save(tilesetAbs, _tileTypes);
            }

            // 3. areas.json
            AreasService.Save(_currentMap, _area);

            // 4. entries.json
            EntriesService.Save(ProjectPaths.EntriesFile, _mapEntries);
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[SAVE] Ошибка сохранения: {ex.Message}");
        }
    }

    // ══════════════════════════════════════════════════════════════
    //   МУЗЫКА
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Собирает список .mp3 из assets/sounds и наполняет комбо.
    /// </summary>
    private void PopulateMusicList()
    {
        _musicFiles.Clear();

        var dir = ProjectPaths.SoundsDir;
        if (Directory.Exists(dir))
        {
            foreach (var f in Directory.GetFiles(dir, "*.mp3"))
                _musicFiles.Add(Path.GetFileName(f));

            _musicFiles.Sort(StringComparer.OrdinalIgnoreCase);
        }

        MapMusicSelector.ItemsSource = null;
        MapMusicSelector.ItemsSource = _musicFiles;
    }

    /// <summary>
    /// Обработчик комбо «Музыка» — обновляет поле Music в MapEntry
    /// и в текущей карте (чтобы SAVE записал и в layout.json).
    /// </summary>
    private void OnMapMusicChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (_suppressMusicChange) return;

        int idx = MapSelector.SelectedIndex;
        if (idx < 0 || idx >= _mapEntries.Count) return;
        if (MapMusicSelector.SelectedItem is not string name) return;

        string musicPath = "assets/sounds/" + name;
        _mapEntries[idx].Music = musicPath;

        if (_currentMap != null)
            _currentMap.MusicFile = musicPath;
    }

    // ══════════════════════════════════════════════════════════════
    //   ЗУМ
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Обработчик комбо Zoom — передаёт коэффициент в MapCanvas
    /// и обновляет зум в статус-баре.
    /// </summary>
    private void OnZoomChanged(object? sender, SelectionChangedEventArgs e)
    {
        // Во время инициализации XAML контролы ещё не готовы — выходим тихо
        if (MapCanvasControl == null) return;
        if (ZoomSelector?.SelectedItem is not ComboBoxItem item) return;
        if (item.Content is not string text) return;

        double zoom = text switch
        {
            "1/4x" => 0.25,
            "1/2x" => 0.5,
            "1x"   => 1.0,
            "2x"   => 2.0,
            "4x"   => 4.0,
            _      => 1.0
        };

        MapCanvasControl.SetZoom(zoom);

        // Обновить текст в статус-баре MainWindow (зум + последняя позиция)
        PushStatusBar();
    }

    // ══════════════════════════════════════════════════════════════
    //   LOAD TILESET ИЗ ДИАЛОГА
    // ══════════════════════════════════════════════════════════════
    /// <summary>
    /// Кнопка «Load Tileset» — открывает диалог выбора PNG,
    /// нарезает его и обновляет канвас (если карта уже открыта).
    /// </summary>
    private async void OnLoadTilesetClick(object? sender, RoutedEventArgs e)
    {
        var options = new FilePickerOpenOptions
        {
            Title = "Выберите тайлсет",
            AllowMultiple = false,
            FileTypeFilter = new[]
            {
                new FilePickerFileType("PNG изображения") { Patterns = new[] { "*.png" } }
            }
        };

        try
        {
            var defaultDir = Path.GetFullPath(
                Path.Combine(Environment.CurrentDirectory, "..", "..", "..", "assets", "tilesets"));

            if (Directory.Exists(defaultDir))
            {
                var topLevel = TopLevel.GetTopLevel(this);
                if (topLevel != null)
                {
                    var folder = await topLevel.StorageProvider
                        .TryGetFolderFromPathAsync(defaultDir);
                    if (folder != null) options.SuggestedStartLocation = folder;
                }
            }
        }
        catch (Exception ex) { Debug.WriteLine($"Путь: {ex.Message}"); }

        var tl = TopLevel.GetTopLevel(this);
        if (tl == null) return;

        var files = await tl.StorageProvider.OpenFilePickerAsync(options);
        if (files.Count == 0) return;

        var newPath = files[0].Path.LocalPath;

        // Нарезать тайлсет (метод из Palette.cs)
        LoadTileset(newPath);

        // Обновить канвас — иначе старые CroppedBitmap ссылаются на освобождённый Bitmap
        if (_currentMap != null)
        {
            _currentMap.TilesetPath = "assets/tilesets/" + Path.GetFileName(newPath);
            MapCanvasControl.SetMap(_currentMap, _tiles.Select(t => t.Image).ToList());
            MapTilesetText.Text = Path.GetFileName(newPath);

            // Сменился тайлсет — индексы теперь ссылаются на другие картинки,
            // старая история undo визуально бессмысленна
            ClearHistory();
        }
    }
}
