using System.Collections.ObjectModel;
using System.IO;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using RpgShinzoMaker.Core.Models;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.ViewModels;

public partial class SystemViewModel : ObservableObject
{
    private readonly GlobalService _globalService = new();

    private readonly string _globalPath;
    private readonly string _entriesPath;

    [ObservableProperty] private GlobalSettings _settings = new();
    
    // Список всех карт для выпадающего списка
    public ObservableCollection<MapEntry> AvailableMaps { get; } = new();

    public SystemViewModel()
    {
        _globalPath = Path.Combine(ProjectPaths.DataDir, "global.json");
        _entriesPath = Path.Combine(ProjectPaths.DataDir, "maps", "entries.json");
        
        LoadData();
    }

    public void LoadData()
    {
        // 1. Загружаем список карт через твой сервис (он статический, вызываем напрямую)
        var maps = EntriesService.Load(_entriesPath); 
        AvailableMaps.Clear();
        foreach (var m in maps) AvailableMaps.Add(m);

        // 2. Загружаем настройки
        Settings = _globalService.LoadGlobal(_globalPath);
    }

    public void SaveData()
    {
        _globalService.SaveGlobal(_globalPath, Settings);
    }

    // ==========================================
    // КОМАНДЫ ДЛЯ СТРЕЛОЧЕК
    // ==========================================
    [RelayCommand] private void IncrementStartX() { Settings.StartX++; }
    [RelayCommand] private void DecrementStartX() { if (Settings.StartX > 0) Settings.StartX--; }

    [RelayCommand] private void IncrementStartY() { Settings.StartY++; }
    [RelayCommand] private void DecrementStartY() { if (Settings.StartY > 0) Settings.StartY--; }

    [RelayCommand] private void IncrementGold() { Settings.Gold += 10; }
    [RelayCommand] private void DecrementGold() { if (Settings.Gold >= 10) Settings.Gold -= 10; }
}