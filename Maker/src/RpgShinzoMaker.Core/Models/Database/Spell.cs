using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RpgShinzoMaker.Core.Models.Database;

public partial class Spell : ObservableObject
{
    [ObservableProperty] private int _id;
    [ObservableProperty] private string _name = string.Empty;
    [ObservableProperty] private int _level = 1;
    [ObservableProperty] private int _mp = 5;
    [ObservableProperty] private string _type = "heal";
    [ObservableProperty] private int _power = 5;
    [ObservableProperty] private int _rangeMin = 0;
    [ObservableProperty] private int _rangeMax = 3;
    [ObservableProperty] private int _radius = 1;
    [ObservableProperty] private string _icon = "assets/spells/magic_empty.png";
}