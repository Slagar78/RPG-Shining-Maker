using System;
using System.IO;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Editors.BattleSceneEditor;

public partial class BattleSceneEditorView
{
    // Ищем корень игры для диалогов
    private string GetGameRootForDialogs()
    {
        var dir = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
        for (int i = 0; i < 10 && dir != null; i++)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "assets")) && Directory.Exists(Path.Combine(dir.FullName, "data")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return AppDomain.CurrentDomain.BaseDirectory;
    }

    private async void OnChangeBgClick(object? sender, RoutedEventArgs e)
    {
        if (CurrentBattle == null) return;
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel == null) return;

        // Ищем папку фонов
        string bgDir = Path.Combine(GetGameRootForDialogs(), "assets", "battles", "backgrounds");
        var startFolder = await topLevel.StorageProvider.TryGetFolderFromPathAsync(bgDir);

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Выберите фон для битвы",
            AllowMultiple = false,
            SuggestedStartLocation = startFolder,
            FileTypeFilter = new[] { new FilePickerFileType("PNG Images") { Patterns = new[] { "*.png" } }, new FilePickerFileType("All Files") { Patterns = new[] { "*.*" } } }
        });

        if (files.Count > 0)
        {
            string absPath = files[0].Path.LocalPath;
            string relPath = GetRelativePath(absPath);
            CurrentBattle.Background = relPath;
            _imageCache.Remove(Path.GetFullPath(absPath));
            UpdatePreview();
            StatusChanged?.Invoke($"Фон изменен: {relPath}");
        }
    }

    private async void OnLoadAllyClick(object? sender, RoutedEventArgs e)
    {
        var path = await PickAnimationJson("Выберите animation.json для Союзника", "allies");
        if (path != null)
        {
            AllyAnim = _animService.Load(path);
            ActiveSlot = 0;
            UpdateInputsUI();
            UpdatePreview();
            StatusChanged?.Invoke($"Загружена анимация союзника");
        }
    }

    private async void OnLoadEnemyClick(object? sender, RoutedEventArgs e)
    {
        var path = await PickAnimationJson("Выберите animation.json для Врага", "enemies");
        if (path != null)
        {
            EnemyAnim = _animService.Load(path);
            ActiveSlot = 1;
            UpdateInputsUI();
            UpdatePreview();
            StatusChanged?.Invoke($"Загружена анимация врага");
        }
    }

    private async Task<string?> PickAnimationJson(string title, string subFolder)
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel == null) return null;

        // ★ УКАЗЫВАЕМ ПУТЬ К ПАПКЕ (allies или enemies) ★
        string targetDir = Path.Combine(GetGameRootForDialogs(), "assets", "battles", "battlesprites", subFolder);
        var startFolder = await topLevel.StorageProvider.TryGetFolderFromPathAsync(targetDir);

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = title,
            AllowMultiple = false,
            SuggestedStartLocation = startFolder, // ★ Открывается сразу в нужной папке ★
            FileTypeFilter = new[] 
            { 
                new FilePickerFileType("Animation JSON") { Patterns = new[] { "animation.json" } }, 
                new FilePickerFileType("All JSON") { Patterns = new[] { "*.json" } } 
            }
        });

        return files.Count > 0 ? files[0].Path.LocalPath : null;
    }

    private void OnRescanFramesClick(object? sender, RoutedEventArgs e)
    {
        var activeAnim = (ActiveSlot == 0) ? AllyAnim : EnemyAnim;
        if (!activeAnim.Loaded || string.IsNullOrEmpty(activeAnim.JsonPath))
        {
            StatusChanged?.Invoke("Сначала загрузите анимацию!");
            return;
        }

        _animService.RescanFrames(activeAnim);
        activeAnim.CurrentFrame[0] = 0; activeAnim.CurrentFrame[1] = 0; activeAnim.CurrentFrame[2] = 0;
        activeAnim.CurrentPhase = 0; activeAnim.AnimTimer = 0;
        UpdateInputsUI();
        UpdatePreview();
        StatusChanged?.Invoke($"Кадры пересканированы: {(ActiveSlot == 0 ? "Ally" : "Enemy")}");
    }

    private string GetRelativePath(string absPath)
    {
        int index = absPath.IndexOf("assets", StringComparison.OrdinalIgnoreCase);
        if (index >= 0) return absPath.Substring(index).Replace('\\', '/');
        return Path.GetFileName(absPath);
    }
}