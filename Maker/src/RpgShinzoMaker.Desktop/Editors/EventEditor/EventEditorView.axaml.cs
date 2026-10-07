// RpgShinzoMaker.Desktop/Editors/EventEditor/EventEditorView.axaml.cs
using System;
using System.Collections.Generic;
using Avalonia.Controls;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Desktop.Shared.Interfaces;

namespace RpgShinzoMaker.Desktop.Editors.EventEditor;

/// <summary>
/// Редактор событий карты.
/// Корневой partial-класс: общие поля состояния, конструктор, IEditor.
/// Вся логика — в соседних partial-файлах.
/// </summary>
public partial class EventEditorView : UserControl, IEditor
{
    // ══════════════════════════════════════════════════════════════
    //   СОБЫТИЕ ДЛЯ СТАТУС-БАРА MAINWINDOW
    // ══════════════════════════════════════════════════════════════

    public event Action<string>? StatusChanged;

    // ══════════════════════════════════════════════════════════════
    //   СОСТОЯНИЕ
    // ══════════════════════════════════════════════════════════════

    /// <summary>Список карт из entries.json.</summary>
    private List<MapEntry> _mapEntries = new();

    /// <summary>Индекс активной карты в _mapEntries.</summary>
    private int _currentMapIndex = -1;

    /// <summary>Текущая карта (для отрисовки).</summary>
    private GameMap? _currentMap;

    /// <summary>Все события текущей карты.</summary>
    private MapEvents _events = new();

    /// <summary>Активный раздел: "roof" | "tile_change" | "stair" | "warp" | "npc".</summary>
    private string _currentSection = "roof";

    /// <summary>Индекс выбранного события в текущем разделе (-1 = нет).</summary>
    private int _selectedIndex = -1;

    // ─── Слои ───
    private bool _showLayer1 = true;
    private bool _showLayer2 = true;

    // ─── NPC спрайты ───
    private List<string> _npcSpriteNames = new();

    // ─── Защита от рекурсии при обновлении полей ───
    private bool _suppressFieldEvents;

    // ══════════════════════════════════════════════════════════════
    //   КОНСТРУКТОР
    // ══════════════════════════════════════════════════════════════

    public EventEditorView()
    {
        InitializeComponent();

        // Начальный раздел — Roof
        _currentSection = "roof";
        UpdateSectionButtons();
        UpdatePanelVisibility();

        // Подписка на клик по канвасу
        CanvasControl.TileClicked += OnCanvasTileClicked;

        // Загрузить проект (метод из Map.cs)
        LoadProject();
    }

    // ══════════════════════════════════════════════════════════════
    //   IEditor
    // ══════════════════════════════════════════════════════════════

    /// <summary>Сохранить события текущей карты.</summary>
    public void Save() => SaveCurrentEvents();

    /// <summary>Перечитать список карт и текущую карту.</summary>
    public void Reload() => LoadProject();

    public void OnActivate()   { }
    public void OnDeactivate() { }
}