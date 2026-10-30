using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.Mvvm.Messaging;
using RpgShinzoMaker.Core.Models.Database;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.ViewModels;

public class SpellsUpdatedMessage { }

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

        WeakReferenceMessenger.Default.Register<ItemsViewModel, SpellsUpdatedMessage>(this, (r, m) =>
        {
            r.LoadSpells();
        });

        LoadData();
    }

    public void LoadData()
    {
        LoadSpells();

        var items = _itemsService.LoadItems(_itemsPath);
        Items.Clear();
        foreach (var item in items) Items.Add(item);

        if (Items.Count > 0) SelectedItem = Items[0];
    }

    public void LoadSpells()
    {
        var spells = _itemsService.LoadSpells(_spellsPath);
        string? currentSelected = SelectedItem?.UseSpell;

        AvailableSpells.Clear();
        foreach (var spellName in spells.Select(s => s.Name).Distinct())
        {
            AvailableSpells.Add(spellName);
        }
        if (AvailableSpells.Count == 0) AvailableSpells.Add("HEAL");

        if (SelectedItem != null && !string.IsNullOrWhiteSpace(currentSelected))
        {
            if (!AvailableSpells.Contains(currentSelected))
            {
                AvailableSpells.Add(currentSelected);
            }
        }
    }

    public void SaveData()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(_itemsPath)!);
        _itemsService.SaveItems(_itemsPath, Items.ToList());
    }

    [RelayCommand]
    private void AddItem()
    {
        var existingIds = Items.Select(i => i.Id).ToHashSet();
        int newId = 0;
        while (existingIds.Contains(newId)) { newId++; }

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

        int insertIndex = 0;
        while (insertIndex < Items.Count && Items[insertIndex].Id < newId)
        {
            insertIndex++;
        }

        Items.Insert(insertIndex, newItem);
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