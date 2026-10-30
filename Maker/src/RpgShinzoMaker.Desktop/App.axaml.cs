using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using RpgShinzoMaker.Core.Services;
using RpgShinzoMaker.Desktop.ViewModels;
using RpgShinzoMaker.Desktop.Views;

namespace RpgShinzoMaker.Desktop;

public partial class App : Application
{
    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            // Она ищет папку data/maps вверх по дереву и запоминает корень
            ProjectPaths.Initialize(); 

            desktop.MainWindow = new MainWindow
            {
                DataContext = new MainViewModel(),
            };
        }

        base.OnFrameworkInitializationCompleted();
    }
}