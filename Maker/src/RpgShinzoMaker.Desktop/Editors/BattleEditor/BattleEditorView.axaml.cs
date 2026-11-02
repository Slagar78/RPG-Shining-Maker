using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using Avalonia.Controls;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Desktop.Shared.Interfaces;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

/// <summary>
/// Боевой редактор. Корневой partial-класс: общие поля, конструктор, IEditor.
/// Вся конкретная логика разнесена по partial-файлам:
///   - BattleEditorView.Battles.cs — загрузка entries.json, switch_battle
///   - BattleEditorView.Map.cs     — тайлсет, зум, режимы Map/Battle/Terrain
///   - BattleEditorView.Canvas.cs  — Overlay поверх MapCanvas
///   - BattleEditorView.Units.cs   — панели Allies/Enemies
///   - BattleEditorView.Terrain.cs — кисть terrain
///   - BattleEditorView.Zone.cs    — выделение боевой зоны
///   - BattleEditorView.Inspector.cs — ID ◄►
///   - BattleEditorView.Save.cs    — сохранение 3 JSON
/// </summary>
public partial class BattleEditorView : UserControl, IEditor
{
    public event Action<string>? StatusChanged;

    // ─── Данные ───
    private List<BattleEntry> _battleEntries = new();
    private List<MapEntry>    _mapEntries    = new();
    private int               _currentBattleIndex = -1;
    private GameMap?          _currentMap;

    private readonly ObservableCollection<BattleUnit> _allies  = new();
    private readonly ObservableCollection<BattleUnit> _enemies = new();

    private int[][]? _terrain;

    // ─── Режимы ───
    private int _viewMode = 0;                    // 0 = Map, 1 = Battle, 2 = Terrain
    private int _currentCollisionType = 1;         // 1 = Walkable

    // ─── Боевая зона ───
    private bool _selectingZone;
    private bool _zoneDragActive;
    private int  _selStartX, _selStartY;
    private int  _selCurrentX, _selCurrentY;

    // ─── Инспектор ───
    private bool _inspectorVisible;
    private int  _inspectorPanel;
    private int  _inspectorSlotIdx = -1;

    // ─── Активный слот ───
    private int _activePanel;
    private int _selectedSlot = -1;

    // ─── Скролл-мост к внутреннему ScrollViewer MapCanvas ───
    private Avalonia.Controls.ScrollViewer? _mapScrollViewer;

    // ─── Overlay ───
    private readonly List<OverlayElement> _overlayElements = new();
    private bool _initialized;

    private class OverlayElement
    {
        public required Control Control;
        public int  TileX;
        public int  TileY;
        public bool IsTileSized;
        public int  Width, Height;
    }

    // ══════════════════════════════════════════════════════════
    //   КОНСТРУКТОР
    // ══════════════════════════════════════════════════════════
    public BattleEditorView()
    {
        InitializeComponent();

        AlliesPanel.ItemsSource  = _allies;
        EnemiesPanel.ItemsSource = _enemies;

        // Инициализация только когда контрол попал в визуальное дерево —
        // иначе ScrollViewer внутри MapCanvas ещё не создан.
        this.AttachedToVisualTree += (_, _) =>
        {
            if (_initialized) return;
            _initialized = true;

            InitCanvas();
            LoadAll();
        };
    }

    // ══════════════════════════════════════════════════════════
    //   IEditor
    // ══════════════════════════════════════════════════════════
    public void Save()         => SaveAll();
    public void Reload()       => LoadAll();
    public void OnActivate()   { }
    public void OnDeactivate() { }

    // ══════════════════════════════════════════════════════════
    //   Общая кнопка Save (связана с XAML)
    // ══════════════════════════════════════════════════════════
    private void OnSaveClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e) => SaveAll();
}