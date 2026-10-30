using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RpgShinzoMaker.Core.Models.Database;

public partial class Actor : ObservableObject
{
    [ObservableProperty]
    [property: JsonPropertyName("id")]
    private int _id;

    [ObservableProperty]
    [property: JsonPropertyName("name")]
    private string _name = string.Empty;

    [ObservableProperty]
    [property: JsonPropertyName("class_id")]
    private int _classId;

    [ObservableProperty]
    [property: JsonPropertyName("portrait")]
    private string _portrait = string.Empty;

    [ObservableProperty]
    [property: JsonPropertyName("mapsprite")]
    private string _mapsprite = string.Empty;

    [ObservableProperty]
    [property: JsonPropertyName("level")]
    private int _level = 1;

    [ObservableProperty]
    [property: JsonPropertyName("exp")]
    private int _exp = 0;

    [ObservableProperty]
    [property: JsonPropertyName("kills")]
    private int _kills = 0;

    [ObservableProperty]
    [property: JsonPropertyName("defeats")]
    private int _defeats = 0;
}