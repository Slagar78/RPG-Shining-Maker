using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;

namespace RpgShinzoMaker.Desktop.Views;

// ─── Модель одного тайла в палитре ───────────
public class TileItem : INotifyPropertyChanged
{
    public CroppedBitmap Image { get; init; } = null!;
    public int Index { get; init; }

    private int _tileType;
    public int TileType
    {
        get => _tileType;
        set
        {
            _tileType = value;
            OnPropertyChanged(nameof(TileType));
            OnPropertyChanged(nameof(TileColor));
        }
    }

    private bool _showType;
    public bool ShowType
    {
        get => _showType;
        set
        {
            _showType = value;
            OnPropertyChanged(nameof(ShowType));
        }
    }

    public IBrush TileColor => TileType switch
    {
        0 => new SolidColorBrush(Color.Parse("#4CAF50")),
        1 => new SolidColorBrush(Color.Parse("#E74C3C")),
        2 => new SolidColorBrush(Color.Parse("#3498DB")),
        3 => new SolidColorBrush(Color.Parse("#E67E22")),
        _ => Brushes.Gray
    };

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged(string name) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}

public partial class MainWindow : Window
{
    private const int PaletteCols = 8;
    private const int TileSize    = 48;

    private Bitmap? _sourceTileset;

    // ВАЖНО: не readonly — будем пересоздавать список при каждой загрузке
    private List<TileItem> _tiles = new();
    private int[] _tileTypes = Array.Empty<int>();

    private bool _gridMode;
    private bool _isModeB;
    private int _currentTileType = 0;

    public MainWindow()
    {
        InitializeComponent();
        UpdateTypeIconSelection();
    }

    // ─── Загрузка тайлсета ──────────────────
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

        LoadTileset(files[0].Path.LocalPath);
    }

    private void LoadTileset(string path)
    {
        try
        {
            _sourceTileset?.Dispose();
            _sourceTileset = new Bitmap(path);

            int cols = _sourceTileset.PixelSize.Width / TileSize;
            int rows = _sourceTileset.PixelSize.Height / TileSize;

            int strips = cols / PaletteCols;

            // ─── Создаём НОВЫЙ список, а не чистим старый ───
            var newTiles = new List<TileItem>();

            int idx = 0;
            for (int strip = 0; strip < strips; strip++)
            {
                int startCol = strip * PaletteCols;
                int endCol   = startCol + PaletteCols;

                for (int r = 0; r < rows; r++)
                {
                    for (int c = startCol; c < endCol; c++)
                    {
                        var crop = new CroppedBitmap(
                            _sourceTileset,
                            new PixelRect(c * TileSize, r * TileSize, TileSize, TileSize));

                        newTiles.Add(new TileItem
                        {
                            Image    = crop,
                            Index    = idx,
                            TileType = 0,
                            ShowType = _isModeB
                        });
                        idx++;
                    }
                }
            }

            // ─── Заменяем старый список новым ───
            _tiles = newTiles;
            _tileTypes = new int[_tiles.Count];

            // Отсоединяем → присоединяем, чтобы Avalonia гарантированно обновила палитру
            PaletteList.ItemsSource = null;
            PaletteList.ItemsSource = _tiles;

            const int expectedTiles = 1024;
            bool isValid = _tiles.Count == expectedTiles;

            PaletteStatus.Text = isValid ? "✓" : "✗";
            PaletteStatus.Foreground = isValid ? Brushes.LightGreen : Brushes.IndianRed;
            PaletteInfo.Text = $"Тайлов: {_tiles.Count}";

            Debug.WriteLine($"Загружено {_tiles.Count} тайлов (strip-based, {strips} полос по {PaletteCols} столбцов)");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Ошибка загрузки тайлсета: {ex.Message}");
        }
    }

    // ─── Режимы A / B ───────────────────────
    private void OnModeAClick(object? sender, RoutedEventArgs e)
    {
        ModeA.IsChecked = true;
        ModeB.IsChecked = false;
        SetModeB(false);
    }

    private void OnModeBClick(object? sender, RoutedEventArgs e)
    {
        ModeA.IsChecked = false;
        ModeB.IsChecked = true;
        SetModeB(true);
    }

    private void SetModeB(bool isB)
    {
        _isModeB = isB;
        foreach (var tile in _tiles)
            tile.ShowType = isB;

        Debug.WriteLine(isB ? "Режим B (типы тайлов)" : "Режим A (рисование)");
    }

    // ─── Grid Mode ──────────────────────────
    private void OnGridModeClick(object? sender, RoutedEventArgs e)
    {
        _gridMode = !_gridMode;
        GridModeToggle.Content = _gridMode ? "ON" : "OFF";
        Debug.WriteLine($"Grid Mode: {_gridMode}");
    }

    // ─── Выбор типа тайла (кружки внизу) ────
    private void OnTileTypeClick(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Border border || border.Tag is not string tagStr) return;
        if (!int.TryParse(tagStr, out int type)) return;

        _currentTileType = type;
        UpdateTypeIconSelection();
        Debug.WriteLine($"Активный тип: {type}");
    }

    private void UpdateTypeIconSelection()
    {
        var icons = new[] { TypeIcon0, TypeIcon1, TypeIcon2, TypeIcon3 };
        for (int i = 0; i < icons.Length; i++)
        {
            bool selected = i == _currentTileType;
            icons[i].BorderBrush = selected ? Brushes.White : new SolidColorBrush(Color.Parse("#555555"));
            icons[i].BorderThickness = selected ? new Thickness(2) : new Thickness(2);
            icons[i].Classes.Set("selected", selected);
        }
    }

    // ─── Клик по тайлу в палитре (режим B) ──
    private void OnPaletteSelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (!_isModeB) return;
        if (PaletteList.SelectedItem is not TileItem tile) return;

        if (tile.Index >= 0 && tile.Index < _tileTypes.Length)
            _tileTypes[tile.Index] = _currentTileType;
        tile.TileType = _currentTileType;

        Debug.WriteLine($"Тайл #{tile.Index} → тип {_currentTileType}");
    }
}