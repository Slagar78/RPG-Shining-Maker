using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
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
    
    // Пути к файлам
    private readonly string _actorsPath = "data/actors/actors.json";
    private readonly string _inventoryPath = "data/actors/start_inventory.json";
    private readonly string _classesPath = "data/actors/classes.json";
    private readonly string _portraitsDir = "assets/ui/portraits";
    private readonly string _mapspritesDir = "assets/mapsprites";

    public ObservableCollection<Actor> Actors { get; } = new();
    public ObservableCollection<CharacterClass> AvailableClasses { get; } = new();
    public ObservableCollection<string> AvailablePortraits { get; } = new();
    public ObservableCollection<string> AvailableMapsprites { get; } = new();
    
    // Для выпадающих списков предметов в стартовом инвентаре
    public ObservableCollection<string> AvailableItems { get; } = new() { "NOTHING" };

    private List<StartInventory> _startInventories = new();

    [ObservableProperty]
    private Actor? _selectedActor;

    [ObservableProperty]
    private StartInventory? _currentInventory;

    public ActorsViewModel()
    {
        LoadData();
    }

    // Этот метод автоматически вызовется при смене SelectedActor
    partial void OnSelectedActorChanged(Actor? value)
    {
        LoadActorInventory();
    }

    public void LoadData()
    {
        // 1. Загружаем классы
        var classes = _actorsService.LoadClasses(_classesPath);
        AvailableClasses.Clear();
        foreach (var c in classes) AvailableClasses.Add(c);

        // 2. Сканируем картинки
        var portraits = _assetScanner.GetPortraits(_portraitsDir);
        AvailablePortraits.Clear();
        foreach (var p in portraits) AvailablePortraits.Add(p);

        var mapsprites = _assetScanner.GetMapsprites(_mapspritesDir);
        AvailableMapsprites.Clear();
        foreach (var m in mapsprites) AvailableMapsprites.Add(m);

        // 3. Загружаем актеров
        var actors = _actorsService.LoadActors(_actorsPath);
        Actors.Clear();
        foreach (var a in actors) Actors.Add(a);

        // 4. Загружаем стартовые инвентари
        _startInventories = _actorsService.LoadStartInventory(_inventoryPath);

        // Выбираем первого
        if (Actors.Count > 0) SelectedActor = Actors[0];
    }

    public void SaveData()
    {
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

        CurrentInventory = inv;
    }

    [RelayCommand]
    private void AddActor()
    {
        int newId = Actors.Count > 0 ? Actors.Max(a => a.Id) + 1 : 0;
        
        var newActor = new Actor
        {
            Id = newId,
            Name = $"Actor{newId:D2}",
            ClassId = AvailableClasses.FirstOrDefault()?.Id ?? 0,
            Portrait = AvailablePortraits.FirstOrDefault() ?? "Mushra",
            Mapsprite = AvailableMapsprites.FirstOrDefault() ?? "mapsprite000",
            Level = 1
        };

        Actors.Add(newActor);
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
}