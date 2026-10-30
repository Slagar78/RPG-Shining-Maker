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

public partial class ClassesViewModel : ObservableObject
{
    private readonly ClassesService _classesService = new();
    private readonly SpellsService _spellsService = new();
    
    private readonly string _classesPath;
    private readonly string _spellsPath;

    public ObservableCollection<CharacterClass> Classes { get; } = new();
    
    public ObservableCollection<string> Races { get; } = new() { "Enterran", "Cadrian", "Human" };
    public ObservableCollection<string> Statuses { get; } = new() { "normal", "hyper", "ultra" };
    public ObservableCollection<string> MoveTypes { get; } = new() { "regular", "centaur", "stealth", "brass_gunner", "flying", "hovering", "aquatic", "archer", "centaur_archer", "stealth_archer", "mage", "healer", "ghost" };
    public ObservableCollection<string> Curves { get; } = new() { "LINEAR", "LATE", "EARLY", "MIDDLE", "EARLYANDLATE", "VERY_LATE", "MIDDLE_PEAK" };
    public ObservableCollection<string> Resistances { get; } = new() { "none", "minor", "major", "weakness", "immunity" };
    
    public ObservableCollection<Spell> AvailableSpells { get; } = new();

    [ObservableProperty]
    private CharacterClass? _selectedClass;

    public ClassesViewModel()
    {
        _classesPath = Path.Combine(ProjectPaths.DataDir, "actors", "classes.json");
        _spellsPath = Path.Combine(ProjectPaths.DataDir, "spells", "spells.json");

        WeakReferenceMessenger.Default.Register<ClassesViewModel, SpellsUpdatedMessage>(this, (r, m) =>
        {
            r.LoadSpells();
        });

        LoadData();
    }

    public void LoadData()
    {
        LoadSpells();

        var classes = _classesService.LoadClasses(_classesPath);
        Classes.Clear();
        foreach (var c in classes) Classes.Add(c);

        if (Classes.Count > 0) SelectedClass = Classes[0];
    }

    public void LoadSpells()
    {
        var spells = _spellsService.LoadSpells(_spellsPath);
        
        AvailableSpells.Clear();
        foreach (var sp in spells)
            AvailableSpells.Add(sp);
            
        if (AvailableSpells.Count == 0) 
            AvailableSpells.Add(new Spell { Id = 0, Name = "HEAL" });
    }

    public void SaveData()
    {
        _classesService.SaveClasses(_classesPath, Classes.ToList());
    }

    [RelayCommand]
    private void AddClass()
    {
        var existingIds = Classes.Select(c => c.Id).ToHashSet();
        int newId = 0;
        while (existingIds.Contains(newId)) { newId++; }

        var newClass = new CharacterClass
        {
            Id = newId,
            Name = $"Class{newId:D2}",
            FullName = $"FullName{newId:D2}"
        };

        int insertIndex = 0;
        while (insertIndex < Classes.Count && Classes[insertIndex].Id < newId)
            insertIndex++;

        Classes.Insert(insertIndex, newClass);
        SelectedClass = newClass;
    }

    [RelayCommand]
    private void DeleteClass()
    {
        if (SelectedClass == null) return;
        Classes.Remove(SelectedClass);
        SelectedClass = Classes.FirstOrDefault();
    }

    [RelayCommand]
    private void AddClassSpell()
    {
        if (SelectedClass == null) return;
        SelectedClass.SpellList.Add(new ClassSpell 
        { 
            Level = 1, 
            SpellId = AvailableSpells.FirstOrDefault()?.Id ?? 0, 
            SpellLevel = 1 
        });
    }

    [RelayCommand]
    private void RemoveClassSpell(ClassSpell spell)
    {
        if (SelectedClass == null || spell == null) return;
        SelectedClass.SpellList.Remove(spell);
    }
}