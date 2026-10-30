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

    // Глобальная кнопка SAVE (в тулбаре)
    [RelayCommand]
    private void Save()
    {
        // Сохраняем всё (пока только базу, позже добавим карты и эвенты)
        SaveDatabase();
    }

    // Локальная кнопка SAVE DATABASE (внутри базы данных)
    [RelayCommand]
    private void SaveDatabase()
    {
        ActorsVM.SaveData();
        SpellsVM.SaveData(); // Магия сохраняется ПЕРВОЙ, чтобы прислать сигнал предметам
        ItemsVM.SaveData();  // Предметы сохраняются уже с обновленным списком магии
    }
}