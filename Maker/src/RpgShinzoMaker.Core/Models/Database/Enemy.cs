using System.Collections.ObjectModel;
using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;

namespace RpgShinzoMaker.Core.Models.Database;

public partial class Enemy : ObservableObject
{
    [ObservableProperty]
    [property: JsonPropertyName("id")]
    private int _id;

    [ObservableProperty]
    [property: JsonPropertyName("name")]
    private string _name = string.Empty;

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
    [property: JsonPropertyName("race")]
    private string _race = string.Empty;

    [ObservableProperty]
    [property: JsonPropertyName("status")]
    private string _status = "normal";

    // Задаётся в битве, в редакторе только хранится
    [ObservableProperty]
    [property: JsonPropertyName("level")]
    private int _level = 1;

    [ObservableProperty]
    [property: JsonPropertyName("spell_power")]
    private string _spellPower = "regular";

    [ObservableProperty]
    [property: JsonPropertyName("stats")]
    private EnemyStats _stats = new();

    [ObservableProperty]
    [property: JsonPropertyName("resistance")]
    private ResistanceBlock _resistance = new();

    [ObservableProperty]
    [property: JsonPropertyName("prowess")]
    private ObservableCollection<string> _prowess = new();

    [ObservableProperty]
    [property: JsonPropertyName("items")]
    private ObservableCollection<EnemyItem> _items = new();

    // В JSON хранятся двумя параллельными массивами: spells[] + spell_levels[].
    // Разворачиваются/сворачиваются в EnemiesService.
    [JsonIgnore]
    public ObservableCollection<EnemySpellSlot> SpellSlots { get; } = new();

    [ObservableProperty]
    [property: JsonPropertyName("initial_status")]
    private string _initialStatus = "none";

    [ObservableProperty]
    [property: JsonPropertyName("move_type")]
    private string _moveType = "normal";
}

public partial class EnemyStats : ObservableObject
{
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

    // Движение от уровня не растёт — оставляем плоским
    [ObservableProperty]
    [property: JsonPropertyName("base_mov")]
    private int _baseMov = 4;
}

public partial class ResistanceBlock : ObservableObject
{
    [ObservableProperty] [property: JsonPropertyName("wind")]       private string _wind = "none";
    [ObservableProperty] [property: JsonPropertyName("lightning")]  private string _lightning = "none";
    [ObservableProperty] [property: JsonPropertyName("ice")]        private string _ice = "none";
    [ObservableProperty] [property: JsonPropertyName("fire")]       private string _fire = "none";
    [ObservableProperty] [property: JsonPropertyName("aqua")]       private string _aqua = "none";
    [ObservableProperty] [property: JsonPropertyName("earth")]      private string _earth = "none";
    [ObservableProperty] [property: JsonPropertyName("neutral")]    private string _neutral = "none";
    [ObservableProperty] [property: JsonPropertyName("affliction")] private string _affliction = "none";
}

public partial class EnemyItem : ObservableObject
{
    [ObservableProperty]
    [property: JsonPropertyName("item")]
    private string _item = "Nothing";

    [ObservableProperty]
    [property: JsonPropertyName("equipped")]
    private bool _equipped = false;
}

public partial class EnemySpellSlot : ObservableObject
{
    [ObservableProperty]
    [property: JsonPropertyName("name")]
    private string _name = "Nothing";

    [ObservableProperty]
    [property: JsonPropertyName("level")]
    private int _level = 1;
}