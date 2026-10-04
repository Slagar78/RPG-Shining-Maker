using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
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
        set { _showType = value; OnPropertyChanged(nameof(ShowType)); }
    }

    private bool _leftSelected;
    public bool LeftSelected
    {
        get => _leftSelected;
        set { _leftSelected = value; OnPropertyChanged(nameof(LeftSelected)); }
    }

    private bool _rightSelected;
    public bool RightSelected
    {
        get => _rightSelected;
        set { _rightSelected = value; OnPropertyChanged(nameof(RightSelected)); }
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
    private List<TileItem> _tiles = new();
    private int[] _tileTypes = Array.Empty<int>();

    private bool _gridMode;
    private bool _isModeB;
    private int _currentTileType = 0;

    private int _leftSelectedIndex  = -1;
    private int _rightSelectedIndex = -1;

    public MainWindow()
    {
        InitializeComponent();
        UpdateTypeIconSelection();
        UpdateTypePreview();
        UpdatePreviewVisibility();

        _gridMode = false;
        GridModeToggle.Content = "OFF";

        LoadProject();
    }

// ─── Хранилище карт проекта ─────
private List<RpgShinzoMaker.Core.Models.MapEntry> _mapEntries = new();

// ─── Загрузка проекта (вызывается из конструктора) ─────
private void LoadProject()
{
    var logPath = Path.Combine(Environment.CurrentDirectory, "startup_log.txt");
    var log = new System.Text.StringBuilder();
    void L(string s) { log.AppendLine(s); Debug.WriteLine(s); }

    try
    {
        // 1. Корень
        var root = FindProjectRoot();
        L($"[TEST] root = {root ?? "NULL"}");
        if (root == null) { L("❌ Корень не найден"); File.WriteAllText(logPath, log.ToString()); return; }

        RpgShinzoMaker.Core.Services.ProjectPaths.Root = root;

        // 2. Читаем entries.json
        _mapEntries = RpgShinzoMaker.Core.Services.EntriesService.Load(
            RpgShinzoMaker.Core.Services.ProjectPaths.EntriesFile);
        L($"[TEST] entries: {_mapEntries.Count}");
        if (_mapEntries.Count == 0) { L("❌ entries пуст"); File.WriteAllText(logPath, log.ToString()); return; }

        // 3. Заполняем ComboBox именами карт
        MapSelector.ItemsSource = null;
        MapSelector.ItemsSource = _mapEntries
            .Select(e => e.Name)
            .ToList();

        // 4. Грузим первую карту
        MapSelector.SelectedIndex = 0;   // вызовет OnMapSelectorChanged → загрузка

        L("[TEST] ✅ Проект загружен");
    }
    catch (Exception ex)
    {
        L($"[TEST] ❌ {ex.Message}\n{ex.StackTrace}");
    }

    File.WriteAllText(logPath, log.ToString());
}

// ─── Загрузка конкретной карты по её entry ─────
private void LoadMapByEntry(RpgShinzoMaker.Core.Models.MapEntry entry)
{
    try
    {
        Debug.WriteLine($"[MAP] Загрузка: {entry.Name} (folder={entry.Folder})");

        // 1. layout.json
        var layoutPath = RpgShinzoMaker.Core.Services.ProjectPaths.LayoutFile(entry.Folder);
        if (!File.Exists(layoutPath))
        {
            Debug.WriteLine($"[MAP] ❌ layout.json не найден: {layoutPath}");
            return;
        }

        // 2. Читаем карту
        var map = RpgShinzoMaker.Core.Services.MapJsonService.Load(layoutPath);
        if (map == null) { Debug.WriteLine("[MAP] ❌ map = NULL"); return; }

        // 3. Метаданные из entries
        map.Name        = entry.Name;
        map.Folder      = entry.Folder;
        map.MusicFile   = entry.Music;
        map.MusicVolume = entry.MusicVolume;
        map.AreasPath   = entry.Areas;

        // 4. Тайлсет
        var tilesetName = Path.GetFileName(map.TilesetPath);
        var tilesetPath = Path.Combine(
            RpgShinzoMaker.Core.Services.ProjectPaths.TilesetsDir,
            tilesetName);

        if (!File.Exists(tilesetPath))
        {
            Debug.WriteLine($"[MAP] ❌ тайлсет не найден: {tilesetPath}");
            return;
        }

        // 5. Грузим в UI
        LoadTileset(tilesetPath);
        var tilesetBmp = new Bitmap(tilesetPath);
        MapCanvasControl.SetMap(map, tilesetBmp);

        // 6. Подписи
        MapSizeText.Text    = $"{map.Width}×{map.Height}";
        MapTilesetText.Text = tilesetName;
        // Музыка (берётся из entries.json)
        var musicName = string.IsNullOrEmpty(entry.Music)
            ? "—"
            : Path.GetFileName(entry.Music);
        MapMusicText.Text = musicName;

        Debug.WriteLine($"[MAP] ✅ Загружено: {entry.Name}");
    }
    catch (Exception ex)
    {
        Debug.WriteLine($"[MAP] ❌ {ex.Message}\n{ex.StackTrace}");
    }
}

// ─── Обработчик ComboBox ─────
private void OnMapSelectorChanged(object? sender, SelectionChangedEventArgs e)
{
    int idx = MapSelector.SelectedIndex;
    if (idx < 0 || idx >= _mapEntries.Count) return;

    var entry = _mapEntries[idx];
    LoadMapByEntry(entry);
}

private static string? ResolveTilesetPath(string basePath, string rawPath)
{
    if (string.IsNullOrWhiteSpace(rawPath)) return null;
    var normalized = rawPath.Replace('\\', '/').Trim();
    if (normalized.Length >= 2 && normalized[1] == ':')
        return normalized.Replace('/', Path.DirectorySeparatorChar);
    if (normalized.StartsWith("assets/", StringComparison.OrdinalIgnoreCase))
        return Path.Combine(basePath, normalized.Replace('/', Path.DirectorySeparatorChar));
    while (normalized.StartsWith("../"))
        normalized = normalized.Substring(3);
    if (!normalized.Contains('/'))
        return Path.Combine(basePath, "assets", "tilesets", normalized);
    return Path.Combine(basePath, normalized.Replace('/', Path.DirectorySeparatorChar));
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

            _tiles = newTiles;
            _tileTypes = new int[_tiles.Count];

            ClearSelections();
            ClearPreviews();

            PaletteList.ItemsSource = null;
            PaletteList.ItemsSource = _tiles;

            const int expectedTiles = 1024;
            bool isValid = _tiles.Count == expectedTiles;

            PaletteStatus.Text = isValid ? "✓" : "✗";
            PaletteStatus.Foreground = isValid ? Brushes.LightGreen : Brushes.IndianRed;
            PaletteInfo.Text = $"Тайлов: {_tiles.Count}";

            Debug.WriteLine($"Загружено {_tiles.Count} тайлов (strip-based, {strips} полос)");
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

        _gridMode = isB;
        GridModeToggle.Content = _gridMode ? "ON" : "OFF";

        ClearSelections();
        ClearPreviews();
        UpdatePreviewVisibility();

        Debug.WriteLine(isB ? "Режим B — типы + Grid ON" : "Режим A — рисование + Grid OFF");
    }

    private void UpdatePreviewVisibility()
    {
        ModeAPreviewGrid.IsVisible  = !_isModeB;
        ModeBPreviewPanel.IsVisible = _isModeB;
    }

    private void ClearSelections()
    {
        _leftSelectedIndex  = -1;
        _rightSelectedIndex = -1;
        foreach (var t in _tiles)
        {
            t.LeftSelected  = false;
            t.RightSelected = false;
        }
    }

    private void ClearPreviews()
    {
        LeftClickPreview.Source  = null;
        RightClickPreview.Source = null;
        LeftClickLabel.Text  = "—";
        RightClickLabel.Text = "—";
    }

    // ─── Grid Mode (заблокирован) ───────────
    private void OnGridModeClick(object? sender, RoutedEventArgs e) { }

    // ─── Клик по кружку типа ────────────────
    private void OnTileTypeClick(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Border border || border.Tag is not string tagStr) return;
        if (!int.TryParse(tagStr, out int type)) return;

        _currentTileType = type;
        UpdateTypeIconSelection();
        UpdateTypePreview();
    }

    private void UpdateTypeIconSelection()
    {
        var icons = new[] { TypeIcon0, TypeIcon1, TypeIcon2, TypeIcon3 };
        for (int i = 0; i < icons.Length; i++)
        {
            bool selected = i == _currentTileType;
            icons[i].BorderBrush = selected ? Brushes.White : new SolidColorBrush(Color.Parse("#555555"));
            icons[i].BorderThickness = new Thickness(2);
            icons[i].Classes.Set("selected", selected);
        }
    }

    private void UpdateTypePreview()
    {
        var (brush, label) = _currentTileType switch
        {
            0 => ((IBrush)new SolidColorBrush(Color.Parse("#4CAF50")), "Passable"),
            1 => (new SolidColorBrush(Color.Parse("#E74C3C")), "Block"),
            2 => (new SolidColorBrush(Color.Parse("#3498DB")), "Slow"),
            3 => (new SolidColorBrush(Color.Parse("#E67E22")), "Under"),
            _ => (Brushes.Gray, "Unknown")
        };
        TypePreviewEllipse.Fill = brush;
        TypePreviewLabel.Text = label;
    }

    // ─── Клик ЛКМ/ПКМ по тайлу палитры ──────
    private void OnPaletteItemPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (_isModeB) return;
        if (sender is not Grid grid || grid.Tag is not TileItem tile) return;

        var props = e.GetCurrentPoint(grid).Properties;

        if (props.IsLeftButtonPressed)
        {
            _leftSelectedIndex = tile.Index;
            foreach (var t in _tiles)
                t.LeftSelected = (t.Index == tile.Index);

            LeftClickPreview.Source = tile.Image;
            LeftClickLabel.Text = $"Tile #{tile.Index}";

            Debug.WriteLine($"ЛКМ тайл #{tile.Index}");
        }
        else if (props.IsRightButtonPressed)
        {
            _rightSelectedIndex = tile.Index;
            foreach (var t in _tiles)
                t.RightSelected = (t.Index == tile.Index);

            RightClickPreview.Source = tile.Image;
            RightClickLabel.Text = $"Tile #{tile.Index}";

            Debug.WriteLine($"ПКМ тайл #{tile.Index}");
        }
    }

    // ─── Назначение типа в режиме B ─────────
    private void OnPaletteSelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (!_isModeB) return;
        if (PaletteList.SelectedItem is not TileItem tile) return;

        if (tile.Index >= 0 && tile.Index < _tileTypes.Length)
            _tileTypes[tile.Index] = _currentTileType;
        tile.TileType = _currentTileType;

        Debug.WriteLine($"Тайл #{tile.Index} → тип {_currentTileType}");
    }
    // ─── Инструменты (пока заглушки) ─────
    private void OnToolPencilClick(object? sender, RoutedEventArgs e)  { Debug.WriteLine("Инструмент: Карандаш"); }
    private void OnToolEraserClick(object? sender, RoutedEventArgs e)  { Debug.WriteLine("Инструмент: Ластик"); }
    private void OnToolFillClick(object? sender, RoutedEventArgs e)    { Debug.WriteLine("Инструмент: Заливка"); }
    private void OnToolRectClick(object? sender, RoutedEventArgs e)    { Debug.WriteLine("Инструмент: Прямоугольник"); }
    private void OnToolPickerClick(object? sender, RoutedEventArgs e)  { Debug.WriteLine("Инструмент: Пипетка"); }
    private void OnToolSelectClick(object? sender, RoutedEventArgs e)  { Debug.WriteLine("Инструмент: Выделение"); }    
}