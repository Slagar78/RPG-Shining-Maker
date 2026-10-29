using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace RpgShinzoMaker.Core.Services;

public class AssetScannerService
{
    public List<string> GetPortraits(string portraitsDirectory)
    {
        var result = new List<string>();
        if (!Directory.Exists(portraitsDirectory)) return result;

        var files = Directory.GetFiles(portraitsDirectory, "*.png");
        foreach (var file in files)
        {
            var name = Path.GetFileNameWithoutExtension(file);
            // Игнорируем файлы для анимации, как в C-коде
            if (name.Contains("_blink") || name.Contains("_talk")) continue;
            result.Add(name);
        }
        return result.OrderBy(x => x).ToList();
    }

    public List<string> GetMapsprites(string mapspritesDirectory)
    {
        var result = new List<string>();
        if (!Directory.Exists(mapspritesDirectory)) return result;

        var files = Directory.GetFiles(mapspritesDirectory, "*.png");
        foreach (var file in files)
        {
            result.Add(Path.GetFileNameWithoutExtension(file));
        }
        return result.OrderBy(x => x).ToList();
    }
}