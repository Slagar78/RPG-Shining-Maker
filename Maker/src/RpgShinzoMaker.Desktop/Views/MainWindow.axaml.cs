// RpgShinzoMaker.Desktop/Views/MainWindow.axaml.cs
using System;
using System.Diagnostics;
using System.IO;
using Avalonia.Controls;
using Avalonia.Interactivity;
using RpgShinzoMaker.Desktop.Shared.Interfaces;

namespace RpgShinzoMaker.Desktop.Views;

/// <summary>
/// Оболочка приложения.
/// Отвечает только за: тулбар, статус-бар, переключение вкладок.
/// Вся логика редактирования — внутри конкретных IEditor'ов (MapEditor, EventEditor...).
/// </summary>
public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();

        // Мост для статус-бара: каждый редактор отправляет текст — MainWindow его ставит
        if (MapEditor != null)
            MapEditor.StatusChanged += OnEditorStatusChanged;

        if (EventEditor != null)
            EventEditor.StatusChanged += OnEditorStatusChanged;

        if (TilesetImporter != null)
            TilesetImporter.StatusChanged += OnEditorStatusChanged;
    }

    // ══════════════════════════════════════════════════════════════
    //   СТАТУС-БАР — приём текста от редакторов
    // ══════════════════════════════════════════════════════════════
    private void OnEditorStatusChanged(string text)
    {
        if (StatusInfoText != null)
            StatusInfoText.Text = text;
    }

    // ══════════════════════════════════════════════════════════════
    //   SAVE — делегируется активному редактору
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
    //   Переключение вкладок — активируем/деактивируем редактор
    // ══════════════════════════════════════════════════════════════
    private void OnTabChanged(object? sender, SelectionChangedEventArgs e)
    {
        // На этапе инициализации XAML EditorTabs ещё null — тихо выходим
        if (EditorTabs == null) return;
        if (sender is not TabControl tabs) return;

        // Деактивировать старую вкладку
        if (e.RemovedItems.Count > 0 &&
            e.RemovedItems[0] is TabItem oldTab &&
            oldTab.Content is IEditor oldEditor)
        {
            oldEditor.OnDeactivate();
        }

        // Активировать новую вкладку
        if (tabs.SelectedItem is TabItem newTab &&
            newTab.Content is IEditor newEditor)
        {
            newEditor.OnActivate();
        }

        // Обновить статус-бар на «Готово»
        StatusText.Text = "Готово";
    }

    // ══════════════════════════════════════════════════════════════
    //   ВСПОМОГАТЕЛЬНОЕ
    // ══════════════════════════════════════════════════════════════
    private IEditor? GetActiveEditor()
    {
        if (EditorTabs == null) return null;

        if (EditorTabs.SelectedItem is TabItem tab &&
            tab.Content is IEditor editor)
            return editor;

        return null;
    }

        // ══════════════════════════════════════════════════════════════
    //   ЗАПУСК ИГРЫ (PLAYTEST)
    // ══════════════════════════════════════════════════════════════
    private void OnPlaytestClick(object? sender, RoutedEventArgs e)
    {
        LaunchRubyScript("game.rb", "--playtest");
    }

    /// <summary>
    /// Запускает игру на Ruby без сборки в EXE (как в C-конструкторе).
    /// </summary>
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

            string rubyExe = Path.Combine(projectRoot, "PortableRuby", "bin", "ruby.exe");
            string scriptPath = Path.Combine(projectRoot, scriptName);
            string gemHome = Path.Combine(projectRoot, "PortableRuby", "gems");
            string dllDir = Path.Combine(projectRoot, "PortableRuby", "dll");
            string binDir = Path.Combine(projectRoot, "PortableRuby", "bin");

            if (!File.Exists(rubyExe) || !File.Exists(scriptPath))
            {
                StatusText.Text = "Ошибка: ruby.exe или скрипт не найдены!";
                return;
            }

            // Создаем временный bat-файл
            string batPath = Path.Combine(Path.GetTempPath(), "playtest.bat");
            using (StreamWriter sw = new StreamWriter(batPath, false, new System.Text.UTF8Encoding(false)))
            {
                sw.WriteLine("@echo off");
                sw.WriteLine("chcp 65001 >nul"); 
                sw.WriteLine($"cd /d \"{projectRoot}\"");
                sw.WriteLine($"set \"GEM_HOME={gemHome}\"");
                sw.WriteLine($"set \"GEM_PATH={gemHome}\"");
                sw.WriteLine($"set \"PATH={dllDir};{binDir};%PATH%\"");
                
                // ★ ВОТ ОНА ★
                sw.WriteLine("set GALLIUM_DRIVER=llvmpipe");
                
                // Запускаем ruby
                sw.WriteLine($"\"{rubyExe}\" \"{scriptPath}\" {arguments}");
                // Пауза, чтобы окно не закрылось мгновенно, если будет ошибка
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

    /// <summary>
    /// Ищет папку, где лежат PortableRuby и game.rb.
    /// </summary>
    private string? FindGameRoot()
    {
        // Более надежный способ найти директорию приложения
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
}
