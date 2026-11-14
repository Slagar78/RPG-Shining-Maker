using System;
using System.Collections.Generic;
using System.IO;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Threading;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;
using RpgShinzoMaker.Desktop.Shared.Interfaces;

namespace RpgShinzoMaker.Desktop.Editors.BattleSceneEditor;

public partial class BattleSceneEditorView : UserControl, IEditor
{
    public List<BattleEntry> Battles { get; set; } = new();
    public BattleEntry? CurrentBattle { get; set; }

    public AnimationSet AllyAnim { get; set; } = new();
    public AnimationSet EnemyAnim { get; set; } = new();

    public int ActiveSlot { get; set; } = 0;
    public int ActivePhase { get; set; } = 0;

    private DispatcherTimer _animTimer;
    private AnimationService _animService = new();
    
    public const float GameScreenW = 1152f;
    public const float GameScreenH = 672f;
    public const int TopBarH = 144;

    public BattleSceneEditorView()
    {
        InitializeComponent();
        InitializeInputs();

        _animTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
        _animTimer.Tick += OnAnimTimerTick;
        _animTimer.Start();
    }

    public event Action<string>? StatusChanged;

    public void OnActivate() => LoadBattleEntries();
    public void OnDeactivate() { }
    public void Reload() => LoadBattleEntries();

    public void Save()
    {
        SaveBattleEntries();
        _animService.Save(AllyAnim);
        _animService.Save(EnemyAnim);
        StatusChanged?.Invoke("Сцены сохранены!");
    }

    private void LoadBattleEntries()
    {
        // ★ УМНЫЙ ПОИСК ФАЙЛА ★
        string path = FindDataFile("data/battles/entries.json");
        
        if (!File.Exists(path))
        {
            StatusChanged?.Invoke($"Файл не найден: {path}");
            return;
        }

        try
        {
            var json = File.ReadAllText(path);
            var options = new System.Text.Json.JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true,
                PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.SnakeCaseLower
            };
            Battles = System.Text.Json.JsonSerializer.Deserialize<List<BattleEntry>>(json, options) ?? new();
            
            BattlesList.ItemsSource = Battles;
            if (Battles.Count > 0)
                BattlesList.SelectedIndex = 0;
        }
        catch (Exception ex)
        {
            StatusChanged?.Invoke($"Ошибка загрузки: {ex.Message}");
        }
    }

    private void SaveBattleEntries() { }

    private void OnBattleSelected(object? sender, SelectionChangedEventArgs e)
    {
        if (BattlesList.SelectedItem is BattleEntry be)
        {
            CurrentBattle = be;
            UpdatePreview();
        }
    }

    private void OnSaveClick(object? sender, RoutedEventArgs e) => Save();
    private void OnSaveAllyClick(object? sender, RoutedEventArgs e) => _animService.Save(AllyAnim);
    private void OnSaveEnemyClick(object? sender, RoutedEventArgs e) => _animService.Save(EnemyAnim);

    private void OnAnimTimerTick(object? sender, EventArgs e)
    {
        UpdateAnimState(AllyAnim);
        UpdateAnimState(EnemyAnim);
        UpdateInputsUI();
        UpdatePreview();
    }

    private void UpdateAnimState(AnimationSet set)
    {
        if (!set.Loaded) return;
        for (int p = 0; p < 3; p++)
        {
            var phase = set.GetPhase(p);
            if (phase.Animate && phase.Frames.Count > 0)
            {
                int idx = set.CurrentFrame[p] % phase.Frames.Count;
                float dur = phase.Frames[idx].Duration;
                set.AnimTimer += 16.0f / 1000.0f;

                if (set.AnimTimer >= dur)
                {
                    set.AnimTimer -= dur;
                    set.CurrentFrame[p] = (set.CurrentFrame[p] + 1) % phase.Frames.Count;
                }
            }
        }
    }

    // Ищем папку data вверх по дереву (решает проблему путей при сборке)
    private string FindDataFile(string relativePath)
    {
        string baseDir = AppDomain.CurrentDomain.BaseDirectory;
        var dir = new DirectoryInfo(baseDir);
        for (int i = 0; i < 10 && dir != null; i++)
        {
            string testPath = Path.Combine(dir.FullName, relativePath.Replace('/', Path.DirectorySeparatorChar));
            if (File.Exists(testPath)) return testPath;
            dir = dir.Parent;
        }
        return relativePath;
    }
}