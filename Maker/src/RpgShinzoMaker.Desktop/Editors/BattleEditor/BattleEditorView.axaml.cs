using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using Avalonia.Controls;
using Avalonia.Interactivity;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Desktop.Shared.Interfaces;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

/// <summary>
/// Боевой редактор. Корневой partial-класс: общие поля, конструктор, IEditor.
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
    private int _viewMode = 1; 
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

    // ─── Скролл-мост ───
    private Avalonia.Controls.ScrollViewer? _mapScrollViewer;

    // ─── Overlay ───
    private readonly List<OverlayElement> _overlayElements = new();
    private bool _initialized;
    private BattleUnit? _hoveredUnit;

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

    private void OnSaveClick(object? sender, RoutedEventArgs e) => SaveAll();
}