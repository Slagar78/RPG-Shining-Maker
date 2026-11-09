using System.ComponentModel;
using Avalonia.Media;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public class BattleUnit : INotifyPropertyChanged
{
    private int _id = -1;
    public int Id
    {
        get => _id;
        set { _id = value; OnPropertyChanged(nameof(Id)); }
    }

    // ★ ДОБАВЛЕНО: Уровень юнита (пока только для визуала) ★
    private int _level = 1;
    public int Level
    {
        get => _level;
        set { _level = value; OnPropertyChanged(nameof(Level)); }
    }

    private int _goldReward = 0;
    public int GoldReward
    {
        get => _goldReward;
        set { _goldReward = value; OnPropertyChanged(nameof(GoldReward)); }
    }

    private string _name = "";
    public string Name
    {
        get => _name;
        set { _name = value; OnPropertyChanged(nameof(Name)); }
    }

    private string _sprite = "";
    public string Sprite
    {
        get => _sprite;
        set { _sprite = value; OnPropertyChanged(nameof(Sprite)); }
    }

    private int _x;
    public int X
    {
        get => _x;
        set { _x = value; OnPropertyChanged(nameof(X)); }
    }

    private int _y;
    public int Y
    {
        get => _y;
        set { _y = value; OnPropertyChanged(nameof(Y)); }
    }

    private bool _used;
    public bool Used
    {
        get => _used;
        set { _used = value; OnPropertyChanged(nameof(Used)); OnPropertyChanged(nameof(Image)); }
    }

    private IImage? _image;
    public IImage? Image
    {
        get => _image;
        set { _image = value; OnPropertyChanged(nameof(Image)); }
    }

    public bool IsEnemy { get; set; }

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged(string n) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
}