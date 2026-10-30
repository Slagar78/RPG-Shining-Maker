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

public partial class SpellGroup : ObservableObject
{
    [ObservableProperty] private string _name = string.Empty;
    public ObservableCollection<Spell> Levels { get; } = new();
}

public partial class SpellsViewModel : ObservableObject
{
    private readonly SpellsService _spellsService = new();
    private readonly string _spellsPath;
    private readonly string _iconsDir = "assets/spells";

    public ObservableCollection<SpellGroup> SpellGroups { get; } = new();
    public ObservableCollection<string> AvailableIcons { get; } = new();

    [ObservableProperty] private object? _selectedTreeItem;
    [ObservableProperty] private Spell? _selectedSpell;

    public SpellsViewModel()
    {
        _spellsPath = Path.Combine(ProjectPaths.DataDir, "spells", "spells.json");
        LoadData();
    }

    partial void OnSelectedTreeItemChanged(object? value)
    {
        if (value is SpellGroup group && group.Levels.Count > 0)
        {
            SelectedSpell = group.Levels[0];
        }
        else if (value is Spell spell)
        {
            SelectedSpell = spell;
        }
        else
        {
            SelectedSpell = null;
        }
    }

    public void LoadData()
    {
        var iconPath = Path.Combine(ProjectPaths.Root, _iconsDir);
        if (Directory.Exists(iconPath))
        {
            foreach (var file in Directory.GetFiles(iconPath, "*.png"))
                AvailableIcons.Add(Path.GetFileName(file));
        }
        if (AvailableIcons.Count == 0) AvailableIcons.Add("magic_empty.png");

        var flatSpells = _spellsService.LoadSpells(_spellsPath);
        
        SpellGroups.Clear();
        var grouped = flatSpells.GroupBy(s => s.Name);
        foreach (var group in grouped)
        {
            var spellGroup = new SpellGroup { Name = group.Key };
            foreach (var spell in group.OrderBy(s => s.Level))
            {
                spellGroup.Levels.Add(spell);
            }
            SpellGroups.Add(spellGroup);
        }

        if (SpellGroups.Count > 0) SelectedTreeItem = SpellGroups[0];
    }

    public void SaveData()
    {
        var flatSpells = new List<Spell>();
        foreach (var group in SpellGroups)
        {
            flatSpells.AddRange(group.Levels);
        }
        _spellsService.SaveSpells(_spellsPath, flatSpells);
    }

    [RelayCommand]
    private void AddSpell()
    {
        int newId = 0;
        foreach (var g in SpellGroups)
            foreach (var s in g.Levels)
                if (s.Id >= newId) newId = s.Id + 1;

        string newName = "magic00";
        int idx = 0;
        while (SpellGroups.Any(g => g.Name.Equals(newName, StringComparison.OrdinalIgnoreCase)))
        {
            idx++;
            newName = $"magic{idx:D2}";
        }

        var newSpell = new Spell
        {
            Id = newId,
            Name = newName,
            Level = 1,
            Mp = 5,
            Type = "heal",
            Power = 5,
            RangeMin = 0,
            RangeMax = 3,
            Radius = 1,
            Icon = AvailableIcons.FirstOrDefault() ?? "magic_empty.png"
        };

        var newGroup = new SpellGroup { Name = newName };
        newGroup.Levels.Add(newSpell);
        SpellGroups.Add(newGroup);

        SelectedTreeItem = newGroup;
    }

    [RelayCommand]
    private void DeleteSpell()
    {
        if (SelectedTreeItem is SpellGroup group)
        {
            SpellGroups.Remove(group);
            SelectedTreeItem = SpellGroups.FirstOrDefault();
        }
        else if (SelectedTreeItem is Spell spell)
        {
            var parent = SpellGroups.FirstOrDefault(g => g.Levels.Contains(spell));
            if (parent != null)
            {
                parent.Levels.Remove(spell);
                if (parent.Levels.Count == 0) SpellGroups.Remove(parent);
                else SelectedTreeItem = parent;
            }
        }
    }

    // ★★★ КОМАНДЫ ДЛЯ СТРЕЛОК ★★★
    [RelayCommand] private void IncLevel() { if (SelectedSpell != null) SelectedSpell.Level++; }
    [RelayCommand] private void DecLevel() { if (SelectedSpell != null && SelectedSpell.Level > 1) SelectedSpell.Level--; }

    [RelayCommand] private void IncMp() { if (SelectedSpell != null) SelectedSpell.Mp++; }
    [RelayCommand] private void DecMp() { if (SelectedSpell != null && SelectedSpell.Mp > 0) SelectedSpell.Mp--; }

    [RelayCommand] private void IncPower() { if (SelectedSpell != null) SelectedSpell.Power++; }
    [RelayCommand] private void DecPower() { if (SelectedSpell != null && SelectedSpell.Power > 0) SelectedSpell.Power--; }

    [RelayCommand] private void IncRadius() { if (SelectedSpell != null) SelectedSpell.Radius++; }
    [RelayCommand] private void DecRadius() { if (SelectedSpell != null && SelectedSpell.Radius > 0) SelectedSpell.Radius--; }

    [RelayCommand] private void IncRangeMin() { if (SelectedSpell != null) SelectedSpell.RangeMin++; }
    [RelayCommand] private void DecRangeMin() { if (SelectedSpell != null && SelectedSpell.RangeMin > 0) SelectedSpell.RangeMin--; }

    [RelayCommand] private void IncRangeMax() { if (SelectedSpell != null) SelectedSpell.RangeMax++; }
    [RelayCommand] private void DecRangeMax() { if (SelectedSpell != null && SelectedSpell.RangeMax > 0) SelectedSpell.RangeMax--; }
}