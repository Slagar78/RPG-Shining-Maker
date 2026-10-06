// RpgShinzoMaker.Desktop/Editors/MapEditor/MapEditorView.axaml.cs
using System;
using System.Collections.Generic;
using Avalonia.Controls;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;
using RpgShinzoMaker.Desktop.Shared.Interfaces;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Редактор карт. Корневой partial-класс: общие поля состояния,
/// конструктор, реализация IEditor. Вся конкретная логика — в
/// соседних partial-файлах (Palette, Map, Tileset, Painting, Select, Dialogs, Layers, TileEditor).
/// </summary>
public partial class MapEditorView : UserControl, IEditor
{
    // ══════════════════════════════════════════════════════════════
    //   СОБЫТИЕ ДЛЯ СТАТУС-БАРА MAINWINDOW
    // ══════════════════════════════════════════════════════════════

    /// <summary>
    /// Событие — обновить текст в статус-баре MainWindow.
    /// Передаёт строку вида "Позиция: 5, 3    Зум: 1x".
    /// </summary>
    public event Action<string>? StatusChanged;

    // ══════════════════════════════════════════════════════════════
    //   СОСТОЯНИЕ РЕЖИМОВ РЕДАКТОРА
    // ══════════════════════════════════════════════════════════════

    /// <summary>Grid Mode включён (назначаем типы тайлам на карте).</summary>
    private bool _gridMode;

    /// <summary>Tile Editor включён (трансформации тайлов).</summary>
    private bool _tileEditorMode;

    /// <summary>Активная трансформация: 0=нет, 1=rotate, 2=flipH, 3=flipV, 4=delete.</summary>
    private int _transformMode;

    /// <summary>Select Mode включён (выделение и вставка).</summary>
    private bool _selectMode;

    // ══════════════════════════════════════════════════════════════
    //   СОСТОЯНИЕ КАРТ
    // ══════════════════════════════════════════════════════════════

    /// <summary>Список карт из entries.json.</summary>
    private List<MapEntry> _mapEntries = new();

    /// <summary>Список mp3-файлов из assets/sounds.</summary>
    private List<string> _musicFiles = new();

    /// <summary>Защита от рекурсивного вызова при смене выделения в комбо музыки.</summary>
    private bool _suppressMusicChange;

    /// <summary>Показывать ли первый слой.</summary>
    private bool _showLayer1 = true;

    /// <summary>Показывать ли второй слой.</summary>
    private bool _showLayer2 = true;

    /// <summary>Активный слой для рисования: 0 = L1, 1 = L2.</summary>
    private int _currentLayer = 0;

    /// <summary>Текущая загруженная карта.</summary>
    private GameMap? _currentMap;

    /// <summary>Границы основной области карты (mainLayerStart / mainLayerEnd).</summary>
    private Areas _area = new();

    // ══════════════════════════════════════════════════════════════
    //   СОСТОЯНИЕ ВЫДЕЛЕНИЯ И БУФЕРА ОБМЕНА
    // ══════════════════════════════════════════════════════════════

    /// <summary>Идёт выделение мышью прямо сейчас.</summary>
    private bool _selecting;

    /// <summary>Координаты начала и конца выделения.</summary>
    private int _selStartX, _selStartY, _selEndX, _selEndY;

    /// <summary>Последняя позиция курсора на карте (-1, -1 если вне карты).</summary>
    private int _lastHoverX = -1;
    private int _lastHoverY = -1;

    /// <summary>Есть ли данные в буфере обмена.</summary>
    private bool _hasClipboard;

    /// <summary>Размер буфера.</summary>
    private int _clipboardW, _clipboardH;

    // Буфер слоя 1
    private int[]?  _clipboardTiles;
    private int[]?  _clipboardRot;
    private bool[]? _clipboardMX;
    private bool[]? _clipboardMY;

    // Буфер слоя 2
    private int[]?  _clipboardTiles2;
    private int[]?  _clipboardRot2;
    private bool[]? _clipboardMX2;
    private bool[]? _clipboardMY2;

    // ══════════════════════════════════════════════════════════════
    //   КОНСТРУКТОР
    // ══════════════════════════════════════════════════════════════

    public MapEditorView()
    {
        InitializeComponent();

        // Начальное состояние иконок типов и превью (логика в Palette.cs)
        UpdateTypeIconSelection();
        UpdateTypePreview();
        UpdatePreviewVisibility();

        // Маленький тумблер Grid в блоксете — по умолчанию OFF
        _gridMode = false;
        GridModeToggle.Content = "OFF";

        // Загрузить проект (логика в Map.cs)
        LoadProject();

        // Подписка на события канваса (обработчики в Painting.cs)
        MapCanvasControl.TileClicked  += OnMapTileClicked;
        MapCanvasControl.TileDragged  += OnMapTileDragged;
        MapCanvasControl.TileReleased += OnMapTileReleased;
        MapCanvasControl.TileHover    += OnMapTileHover;
    }

    // ══════════════════════════════════════════════════════════════
    //   IEditor — делегирует в методы из partial-файлов
    // ══════════════════════════════════════════════════════════════

    /// <summary>Сохранить текущую карту (layout.json + tile_types + areas + entries).</summary>
    public void Save() => SaveCurrentMap();

    /// <summary>Перечитать список карт и текущую карту с диска.</summary>
    public void Reload() => LoadProject();

    /// <summary>Редактор стал активной вкладкой — ничего специального не делаем.</summary>
    public void OnActivate() { }

    /// <summary>Редактор перестал быть активной вкладкой — ничего специального не делаем.</summary>
    public void OnDeactivate() { }
}