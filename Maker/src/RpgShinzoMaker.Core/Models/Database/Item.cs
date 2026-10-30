using CommunityToolkit.Mvvm.ComponentModel;
using System.Text.Json.Serialization;

namespace RpgShinzoMaker.Core.Models.Database;

public partial class Item : ObservableObject
{
    [ObservableProperty] private int _id;
    [ObservableProperty] private string _name = string.Empty;
    
    // Категория: consumable, Weapon, Ring
    [ObservableProperty] 
    [NotifyPropertyChangedFor(nameof(IsEquipment))]
    private string _category = "consumable";

    [ObservableProperty] private int _price;
    [ObservableProperty] private int _rangeMin;
    [ObservableProperty] private int _rangeMax;
    [ObservableProperty] private string _useSpell = "HEAL";
    [ObservableProperty] private int _spellLevel = 1;
    [ObservableProperty] private string _icon = "assets/items/empty.png";

    // Дополнительные статы (видны только для Weapon/Ring)
    [ObservableProperty] private int _attack;
    [ObservableProperty] private int _defense;
    [ObservableProperty] private int _hp;
    [ObservableProperty] private int _mp;
    [ObservableProperty] private int _agility;
    [ObservableProperty] private int _mov;

    // Свойство для UI: показывать ли блок статов
    [JsonIgnore]
    public bool IsEquipment => Category != "consumable";
}