using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;
using RpgShinzoMaker.Core.Models;

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

    // Список карт из entries.json
    private List<RpgShinzoMaker.Core.Models.MapEntry> _mapEntries = new();
    private List<string> _musicFiles = new();
    private bool _suppressMusicChange = false;
    private bool _showLayer1 = true;
    private bool _showLayer2 = true;
    private int  _currentLayer = 0;
    private GameMap? _currentMap;
    private bool _tileEditorMode = false;
    private int  _transformMode  = 0;   // 0=none 1=rotate 2=flipH 3=flipV 4=delete
    
    // Границы основной области карты (из areas.json)
    private int _areaStartX = 0;
    private int _areaStartY = 0;
    private int _areaEndX   = 0;
    private int _areaEndY   = 0;

    // ═══ Select Mode / Clipboard ═══
    private bool _selectMode = false;
    private bool _selecting  = false;
    private int _selStartX = 0, _selStartY = 0, _selEndX = 0, _selEndY = 0;

    private bool _hasClipboard = false;
    private int _clipboardW = 0, _clipboardH = 0;
    private int[]?  _clipboardTiles;
    private int[]?  _clipboardRot;
    private bool[]? _clipboardMX;
    private bool[]? _clipboardMY;
    private int[]?  _clipboardTiles2;
    private int[]?  _clipboardRot2;
    private bool[]? _clipboardMX2;
    private bool[]? _clipboardMY2;

    public MainWindow()
    {
        InitializeComponent();
        UpdateTypeIconSelection();
        UpdateTypePreview();
        UpdatePreviewVisibility();

        _gridMode = false;
        GridModeToggle.Content = "OFF";

        LoadProject();
        MapCanvasControl.TileClicked  += OnMapTileClicked;
        MapCanvasControl.TileDragged  += OnMapTileDragged;
        MapCanvasControl.TileReleased += OnMapTileReleased;
        MapCanvasControl.TileHover    += OnMapTileHover;
    }

    // ─── Загрузка проекта ─────
    private void LoadProject()
    {
        var log = new System.Text.StringBuilder();
        void L(string s) { log.AppendLine(s); Debug.WriteLine(s); }

        try
        {
            var root = FindProjectRoot();
            L($"[TEST] root = {root ?? "NULL"}");
            if (root == null) { L("Корень не найден"); return; }

            RpgShinzoMaker.Core.Services.ProjectPaths.Root = root;

            _mapEntries = RpgShinzoMaker.Core.Services.EntriesService.Load(
                RpgShinzoMaker.Core.Services.ProjectPaths.EntriesFile);
            L($"[TEST] entries: {_mapEntries.Count}");
            if (_mapEntries.Count == 0) { L("entries пуст"); return; }

            MapSelector.ItemsSource = null;
            MapSelector.ItemsSource = _mapEntries.Select(e => e.Name).ToList();

            PopulateMusicList();

            MapSelector.SelectedIndex = 0;

            L("[TEST] Проект загружен");
        }
        catch (Exception ex)
        {
            L($"[TEST] Ошибка: {ex.Message}\n{ex.StackTrace}");
        }
    }

    // ─── Загрузка конкретной карты ─────
    private void LoadMapByEntry(RpgShinzoMaker.Core.Models.MapEntry entry)
    {
        try
        {
            var layoutPath = RpgShinzoMaker.Core.Services.ProjectPaths.LayoutFile(entry.Folder);
            if (!File.Exists(layoutPath)) return;

            var map = RpgShinzoMaker.Core.Services.MapJsonService.Load(layoutPath);
            if (map == null) return;

            map.Name        = entry.Name;
            map.Folder      = entry.Folder;
            map.MusicFile   = entry.Music;
            map.MusicVolume = entry.MusicVolume;
            map.AreasPath   = entry.Areas;

            var tilesetName = Path.GetFileName(map.TilesetPath);
            var tilesetPath = Path.Combine(
                RpgShinzoMaker.Core.Services.ProjectPaths.TilesetsDir, tilesetName);
            if (!File.Exists(tilesetPath)) return;

            LoadTileset(tilesetPath);
            MapCanvasControl.SetMap(map, _tiles.Select(t => t.Image).ToList());
            _currentMap = map;
            MapCanvasControl.CurrentLayer = _currentLayer;
            // Читаем areas.json — границы основной области карты
            LoadAreasForMap(map);

            MapSizeText.Text    = $"{map.Width}×{map.Height}";
            MapTilesetText.Text = tilesetName;

            _suppressMusicChange = true;
            var musicName = string.IsNullOrEmpty(entry.Music) ? null : Path.GetFileName(entry.Music);
            if (musicName != null && _musicFiles.Contains(musicName))
                MapMusicSelector.SelectedItem = musicName;
            else
                MapMusicSelector.SelectedIndex = -1;
            _suppressMusicChange = false;
        }
        catch (Exception ex) { Debug.WriteLine($"[MAP] {ex.Message}"); }
    }
    // ─── Сохранение текущей карты ─────
    private void OnSaveClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) 
        {
            Debug.WriteLine("[SAVE] Нет активной карты для сохранения.");
            return;
        }

        try
        {
            // 1. Сохраняем layout.json (тайлы, повороты, коллизии)
            var layoutPath = RpgShinzoMaker.Core.Services.ProjectPaths.LayoutFile(_currentMap.Folder);
            RpgShinzoMaker.Core.Services.MapJsonService.Save(layoutPath, _currentMap);
            Debug.WriteLine($"[SAVE] Карта сохранена: {layoutPath}");

            // 2. Сохраняем типы тайлов для текущего тайлсета
            if (!string.IsNullOrEmpty(_currentMap.TilesetPath))
            {
                var tilesetName = Path.GetFileName(_currentMap.TilesetPath);
                var tilesetAbs  = Path.Combine(
                    RpgShinzoMaker.Core.Services.ProjectPaths.TilesetsDir, tilesetName);
                SaveTileTypesForTileset(tilesetAbs);
            }

            // 3. Сохраняем areas.json (границы основной области карты)
            SaveAreasForMap(_currentMap);

            // 4. Сохраняем entries.json (музыка, громкость, области)
            RpgShinzoMaker.Core.Services.EntriesService.Save(
                RpgShinzoMaker.Core.Services.ProjectPaths.EntriesFile,
                _mapEntries);
            // (Если у вас есть TextBlock для статуса, раскомментируйте и используйте)
            // StatusText.Text = "Сохранено!";
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[SAVE] Ошибка сохранения: {ex.Message}");
        }
    }
    
    private void OnMapSelectorChanged(object? sender, SelectionChangedEventArgs e)
    {
        int idx = MapSelector.SelectedIndex;
        if (idx < 0 || idx >= _mapEntries.Count) return;
        LoadMapByEntry(_mapEntries[idx]);
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

    // ─── Загрузка тайлсета по кнопке ─────
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
                if (folder != null) options.SuggestedStartLocation = folder;
            }
        }
        catch (Exception ex) { Debug.WriteLine($"Путь: {ex.Message}"); }

        var files = await StorageProvider.OpenFilePickerAsync(options);
        if (files.Count == 0) return;

        var newPath = files[0].Path.LocalPath;
        LoadTileset(newPath);

        // Обновить canvas — иначе старые CroppedBitmap'ы ссылаются на освобождённый Bitmap
        if (_currentMap != null)
        {
            _currentMap.TilesetPath = "assets/tilesets/" + Path.GetFileName(newPath);
            MapCanvasControl.SetMap(_currentMap, _tiles.Select(t => t.Image).ToList());
            MapTilesetText.Text = Path.GetFileName(newPath);
        }
    }
    // ─── Относительный путь тайлсета (как в C) ─────
    private static string GetRelativeTilesetPath(string absPath)
    {
        var root = RpgShinzoMaker.Core.Services.ProjectPaths.Root;
        if (!string.IsNullOrEmpty(root) &&
            absPath.StartsWith(root, StringComparison.OrdinalIgnoreCase))
        {
            var rel = absPath.Substring(root.Length).TrimStart('\\', '/');
            return rel.Replace('\\', '/');
        }
        // Fallback — только имя файла
        return Path.GetFileName(absPath);
    }

    // ─── Загрузить типы тайлов из JSON ─────
    private void LoadTileTypesForTileset(string tilesetAbsPath)
    {
        _tileTypes = new int[_tiles.Count];   // по умолчанию все 0 (Passable)

        string rel  = GetRelativeTilesetPath(tilesetAbsPath);
        string file = RpgShinzoMaker.Core.Services.ProjectPaths.TileTypesFile(rel);

        if (!File.Exists(file))
        {
            Debug.WriteLine($"[TILE_TYPES] Файл не найден: {file} — все типы 0");
            return;
        }

        try
        {
            var arr = JsonNode.Parse(File.ReadAllText(file))?.AsArray();
            if (arr == null) return;

            int n = Math.Min(arr.Count, _tiles.Count);
            for (int i = 0; i < n; i++)
            {
                int t = arr[i]?.GetValue<int>() ?? 0;
                _tileTypes[i] = t;
                _tiles[i].TileType = t;
            }

            Debug.WriteLine($"[TILE_TYPES] Загружено {n} типов из {file}");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[TILE_TYPES] Ошибка загрузки: {ex.Message}");
        }
    }

    // ─── Сохранить типы тайлов в JSON ─────
    private void SaveTileTypesForTileset(string tilesetAbsPath)
    {
        if (_tiles.Count == 0 || _tileTypes.Length == 0) return;

        string rel  = GetRelativeTilesetPath(tilesetAbsPath);
        string file = RpgShinzoMaker.Core.Services.ProjectPaths.TileTypesFile(rel);

        try
        {
            var dir = Path.GetDirectoryName(file);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

            var arr = new JsonArray();
            for (int i = 0; i < _tileTypes.Length; i++)
                arr.Add(_tileTypes[i]);

            File.WriteAllText(file, arr.ToJsonString(new JsonSerializerOptions
            {
                WriteIndented = false
            }));

            Debug.WriteLine($"[TILE_TYPES] Сохранено: {file}");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[TILE_TYPES] Ошибка сохранения: {ex.Message}");
        }
    }
    
    // ─── Нарезка тайлсета (strip-based) ─────
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
                for (int c = startCol; c < endCol; c++)
                {
                    var crop = new CroppedBitmap(_sourceTileset,
                        new PixelRect(c * TileSize, r * TileSize, TileSize, TileSize));
                    newTiles.Add(new TileItem
                    {
                        Image = crop, Index = idx, TileType = 0, ShowType = _isModeB
                    });
                    idx++;
                }
            }

            _tiles = newTiles;

            // Загрузить реальные типы из data/tile_types/{tileset}.json
            LoadTileTypesForTileset(path);

            // Отдать типы канвасу для Grid Mode
            MapCanvasControl.TileTypes = _tileTypes;

            ClearSelections();
            ClearPreviews();

            PaletteList.ItemsSource = null;
            PaletteList.ItemsSource = _tiles;

            const int expectedTiles = 1024;
            bool isValid = _tiles.Count == expectedTiles;

            PaletteStatus.Text = isValid ? "✓" : "✗";
            PaletteStatus.Foreground = isValid ? Brushes.LightGreen : Brushes.IndianRed;
            PaletteInfo.Text = $"Тайлов: {_tiles.Count}";
        }
        catch (Exception ex) { Debug.WriteLine($"Ошибка: {ex.Message}"); }
    }

    // ─── Режимы A / B ─────
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

        // Grid Mode больше не привязан к режиму B — управляется отдельной кнопкой
        ClearSelections();
        ClearPreviews();
        UpdatePreviewVisibility();

        // Обновить стиль иконок типов (мигание только в B)
        UpdateTypeIconSelection();
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

    private void OnGridModeClick(object? sender, RoutedEventArgs e) { }

    // ─── Клик по кружку типа ─────
    private void OnTileTypeClick(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Border border || border.Tag is not string tagStr) return;
        if (!int.TryParse(tagStr, out int type)) return;

        _currentTileType = type;
        UpdateTypeIconSelection();
        UpdateTypePreview();
    }
    
    // ─── Grid Mode toggle ─────
    private void OnGridModeToggle(object? sender, RoutedEventArgs e)
    {
        _gridMode = GridModeButton.IsChecked == true;

        GridModeButton.Background = new SolidColorBrush(
            Color.Parse(_gridMode ? "#C83232" : "#3E3E42"));

        // Синхронизируем старый маленький тумблер в палитре
        GridModeToggle.Content = _gridMode ? "ON" : "OFF";

        // ═══ Взаимоисключение с Select Mode ═══
        if (_gridMode && _selectMode)
        {
            SelectButton.IsChecked = false;
            _selectMode = false;
            SelectButton.Background = new SolidColorBrush(Color.Parse("#3E3E42"));
            ClearSelectState();
        }

        // ═══ Взаимоисключение с Tile Editor ═══
        if (_gridMode && _tileEditorMode)
        {
            // Принудительно выключаем Tile Editor
            TileEditorToggle.IsChecked = false;
            _tileEditorMode = false;
            _transformMode = 0;

            TileEditorIcon.Text = "✗";
            TileEditorIcon.Foreground = new SolidColorBrush(Color.Parse("#E74C3C"));

            RotateBtn.IsEnabled = false;
            FlipHBtn.IsEnabled  = false;
            FlipVBtn.IsEnabled  = false;
            DeleteBtn.IsEnabled = false;

            UpdateTransformHighlight();

            MapLeftPreview.Source  = null;
            MapRightPreview.Source = null;
            MapLeftLabel.Text  = "—";
            MapRightLabel.Text = "—";
        }

        // ═══ Авто-переключение блоксета A/B ═══
        if (_gridMode)
        {
            // Включаем Grid Mode → переключаем блоксет в Mode B
            ModeA.IsChecked = false;
            ModeB.IsChecked = true;
            SetModeB(true);
        }
        else
        {
            // Выключаем Grid Mode → переключаем блоксет обратно в Mode A
            ModeA.IsChecked = true;
            ModeB.IsChecked = false;
            SetModeB(false);
        }

        MapCanvasControl.ShowGridMode = _gridMode;
        MapCanvasControl.UpdateGridOverlay();
    }
    // ─── Zoom ─────
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

        // Обновить зум в статус-баре
        if (StatusInfoText != null)
            StatusInfoText.Text = $"Позиция: —    Зум: {text}";
    }
    
    private void UpdateTypeIconSelection()
    {
        var icons = new[] { TypeIcon0, TypeIcon1, TypeIcon2, TypeIcon3 };
        for (int i = 0; i < icons.Length; i++)
        {
            bool selected = i == _currentTileType;
            icons[i].BorderBrush = selected ? Brushes.White : new SolidColorBrush(Color.Parse("#555555"));
            icons[i].BorderThickness = new Thickness(2);

            // Мигание — только в режиме B (Mode A — просто статичное выделение)
            icons[i].Classes.Set("selected", selected && _isModeB);
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

    // ─── Клик ЛКМ/ПКМ по тайлу палитры ─────
    private void OnPaletteItemPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Grid grid || grid.Tag is not TileItem tile) return;

        var props = e.GetCurrentPoint(grid).Properties;

        // ═══ Mode B → назначаем текущий тип тайлу палитры ═══
        if (_isModeB)
        {
            if (props.IsLeftButtonPressed || props.IsRightButtonPressed)
            {
                AssignTypeToPaletteTile(tile);
                e.Pointer.Capture(null);   // снять захват, иначе PointerMoved не пойдёт на соседние Grid
            }
            return;
        }

        // ═══ Mode A → выбор тайла для рисования ═══
        if (props.IsLeftButtonPressed)
        {
            _leftSelectedIndex = tile.Index;
            foreach (var t in _tiles) t.LeftSelected = (t.Index == tile.Index);
            LeftClickPreview.Source = tile.Image;
            LeftClickLabel.Text = $"Tile #{tile.Index}";
        }
        else if (props.IsRightButtonPressed)
        {
            _rightSelectedIndex = tile.Index;
            foreach (var t in _tiles) t.RightSelected = (t.Index == tile.Index);
            RightClickPreview.Source = tile.Image;
            RightClickLabel.Text = $"Tile #{tile.Index}";
        }
    }

    // ─── Назначить текущий тип тайлу палитры ─────
    private void AssignTypeToPaletteTile(TileItem tile)
    {
        if (tile.Index < 0 || tile.Index >= _tileTypes.Length) return;
        _tileTypes[tile.Index] = _currentTileType;
        tile.TileType = _currentTileType;

        // Обновить оверлей на карте, если Grid Mode включён
        MapCanvasControl.UpdateGridOverlay();
    }
    // ─── Назначить тип тайлу, который лежит на карте в клетке idx ─────
    private void AssignTypeToMapTile(int idx)
    {
        if (_currentMap == null) return;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        int tileId = (_currentLayer == 0)
            ? _currentMap.Tiles[idx]
            : _currentMap.Tiles2[idx];

        if (tileId < 0 || tileId >= _tileTypes.Length) return;

        // Если тип не меняется — ничего не делаем
        if (_tileTypes[tileId] == _currentTileType) return;

        _tileTypes[tileId] = _currentTileType;

        // Обновить квадратик в палитре (может быть null, если тайл не в списке)
        if (tileId < _tiles.Count)
            _tiles[tileId].TileType = _currentTileType;

        // Обновить точки Grid Mode на карте
        MapCanvasControl.UpdateGridOverlay();
    }
    
    // ─── Непрерывное «мазание» типа по палитре ─────
    private void OnPaletteItemPointerMoved(object? sender, PointerEventArgs e)
    {
        if (!_isModeB) return;
        if (sender is not Grid grid || grid.Tag is not TileItem tile) return;

        var props = e.GetCurrentPoint(grid).Properties;
        if (!props.IsLeftButtonPressed && !props.IsRightButtonPressed) return;

        AssignTypeToPaletteTile(tile);
    }

    // ─── Назначение типа в режиме B ─────
    private void OnPaletteSelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (!_isModeB) return;
        if (PaletteList.SelectedItem is not TileItem tile) return;

        if (tile.Index >= 0 && tile.Index < _tileTypes.Length)
            _tileTypes[tile.Index] = _currentTileType;
        tile.TileType = _currentTileType;
    }

    // ─── Инструменты (заглушки) ─────
    private void OnToolPencilClick(object? sender, RoutedEventArgs e)  { Debug.WriteLine("Карандаш"); }
    private void OnToolEraserClick(object? sender, RoutedEventArgs e)  { Debug.WriteLine("Ластик"); }
    private void OnToolFillClick(object? sender, RoutedEventArgs e)    { Debug.WriteLine("Заливка"); }
    private void OnToolRectClick(object? sender, RoutedEventArgs e)    { Debug.WriteLine("Прямоугольник"); }
    private void OnToolPickerClick(object? sender, RoutedEventArgs e)  { Debug.WriteLine("Пипетка"); }
    private void OnToolSelectClick(object? sender, RoutedEventArgs e)  { Debug.WriteLine("Выделение"); }

     // ─── Прочитать areas.json ─────
    private void LoadAreasForMap(GameMap map)
    {
        _areaStartX = 0;
        _areaStartY = 0;
        _areaEndX   = map.Width  - 1;
        _areaEndY   = map.Height - 1;

        if (string.IsNullOrEmpty(map.Folder)) return;

        var file = RpgShinzoMaker.Core.Services.ProjectPaths.AreasFile(map.Folder);
        if (!File.Exists(file)) return;

        try
        {
            var arr = JsonNode.Parse(File.ReadAllText(file))?.AsArray();
            if (arr == null || arr.Count == 0) return;

            var first = arr[0]?.AsObject();
            if (first == null) return;

            var start = first["mainLayerStart"]?.AsArray();
            var end   = first["mainLayerEnd"]?.AsArray();

            if (start != null && start.Count >= 2)
            {
                _areaStartX = start[0]?.GetValue<int>() ?? 0;
                _areaStartY = start[1]?.GetValue<int>() ?? 0;
            }
            if (end != null && end.Count >= 2)
            {
                _areaEndX = end[0]?.GetValue<int>() ?? (map.Width  - 1);
                _areaEndY = end[1]?.GetValue<int>() ?? (map.Height - 1);
            }

            Debug.WriteLine($"[AREAS] Загружено: start=({_areaStartX},{_areaStartY}) end=({_areaEndX},{_areaEndY})");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[AREAS] Ошибка чтения: {ex.Message}");
        }
    }

    // ─── Сохранить areas.json ─────
    private void SaveAreasForMap(GameMap map)
    {
        if (string.IsNullOrEmpty(map.Folder)) return;

        var file = RpgShinzoMaker.Core.Services.ProjectPaths.AreasFile(map.Folder);

        try
        {
            var dir = Path.GetDirectoryName(file);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

            var areas = new JsonArray
            {
                new JsonObject
                {
                    ["mainLayerStart"] = new JsonArray { _areaStartX, _areaStartY },
                    ["mainLayerEnd"]   = new JsonArray { _areaEndX,   _areaEndY   },
                }
            };

            File.WriteAllText(file, areas.ToJsonString(new JsonSerializerOptions
            {
                WriteIndented = false
            }));

            Debug.WriteLine($"[AREAS] Сохранено: start=({_areaStartX},{_areaStartY}) end=({_areaEndX},{_areaEndY})");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[AREAS] Ошибка сохранения: {ex.Message}");
        }
    }
    // ─── Select Mode toggle ─────
    private void OnSelectToggle(object? sender, RoutedEventArgs e)
    {
        _selectMode = SelectButton.IsChecked == true;

        SelectButton.Background = new SolidColorBrush(
            Color.Parse(_selectMode ? "#C83232" : "#3E3E42"));

        if (_selectMode)
        {
            // ═══ Взаимоисключение с Tile Editor ═══
            if (_tileEditorMode)
            {
                TileEditorToggle.IsChecked = false;
                _tileEditorMode = false;
                _transformMode = 0;
                TileEditorIcon.Text = "✗";
                TileEditorIcon.Foreground = new SolidColorBrush(Color.Parse("#E74C3C"));
                RotateBtn.IsEnabled = false;
                FlipHBtn.IsEnabled  = false;
                FlipVBtn.IsEnabled  = false;
                DeleteBtn.IsEnabled = false;
                UpdateTransformHighlight();
                MapLeftPreview.Source  = null;
                MapRightPreview.Source = null;
                MapLeftLabel.Text  = "—";
                MapRightLabel.Text = "—";
            }

            // ═══ Взаимоисключение с Grid Mode ═══
            if (_gridMode)
            {
                GridModeButton.IsChecked = false;
                _gridMode = false;
                GridModeButton.Background = new SolidColorBrush(Color.Parse("#3E3E42"));
                GridModeToggle.Content = "OFF";
                MapCanvasControl.ShowGridMode = false;
                MapCanvasControl.UpdateGridOverlay();
            }
        }
        else
        {
            ClearSelectState();
        }
    }

    // ─── Очистить состояние выделения ─────
    private void ClearSelectState()
    {
        _selecting = false;
        _hasClipboard = false;
        _clipboardTiles = null;
        _clipboardRot   = null;
        _clipboardMX    = null;
        _clipboardMY    = null;
        _clipboardTiles2 = null;
        _clipboardRot2   = null;
        _clipboardMX2    = null;
        _clipboardMY2    = null;
        MapCanvasControl.HideSelectionRect();
        MapCanvasControl.HidePasteRect();
    }
    
    // ─── New Map ─────
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
    // ─── Фильтр ввода имени карты (только [A-Za-z0-9_-]) ─────
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
                // Символ не разрешён — гасим событие, он не введётся
                e.Handled = true;
                return;
            }
        }
    }
    private void OnNewMapCancelClick(object? sender, RoutedEventArgs e)
    {
        NewMapOverlay.IsVisible = false;
    }

    private void OnNewMapOkClick(object? sender, RoutedEventArgs e)
    {
        string name = (NewMapName.Text ?? "").Trim();
        if (string.IsNullOrEmpty(name))
        {
            ShowNewMapStatus("Name cannot be empty", isError: true);
            return;
        }

        // Только [A-Za-z0-9_-]
        if (!System.Text.RegularExpressions.Regex.IsMatch(name, @"^[A-Za-z0-9_\-]+$"))
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
        if (mapW > 999 || mapH > 999)
        {
            ShowNewMapStatus("Map size cannot exceed 999x999", isError: true);
            return;
        }

        // Уникальный folder
        string folder = name;
        int suffix = 1;
        var mapsDir = RpgShinzoMaker.Core.Services.ProjectPaths.MapsDir;
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
            var layoutPath = RpgShinzoMaker.Core.Services.ProjectPaths.LayoutFile(folder);
            RpgShinzoMaker.Core.Services.MapJsonService.Save(layoutPath, map);

            // Areas = вся карта
            _areaStartX = 0;
            _areaStartY = 0;
            _areaEndX   = mapW - 1;
            _areaEndY   = mapH - 1;
            SaveAreasForMap(map);

            // Добавить в entries
            var entry = new RpgShinzoMaker.Core.Models.MapEntry
            {
                Folder      = folder,
                Name        = name,
                Music       = defaultMusic,
                MusicVolume = 0.8f,
                Areas       = $"data/maps/{folder}/areas.json",
            };
            _mapEntries.Add(entry);

            // Сохранить entries.json
            RpgShinzoMaker.Core.Services.EntriesService.Save(
                RpgShinzoMaker.Core.Services.ProjectPaths.EntriesFile,
                _mapEntries);

            // Обновить селектор и выбрать новую карту
            MapSelector.ItemsSource = null;
            MapSelector.ItemsSource = _mapEntries.Select(e => e.Name).ToList();
            MapSelector.SelectedIndex = _mapEntries.Count - 1;

            Debug.WriteLine($"[NEW MAP] '{name}' ({mapW}x{mapH}), folder={folder}");

            // ─── Success + закрыть через 1.2 сек ───
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

    // ─── Строка статуса New Map ─────
    private void ShowNewMapStatus(string msg, bool isError)
    {
        NewMapStatus.Text = msg;
        NewMapStatus.Foreground = new SolidColorBrush(
            Color.Parse(isError ? "#E74C3C" : "#4CAF50"));
        NewMapStatus.IsVisible = true;
    }

    // Первый .png в assets/tilesets
    private string GetDefaultTilesetPath()
    {
        var dir = RpgShinzoMaker.Core.Services.ProjectPaths.TilesetsDir;
        if (Directory.Exists(dir))
        {
            var files = Directory.GetFiles(dir, "*.png");
            if (files.Length > 0)
                return "assets/tilesets/" + Path.GetFileName(files[0]);
        }
        return "assets/tilesets/tileset01.png";
    }

    // Первый .mp3 в assets/sounds
    private string GetDefaultMusicPath()
    {
        if (_musicFiles.Count > 0)
            return "assets/sounds/" + _musicFiles[0];
        return "";
    }
    
    // ─── Delete Map (заглушка) ─────
    private void OnDeleteMapClick(object? sender, RoutedEventArgs e)
    {
        Debug.WriteLine("[DELETE MAP] Кнопка нажата — TODO: реализовать");
    }

    // ─── Resize Map (заглушка) ─────
    private void OnResizeMapClick(object? sender, RoutedEventArgs e)
    {
        Debug.WriteLine("[RESIZE MAP] Кнопка нажата — TODO: реализовать");
    }
    
    // ─── Открыть диалог Areas ─────
    private void OnAreasClick(object? sender, RoutedEventArgs e)
    {
        if (_currentMap == null) return;

        int w = _areaEndX - _areaStartX + 1;
        int h = _areaEndY - _areaStartY + 1;

        AreaStartX.Value = _areaStartX;
        AreaStartY.Value = _areaStartY;
        AreaWidth.Value  = w;
        AreaHeight.Value = h;

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

        _areaStartX = sx;
        _areaStartY = sy;
        _areaEndX   = sx + w - 1;
        _areaEndY   = sy + h - 1;

        Debug.WriteLine($"[AREAS] Установлено: start=({_areaStartX},{_areaStartY}) end=({_areaEndX},{_areaEndY})");

        AreasOverlay.IsVisible = false;
    }
    
    private void PopulateMusicList()
    {
        _musicFiles.Clear();
        var dir = RpgShinzoMaker.Core.Services.ProjectPaths.SoundsDir;
        if (Directory.Exists(dir))
        {
            foreach (var f in Directory.GetFiles(dir, "*.mp3"))
                _musicFiles.Add(Path.GetFileName(f));
            _musicFiles.Sort(StringComparer.OrdinalIgnoreCase);
        }
        MapMusicSelector.ItemsSource = null;
        MapMusicSelector.ItemsSource = _musicFiles;
    }

    private void OnMapMusicChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (_suppressMusicChange) return;
        int idx = MapSelector.SelectedIndex;
        if (idx < 0 || idx >= _mapEntries.Count) return;
        if (MapMusicSelector.SelectedItem is not string name) return;

        string musicPath = "assets/sounds/" + name;
        _mapEntries[idx].Music = musicPath;

        // Синхронизируем текущую карту, чтобы при SAVE это ушло и в layout.json
        if (_currentMap != null)
            _currentMap.MusicFile = musicPath;
    }

    // ─── Слои ─────
    private void OnLayer1ToggleClick(object? sender, RoutedEventArgs e)
    {
        _showLayer1 = Layer1Toggle.IsChecked == true;
        Layer1Toggle.Background = new SolidColorBrush(
            Color.Parse(_showLayer1 ? "#4CAF50" : "#555555"));
        UpdateCanvasLayers();
    }

    private void OnLayer2ToggleClick(object? sender, RoutedEventArgs e)
    {
        _showLayer2 = Layer2Toggle.IsChecked == true;
        Layer2Toggle.Background = new SolidColorBrush(
            Color.Parse(_showLayer2 ? "#4CAF50" : "#555555"));
        UpdateCanvasLayers();
    }

    private void OnActiveLayerClick(object? sender, RoutedEventArgs e)
    {
        _currentLayer = 1 - _currentLayer;
        ActiveLayerButton.Content = (_currentLayer + 1).ToString();
        ActiveLayerButton.Background = new SolidColorBrush(
            Color.Parse(_currentLayer == 0 ? "#6497C8" : "#4169E1"));

        MapCanvasControl.CurrentLayer = _currentLayer;
        MapCanvasControl.UpdateGridOverlay();
    }

    private void UpdateCanvasLayers()
    {
        MapCanvasControl.ShowLayer1 = _showLayer1;
        MapCanvasControl.ShowLayer2 = _showLayer2;
        MapCanvasControl.Redraw();
    }

    // ══════════════════════════════════════════
    //   TILE EDITOR
    // ══════════════════════════════════════════

    private void OnTileEditorToggle(object? sender, RoutedEventArgs e)
    {
        _tileEditorMode = TileEditorToggle.IsChecked == true;

        // ═══ Взаимоисключение с Select Mode ═══
        if (_tileEditorMode && _selectMode)
        {
            SelectButton.IsChecked = false;
            _selectMode = false;
            SelectButton.Background = new SolidColorBrush(Color.Parse("#3E3E42"));
            ClearSelectState();
        }

        // ═══ Взаимоисключение с Grid Mode ═══
        if (_tileEditorMode && _gridMode)
        {
            // Принудительно выключаем Grid Mode
            GridModeButton.IsChecked = false;
            _gridMode = false;

            GridModeButton.Background = new SolidColorBrush(Color.Parse("#3E3E42"));
            GridModeToggle.Content = "OFF";

            MapCanvasControl.ShowGridMode = false;
            MapCanvasControl.UpdateGridOverlay();
        }

        // Галочка / крестик
        TileEditorIcon.Text = _tileEditorMode ? "✓" : "✗";
        TileEditorIcon.Foreground = new SolidColorBrush(
            Color.Parse(_tileEditorMode ? "#4CAF50" : "#E74C3C"));

        // 4 кнопки трансформации — активны только в режиме
        RotateBtn.IsEnabled = _tileEditorMode;
        FlipHBtn.IsEnabled  = _tileEditorMode;
        FlipVBtn.IsEnabled  = _tileEditorMode;
        DeleteBtn.IsEnabled = _tileEditorMode;

        if (_tileEditorMode)
        {
            // Автовыбор первого режима — Rotate
            _transformMode = 1;
            UpdateTransformHighlight();
        }
        else
        {
            // Сброс превью когда выключаем
            _transformMode = 0;
            UpdateTransformHighlight();

            MapLeftPreview.Source  = null;
            MapRightPreview.Source = null;
            MapLeftLabel.Text  = "—";
            MapRightLabel.Text = "—";
        }
    }

    private void OnRotateClick(object? sender, RoutedEventArgs e) { _transformMode = 1; UpdateTransformHighlight(); }
    private void OnFlipHClick(object? sender, RoutedEventArgs e)  { _transformMode = 2; UpdateTransformHighlight(); }
    private void OnFlipVClick(object? sender, RoutedEventArgs e)  { _transformMode = 3; UpdateTransformHighlight(); }
    
    private void OnDeleteClick(object? sender, RoutedEventArgs e)
    {
        _transformMode = 4;
        UpdateTransformHighlight();

        // Delete не использует превью — сбрасываем его
        MapLeftPreview.Source  = null;
        MapRightPreview.Source = null;
        MapLeftLabel.Text  = "—";
        MapRightLabel.Text = "—";
    }

    private void UpdateTransformHighlight()
    {
        var yellow = new SolidColorBrush(Color.Parse("#FFD700"));
        var normal = new SolidColorBrush(Color.Parse("#3E3E42"));

        RotateBtn.BorderBrush = (_transformMode == 1) ? yellow : normal;
        FlipHBtn.BorderBrush  = (_transformMode == 2) ? yellow : normal;
        FlipVBtn.BorderBrush  = (_transformMode == 3) ? yellow : normal;
        DeleteBtn.BorderBrush = (_transformMode == 4) ? yellow : normal;
    }

    // ─── Клик по тайлу на карте ─────
    private void OnMapTileClicked(int tx, int ty, int tileId, bool isLeftButton)
    {
        if (_currentMap == null) return;

        int idx = tx * _currentMap.Height + ty;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

        // ═══ Select Mode ═══
        if (_selectMode)
        {
            if (!isLeftButton)
            {
                // ПКМ — сброс clipboard
                _hasClipboard = false;
                MapCanvasControl.HideSelectionRect();
                MapCanvasControl.HidePasteRect();
                return;
            }

            if (_hasClipboard)
            {
                // ЛКМ при clipboard — вставка
                PasteClipboard(tx, ty);
                return;
            }

            // Начать выделение
            _selecting = true;
            _selStartX = _selEndX = tx;
            _selStartY = _selEndY = ty;
            MapCanvasControl.ShowSelectionRect(tx, ty, tx, ty);
            return;
        }

        // ═══ Grid Mode ВКЛЮЧЁН → назначаем тип тайла под курсором ═══
        if (_gridMode)
        {
            AssignTypeToMapTile(idx);
            return;
        }

        // ═══ Tile Editor ВЫКЛЮЧЕН → рисуем тайлом из палитры ═══
        if (!_tileEditorMode)
        {
            if (_isModeB) return;  // в режиме типов по карте не рисуем

            int paintTile = isLeftButton ? _leftSelectedIndex : _rightSelectedIndex;
            if (paintTile < 0) return;

            PaintTile(idx, paintTile);
            MapCanvasControl.RedrawTile(tx, ty);
            return;
        }

        // ═══ Tile Editor ВКЛЮЧЁН → трансформации ═══
        if (_transformMode == 0) return;

        ApplyTransform(tx, ty);

        int newTileId = (_currentLayer == 0) ? _currentMap.Tiles[idx] : _currentMap.Tiles2[idx];
        int rot = (_currentLayer == 0) ? _currentMap.Rot[idx]     : _currentMap.Rot2[idx];
        bool mx  = (_currentLayer == 0) ? _currentMap.MirrorX[idx] : _currentMap.MirrorX2[idx];
        bool my  = (_currentLayer == 0) ? _currentMap.MirrorY[idx] : _currentMap.MirrorY2[idx];

        if (_transformMode != 4)
        {
            var img = RenderTileWithTransform(newTileId, rot, mx, my);
            if (isLeftButton) { MapLeftPreview.Source  = img; MapLeftLabel.Text  = newTileId >= 0 ? $"#{newTileId}" : "—"; }
            else              { MapRightPreview.Source = img; MapRightLabel.Text = newTileId >= 0 ? $"#{newTileId}" : "—"; }
        }
    }
    
    /// Рендерит тайл с учётом rot/mirror — для превью.
    private Bitmap? RenderTileWithTransform(int tileId, int rot, bool mx, bool my)
    {
        if (tileId < 0 || tileId >= _tiles.Count) return null;

        var src = _tiles[tileId].Image;
        int ts = TileSize;

        var rtb = new RenderTargetBitmap(new PixelSize(ts, ts), new Vector(96, 96));

        using (var ctx = rtb.CreateDrawingContext())
        {
            double cx = ts / 2.0;
            double cy = ts / 2.0;

            var matrix = Matrix.Identity;
            matrix = matrix * Matrix.CreateTranslation(-cx, -cy);
            if (mx) matrix = matrix * Matrix.CreateScale(-1, 1);
            if (my) matrix = matrix * Matrix.CreateScale(1, -1);
            matrix = matrix * Matrix.CreateRotation(rot * Math.PI / 2);
            matrix = matrix * Matrix.CreateTranslation(cx, cy);

            using (ctx.PushTransform(matrix))
            {
                ctx.DrawImage(src, new Rect(0, 0, ts, ts));
            }
        }

        return rtb;
    }   
    
        // ═══ Tile Editor ВЫКЛЮЧЕН → непрерывное рисование ═══
        private void OnMapTileDragged(int tx, int ty, int tileId, bool isLeftButton)
        {
            if (_currentMap == null) return;

            int idx = tx * _currentMap.Height + ty;
            if (idx < 0 || idx >= _currentMap.TotalCells) return;

            // ═══ Select Mode ═══
            if (_selectMode)
            {
                if (!isLeftButton) return;
                if (!_selecting) return;

                _selEndX = tx;
                _selEndY = ty;
                MapCanvasControl.ShowSelectionRect(_selStartX, _selStartY, _selEndX, _selEndY);
                return;
            }

            // ═══ Grid Mode ВКЛЮЧЁН → непрерывно назначаем тип ═══
            if (_gridMode)
            {
                AssignTypeToMapTile(idx);
                return;
            }

            // ═══ Tile Editor ВЫКЛЮЧЕН → непрерывное рисование ═══
            if (!_tileEditorMode)
            {
                if (_isModeB) return;

                int paintTile = isLeftButton ? _leftSelectedIndex : _rightSelectedIndex;
                if (paintTile < 0) return;

                // Уже такой тайл — не перерисовываем (оптимизация)
                int existing = (_currentLayer == 0) ? _currentMap.Tiles[idx] : _currentMap.Tiles2[idx];
                if (existing == paintTile) return;

                PaintTile(idx, paintTile);
                MapCanvasControl.RedrawTile(tx, ty);
                return;
            }

            // ═══ Tile Editor ВКЛЮЧЁН → drag для Delete ═══
            if (_transformMode != 4) return;

            int currentTile = (_currentLayer == 0) ? _currentMap.Tiles[idx] : _currentMap.Tiles2[idx];
            if (currentTile < 0) return;

            ApplyTransform(tx, ty);
        }
    // ─── Отпускание ЛКМ по карте ─────
    private void OnMapTileReleased(int tx, int ty, bool isLeftButton)
    {
        if (_currentMap == null) return;
        if (!_selectMode) return;
        if (!isLeftButton) return;
        if (!_selecting) return;

        _selecting = false;
        CopySelectionToClipboard();
    }

    // ─── Наведение мыши (без зажатой кнопки) ─────
    private void OnMapTileHover(int tx, int ty)
    {
        // 1) Обновляем статус-бар (позиция курсора на карте)
        UpdateStatusPosition(tx, ty);

        // 2) Логика превью вставки (Select Mode)
        if (!_selectMode || !_hasClipboard)
        {
            MapCanvasControl.HidePasteRect();
            return;
        }
        MapCanvasControl.ShowPasteRect(tx, ty, _clipboardW, _clipboardH);
    }

    // ─── Обновить позицию в статус-баре ─────
    private void UpdateStatusPosition(int tx, int ty)
    {
        if (StatusInfoText == null) return;

        string pos = (tx < 0 || ty < 0) ? "—" : $"{tx}, {ty}";
        string zoom = CurrentZoomLabel();

        StatusInfoText.Text = $"Позиция: {pos}    Зум: {zoom}";
    }

    // ─── Текущая метка зума ─────
    private string CurrentZoomLabel()
    {
        if (ZoomSelector?.SelectedItem is ComboBoxItem item &&
            item.Content is string text)
            return text;

        return "1x";
    }

    // ─── Скопировать выделение в буфер ─────
    private void CopySelectionToClipboard()
    {
        if (_currentMap == null) return;

        int x1 = Math.Min(_selStartX, _selEndX);
        int y1 = Math.Min(_selStartY, _selEndY);
        int x2 = Math.Max(_selStartX, _selEndX);
        int y2 = Math.Max(_selStartY, _selEndY);

        int w = x2 - x1 + 1;
        int h = y2 - y1 + 1;

        _clipboardW = w;
        _clipboardH = h;
        int sz = w * h;

        _clipboardTiles  = new int[sz];
        _clipboardRot    = new int[sz];
        _clipboardMX     = new bool[sz];
        _clipboardMY     = new bool[sz];
        _clipboardTiles2 = new int[sz];
        _clipboardRot2   = new int[sz];
        _clipboardMX2    = new bool[sz];
        _clipboardMY2    = new bool[sz];

        for (int dx = 0; dx < w; dx++)
        for (int dy = 0; dy < h; dy++)
        {
            int mapX = x1 + dx;
            int mapY = y1 + dy;
            int srcIdx = mapX * _currentMap.Height + mapY;
            int dstIdx = dx * h + dy;

            // Берём ТОЛЬКО активный слой
            if (_currentLayer == 0)
            {
                _clipboardTiles[dstIdx]  = _currentMap.Tiles[srcIdx];
                _clipboardRot[dstIdx]    = _currentMap.Rot[srcIdx];
                _clipboardMX[dstIdx]     = _currentMap.MirrorX[srcIdx];
                _clipboardMY[dstIdx]     = _currentMap.MirrorY[srcIdx];
            }
            else
            {
                _clipboardTiles2[dstIdx] = _currentMap.Tiles2[srcIdx];
                _clipboardRot2[dstIdx]   = _currentMap.Rot2[srcIdx];
                _clipboardMX2[dstIdx]    = _currentMap.MirrorX2[srcIdx];
                _clipboardMY2[dstIdx]    = _currentMap.MirrorY2[srcIdx];
            }
        }

        _hasClipboard = true;
        Debug.WriteLine($"[SELECT] Скопировано {w}x{h} (from {x1},{y1})");
    }

    // ─── Вставить буфер в точку (destX, destY) ─────
    private void PasteClipboard(int destX, int destY)
    {
        if (_currentMap == null) return;
        if (!_hasClipboard) return;
        if (_clipboardTiles == null) return;

        for (int dx = 0; dx < _clipboardW; dx++)
        for (int dy = 0; dy < _clipboardH; dy++)
        {
            int tx = destX + dx;
            int ty = destY + dy;
            if (tx < 0 || tx >= _currentMap.Width)  continue;
            if (ty < 0 || ty >= _currentMap.Height) continue;

            int dstIdx = tx * _currentMap.Height + ty;
            int srcIdx = dx * _clipboardH + dy;

            if (_currentLayer == 0)
            {
                _currentMap.Tiles[dstIdx]   = _clipboardTiles![srcIdx];
                _currentMap.Rot[dstIdx]     = _clipboardRot![srcIdx];
                _currentMap.MirrorX[dstIdx] = _clipboardMX![srcIdx];
                _currentMap.MirrorY[dstIdx] = _clipboardMY![srcIdx];
            }
            else
            {
                _currentMap.Tiles2[dstIdx]   = _clipboardTiles2![srcIdx];
                _currentMap.Rot2[dstIdx]     = _clipboardRot2![srcIdx];
                _currentMap.MirrorX2[dstIdx] = _clipboardMX2![srcIdx];
                _currentMap.MirrorY2[dstIdx] = _clipboardMY2![srcIdx];
            }
        }

        MapCanvasControl.Redraw();
        Debug.WriteLine($"[SELECT] Вставлено в ({destX},{destY})");
    }
        
    private void ApplyTransform(int tx, int ty)
    {
        var map = _currentMap;
        if (map == null) return;

        int idx = tx * map.Height + ty;
        if (idx < 0 || idx >= map.TotalCells) return;

        switch (_transformMode)
        {
            case 1: // Rotate
                if (_currentLayer == 0) map.Rot[idx]   = (map.Rot[idx]   + 1) % 4;
                else                    map.Rot2[idx]  = (map.Rot2[idx]  + 1) % 4;
                break;

            case 2: // Flip H
                if (_currentLayer == 0) map.MirrorX[idx]  = !map.MirrorX[idx];
                else                    map.MirrorX2[idx] = !map.MirrorX2[idx];
                break;

            case 3: // Flip V
                if (_currentLayer == 0) map.MirrorY[idx]  = !map.MirrorY[idx];
                else                    map.MirrorY2[idx] = !map.MirrorY2[idx];
                break;

            case 4: // Delete
                if (_currentLayer == 0) map.Tiles[idx]  = -1;
                else                    map.Tiles2[idx] = -1;
                break;
        }

        MapCanvasControl.RedrawTile(tx, ty);
    }
    // ─── Записать тайл в активный слой ─────
    private void PaintTile(int idx, int tileId)
    {
        if (_currentMap == null) return;

        if (_currentLayer == 0)
        {
            _currentMap.Tiles[idx]   = tileId;
            _currentMap.Rot[idx]     = 0;
            _currentMap.MirrorX[idx] = false;
            _currentMap.MirrorY[idx] = false;
        }
        else
        {
            _currentMap.Tiles2[idx]   = tileId;
            _currentMap.Rot2[idx]     = 0;
            _currentMap.MirrorX2[idx] = false;
            _currentMap.MirrorY2[idx] = false;
        }
    }
    
}