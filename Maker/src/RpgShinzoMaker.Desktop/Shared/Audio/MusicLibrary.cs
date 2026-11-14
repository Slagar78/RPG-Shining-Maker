// RpgShinzoMaker.Desktop/Shared/Audio/MusicLibrary.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace RpgShinzoMaker.Desktop.Shared.Audio;

public class MusicTrack
{
    public string FilePath { get; init; } = "";
    public string FileName    => Path.GetFileName(FilePath);
    public string DisplayName => Path.GetFileNameWithoutExtension(FilePath);
}

public static class MusicLibrary
{
    private static readonly string[] SupportedExtensions =
        { ".mp3", ".ogg", ".wav", ".flac", ".m4a", ".aac" };

    /// <summary>
    /// Ищет корень игры (там, где лежит game.rb + PortableRuby/bin).
    /// </summary>
    public static string? FindGameRoot()
    {
        var dir = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
        for (int i = 0; i < 10 && dir != null; i++)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "PortableRuby", "bin")) &&
                File.Exists(Path.Combine(dir.FullName, "game.rb")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return null;
    }

    /// <summary>
    /// Возвращает все треки из &lt;корень_игры&gt;/assets/sounds/, отсортированные по имени.
    /// </summary>
    public static List<MusicTrack> ScanSounds()
    {
        var result = new List<MusicTrack>();
        var root = FindGameRoot();
        if (root == null) return result;

        var soundsDir = Path.Combine(root, "assets", "sounds");
        if (!Directory.Exists(soundsDir)) return result;

        try
        {
            var files = Directory.GetFiles(soundsDir)
                .Where(f => SupportedExtensions.Contains(Path.GetExtension(f).ToLowerInvariant()))
                .OrderBy(f => Path.GetFileName(f), StringComparer.OrdinalIgnoreCase);

            foreach (var f in files)
                result.Add(new MusicTrack { FilePath = f });
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[MusicLibrary] Scan error: {ex.Message}");
        }

        return result;
    }
}