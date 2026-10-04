using System.IO;

namespace RpgShinzoMaker.Core.Services;

/// <summary>
/// Все пути проекта в одном месте. Заполняется в MainWindow при старте.
/// </summary>
public static class ProjectPaths
{
    /// <summary>
    /// Корень проекта. Пример: C:\SHINZO\RPG-Shining-Maker (заполняется в рантайме).
    /// </summary>
    public static string Root { get; set; } = "";

    public static string DataDir     => Path.Combine(Root, "data");
    public static string MapsDir     => Path.Combine(DataDir, "maps");
    public static string AssetsDir   => Path.Combine(Root, "assets");
    public static string TilesetsDir => Path.Combine(AssetsDir, "tilesets");

    public static string EntriesFile => Path.Combine(MapsDir, "entries.json");

    public static string LayoutFile(string folder)   => Path.Combine(MapsDir, folder, "layout.json");
    public static string AreasFile(string folder)    => Path.Combine(MapsDir, folder, "areas.json");

    public static string TileTypesFile(string tileset) =>
        Path.Combine(DataDir, "tile_types", SafeName(tileset) + ".json");

    private static string SafeName(string s) =>
        s.Replace('\\', '_').Replace('/', '_').Replace(':', '_');
}