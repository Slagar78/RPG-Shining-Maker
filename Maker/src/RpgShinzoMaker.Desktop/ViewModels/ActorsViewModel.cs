using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using RpgShinzoMaker.Core.Models.Database;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.ViewModels;

public partial class ActorsViewModel : ObservableObject
{
    private readonly ActorsService _actorsService = new();
    private readonly AssetScannerService _assetScanner = new();
    private readonly SpellsService _spellsService = new();
    private readonly ClassesService _classesService = new();
    private readonly ItemsService _itemsService = new();
    
    private readonly string _actorsPath;
    private readonly string _inventoryPath;
    private readonly string _classesPath;
    private readonly string _portraitsDir;
    private readonly string _mapspritesDir;
    private readonly string _spellsPath;
    private readonly string _itemsPath;

    public ObservableCollection<Actor> Actors { get; } = new();
    public ObservableCollection<CharacterClass> AvailableClasses { get; } = new();
    public ObservableCollection<string> AvailablePortraits { get; } = new();
    public ObservableCollection<string> AvailableMapsprites { get; } = new();
    public ObservableCollection<string> AvailableItems { get; } = new();
    public ObservableCollection<Spell> AvailableSpells { get; } = new();
    public ObservableCollection<string> Curves { get; } = new() { "LINEAR", "LATE", "EARLY", "MIDDLE", "EARLYANDLATE", "VERY_LATE", "MIDDLE_PEAK", "NONE" };

    public ObservableCollection<ActorBlock> BlocksForSelectedActor { get; } = new();

    private List<StartInventory> _startInventories = new();

    [ObservableProperty] private Actor? _selectedActor;
    [ObservableProperty] private ActorBlock? _selectedBlock;
    [ObservableProperty] private StartInventory? _currentInventory;

    public ActorsViewModel()
    {
        _actorsPath = Path.Combine(ProjectPaths.DataDir, "actors", "actors.json");
        _inventoryPath = Path.Combine(ProjectPaths.DataDir, "actors", "start_inventory.json");
        _classesPath = Path.Combine(ProjectPaths.DataDir, "actors", "classes.json");
        _spellsPath = Path.Combine(ProjectPaths.DataDir, "spells", "spells.json");
        _itemsPath = Path.Combine(ProjectPaths.DataDir, "items", "items.json");
        _portraitsDir = Path.Combine(ProjectPaths.AssetsDir, "ui", "portraits");
        _mapspritesDir = Path.Combine(ProjectPaths.AssetsDir, "mapsprites");

        LoadData();
    }

    partial void OnSelectedActorChanged(Actor? value)
    {
        LoadActorInventory();
        
        BlocksForSelectedActor.Clear();
        if (value != null)
        {
            BlocksForSelectedActor.Add(value);
            foreach (var promo in value.Promotions)
                BlocksForSelectedActor.Add(promo);
            SelectedBlock = value;
        }
        else
        {
            SelectedBlock = null;
        }
    }

    public void LoadData()
    {
        var classes = _classesService.LoadClasses(_classesPath);
        AvailableClasses.Clear();
        foreach (var c in classes) AvailableClasses.Add(c);

        var spells = _spellsService.LoadSpells(_spellsPath);
        AvailableSpells.Clear();
        foreach (var s in spells) AvailableSpells.Add(s);

        var portraits = _assetScanner.GetPortraits(_portraitsDir);
        AvailablePortraits.Clear();
        foreach (var p in portraits) AvailablePortraits.Add(p);

        var mapsprites = _assetScanner.GetMapsprites(_mapspritesDir);
        AvailableMapsprites.Clear();
        foreach (var m in mapsprites) AvailableMapsprites.Add(m);

        var items = _itemsService.LoadItems(_itemsPath);
        AvailableItems.Clear();
        AvailableItems.Add("NOTHING");
        foreach (var item in items)
        {
            if (!AvailableItems.Contains(item.Name)) AvailableItems.Add(item.Name);
        }

        var actors = _actorsService.LoadActors(_actorsPath);
        Actors.Clear();
        foreach (var a in actors) Actors.Add(a);

        _startInventories = _actorsService.LoadStartInventory(_inventoryPath);
        
        foreach (var inv in _startInventories)
        {
            if (inv.Items == null) continue;
            foreach (var slot in inv.Items)
            {
                if (!string.IsNullOrWhiteSpace(slot.Item) && slot.Item != "NOTHING" && !AvailableItems.Contains(slot.Item))
                {
                    AvailableItems.Add(slot.Item);
                }
            }
        }

        if (Actors.Count > 0) SelectedActor = Actors[0];
    }

    public void SaveData()
    {
        _actorsService.SaveActors(_actorsPath, Actors.ToList());
        _actorsService.SaveStartInventory(_inventoryPath, _startInventories);
    }

    private void LoadActorInventory()
    {
        if (SelectedActor == null) { CurrentInventory = null; return; }
        
        var inv = _startInventories.FirstOrDefault(i => i.ActorId == SelectedActor.Id);
        if (inv == null)
        {
            inv = new StartInventory 
            { 
                ActorId = SelectedActor.Id, 
                StartLevel = 1, 
                Items = new List<StartItem>() 
            };
            for(int i=0; i<4; i++) 
                inv.Items.Add(new StartItem { Item = "NOTHING" });
            
            _startInventories.Add(inv);
        }
        
        if (inv.Items != null)
        {
            foreach (var slot in inv.Items)
            {
                if (!string.IsNullOrWhiteSpace(slot.Item) && !AvailableItems.Contains(slot.Item))
                {
                    AvailableItems.Add(slot.Item);
                }
            }
        }
        
        CurrentInventory = inv;
    }

    [RelayCommand]
    private void AddActor()
    {
        var existingIds = Actors.Select(a => a.Id).ToHashSet();
        int newId = 0;
        while (existingIds.Contains(newId)) { newId++; }

        var newActor = new Actor
        {
            Id = newId,
            Name = $"Actor{newId:D2}",
            ForClass = AvailableClasses.FirstOrDefault()?.Id ?? 0,
            Portrait = AvailablePortraits.FirstOrDefault() ?? "Mushra",
            Mapsprite = AvailableMapsprites.FirstOrDefault() ?? "mapsprite000",
            Exp = 0,
            Kills = 0,
            Defeats = 0
        };

        int insertIndex = 0;
        while (insertIndex < Actors.Count && Actors[insertIndex].Id < newId)
            insertIndex++;

        Actors.Insert(insertIndex, newActor);
        SelectedActor = newActor;
    }

    [RelayCommand]
    private void DeleteActor()
    {
        if (SelectedActor == null) return;

        var inv = _startInventories.FirstOrDefault(i => i.ActorId == SelectedActor.Id);
        if (inv != null) _startInventories.Remove(inv);

        Actors.Remove(SelectedActor);
        SelectedActor = Actors.FirstOrDefault();
    }

    [RelayCommand]
    private void AddPromotion()
    {
        if (SelectedActor == null) return;
        var newPromo = new ActorPromotion
        {
            ForClass = AvailableClasses.FirstOrDefault()?.Id ?? 0,
            Portrait = SelectedActor.Portrait,
            Mapsprite = SelectedActor.Mapsprite,
            BattleSpritePath = SelectedActor.BattleSpritePath
        };
        SelectedActor.Promotions.Add(newPromo);
        BlocksForSelectedActor.Add(newPromo);
        SelectedBlock = newPromo;
    }

    [RelayCommand]
    private void DeletePromotion()
    {
        if (SelectedActor == null || SelectedBlock is not ActorPromotion promo) return;
        SelectedActor.Promotions.Remove(promo);
        BlocksForSelectedActor.Remove(promo);
        SelectedBlock = SelectedActor;
    }

    [RelayCommand]
    private void AddSpell()
    {
        if (SelectedBlock == null) return;
        SelectedBlock.SpellList.Add(new ClassSpell { Level = 1, SpellId = AvailableSpells.FirstOrDefault()?.Id ?? 0, SpellLevel = 1 });
    }

    [RelayCommand]
    private void RemoveSpell(ClassSpell spell)
    {
        if (SelectedBlock == null || spell == null) return;
        SelectedBlock.SpellList.Remove(spell);
    }

    // ==========================================
    // КОМАНДЫ ДЛЯ СТРЕЛОЧЕК (АКТЁР)
    // ==========================================
    [RelayCommand] private void IncrementExp() { if (SelectedActor != null) SelectedActor.Exp++; }
    [RelayCommand] private void DecrementExp() { if (SelectedActor != null && SelectedActor.Exp > 0) SelectedActor.Exp--; }

    [RelayCommand] private void IncrementKills() { if (SelectedActor != null) SelectedActor.Kills++; }
    [RelayCommand] private void DecrementKills() { if (SelectedActor != null && SelectedActor.Kills > 0) SelectedActor.Kills--; }

    [RelayCommand] private void IncrementDefeats() { if (SelectedActor != null) SelectedActor.Defeats++; }
    [RelayCommand] private void DecrementDefeats() { if (SelectedActor != null && SelectedActor.Defeats > 0) SelectedActor.Defeats--; }

    [RelayCommand] private void IncrementStartLevel() { if (CurrentInventory != null) CurrentInventory.StartLevel++; }
    [RelayCommand] private void DecrementStartLevel() { if (CurrentInventory != null && CurrentInventory.StartLevel > 1) CurrentInventory.StartLevel--; }

    // ==========================================
    // КОМАНДЫ ДЛЯ СТРЕЛОЧЕК (STATS)
    // ==========================================
    [RelayCommand] private void IncHpStart() { if (SelectedBlock != null) SelectedBlock.HpGrowth.Start++; }
    [RelayCommand] private void DecHpStart() { if (SelectedBlock != null && SelectedBlock.HpGrowth.Start > 0) SelectedBlock.HpGrowth.Start--; }
    [RelayCommand] private void IncHpProj() { if (SelectedBlock != null) SelectedBlock.HpGrowth.Projected++; }
    [RelayCommand] private void DecHpProj() { if (SelectedBlock != null && SelectedBlock.HpGrowth.Projected > 0) SelectedBlock.HpGrowth.Projected--; }

    [RelayCommand] private void IncMpStart() { if (SelectedBlock != null) SelectedBlock.MpGrowth.Start++; }
    [RelayCommand] private void DecMpStart() { if (SelectedBlock != null && SelectedBlock.MpGrowth.Start > 0) SelectedBlock.MpGrowth.Start--; }
    [RelayCommand] private void IncMpProj() { if (SelectedBlock != null) SelectedBlock.MpGrowth.Projected++; }
    [RelayCommand] private void DecMpProj() { if (SelectedBlock != null && SelectedBlock.MpGrowth.Projected > 0) SelectedBlock.MpGrowth.Projected--; }

    [RelayCommand] private void IncAtkStart() { if (SelectedBlock != null) SelectedBlock.AttackGrowth.Start++; }
    [RelayCommand] private void DecAtkStart() { if (SelectedBlock != null && SelectedBlock.AttackGrowth.Start > 0) SelectedBlock.AttackGrowth.Start--; }
    [RelayCommand] private void IncAtkProj() { if (SelectedBlock != null) SelectedBlock.AttackGrowth.Projected++; }
    [RelayCommand] private void DecAtkProj() { if (SelectedBlock != null && SelectedBlock.AttackGrowth.Projected > 0) SelectedBlock.AttackGrowth.Projected--; }

    [RelayCommand] private void IncDefStart() { if (SelectedBlock != null) SelectedBlock.DefenseGrowth.Start++; }
    [RelayCommand] private void DecDefStart() { if (SelectedBlock != null && SelectedBlock.DefenseGrowth.Start > 0) SelectedBlock.DefenseGrowth.Start--; }
    [RelayCommand] private void IncDefProj() { if (SelectedBlock != null) SelectedBlock.DefenseGrowth.Projected++; }
    [RelayCommand] private void DecDefProj() { if (SelectedBlock != null && SelectedBlock.DefenseGrowth.Projected > 0) SelectedBlock.DefenseGrowth.Projected--; }

    [RelayCommand] private void IncAgiStart() { if (SelectedBlock != null) SelectedBlock.AgilityGrowth.Start++; }
    [RelayCommand] private void DecAgiStart() { if (SelectedBlock != null && SelectedBlock.AgilityGrowth.Start > 0) SelectedBlock.AgilityGrowth.Start--; }
    [RelayCommand] private void IncAgiProj() { if (SelectedBlock != null) SelectedBlock.AgilityGrowth.Projected++; }
    [RelayCommand] private void DecAgiProj() { if (SelectedBlock != null && SelectedBlock.AgilityGrowth.Projected > 0) SelectedBlock.AgilityGrowth.Projected--; }

    // ==========================================
    // КОМАНДЫ ДЛЯ СТРЕЛОЧЕК (SPELL LIST)
    // ==========================================
    [RelayCommand] private void IncSpellLevel(ClassSpell spell) { if (spell != null) spell.Level++; }
    [RelayCommand] private void DecSpellLevel(ClassSpell spell) { if (spell != null && spell.Level > 1) spell.Level--; }
    
    [RelayCommand] private void IncSpellLevelCmd(ClassSpell spell) { if (spell != null) spell.SpellLevel++; }
    [RelayCommand] private void DecSpellLevelCmd(ClassSpell spell) { if (spell != null && spell.SpellLevel > 1) spell.SpellLevel--; }
}