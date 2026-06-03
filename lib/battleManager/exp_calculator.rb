# lib/battleManager/exp_calculator.rb
# Формулы опыта с небольшой случайной вариацией

module ExpCalculator
  PER_ACTION_EXP_CAP = 50
  HEALING_ACTION_EXP_CAP = 20
  HEALING_SPELL_EXP_MAX = 20
  HEALING_SPELL_EXP_MIN = 15
  STATUSEFFECT_SPELL_EXP = 10
  EXP_VARIANCE = 2

  # Список классов, получающих опыт за лечение (заполните своими ID)
  HEALER_CLASS_IDS = [].freeze

# ============================================================
# Таблица базового опыта за уничтожение врага
# Разница уровней = эффективный уровень атакующего − уровень цели
# ============================================================
#  diff  | Базовый опыт | Возможные значения (после вариации ±2)
# -------|--------------|--------------------------------------
#  ≤ 0   |     50       |       48, 49, 50
#    1   |     40       |       38, 39, 40
#    2   |     30       |       28, 29, 30
#    3   |     20       |       18, 19, 20
#    4   |     10       |        8,  9, 10
#  ≥ 5   |      0       |        0
# ============================================================
# Вариация: случайно вычитается 0..EXP_VARIANCE (2).
# Итоговый опыт не превышает PER_ACTION_EXP_CAP (50).
# ============================================================

DESTROY_EXP_TABLE = {
  ..0 => 50,   # враг сильнее или равен
  1   => 40,
  2   => 30,
  3   => 20,
  4   => 10
}.freeze
DESTROY_EXP_DEFAULT = 0   # при разнице >=5

  # --- Основные методы ---

  # Опыт за нанесённый урон
  def self.calculate_damage_exp(actor, target, damage_dealt)
    return 0 unless ally?(actor)
    return 0 if target[:max_hp] == 0

    base = destroy_exp(actor, target)
    gained = (base * damage_dealt) / target[:max_hp]
    gained = 1 if gained < 1 && base > 0   # минимум 1 опыт за попадание
    add_with_action_cap(gained)
  end

  # Опыт за уничтожение врага
  def self.calculate_destroy_exp(actor, target)
    return 0 unless ally?(actor)

    gained = destroy_exp(actor, target)
    gained = 1 if gained < 1   # минимум 1 опыт за уничтожение
    add_with_action_cap(gained)
  end

  # Опыт за лечение (только для классов-целителей)
  def self.calculate_healing_exp(actor, target, healed_amount)
    return 0 unless ally?(actor) && healer?(actor)
    return 0 if target[:max_hp] == 0

    gained = (HEALING_SPELL_EXP_MAX * healed_amount) / target[:max_hp]
    gained = [gained, HEALING_SPELL_EXP_MIN].max
    add_with_healing_cap(gained)
  end

  # Опыт за успешное наложение статусного эффекта
  def self.calculate_status_effect_exp(actor)
    return 0 unless ally?(actor)
    add_with_action_cap(STATUSEFFECT_SPELL_EXP)
  end

  # --- Вспомогательные методы ---

  # Базовый опыт за уничтожение с учётом случайного разброса
  def self.destroy_exp(actor, target)
    diff = effective_level(actor) - target_level(target)
    base = DESTROY_EXP_TABLE.find { |range, _| range === diff }&.last || DESTROY_EXP_DEFAULT
    return 0 if base == 0
    apply_random_variation(base)
  end

  def self.effective_level(actor)
    level = actor[:level] || 1
    if promoted?(actor)
      level + 20
    else
      level
    end
  end

  def self.target_level(target)
    target[:enemy] ? (target[:enemy][:level] || 1) : (target[:actor][:level] || 1)
  end

  def self.promoted?(actor)
    actor[:class]&.respond_to?(:promoted?) ? actor[:class].promoted? : false
  end

  def self.ally?(actor)
    actor[:actor] != nil
  end

  def self.healer?(actor)
    HEALER_CLASS_IDS.include?(actor[:actor][:class_id])
  end

  def self.apply_random_variation(exp)
    deduction = rand(0..EXP_VARIANCE)
    [exp - deduction, 0].max
  end

  def self.add_with_action_cap(raw_exp)
    [raw_exp, PER_ACTION_EXP_CAP].min
  end

  def self.add_with_healing_cap(raw_exp)
    [raw_exp, HEALING_ACTION_EXP_CAP].min
  end
end