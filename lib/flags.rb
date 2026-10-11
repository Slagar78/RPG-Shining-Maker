# lib/flags.rb
#
# Флаги игры. Пока поддерживаются только флаги битв:
#   battle_0_unlocked, battle_0_completed
#   battle_1_unlocked, battle_1_completed
#   ...
#   battle_98_unlocked, battle_98_completed
#
# Максимум 99 битв (индексы 0..98).

class Flags
  MAX_BATTLES = 99

  def initialize(data = nil)
    @data = data.is_a?(Hash) ? data : {}
  end

  # ─────────────────────────────────────────────────────────
  # Флаги битв
  # ─────────────────────────────────────────────────────────

  # Открыта ли битва (можно ли её запустить)
  def battle_unlocked?(n)
    on?(battle_key(n, :unlocked))
  end

  # Пройдена ли битва
  def battle_completed?(n)
    on?(battle_key(n, :completed))
  end

  # Открыть битву (например, после диалога с NPC)
  def unlock_battle(n)
    set(battle_key(n, :unlocked))
  end

  # Отметить битву как пройденную (например, после победы)
  def complete_battle(n)
    set(battle_key(n, :completed))
  end

  # Сбросить (на всякий случай — для отладки)
  def reset_battle(n)
    unset(battle_key(n, :unlocked))
    unset(battle_key(n, :completed))
  end

  # Утилита — сгенерировать ключ
  def battle_key(n, kind)
    raise ArgumentError, "battle index out of range: #{n}" if n < 0 || n >= MAX_BATTLES
    raise ArgumentError, "unknown battle flag kind: #{kind}" unless [:unlocked, :completed].include?(kind)
    "battle_#{n}_#{kind}"
  end

  # ─────────────────────────────────────────────────────────
  # Универсальные операции (пригодятся позже для сюжетных флагов)
  # ─────────────────────────────────────────────────────────

  def [](key)
    @data[key.to_s]
  end

  def on?(key)
    v = @data[key.to_s]
    v == true || (v.is_a?(Numeric) && v > 0)
  end

  def off?(key)
    !on?(key)
  end

  def set(key, value = true)
    @data[key.to_s] = value
  end

  def unset(key)
    @data.delete(key.to_s)
  end

  def toggle(key)
    @data[key.to_s] = !on?(key)
  end

  # ─────────────────────────────────────────────────────────
  # Сериализация (для сейвов)
  # ─────────────────────────────────────────────────────────

  def to_h
    @data.dup
  end

  def save(path)
    require 'json'
    File.write(path, JSON.pretty_generate(@data))
  end

  def self.load(path)
    require 'json'
    return new unless File.exist?(path)
    new(JSON.parse(File.read(path)))
  end
end