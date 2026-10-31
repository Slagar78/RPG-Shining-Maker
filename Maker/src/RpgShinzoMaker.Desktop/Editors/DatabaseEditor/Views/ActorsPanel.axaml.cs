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
        
        AddHandler(TextInputEvent, OnTextInput, RoutingStrategies.Tunnel);
    }

    private void OnTextInput(object? sender, TextInputEventArgs e)
    {
        if (string.IsNullOrEmpty(e.Text)) return;

        if (e.Source is TextBox textBox && textBox.Classes.Contains("NumBox"))
        {
            if (!e.Text.All(char.IsDigit) || textBox.Text?.Length >= 5)
            {
                e.Handled = true;
            }
        }
    }
}