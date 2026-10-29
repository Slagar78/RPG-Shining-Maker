using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace RpgShinzoMaker.Desktop.ViewModels;

public partial class MainViewModel : ViewModelBase
{
    [ObservableProperty]
    public partial string Greeting { get; set; } = "Welcome to Avalonia!";

    // Сюда мы будем добавлять и другие вкладки (ItemsVM, SpellsVM и т.д.)
    public ActorsViewModel ActorsVM { get; } = new();

    [RelayCommand]
    private void Save()
    {
        // Вызываем сохранение у всех вкладок
        ActorsVM.SaveData();
    }
}