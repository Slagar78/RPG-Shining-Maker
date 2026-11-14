// RpgShinzoMaker.Desktop/Views/MainWindow.axaml.cs
using System;
using System.Diagnostics;
using System.IO;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.Media;
using RpgShinzoMaker.Desktop.Shared.Audio;
using RpgShinzoMaker.Desktop.Shared.Interfaces;

namespace RpgShinzoMaker.Desktop.Views;

public partial class MainWindow : Window
{
    private MusicPlayer? _musicPlayer;

    public MainWindow()
    {
        InitializeComponent();

        this.Opened += OnWindowOpened;

        if (MapEditor != null)
            MapEditor.StatusChanged += OnEditorStatusChanged;

        if (EventEditor != null)
            EventEditor.StatusChanged += OnEditorStatusChanged;

        if (TilesetImporter != null)
            TilesetImporter.StatusChanged += OnEditorStatusChanged;

        if (BattleEditorControl != null)
            BattleEditorControl.StatusChanged += OnEditorStatusChanged;

        if (BattleSceneEditor != null)
            BattleSceneEditor.StatusChanged += OnEditorStatusChanged;

        InitializeMusicPlayer();
    }

    // ══════════════════════════════════════════════════════════════
    //   МУЗЫКАЛЬНЫЙ ПЛЕЕР
    // ══════════════════════════════════════════════════════════════
    private void InitializeMusicPlayer()
    {
        _musicPlayer = new MusicPlayer();

        _musicPlayer.TrackStarted += path =>
        {
            if (NowPlayingText != null)
                NowPlayingText.Text = "▶ " + Path.GetFileNameWithoutExtension(path);
        };

        // ★ СТАРУЮ ПОДПИСКУ PlayStateChanged Я УДАЛИЛ, ТЕПЕРЬ ЦВЕТА МЕНЯЮТСЯ ВРУЧНУЮ ★

        var tracks = MusicLibrary.ScanSounds();
        if (MusicList != null)
        {
            MusicList.ItemsSource = tracks;
            if (tracks.Count == 0 && NowPlayingText != null)
                NowPlayingText.Text = "Папка assets/sounds пуста";
        }

        if (VolumeSlider != null && _musicPlayer != null)
            _musicPlayer.Volume = (float)(VolumeSlider.Value / 100.0);
    }

    // ★ НОВЫЙ МЕТОД: ПОДСВЕТКА КНОПОК ★
    private void UpdatePlayerButtons(string activeButton)
    {
        var activeColor = new SolidColorBrush(Color.Parse("#4CAF50")); // Зеленый
        var inactiveColor = new SolidColorBrush(Color.Parse("#3E3E42")); // Темно-серый

        if (PlayBtn != null) PlayBtn.Background = activeButton == "Play" ? activeColor : inactiveColor;
        if (PauseBtn != null) PauseBtn.Background = activeButton == "Pause" ? activeColor : inactiveColor;
        if (StopBtn != null) StopBtn.Background = activeButton == "Stop" ? activeColor : inactiveColor;
        if (NextBtn != null) NextBtn.Background = inactiveColor;
        if (PrevBtn != null) PrevBtn.Background = inactiveColor;
    }

    // ★ НОВЫЙ МЕТОД: КНОПКА ВПЕРЁД ★
    private void OnNextClick(object? sender, RoutedEventArgs e)
    {
        if (_musicPlayer == null) return;
        if (MusicList?.ItemsSource is System.Collections.IList list && list.Count > 0)
        {
            int newIndex = MusicList.SelectedIndex + 1;
            if (newIndex >= list.Count) newIndex = 0; // Если были на последнем, уходим на первый
            MusicList.SelectedIndex = newIndex;
            UpdatePlayerButtons("Play");
        }
    }

    // ★ НОВЫЙ МЕТОД: КНОПКА НАЗАД ★
    private void OnPrevClick(object? sender, RoutedEventArgs e)
    {
        if (_musicPlayer == null) return;
        
        if (MusicList?.ItemsSource is System.Collections.IList list && list.Count > 0)
        {
            int newIndex = MusicList.SelectedIndex - 1;
            if (newIndex < 0) newIndex = list.Count - 1; // Если были на первом, уходим на последний
            MusicList.SelectedIndex = newIndex;
            UpdatePlayerButtons("Play");
        }
    }

    private void OnPlayClick(object? sender, RoutedEventArgs e)
    {
        if (_musicPlayer == null) return;
        if (MusicList?.SelectedItem is MusicTrack track)
        {
            _musicPlayer.Play(track.FilePath);
            UpdatePlayerButtons("Play"); // ★ Красим Play в зеленый ★
            return;
        }
        if (MusicList?.ItemsSource is System.Collections.IEnumerable all)
        {
            foreach (var t in all)
            {
                MusicList.SelectedItem = t;
                break;
            }
        }
    }

    private void OnPauseClick(object? sender, RoutedEventArgs e)
    {
        _musicPlayer?.Pause();
        UpdatePlayerButtons("Pause"); // ★ Красим Pause в зеленый ★
    }

    private void OnStopClick(object? sender, RoutedEventArgs e)
    {
        _musicPlayer?.Stop();
        UpdatePlayerButtons("Stop"); // ★ Красим Stop в зеленый ★
    }

    private void OnVolumeChanged(object? sender, RangeBaseValueChangedEventArgs e)
    {
        if (_musicPlayer != null)
            _musicPlayer.Volume = (float)(e.NewValue / 100.0);
    }

    private void OnMusicSelected(object? sender, SelectionChangedEventArgs e)
    {
        if (_musicPlayer == null) return;
        if (MusicList?.SelectedItem is MusicTrack track)
        {
            _musicPlayer.Play(track.FilePath);
            UpdatePlayerButtons("Play"); // Если выбрали трек из списка, Play загорается
        }
    }

    protected override void OnClosed(EventArgs e)
    {
        _musicPlayer?.Dispose();
        base.OnClosed(e);
    }

    // ══════════════════════════════════════════════════════════════
    //   СТАТУС-БАР
    // ══════════════════════════════════════════════════════════════
    private void OnEditorStatusChanged(string text)
    {
        if (StatusInfoText != null)
            StatusInfoText.Text = text;
    }

    // ══════════════════════════════════════════════════════════════
    //   SAVE
    // ══════════════════════════════════════════════════════════════
    private void OnSaveClick(object? sender, RoutedEventArgs e)
    {
        var editor = GetActiveEditor();
        if (editor == null)
        {
            Debug.WriteLine("[MAIN] Активный редактор не поддерживает Save()");
            StatusText.Text = "Нечего сохранять";
            return;
        }

        editor.Save();
        StatusText.Text = "Сохранено!";
    }

    // ══════════════════════════════════════════════════════════════
    //   ВКЛАДКИ
    // ══════════════════════════════════════════════════════════════
    private void OnTabChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (EditorTabs == null) return;
        if (sender is not TabControl tabs) return;

        if (e.RemovedItems.Count > 0 &&
            e.RemovedItems[0] is TabItem oldTab &&
            oldTab.Content is IEditor oldEditor)
        {
            oldEditor.OnDeactivate();
        }

        if (tabs.SelectedItem is TabItem newTab &&
            newTab.Content is IEditor newEditor)
        {
            newEditor.OnActivate();
        }

        StatusText.Text = "Готово";
    }

    private IEditor? GetActiveEditor()
    {
        if (EditorTabs == null) return null;

        if (EditorTabs.SelectedItem is TabItem tab &&
            tab.Content is IEditor editor)
            return editor;

        return null;
    }

    // ══════════════════════════════════════════════════════════════
    //   PLAYTEST
    // ══════════════════════════════════════════════════════════════
    private void OnPlaytestClick(object? sender, RoutedEventArgs e)
    {
        LaunchRubyScript("game.rb", "--playtest");
    }

    private void LaunchRubyScript(string scriptName, string arguments)
    {
        try
        {
            string? projectRoot = FindGameRoot();
            if (projectRoot == null)
            {
                StatusText.Text = "Ошибка: Не найдена папка игры (PortableRuby/game.rb)";
                Debug.WriteLine("[MAIN] Не найдена папка игры!");
                return;
            }

            string rubyExe    = Path.Combine(projectRoot, "PortableRuby", "bin", "ruby.exe");
            string scriptPath = Path.Combine(projectRoot, scriptName);
            string gemHome    = Path.Combine(projectRoot, "PortableRuby", "gems");
            string dllDir     = Path.Combine(projectRoot, "PortableRuby", "dll");
            string binDir     = Path.Combine(projectRoot, "PortableRuby", "bin");

            if (!File.Exists(rubyExe) || !File.Exists(scriptPath))
            {
                StatusText.Text = "Ошибка: ruby.exe или скрипт не найдены!";
                return;
            }

            string batPath = Path.Combine(Path.GetTempPath(), "playtest.bat");
            using (StreamWriter sw = new StreamWriter(batPath, false, new System.Text.UTF8Encoding(false)))
            {
                sw.WriteLine("@echo off");
                sw.WriteLine("chcp 65001 >nul");
                sw.WriteLine($"cd /d \"{projectRoot}\"");
                sw.WriteLine($"set \"GEM_HOME={gemHome}\"");
                sw.WriteLine($"set \"GEM_PATH={gemHome}\"");
                sw.WriteLine($"set \"PATH={dllDir};{binDir};%PATH%\"");
                sw.WriteLine("set GALLIUM_DRIVER=llvmpipe");
                sw.WriteLine($"\"{rubyExe}\" \"{scriptPath}\" {arguments}");
                sw.WriteLine("if %errorlevel% neq 0 pause");
            }

            var psi = new ProcessStartInfo
            {
                FileName = batPath,
                UseShellExecute = true,
                WindowStyle = ProcessWindowStyle.Minimized
            };

            Process.Start(psi);
            StatusText.Text = $"Запуск: {scriptName}...";
            Debug.WriteLine($"[MAIN] Успешный запуск: {scriptName}");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[MAIN] Ошибка запуска: {ex}");
            StatusText.Text = $"Ошибка при запуске: {ex.Message}";
        }
    }

    private string? FindGameRoot()
    {
        var dir = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
        for (int i = 0; i < 10 && dir != null; i++)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "PortableRuby", "bin")) &&
                File.Exists(Path.Combine(dir.FullName, "game.rb")))
            {
                return dir.FullName;
            }
            dir = dir.Parent;
        }
        return null;
    }

    private async void OnWindowOpened(object? sender, EventArgs e)
    {
        await System.Threading.Tasks.Task.Delay(3000);

        if (SplashOverlay != null)
        {
            SplashOverlay.Opacity = 0;
            await System.Threading.Tasks.Task.Delay(1500);
            SplashOverlay.IsVisible = false;
        }
    }
}