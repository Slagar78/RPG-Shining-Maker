using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using System.Linq;

namespace RpgShinzoMaker.Desktop.Editors.DatabaseEditor.Views;

public partial class SystemPanel : UserControl
{
    public SystemPanel()
    {
        InitializeComponent();
        
        // Блокируем ввод букв и спецсимволов (как в ActorsPanel и EnemiesPanel)
        AddHandler(TextInputEvent, OnTextInput, RoutingStrategies.Tunnel);
    }

    private void OnTextInput(object? sender, TextInputEventArgs e)
    {
        if (string.IsNullOrEmpty(e.Text)) return;

        // Находим TextBox, на который кликнули
        if (e.Source is TextBox textBox)
        {
            // Проверяем, что это числовое поле (мы задали им выравнивание по центру)
            // В XAML это поля StartX, StartY, Gold. В StartMap у нас ComboBox.
            if (textBox.TextAlignment == Avalonia.Media.TextAlignment.Center)
            {
                // Разрешаем только цифры
                if (!e.Text.All(char.IsDigit))
                {
                    e.Handled = true; // Блокируем ввод
                }
            }
        }
    }
}