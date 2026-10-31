using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.Mvvm.Messaging;
using RpgShinzoMaker.Core.Models.Database;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.ViewModels;

public partial class SpellsViewModel : ObservableObject
{
    private readonly SpellsService _spellsService = new();
    private readonly string _spellsPath;
    private readonly string _iconsDir = "assets/spells";

    public ObservableCollection<Spell> Spells { get; } = new();
    public ObservableCollection<string> AvailableIcons { get; } = new();

    [ObservableProperty] private Spell? _selectedSpell;

    public SpellsViewModel()
    {
        _spellsPath = Path.Combine(ProjectPaths.DataDir, "spells", "spells.json");
        LoadData();
    }

    public void LoadData()
    {
        var iconPath = Path.Combine(ProjectPaths.Root, _iconsDir);
        AvailableIcons.Clear();
        if (Directory.Exists(iconPath))
        {
            foreach (var file in Directory.GetFiles(iconPath, "*.png"))
            {
                // Формируем путь вида "assets/spells/Heal.png"
                string relPath = $"{_iconsDir}/{Path.GetFileName(file)}".Replace("\\", "/");
                AvailableIcons.Add(relPath);
            }
        }
        if (AvailableIcons.Count == 0) AvailableIcons.Add("assets/spells/magic_empty.png");

        var loadedSpells = _spellsService.LoadSpells(_spellsPath);
        
        Spells.Clear();
        foreach (var spell in loadedSpells) Spells.Add(spell);

        if (Spells.Count > 0) SelectedSpell = Spells[0];
    }

    public void SaveData()
    {
        _spellsService.SaveSpells(_spellsPath, Spells.ToList());
        WeakReferenceMessenger.Default.Send(new SpellsUpdatedMessage());
    }

    [RelayCommand]
    private void AddSpell()
    {
        var existingIds = Spells.Select(s => s.Id).ToHashSet();
        int newId = 0;
        while (existingIds.Contains(newId)) { newId++; }

        string newName = "magic00";
        int idx = 0;
        while (Spells.Any(s => s.Name.Equals(newName, StringComparison.OrdinalIgnoreCase)))
        {
            idx++;
            newName = $"magic{idx:D2}";
        }

        var newSpell = new Spell
        {
            Id = newId,
            Name = newName,
            Type = "heal",
            Icon = AvailableIcons.FirstOrDefault() ?? "magic_empty.png"
        };
        
        // Добавляем базовый 1 уровень
        newSpell.Levels.Add(new SpellLevel { Level = 1, Mp = 5, Power = 5, Radius = 1, RangeMin = 0, RangeMax = 3 });

        int insertIndex = 0;
        while (insertIndex < Spells.Count && Spells[insertIndex].Id < newId)
            insertIndex++;

        Spells.Insert(insertIndex, newSpell);
        SelectedSpell = newSpell;
    }

    [RelayCommand]
    private void DeleteSpell()
    {
        if (SelectedSpell == null) return;
        Spells.Remove(SelectedSpell);
        SelectedSpell = Spells.FirstOrDefault();
    }

    [RelayCommand]
    private void AddLevel()
    {
        if (SelectedSpell == null) return;
        int nextLevel = SelectedSpell.Levels.Count > 0 ? SelectedSpell.Levels.Max(l => l.Level) + 1 : 1;
        SelectedSpell.Levels.Add(new SpellLevel { Level = nextLevel, Mp = 5, Power = 5, Radius = 1, RangeMin = 0, RangeMax = 3 });
    }

    [RelayCommand]
    private void DeleteLevel(SpellLevel level)
    {
        if (SelectedSpell == null || level == null) return;
        SelectedSpell.Levels.Remove(level);
    }
}