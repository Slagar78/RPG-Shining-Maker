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

    public MainWindow()
    {
        InitializeComponent();
        UpdateTypeIconSelection();
        UpdateTypePreview();
        UpdatePreviewVisibility();

        _gridMode = false;
        GridModeToggle.Content = "OFF";

        LoadProject();
        MapCanvasControl.TileClicked += OnMapTileClicked;
        MapCanvasControl.TileDragged += OnMapTileDragged;
    }

    // ─── Загрузка проекта ─────
    private void LoadProject()
    {
        var logPath = Path.Combine(Environment.CurrentDirectory, "startup_log.txt");
        var log = new System.Text.StringBuilder();
        void L(string s) { log.AppendLine(s); Debug.WriteLine(s); }

        try
        {
            var root = FindProjectRoot();
            L($"[TEST] root = {root ?? "NULL"}");
            if (root == null) { L("Корень не найден"); File.WriteAllText(logPath, log.ToString()); return; }

            RpgShinzoMaker.Core.Services.ProjectPaths.Root = root;

            _mapEntries = RpgShinzoMaker.Core.Services.EntriesService.Load(
                RpgShinzoMaker.Core.Services.ProjectPaths.EntriesFile);
            L($"[TEST] entries: {_mapEntries.Count}");
            if (_mapEntries.Count == 0) { L("entries пуст"); File.WriteAllText(logPath, log.ToString()); return; }

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

        File.WriteAllText(logPath, log.ToString());
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

            // 2. Обновляем статус-бар
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
            _tileTypes = new int[_tiles.Count];
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
        _mapEntries[idx].Music = "assets/sounds/" + name;
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

        // ═══ Tile Editor ВКЛЮЧЁН → трансформации (как было) ═══
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
    // ─── Drag по карте (только для Delete) ─────
    private void OnMapTileDragged(int tx, int ty, int tileId, bool isLeftButton)
    {
        if (_currentMap == null) return;

        int idx = tx * _currentMap.Height + ty;
        if (idx < 0 || idx >= _currentMap.TotalCells) return;

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