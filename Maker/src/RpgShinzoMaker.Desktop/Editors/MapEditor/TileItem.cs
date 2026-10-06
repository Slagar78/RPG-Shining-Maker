// RpgShinzoMaker.Desktop/Editors/MapEditor/TileItem.cs
using System.ComponentModel;
using Avalonia.Media;
using Avalonia.Media.Imaging;

namespace RpgShinzoMaker.Desktop.Editors.MapEditor;

/// <summary>
/// Модель одного тайла в палитре редактора карт.
/// Хранит изображение тайла, его индекс и текущий тип (Passable / Block / Slow / Under).
/// </summary>
public class TileItem : INotifyPropertyChanged
{
    // ─── Картинка тайла (нарезанный кусочек тайлсета) ───
    public CroppedBitmap Image { get; init; } = null!;

    // ─── Порядковый номер тайла в тайлсете (индекс) ───
    public int Index { get; init; }

    // ─── Тип тайла (0..3), обновляется при клике в Mode B ───
    private int _tileType;
    public int TileType
    {
        get => _tileType;
        set
        {
            _tileType = value;
            OnPropertyChanged(nameof(TileType));
            OnPropertyChanged(nameof(TileColor));
        }
    }

    // ─── Показывать ли цветной квадратик типа в палитре (только в Mode B) ───
    private bool _showType;
    public bool ShowType
    {
        get => _showType;
        set
        {
            _showType = value;
            OnPropertyChanged(nameof(ShowType));
        }
    }

    // ─── Выделен ли тайл как «левый клик» (для рисования ЛКМ) ───
    private bool _leftSelected;
    public bool LeftSelected
    {
        get => _leftSelected;
        set
        {
            _leftSelected = value;
            OnPropertyChanged(nameof(LeftSelected));
        }
    }

    // ─── Выделен ли тайл как «правый клик» (для рисования ПКМ) ───
    private bool _rightSelected;
    public bool RightSelected
    {
        get => _rightSelected;
        set
        {
            _rightSelected = value;
            OnPropertyChanged(nameof(RightSelected));
        }
    }

    // ─── Цвет квадратика типа (зависит от TileType) ───
    public IBrush TileColor => TileType switch
    {
        0 => new SolidColorBrush(Color.Parse("#4CAF50")), // Passable — зелёный
        1 => new SolidColorBrush(Color.Parse("#E74C3C")), // Block    — красный
        2 => new SolidColorBrush(Color.Parse("#3498DB")), // Slow     — синий
        3 => new SolidColorBrush(Color.Parse("#E67E22")), // Under    — оранжевый
        _ => Brushes.Gray
    };

    // ─── Уведомления об изменениях свойств (для биндингов в XAML) ───
    public event PropertyChangedEventHandler? PropertyChanged;

    private void OnPropertyChanged(string name) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}