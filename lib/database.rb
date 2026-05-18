# database.rb
require 'json'
require 'ostruct'

class Database
  attr_reader :items, :actors, :classes, :spells, :growth_curves, :globals, :enemies, :movetypes

  def initialize
    @items = []
    @actors = []
    @classes = []
    @spells = []
    @growth_curves = []
    @enemies = []
    @movetypes = {}

    load_items
    load_actors
    load_classes
    load_spells
    load_growth_curves
    load_globals
    load_enemies
    load_movetypes        # ← новая загрузка
  end

  # ---------- предметы ----------
  def load_items
    if File.exist?("data/items/items.json")
      data = JSON.parse(File.read("data/items/items.json"))
      @items = data["items"].map { |h| OpenStruct.new(h) }
      puts "Загружено предметов: #{@items.size}"
    else
      puts "Файл data/items/items.json не найден!"
    end
  end

  # ---------- актёры ----------
  def load_actors
    if File.exist?("data/actors/actors.json")
      data = JSON.parse(File.read("data/actors/actors.json"))
      @actors = data["actors"] || []
      puts "Загружено актёров: #{@actors.size}"
    else
      puts "Файл data/actors/actors.json не найден!"
    end
  end

  # ---------- классы ----------
  def load_classes
    if File.exist?("data/actors/classes.json")
      data = JSON.parse(File.read("data/actors/classes.json"))
      @classes = data["classes"] || []
      puts "Загружено классов: #{@classes.size}"
    else
      puts "Файл data/actors/classes.json не найден!"
    end
  end

  # ---------- заклинания ----------
  def load_spells
    if File.exist?("data/spells/spells.json")
      data = JSON.parse(File.read("data/spells/spells.json"))
      @spells = data["spells"] || []
      puts "Загружено заклинаний: #{@spells.size}"
    else
      puts "Файл data/spells/spells.json не найден!"
    end
  end

  # ---------- кривые роста ----------
  def load_growth_curves
    if File.exist?("data/actors/growth_curves.json")
      data = JSON.parse(File.read("data/actors/growth_curves.json"))
      @growth_curves = data["curves"] || []
      puts "Загружено кривых роста: #{@growth_curves.size}"
    else
      puts "Файл data/actors/growth_curves.json не найден!"
    end
  end

  # ---------- глобальные настройки ----------
  def load_globals
    if File.exist?("data/global.json")
      data = JSON.parse(File.read("data/global.json"))
      @globals = data
      puts "Загружены глобальные настройки"
    else
      @globals = {
        "start_map" => "Granseal",
        "start_x" => 10,
        "start_y" => 10,
        "gold" => 100
      }
    end
  end

  # ---------- враги ----------
  def load_enemies
    if File.exist?("data/enemies/enemies.json")
      data = JSON.parse(File.read("data/enemies/enemies.json"))
      @enemies = data["enemies"].map { |h| OpenStruct.new(h) }
      puts "Загружено врагов: #{@enemies.size}"
    else
      @enemies = []
      puts "Файл data/enemies/enemies.json не найден!"
    end
  end

  # ---------- типы передвижения ----------
  def load_movetypes
    path = "data/movetypes.json"
    if File.exist?(path)
      data = JSON.parse(File.read(path))
      # Превращаем массив в хэш по ключу "key" (например "flying")
      @movetypes = data.each_with_object({}) do |entry, hash|
        hash[entry["key"]] = entry
      end
      puts "Загружено типов передвижения: #{@movetypes.size}"
    else
      puts "Файл #{path} не найден, типы передвижения не загружены!"
    end
  end

  # ---------- утилиты ----------
  def item(id)
    @items.find { |i| i.id == id }
  end

  def find_by_name(name)
    @items.find { |i| i.name == name }
  end

  # Поиск кривой роста по id (например, "LINEAR")
  def growth_curve(curve_id)
    @growth_curves.find { |c| c["id"] == curve_id }
  end

  # Получить значение стата на заданном уровне (1..30)
  def stat_at_level(growth, level)
    return growth["start"] if level <= 1
    curve = growth_curve(growth["curve"])
    return growth["start"] unless curve

    diff = growth["projected"] - growth["start"]
    index = level - 2
    return growth["projected"] if index >= curve["levels"].length

    a = curve["levels"][index][0]
    growth["start"] + (a * diff / 256.0).round
  end

  # Шанс уклонения от магии для типа передвижения
  def magic_dodge_chance(movetype_key)
    @movetypes.dig(movetype_key, "magic_dodge") || 0
  end

  # Шанс уклонения от стрел (на будущее)
  def ranged_dodge_chance(movetype_key)
    @movetypes.dig(movetype_key, "ranged_dodge") || 0
  end
end