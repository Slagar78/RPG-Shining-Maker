using System.Collections.ObjectModel;
using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RpgShinzoMaker.Core.Models.Database;

public partial class Spell : ObservableObject
{
    [ObservableProperty] private int _id;
    [ObservableProperty] private string _name = string.Empty;
    [ObservableProperty] private string _type = "heal";
    [ObservableProperty] private string _icon = "assets/spells/magic_empty.png";
    
    [ObservableProperty] 
    private ObservableCollection<SpellLevel> _levels = new();
}

public partial class SpellLevel : ObservableObject
{
    [ObservableProperty] private int _level = 1;
    [ObservableProperty] private int _mp = 5;
    [ObservableProperty] private int _power = 5;
    [ObservableProperty] private int _radius = 1;
    
    // В JSON это массив range: [0, 1], поэтому при сохранении мы их склеим
    [ObservableProperty] private int _rangeMin = 0;
    [ObservableProperty] private int _rangeMax = 3;
}