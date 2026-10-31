using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using System.Linq;

namespace RpgShinzoMaker.Desktop.Editors.DatabaseEditor.Views;

public partial class ClassesPanel : UserControl
{
    public ClassesPanel()
    {
        InitializeComponent();
        
        // Блокируем ввод букв в поля с классом NumBox
        AddHandler(TextInputEvent, OnTextInput, RoutingStrategies.Tunnel);
    }

    private void OnTextInput(object? sender, TextInputEventArgs e)
    {
        if (string.IsNullOrEmpty(e.Text)) return;

        if (e.Source is TextBox textBox && textBox.Classes.Contains("NumBox"))
        {
            // Только цифры и максимум 3 символа
            if (!e.Text.All(char.IsDigit) || textBox.Text?.Length >= 3)
            {
                e.Handled = true;
            }
        }
    }
}