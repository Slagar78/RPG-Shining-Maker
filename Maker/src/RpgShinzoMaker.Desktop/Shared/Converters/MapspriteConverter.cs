using System;
using System.Globalization;
using System.IO;
using System.Linq;
using Avalonia.Data.Converters;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Shared.Converters;

public class MapspriteConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is not string name || string.IsNullOrWhiteSpace(name))
            return null;

        // Подпапка внутри assets: по умолчанию "mapsprites" (актёры),
        // для врагов приходит через ConverterParameter
        var subDir = parameter as string;
        if (string.IsNullOrWhiteSpace(subDir))
            subDir = "mapsprites";

        try
        {
            var dir = Path.Combine(ProjectPaths.AssetsDir, subDir);

            // 1) Имя уже с расширением: "mapsprite000.png"
            var direct = Path.Combine(dir, name);
            if (File.Exists(direct)) return Load(direct);

            // 2) Имя без расширения: "mapsprite000" -> "mapsprite000.png"
            var file = Path.Combine(dir, name + ".png");
            if (File.Exists(file)) return Load(file);

            // 3) Папка со спрайтами — берём первый png как кадр по умолчанию
            var folder = Path.Combine(dir, name);
            if (Directory.Exists(folder))
            {
                var first = Directory.EnumerateFiles(folder, "*.png")
                                     .OrderBy(f => f, StringComparer.OrdinalIgnoreCase)
                                     .FirstOrDefault();
                if (first != null) return Load(first);
            }
        }
        catch
        {
            // битый файл / нет доступа — просто без превью
        }

        return null;
    }

    private static Bitmap Load(string path)
    {
        // Копия в памяти, чтобы не держать файл залоченным (Windows)
        using var fs = File.OpenRead(path);
        var ms = new MemoryStream();
        fs.CopyTo(ms);
        ms.Position = 0;
        return new Bitmap(ms);
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}