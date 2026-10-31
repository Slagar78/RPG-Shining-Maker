using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RpgShinzoMaker.Core.Models.Database;

public partial class CharacterClass : ObservableObject
{
    [ObservableProperty] private int _id;
    [ObservableProperty] private string _name = "Class00";
    [ObservableProperty] private string _fullName = "FullName";
    [ObservableProperty] private int _move = 5;
    [ObservableProperty] private string _moveType = "regular";
    [ObservableProperty] private string _race = "Enterran";
    [ObservableProperty] private string _status = "normal";
    [ObservableProperty] private string _initialStatus = "none";
    [ObservableProperty] private List<string> _prowess = new();
    [ObservableProperty] private ResistanceData _resistance = new();

    // Для удобного редактирования в одну строку через запятую
    [JsonIgnore]
    public string ProwessString 
    {
        get => string.Join(", ", Prowess);
        set => Prowess = value.Split(',').Select(s => s.Trim()).Where(s => !string.IsNullOrEmpty(s)).ToList();
    }
}

public partial class ResistanceData : ObservableObject
{
    [ObservableProperty] private string _wind = "none";
    [ObservableProperty] private string _lightning = "none";
    [ObservableProperty] private string _ice = "none";
    [ObservableProperty] private string _fire = "none";
    [ObservableProperty] private string _aqua = "none";
    [ObservableProperty] private string _earth = "none";
    [ObservableProperty] private string _neutral = "none";
    [ObservableProperty] private string _affliction = "none";
}