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

    [RelayCommand]
    private void Save()
    {
        // Вызываем сохранение у всех вкладок
        ActorsVM.SaveData();
        ItemsVM.SaveData();
    }
}