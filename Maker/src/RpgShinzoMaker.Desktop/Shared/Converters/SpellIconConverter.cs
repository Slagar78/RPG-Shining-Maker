using System;
using System.Globalization;
using System.IO;
using Avalonia.Data.Converters;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Shared.Converters;

public class SpellIconConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is not string relPath || string.IsNullOrWhiteSpace(relPath))
            return null;

        try
        {
            // Spell.Icon = "assets/spells/Heal.png" — путь от корня игры
            var path = Path.Combine(ProjectPaths.Root, relPath.Replace('/', Path.DirectorySeparatorChar));

            // Запасной вариант: если вдруг голое имя "Heal.png" / "magic_empty.png"
            if (!File.Exists(path))
                path = Path.Combine(ProjectPaths.AssetsDir, "spells", Path.GetFileName(relPath));

            if (!File.Exists(path)) return null;

            // Копия в памяти, чтобы не держать файл залоченным (Windows)
            using var fs = File.OpenRead(path);
            var ms = new MemoryStream();
            fs.CopyTo(ms);
            ms.Position = 0;
            return new Bitmap(ms);
        }
        catch
        {
            // битый файл — просто без превью
        }

        return null;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}