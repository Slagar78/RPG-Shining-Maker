using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;

namespace RpgShinzoMaker.Desktop.Views;

public partial class MainWindow : Window
{
    private Bitmap? _sourceTileset;

    public MainWindow()
    {
        InitializeComponent();
    }

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

        // Путь по умолчанию: ../assets/tilesets от рабочей директории
        try
        {
            var defaultDir = Path.GetFullPath(
                Path.Combine(Environment.CurrentDirectory, "..", "..", "..", "assets", "tilesets"));

            if (Directory.Exists(defaultDir))
            {
                var folder = await StorageProvider.TryGetFolderFromPathAsync(defaultDir);
                if (folder != null)
                    options.SuggestedStartLocation = folder;
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Не удалось установить путь по умолчанию: {ex.Message}");
        }

        var files = await StorageProvider.OpenFilePickerAsync(options);
        if (files.Count == 0) return;

        var path = files[0].Path.LocalPath;
        LoadTileset(path);
    }

private void LoadTileset(string path)
{
    try
    {
        _sourceTileset?.Dispose();
        _sourceTileset = new Bitmap(path);

        const int tileSize = 48;
        int cols = _sourceTileset.PixelSize.Width / tileSize;
        int rows = _sourceTileset.PixelSize.Height / tileSize;

        var tiles = new List<CroppedBitmap>(cols * rows);

        for (int y = 0; y < rows; y++)
        {
            for (int x = 0; x < cols; x++)
            {
                var crop = new CroppedBitmap(
                    _sourceTileset,
                    new PixelRect(x * tileSize, y * tileSize, tileSize, tileSize));
                tiles.Add(crop);
            }
        }

        PaletteList.ItemsSource = tiles;

        // Проверка: должно быть ровно 1024 тайла
        const int expectedTiles = 1024;
        bool isValid = tiles.Count == expectedTiles;

        PaletteStatus.Text = isValid ? "✓" : "✗";
        PaletteStatus.Foreground = isValid 
            ? Avalonia.Media.Brushes.LightGreen 
            : Avalonia.Media.Brushes.IndianRed;

        PaletteInfo.Text = $"Тайлов: {tiles.Count}";
    }
    catch (Exception ex)
    {
        Debug.WriteLine($"Ошибка загрузки тайлсета: {ex.Message}");
    }
}
}