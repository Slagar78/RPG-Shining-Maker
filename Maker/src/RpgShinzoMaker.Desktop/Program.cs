using Avalonia;
using System;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop
{
    internal class Program
    {
        [STAThread]
        public static void Main(string[] args)
        {
            ProjectPaths.Initialize();
            BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
        }

        public static AppBuilder BuildAvaloniaApp()
            => AppBuilder.Configure<App>()
                .UsePlatformDetect()
                .WithInterFont()
                .LogToTrace();
    }
}