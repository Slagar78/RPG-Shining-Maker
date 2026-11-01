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
    [property: JsonPropertyName("class_id")]
    private int _classId;

    [ObservableProperty]
    [property: JsonPropertyName("start_level")]
    private int _startLevel = 1;

    [ObservableProperty]
    [property: JsonPropertyName("items")]
    private List<StartItem> _items = new();
}

public partial class StartItem : ObservableObject
{
    public const int NothingId = -1;

    [ObservableProperty]
    [property: JsonPropertyName("item_id")]
    private int _itemId = NothingId;

    [ObservableProperty]
    [property: JsonPropertyName("equipped")]
    private bool _equipped = false;

    [JsonIgnore]
    public bool HasItem => ItemId != NothingId;

    partial void OnItemIdChanged(int value)
    {
        if (value == NothingId)
            Equipped = false;
        OnPropertyChanged(nameof(HasItem));
    }
}