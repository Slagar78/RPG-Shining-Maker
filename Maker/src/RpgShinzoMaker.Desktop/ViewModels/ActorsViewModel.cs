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
    private readonly ItemsService _itemsService = new();
    private readonly ClassesService _classesService = new(); // <--- СЕРВИС КЛАССОВ
    
    private readonly string _actorsPath;
    private readonly string _inventoryPath;
    private readonly string _classesPath;
    private readonly string _portraitsDir;
    private readonly string _mapspritesDir;
    private readonly string _itemsPath;

    public ObservableCollection<Actor> Actors { get; } = new();
    public ObservableCollection<CharacterClass> AvailableClasses { get; } = new();
    public ObservableCollection<string> AvailablePortraits { get; } = new();
    public ObservableCollection<string> AvailableMapsprites { get; } = new();
    public ObservableCollection<string> AvailableItems { get; } = new();

    private List<StartInventory> _startInventories = new();

    [ObservableProperty]
    private Actor? _selectedActor;

    [ObservableProperty]
    private StartInventory? _currentInventory;

    public ActorsViewModel()
    {
        _actorsPath = Path.Combine(ProjectPaths.DataDir, "actors", "actors.json");
        _inventoryPath = Path.Combine(ProjectPaths.DataDir, "actors", "start_inventory.json");
        _classesPath = Path.Combine(ProjectPaths.DataDir, "actors", "classes.json");
        _portraitsDir = Path.Combine(ProjectPaths.AssetsDir, "ui", "portraits");
        _mapspritesDir = Path.Combine(ProjectPaths.AssetsDir, "mapsprites");
        _itemsPath = Path.Combine(ProjectPaths.DataDir, "items", "items.json");

        LoadData();
    }

    partial void OnSelectedActorChanged(Actor? value)
    {
        LoadActorInventory();
    }

    public void LoadData()
    {
        // ★★★ ТЕПЕРЬ ИСПОЛЬЗУЕМ _classesService ★★★
        var classes = _classesService.LoadClasses(_classesPath);
        AvailableClasses.Clear();
        foreach (var c in classes) AvailableClasses.Add(c);
        if (AvailableClasses.Count == 0) AvailableClasses.Add(new CharacterClass { Id = 0, Name = "Hero" });

        var portraits = _assetScanner.GetPortraits(_portraitsDir);
        AvailablePortraits.Clear();
        foreach (var p in portraits) AvailablePortraits.Add(p);
        if (AvailablePortraits.Count == 0) AvailablePortraits.Add("Mushra");

        var mapsprites = _assetScanner.GetMapsprites(_mapspritesDir);
        AvailableMapsprites.Clear();
        foreach (var m in mapsprites) AvailableMapsprites.Add(m);
        if (AvailableMapsprites.Count == 0) AvailableMapsprites.Add("mapsprite000");

        var items = _itemsService.LoadItems(_itemsPath);
        AvailableItems.Clear();
        AvailableItems.Add("NOTHING");
        foreach (var item in items)
        {
            if (!AvailableItems.Contains(item.Name)) AvailableItems.Add(item.Name);
        }

        var actors = _actorsService.LoadActors(_actorsPath);
        Actors.Clear();
        if (actors != null && actors.Count > 0)
        {
            foreach (var a in actors) Actors.Add(a);
        }
        else
        {
            Actors.Add(new Actor { Id = 0, Name = "Harold", ClassId = 0, Level = 1, Portrait = "Mushra", Mapsprite = "mapsprite000" });
        }

        _startInventories = _actorsService.LoadStartInventory(_inventoryPath);

        // Собираем все предметы из инвентарей, чтобы ComboBox их видел
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
        Directory.CreateDirectory(Path.GetDirectoryName(_actorsPath)!);
        Directory.CreateDirectory(Path.GetDirectoryName(_inventoryPath)!);

        _actorsService.SaveActors(_actorsPath, Actors.ToList());
        _actorsService.SaveStartInventory(_inventoryPath, _startInventories);
    }

    private void LoadActorInventory()
    {
        if (SelectedActor == null)
        {
            CurrentInventory = null;
            return;
        }

        var inv = _startInventories.FirstOrDefault(i => i.ActorId == SelectedActor.Id);
        
        if (inv == null)
        {
            inv = new StartInventory
            {
                ActorId = SelectedActor.Id,
                StartLevel = 1,
                Items = new List<StartItem>
                {
                    new() { Item = "NOTHING", Equipped = false },
                    new() { Item = "NOTHING", Equipped = false },
                    new() { Item = "NOTHING", Equipped = false },
                    new() { Item = "NOTHING", Equipped = false }
                }
            };
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
            ClassId = AvailableClasses.FirstOrDefault()?.Id ?? 0,
            Portrait = AvailablePortraits.FirstOrDefault() ?? "Mushra",
            Mapsprite = AvailableMapsprites.FirstOrDefault() ?? "mapsprite000",
            Level = 1
        };

        int insertIndex = 0;
        while (insertIndex < Actors.Count && Actors[insertIndex].Id < newId)
        {
            insertIndex++;
        }

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

    [RelayCommand] private void IncrementLevel() { if (SelectedActor != null && SelectedActor.Level < 999) SelectedActor.Level++; }
    [RelayCommand] private void DecrementLevel() { if (SelectedActor != null && SelectedActor.Level > 1) SelectedActor.Level--; }
    [RelayCommand] private void IncrementStartLevel() { if (CurrentInventory != null && CurrentInventory.StartLevel < 999) CurrentInventory.StartLevel++; }
    [RelayCommand] private void DecrementStartLevel() { if (CurrentInventory != null && CurrentInventory.StartLevel > 1) CurrentInventory.StartLevel--; }
}