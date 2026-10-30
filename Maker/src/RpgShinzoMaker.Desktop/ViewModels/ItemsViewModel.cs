using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using RpgShinzoMaker.Core.Models.Database;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.ViewModels;

public partial class ItemsViewModel : ObservableObject
{
    private readonly ItemsService _itemsService = new();
    
    private readonly string _itemsPath;
    private readonly string _spellsPath;

    public ObservableCollection<Item> Items { get; } = new();
    public ObservableCollection<string> Categories { get; } = new() { "consumable", "Weapon", "Ring" };
    public ObservableCollection<string> AvailableSpells { get; } = new();

    [ObservableProperty]
    private Item? _selectedItem;

    public ItemsViewModel()
    {
        _itemsPath = Path.Combine(ProjectPaths.DataDir, "items", "items.json");
        _spellsPath = Path.Combine(ProjectPaths.DataDir, "spells", "spells.json");

        LoadData();
    }

    public void LoadData()
    {
        // Загружаем список заклинаний для ComboBox
        var spells = _itemsService.LoadSpells(_spellsPath);
        AvailableSpells.Clear();
        foreach (var spellName in spells.Select(s => s.Name).Distinct())
        {
            AvailableSpells.Add(spellName);
        }
        if (AvailableSpells.Count == 0) AvailableSpells.Add("HEAL");

        // Загружаем предметы
        var items = _itemsService.LoadItems(_itemsPath);
        Items.Clear();
        foreach (var item in items) Items.Add(item);

        if (Items.Count > 0) SelectedItem = Items[0];
    }

    public void SaveData()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(_itemsPath)!);
        _itemsService.SaveItems(_itemsPath, Items.ToList());
    }

    [RelayCommand]
    private void AddItem()
    {
        int newId = Items.Count > 0 ? Items.Max(i => i.Id) + 1 : 0;
        
        var newItem = new Item
        {
            Id = newId,
            Name = $"Item{newId:D2}",
            Category = "consumable",
            Price = 10,
            RangeMin = 0,
            RangeMax = 1,
            UseSpell = AvailableSpells.FirstOrDefault() ?? "HEAL",
            SpellLevel = 1,
            Icon = "assets/items/empty.png"
        };

        Items.Add(newItem);
        SelectedItem = newItem;
    }

    [RelayCommand]
    private void DeleteItem()
    {
        if (SelectedItem == null) return;
        Items.Remove(SelectedItem);
        SelectedItem = Items.FirstOrDefault();
    }
}