using System.Collections.Generic;
using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RpgShinzoMaker.Core.Models.Database;

public partial class StartInventory : ObservableObject
{
    [ObservableProperty]
    [property: JsonPropertyName("actor_id")]
    private int _actorId;

    [ObservableProperty]
    [property: JsonPropertyName("start_level")]
    private int _startLevel = 1;

    [ObservableProperty]
    [property: JsonPropertyName("items")]
    private List<StartItem> _items = new();
}

public partial class StartItem : ObservableObject
{
    [ObservableProperty]
    [property: JsonPropertyName("item")]
    private string _item = "NOTHING";

    [ObservableProperty]
    [property: JsonPropertyName("equipped")]
    private bool _equipped = false;
}