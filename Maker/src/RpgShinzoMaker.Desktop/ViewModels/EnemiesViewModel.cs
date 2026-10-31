using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using RpgShinzoMaker.Core.Models.Database;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.ViewModels;

public partial class EnemiesViewModel : ObservableObject
{
    private readonly EnemiesService _enemiesService = new();
    private readonly AssetScannerService _assetScanner = new();
    private readonly SpellsService _spellsService = new();
    private readonly ClassesService _classesService = new();
    private readonly ItemsService _itemsService = new();

    private readonly string _enemiesPath;
    private readonly string _classesPath;
    private readonly string _spellsPath;
    private readonly string _itemsPath;
    private readonly string _movetypesPath;
    private readonly string _portraitsDir;
    private readonly string _mapspritesDir;

    public ObservableCollection<Enemy> Enemies { get; } = new();

    // Справочники для ComboBox
    public ObservableCollection<string> AvailablePortraits { get; } = new();
    public ObservableCollection<string> AvailableMapsprites { get; } = new();
    public ObservableCollection<string> AvailableItems { get; } = new();
    public ObservableCollection<string> AvailableSpells { get; } = new();
    public ObservableCollection<string> AvailableRaces { get; } = new();
    public ObservableCollection<string> AvailableMovetypes { get; } = new();

    public ObservableCollection<string> AvailableResistances { get; } = new()
        { "none", "minor", "major", "weakness", "immunity" };

    public ObservableCollection<string> AvailableSpellPowers { get; } = new()
        { "regular", "enhanced", "powerful" };

    public ObservableCollection<string> AvailableStatuses { get; } = new()
        { "normal", "poisoned", "sleeping", "silenced", "stunned", "confused", "charmed", "berserk" };

    public ObservableCollection<string> Curves { get; } = new()
        { "LINEAR", "LATE", "EARLY", "MIDDLE", "EARLYANDLATE", "VERY_LATE", "MIDDLE_PEAK", "NONE" };

    [ObservableProperty] private Enemy? _selectedEnemy;

    public EnemiesViewModel()
    {
        _enemiesPath  = Path.Combine(ProjectPaths.DataDir, "enemies", "enemies.json");
        _classesPath  = Path.Combine(ProjectPaths.DataDir, "actors", "classes.json");
        _spellsPath   = Path.Combine(ProjectPaths.DataDir, "spells", "spells.json");
        _itemsPath    = Path.Combine(ProjectPaths.DataDir, "items", "items.json");
        _movetypesPath = Path.Combine(ProjectPaths.DataDir, "movetypes.json");
        _portraitsDir = Path.Combine(ProjectPaths.AssetsDir, "ui", "portraits");
        _mapspritesDir = Path.Combine(ProjectPaths.AssetsDir, "mapsprites");

        LoadData();
    }

    public void LoadData()
    {
        // Портреты и мапспрайты
        AvailablePortraits.Clear();
        foreach (var p in _assetScanner.GetPortraits(_portraitsDir)) AvailablePortraits.Add(p);

        AvailableMapsprites.Clear();
        foreach (var m in _assetScanner.GetMapsprites(_mapspritesDir)) AvailableMapsprites.Add(m);

        // Предметы — по именам, "Nothing" первым (Title Case — как у врагов)
        AvailableItems.Clear();
        AvailableItems.Add(ProwessCatalog.EmptySlot);
        foreach (var item in _itemsService.LoadItems(_itemsPath))
            if (!string.IsNullOrWhiteSpace(item.Name) && !AvailableItems.Contains(item.Name))
                AvailableItems.Add(item.Name);

        // Заклинания — по именам
        AvailableSpells.Clear();
        AvailableSpells.Add(ProwessCatalog.EmptySlot);
        foreach (var spell in _spellsService.LoadSpells(_spellsPath))
            if (!string.IsNullOrWhiteSpace(spell.Name) && !AvailableSpells.Contains(spell.Name))
                AvailableSpells.Add(spell.Name);

        // Расы — Distinct из classes.json
        AvailableRaces.Clear();
        foreach (var race in _classesService.LoadClasses(_classesPath)
                     .Select(c => c.Race)
                     .Where(r => !string.IsNullOrWhiteSpace(r))
                     .Distinct()
                     .OrderBy(r => r))
            AvailableRaces.Add(race);

        // Типы передвижения — из movetypes.json (поле "key")
        LoadMovetypes();

        // Загружаем врагов
        var enemies = _enemiesService.LoadEnemies(_enemiesPath);
        Enemies.Clear();
        foreach (var e in enemies) Enemies.Add(e);

        // Дополняем справочники именами из уже загруженных врагов
        // (на случай кастомных предметов/магии, которых нет в каталоге)
        foreach (var e in Enemies)
        {
            foreach (var it in e.Items)
                if (!string.IsNullOrWhiteSpace(it.Item) && !AvailableItems.Contains(it.Item))
                    AvailableItems.Add(it.Item);

            foreach (var sp in e.SpellSlots)
                if (!string.IsNullOrWhiteSpace(sp.Name) && !AvailableSpells.Contains(sp.Name))
                    AvailableSpells.Add(sp.Name);
        }

        if (Enemies.Count > 0) SelectedEnemy = Enemies[0];
    }

    private void LoadMovetypes()
    {
        AvailableMovetypes.Clear();
        if (!File.Exists(_movetypesPath)) return;
        try
        {
            var root = JsonNode.Parse(File.ReadAllText(_movetypesPath))?.AsArray();
            if (root == null) return;
            foreach (var node in root)
            {
                var key = node?["key"]?.GetValue<string>();
                if (!string.IsNullOrWhiteSpace(key) && !AvailableMovetypes.Contains(key))
                    AvailableMovetypes.Add(key);
            }
        }
        catch
        {
            // битый файл — просто оставляем пустым
        }
    }

    public void SaveData()
    {
        _enemiesService.SaveEnemies(_enemiesPath, Enemies.ToList());
    }

    // ==========================================
    // СПИСОК ВРАГОВ
    // ==========================================
    [RelayCommand]
    private void AddEnemy()
    {
        var usedIds = Enemies.Select(e => e.Id).ToHashSet();
        int newId = 0;
        while (usedIds.Contains(newId)) newId++;

        var enemy = new Enemy
        {
            Id = newId,
            Name = $"Enemy{newId:D2}",
            Portrait = AvailablePortraits.FirstOrDefault() ?? "",
            Mapsprite = AvailableMapsprites.FirstOrDefault() ?? "",
            Race = AvailableRaces.FirstOrDefault() ?? "",
            MoveType = AvailableMovetypes.FirstOrDefault() ?? "normal",
            Status = "normal",
            SpellPower = "regular",
            InitialStatus = "none",
        };

        for (int i = 0; i < 4; i++)
            enemy.Items.Add(new EnemyItem { Item = ProwessCatalog.EmptySlot, Equipped = false });
        for (int i = 0; i < 4; i++)
            enemy.SpellSlots.Add(new EnemySpellSlot { Name = ProwessCatalog.EmptySlot, Level = 1 });

        int insert = 0;
        while (insert < Enemies.Count && Enemies[insert].Id < newId) insert++;
        Enemies.Insert(insert, enemy);
        SelectedEnemy = enemy;
    }

    [RelayCommand]
    private void DeleteEnemy()
    {
        if (SelectedEnemy == null) return;
        Enemies.Remove(SelectedEnemy);
        SelectedEnemy = Enemies.FirstOrDefault();
    }

    // ==========================================
    // PROWESS
    // ==========================================
    [RelayCommand]
    private void AddProwess()
    {
        if (SelectedEnemy == null) return;
        SelectedEnemy.Prowess.Add(ProwessCatalog.All.FirstOrDefault() ?? "");
    }

    [RelayCommand]
    private void RemoveProwess(string prowess)
    {
        if (SelectedEnemy == null || prowess == null) return;
        SelectedEnemy.Prowess.Remove(prowess);
    }

    // ==========================================
    // УРОВЕНЬ ЗАКЛИНАНИЯ В СЛОТЕ
    // ==========================================
    [RelayCommand] private void IncSpellSlotLevel(EnemySpellSlot slot) { if (slot != null) slot.Level++; }
    [RelayCommand] private void DecSpellSlotLevel(EnemySpellSlot slot) { if (slot != null && slot.Level > 1) slot.Level--; }

    // ==========================================
    // СТАТЫ: HP
    // ==========================================
    [RelayCommand] private void IncHpStart() { if (SelectedEnemy != null) SelectedEnemy.Stats.HpGrowth.Start++; }
    [RelayCommand] private void DecHpStart() { if (SelectedEnemy != null && SelectedEnemy.Stats.HpGrowth.Start > 0) SelectedEnemy.Stats.HpGrowth.Start--; }
    [RelayCommand] private void IncHpProj()  { if (SelectedEnemy != null) SelectedEnemy.Stats.HpGrowth.Projected++; }
    [RelayCommand] private void DecHpProj()  { if (SelectedEnemy != null && SelectedEnemy.Stats.HpGrowth.Projected > 0) SelectedEnemy.Stats.HpGrowth.Projected--; }

    // MP
    [RelayCommand] private void IncMpStart() { if (SelectedEnemy != null) SelectedEnemy.Stats.MpGrowth.Start++; }
    [RelayCommand] private void DecMpStart() { if (SelectedEnemy != null && SelectedEnemy.Stats.MpGrowth.Start > 0) SelectedEnemy.Stats.MpGrowth.Start--; }
    [RelayCommand] private void IncMpProj()  { if (SelectedEnemy != null) SelectedEnemy.Stats.MpGrowth.Projected++; }
    [RelayCommand] private void DecMpProj()  { if (SelectedEnemy != null && SelectedEnemy.Stats.MpGrowth.Projected > 0) SelectedEnemy.Stats.MpGrowth.Projected--; }

    // ATK
    [RelayCommand] private void IncAtkStart() { if (SelectedEnemy != null) SelectedEnemy.Stats.AttackGrowth.Start++; }
    [RelayCommand] private void DecAtkStart() { if (SelectedEnemy != null && SelectedEnemy.Stats.AttackGrowth.Start > 0) SelectedEnemy.Stats.AttackGrowth.Start--; }
    [RelayCommand] private void IncAtkProj()  { if (SelectedEnemy != null) SelectedEnemy.Stats.AttackGrowth.Projected++; }
    [RelayCommand] private void DecAtkProj()  { if (SelectedEnemy != null && SelectedEnemy.Stats.AttackGrowth.Projected > 0) SelectedEnemy.Stats.AttackGrowth.Projected--; }

    // DEF
    [RelayCommand] private void IncDefStart() { if (SelectedEnemy != null) SelectedEnemy.Stats.DefenseGrowth.Start++; }
    [RelayCommand] private void DecDefStart() { if (SelectedEnemy != null && SelectedEnemy.Stats.DefenseGrowth.Start > 0) SelectedEnemy.Stats.DefenseGrowth.Start--; }
    [RelayCommand] private void IncDefProj()  { if (SelectedEnemy != null) SelectedEnemy.Stats.DefenseGrowth.Projected++; }
    [RelayCommand] private void DecDefProj()  { if (SelectedEnemy != null && SelectedEnemy.Stats.DefenseGrowth.Projected > 0) SelectedEnemy.Stats.DefenseGrowth.Projected--; }

    // AGI
    [RelayCommand] private void IncAgiStart() { if (SelectedEnemy != null) SelectedEnemy.Stats.AgilityGrowth.Start++; }
    [RelayCommand] private void DecAgiStart() { if (SelectedEnemy != null && SelectedEnemy.Stats.AgilityGrowth.Start > 0) SelectedEnemy.Stats.AgilityGrowth.Start--; }
    [RelayCommand] private void IncAgiProj()  { if (SelectedEnemy != null) SelectedEnemy.Stats.AgilityGrowth.Projected++; }
    [RelayCommand] private void DecAgiProj()  { if (SelectedEnemy != null && SelectedEnemy.Stats.AgilityGrowth.Projected > 0) SelectedEnemy.Stats.AgilityGrowth.Projected--; }

    // MOV (плоский int, без growth)
    [RelayCommand] private void IncMov() { if (SelectedEnemy != null) SelectedEnemy.Stats.BaseMov++; }
    [RelayCommand] private void DecMov() { if (SelectedEnemy != null && SelectedEnemy.Stats.BaseMov > 1) SelectedEnemy.Stats.BaseMov--; }

    // ==========================================
    // LEVEL (в битве назначается, пока только редактируемое поле)
    // ==========================================
    [RelayCommand] private void IncLevel() { if (SelectedEnemy != null) SelectedEnemy.Level++; }
    [RelayCommand] private void DecLevel() { if (SelectedEnemy != null && SelectedEnemy.Level > 1) SelectedEnemy.Level--; }
}