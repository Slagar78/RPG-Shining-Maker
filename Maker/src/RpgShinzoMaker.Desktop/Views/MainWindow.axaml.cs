// RpgShinzoMaker.Desktop/Views/MainWindow.axaml.cs
using System.Diagnostics;
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

        // Мост для статус-бара: MapEditor отправляет текст — MainWindow его ставит
        if (MapEditor != null)
            MapEditor.StatusChanged += OnEditorStatusChanged;
    }

    // ══════════════════════════════════════════════════════════════
    //   СТАТУС-БАР — приём текста от редактора
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
    }

    // ══════════════════════════════════════════════════════════════
    //   ВСПОМОГАТЕЛЬНОЕ
    // ══════════════════════════════════════════════════════════════
    private IEditor? GetActiveEditor()
    {
        // EditorTabs ещё не готов — возвращаем null без обращения к нему
        if (EditorTabs == null) return null;

        if (EditorTabs.SelectedItem is TabItem tab &&
            tab.Content is IEditor editor)
            return editor;

        return null;
    }
}
