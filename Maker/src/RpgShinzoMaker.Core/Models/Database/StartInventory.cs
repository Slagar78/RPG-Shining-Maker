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

    // ❌ Свойство _level УДАЛЕНО, чтобы не засорять JSON

    [ObservableProperty]
    [property: JsonPropertyName("start_level")]
    private int _startLevel = 1;

    [ObservableProperty]
    [property: JsonPropertyName("items")]
    private List<StartItem> _items = new();
}

public partial class StartItem : ObservableObject
{
    private string _item = "NOTHING";

    [JsonPropertyName("item")]
    public string Item
    {
        get => _item;
        set
        {
            if (value == null) return;

            if (SetProperty(ref _item, value))
            {
                if (string.IsNullOrWhiteSpace(value) || value == "NOTHING")
                {
                    Equipped = false;
                }
            }
        }
    }

    [ObservableProperty]
    [property: JsonPropertyName("equipped")]
    private bool _equipped = false;
}