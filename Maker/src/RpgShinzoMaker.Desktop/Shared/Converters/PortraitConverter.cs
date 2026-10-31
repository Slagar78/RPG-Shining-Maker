using System;
using System.Globalization;
using System.IO;
using Avalonia.Data.Converters;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Shared.Converters;

public class PortraitConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is string portraitName && !string.IsNullOrWhiteSpace(portraitName))
        {
            // Собираем относительный путь до корня проекта
            string path = Path.Combine(ProjectPaths.AssetsDir, "ui", "portraits", $"{portraitName}.png");
            
            if (File.Exists(path))
            {
                try { return new Bitmap(path); } 
                catch { return null; }
            }
        }
        return null; // Если файла нет, будет пустая рамка
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) => throw new NotImplementedException();
}