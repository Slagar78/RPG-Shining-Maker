using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace RpgShinzoMaker.Desktop.ViewModels;

public partial class MainViewModel : ViewModelBase
{
    [ObservableProperty]
    public partial string Greeting { get; set; } = "Welcome to Avalonia!";

    // Наши редакторы
    public ActorsViewModel ActorsVM { get; } = new();
    public ItemsViewModel ItemsVM { get; } = new();
    public SpellsViewModel SpellsVM { get; } = new();
    public ClassesViewModel ClassesVM { get; } = new();

    [RelayCommand]
    private void Save()
    {
        SaveDatabase();
    }

    [RelayCommand]
    private void SaveDatabase()
    {
        ActorsVM.SaveData();
        SpellsVM.SaveData();
        ItemsVM.SaveData();
        ClassesVM.SaveData();
    }
}