using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using System.Linq;

namespace RpgShinzoMaker.Desktop.Editors.DatabaseEditor.Views;

public partial class ActorsPanel : UserControl
{
    public ActorsPanel()
    {
        InitializeComponent();
        
        // Перехватываем ввод текста на уровне всей панели
        AddHandler(TextInputEvent, OnTextInput, RoutingStrategies.Tunnel);
    }

    private void OnTextInput(object? sender, TextInputEventArgs e)
    {
        if (string.IsNullOrEmpty(e.Text)) return;

        // Проверяем, куда именно идет ввод
        if (e.Source is TextBox textBox)
        {
            var parent = textBox.Parent;
            while (parent != null)
            {
                if (parent is NumericUpDown)
                {
                    // Если мы внутри NumericUpDown, разрешаем ТОЛЬКО цифры
                    if (!e.Text.All(char.IsDigit))
                    {
                        e.Handled = true; // Блокируем ввод буквы
                    }
                    break;
                }
                
                if (parent is Control control)
                    parent = control.Parent;
                else
                    break;
            }
        }
    }
}