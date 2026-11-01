using CommunityToolkit.Mvvm.ComponentModel;
using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models;

public partial class GlobalSettings : ObservableObject
{
    [ObservableProperty]
    [property: JsonPropertyName("start_map_id")]
    private int _startMapId = 0;

    [ObservableProperty]
    [property: JsonPropertyName("start_x")]
    private int _startX = 12;

    [ObservableProperty]
    [property: JsonPropertyName("start_y")]
    private int _startY = 14;

    [ObservableProperty]
    [property: JsonPropertyName("gold")]
    private int _gold = 500;
}