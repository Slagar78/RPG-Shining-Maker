using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using RpgShinzoMaker.Core.Models.Database;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.ViewModels;

public partial class ClassesViewModel : ObservableObject
{
    private readonly ClassesService _classesService = new();
    private readonly string _classesPath;

    public ObservableCollection<CharacterClass> Classes { get; } = new();
    
    public ObservableCollection<string> Races { get; } = new() { "Enterran", "Cadrian", "Human" };
    public ObservableCollection<string> Statuses { get; } = new() { "normal", "hyper", "ultra" };
    public ObservableCollection<string> MoveTypes { get; } = new() { "regular", "centaur", "stealth", "brass_gunner", "flying", "hovering", "aquatic", "archer", "centaur_archer", "stealth_archer", "mage", "healer", "ghost" };
    public ObservableCollection<string> Resistances { get; } = new() { "none", "minor", "major", "weakness", "immunity" };

    [ObservableProperty]
    private CharacterClass? _selectedClass;

    public ClassesViewModel()
    {
        _classesPath = Path.Combine(ProjectPaths.DataDir, "actors", "classes.json");
        LoadData();
    }

    public void LoadData()
    {
        var classes = _classesService.LoadClasses(_classesPath);
        Classes.Clear();
        foreach (var c in classes) Classes.Add(c);

        if (Classes.Count > 0) SelectedClass = Classes[0];
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

    // --- НОВЫЕ КОМАНДЫ ДЛЯ СТРЕЛОЧЕК MOVE ---

    [RelayCommand]
    private void IncrementMove()
    {
        if (SelectedClass != null)
            SelectedClass.Move++;
    }

    [RelayCommand]
    private void DecrementMove()
    {
        // Защита от ухода в минус
        if (SelectedClass != null && SelectedClass.Move > 0)
            SelectedClass.Move--;
    }
}