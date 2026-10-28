using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;

#pragma warning disable CS0618 // Игнорируем устаревший метод сохранения (работает нормально)

namespace RpgShinzoMaker.Desktop.Editors.TilesetImporter;

public partial class TilesetImporterView
{
    // Обновление плашки с путем при вводе имени
    private void OnPathFieldsChanged(object? sender, TextChangedEventArgs e)
    {
        if (PathPreviewText == null) return;

        string mapName = (MapNameBox?.Text ?? "").Trim();
        string tilesetName = (TilesetNameBox?.Text ?? "").Trim();

        if (string.IsNullOrEmpty(mapName)) mapName = "имя_карты";
        if (string.IsNullOrEmpty(tilesetName)) tilesetName = "имя_тайлсета";

        PathPreviewText.Text = $"Карта: data/maps/{mapName}/layout.json\nТайлсет: assets/tilesets/{tilesetName}.png";
    }

    private async void OnSaveTilesetClick(object? sender, RoutedEventArgs e)
    {
        if (_tilesetPixels == null) return;

        var tl = TopLevel.GetTopLevel(this);
        if (tl == null) return;

        var file = await tl.StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = "Save Tileset PNG",
            DefaultExtension = "png",
            FileTypeChoices = new[] { new FilePickerFileType("PNG") { Patterns = new[] { "*.png" } } }
        });

        if (file == null) return;

        try
        {
            var tilesetBmp = CreateBitmap(_tilesetPixels, 
                TilesetImportService.TilesetPixels, 
                TilesetImportService.TilesetPixels);
            
            using (var fs = File.Create(file.Path.LocalPath))
            {
                tilesetBmp.Save(fs);
            }

            StatusText.Text = $"Тайлсет сохранен: {Path.GetFileName(file.Path.LocalPath)}";
            StatusBarText.Text = StatusText.Text;
            SetStatus(StatusText.Text);
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Ошибка сохранения: {ex.Message}";
            StatusBarText.Text = StatusText.Text;
            SetStatus(StatusText.Text);
        }
    }

    private void OnSaveMapClick(object? sender, RoutedEventArgs e)
    {
        if (_imported == null || _tilesetPixels == null) return;

        SaveError.IsVisible = false;
        SaveOkBtn.IsEnabled = true;
        SaveCancelBtn.IsEnabled = true;
        
        // Инициализируем путь при открытии
        OnPathFieldsChanged(null, null!); 
        
        SaveOverlay.IsVisible = true;
    }

    private void OnSaveCancelClick(object? sender, RoutedEventArgs e)
    {
        SaveOverlay.IsVisible = false;
    }

    private void OnSaveOkClick(object? sender, RoutedEventArgs e)
    {
        if (_imported == null || _tilesetPixels == null) return;

        string mapName = (MapNameBox.Text ?? "").Trim();
        string tilesetName = (TilesetNameBox.Text ?? "").Trim();

        if (string.IsNullOrEmpty(mapName)) { ShowSaveError("Map name is empty"); return; }
        if (string.IsNullOrEmpty(tilesetName)) { ShowSaveError("Tileset name is empty"); return; }
        if (!Regex.IsMatch(mapName, @"^[A-Za-z0-9_\-]+$")) { ShowSaveError("Map name: only [A-Za-z0-9_-]"); return; }
        if (!Regex.IsMatch(tilesetName, @"^[A-Za-z0-9_\-]+$")) { ShowSaveError("Tileset name: only [A-Za-z0-9_-]"); return; }

        var mapsDir = ProjectPaths.MapsDir;

        string folder = mapName;
        int suffix = 1;
        while (Directory.Exists(Path.Combine(mapsDir, folder)))
        {
            folder = $"{mapName}{suffix}";
            suffix++;
        }

        var tilesetDir  = ProjectPaths.TilesetsDir;
        var tilesetPath = Path.Combine(tilesetDir, $"{tilesetName}.png");
        if (File.Exists(tilesetPath))
        {
            ShowSaveError($"Tileset '{tilesetName}.png' already exists");
            return;
        }

        try
        {
            Directory.CreateDirectory(tilesetDir);
            Directory.CreateDirectory(Path.Combine(mapsDir, folder));

            var tilesetBmp = CreateBitmap(_tilesetPixels, 
                TilesetImportService.TilesetPixels, 
                TilesetImportService.TilesetPixels);
            
            using (var fs = File.Create(tilesetPath))
            {
                tilesetBmp.Save(fs); 
            }

            int total = _imported.Width * _imported.Height;
            var gameMap = new GameMap
            {
                Name        = mapName,
                Folder      = folder,
                Width       = _imported.Width,
                Height      = _imported.Height,
                TilesetPath = $"assets/tilesets/{tilesetName}.png",
                MusicFile   = "",
                MusicVolume = 0.8f,
                AreasPath   = $"data/maps/{folder}/areas.json",

                Tiles    = _imported.TileIndices,
                Rot      = new int[total],
                MirrorX  = new bool[total],
                MirrorY  = new bool[total],
                Tiles2   = new int[total],
                Rot2     = new int[total],
                MirrorX2 = new bool[total],
                MirrorY2 = new bool[total],
                CellType = new int[total],
            };
            for (int i = 0; i < total; i++) gameMap.Tiles2[i] = -1;

            var layoutPath = ProjectPaths.LayoutFile(folder);
            MapJsonService.Save(layoutPath, gameMap);

            var areas = new Areas
            {
                StartX = 0, StartY = 0,
                EndX = _imported.Width - 1,
                EndY = _imported.Height - 1,
            };
            AreasService.Save(gameMap, areas);

            var entries = EntriesService.Load(ProjectPaths.EntriesFile);
            int newId = entries.Count > 0 ? entries.Max(e => e.Id) + 1 : 0;
            entries.Add(new MapEntry
            {
                Id          = newId,
                Folder      = folder,
                Name        = mapName,
                Music       = "",
                MusicVolume = 0.8f,
                Areas       = $"data/maps/{folder}/areas.json",
            });
            EntriesService.Save(ProjectPaths.EntriesFile, entries);

            StatusText.Text = $"Сохранено: data/maps/{folder}/layout.json + assets/tilesets/{tilesetName}.png";
            StatusBarText.Text = StatusText.Text;
            SetStatus(StatusText.Text);
            
            SaveOkBtn.IsEnabled = false;
            SaveCancelBtn.IsEnabled = false;
            Avalonia.Threading.Dispatcher.UIThread.Post(async () =>
            {
                await System.Threading.Tasks.Task.Delay(1200);
                SaveOverlay.IsVisible = false;
            });
        }
        catch (Exception ex)
        {
            ShowSaveError($"Error: {ex.Message}");
            Debug.WriteLine($"[IMPORT] Save error: {ex}");
        }
    }

    private void ShowSaveError(string msg)
    {
        SaveError.Text = msg;
        SaveError.Foreground = new SolidColorBrush(Color.Parse("#E74C3C"));
        SaveError.IsVisible = true;
    }
}