using System.Linq;
using Avalonia.Controls;
using Avalonia.Input;

namespace RpgShinzoMaker.Desktop.Editors.DatabaseEditor.Views;

public partial class EnemiesPanel : UserControl
{
    public EnemiesPanel()
    {
        InitializeComponent();
        AddHandler(TextInputEvent, OnTextInputPreview, Avalonia.Interactivity.RoutingStrategies.Tunnel);
    }

    private void OnTextInputPreview(object? sender, TextInputEventArgs e)
    {
        if (e.Source is not TextBox tb) return;
        if (!tb.Classes.Contains("NumBox")) return;

        // Разрешаем только цифры в числовых полях
        if (!string.IsNullOrEmpty(e.Text) && !e.Text.All(char.IsDigit))
            e.Handled = true;
    }
}