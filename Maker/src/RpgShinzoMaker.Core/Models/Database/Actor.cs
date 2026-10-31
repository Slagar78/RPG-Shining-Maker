using System.Collections.ObjectModel;
using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RpgShinzoMaker.Core.Models.Database;

public partial class ActorBlock : ObservableObject
{
    [ObservableProperty]
    [property: JsonPropertyName("forClass")]
    private int _forClass;

    [ObservableProperty]
    [property: JsonPropertyName("portrait")]
    private string _portrait = string.Empty;

    [ObservableProperty]
    [property: JsonPropertyName("mapsprite")]
    private string _mapsprite = string.Empty;

    [ObservableProperty]
    [property: JsonPropertyName("battle_sprite_path")]
    private string _battleSpritePath = string.Empty;

    [ObservableProperty]
    [property: JsonPropertyName("hp_growth")]
    private GrowthStat _hpGrowth = new();

    [ObservableProperty]
    [property: JsonPropertyName("mp_growth")]
    private GrowthStat _mpGrowth = new();

    [ObservableProperty]
    [property: JsonPropertyName("attack_growth")]
    private GrowthStat _attackGrowth = new();

    [ObservableProperty]
    [property: JsonPropertyName("defense_growth")]
    private GrowthStat _defenseGrowth = new();

    [ObservableProperty]
    [property: JsonPropertyName("agility_growth")]
    private GrowthStat _agilityGrowth = new();

    [ObservableProperty]
    [property: JsonPropertyName("spell_list")]
    private ObservableCollection<ClassSpell> _spellList = new();

    [ObservableProperty] 
    [property: JsonPropertyName("useFirstSpellList")]
    private bool _useFirstSpellList = false;

    [JsonIgnore]
    public string DisplayName => this is Actor ? "Base Class" : $"Promotion (Class {ForClass})";
}

public partial class Actor : ActorBlock
{
    [ObservableProperty]
    [property: JsonPropertyName("id")]
    private int _id;

    [ObservableProperty]
    [property: JsonPropertyName("name")]
    private string _name = string.Empty;

    [ObservableProperty]
    [property: JsonPropertyName("exp")]
    private int _exp = 0;

    [ObservableProperty]
    [property: JsonPropertyName("kills")]
    private int _kills = 0;

    [ObservableProperty]
    [property: JsonPropertyName("defeats")]
    private int _defeats = 0;

    [JsonIgnore]
    public ObservableCollection<ActorPromotion> Promotions { get; set; } = new();
}

public partial class ActorPromotion : ActorBlock
{
}

public partial class GrowthStat : ObservableObject
{
    [ObservableProperty] [property: JsonPropertyName("start")] private int _start = 10;
    [ObservableProperty] [property: JsonPropertyName("projected")] private int _projected = 30;
    [ObservableProperty] [property: JsonPropertyName("curve")] private string _curve = "LINEAR";
}

public partial class ClassSpell : ObservableObject
{
    [ObservableProperty] [property: JsonPropertyName("level")] private int _level = 1;
    [ObservableProperty] [property: JsonPropertyName("spell_id")] private int _spellId = 0;
    [ObservableProperty] [property: JsonPropertyName("spell_level")] private int _spellLevel = 1;
}