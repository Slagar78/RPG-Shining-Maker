using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using System.Linq;

namespace RpgShinzoMaker.Desktop.Editors.DatabaseEditor.Views;

public partial class ItemsPanel : UserControl
{
    public ItemsPanel()
    {
        InitializeComponent();
        
        // Перехватываем ввод текста
        AddHandler(TextInputEvent, OnTextInput, RoutingStrategies.Tunnel);
    }

    private void OnTextInput(object? sender, TextInputEventArgs e)
    {
        if (string.IsNullOrEmpty(e.Text)) return;

        // Если пользователь печатает в поле с классом NumBox
        if (e.Source is TextBox textBox && textBox.Classes.Contains("NumBox"))
        {
            // Блокируем всё, кроме цифр
            if (!e.Text.All(char.IsDigit))
            {
                e.Handled = true; 
            }
        }
    }
}