# lib/battleManager/calculate_damage.rb

module DamageCalculator
  # ============================================================
  #  ФИЗИЧЕСКАЯ АТАКА
  # ============================================================
  ATTACK_MIN = 50   # минимальный % от номинальной атаки
  ATTACK_MAX = 130  # максимальный % от номинальной атаки

  DEFENSE_MIN = 80  # минимальный % от номинальной защиты
  DEFENSE_MAX = 120 # максимальный % от номинальной защиты

  # ============================================================
  #  МАГИЧЕСКАЯ АТАКА
  # ============================================================
  MATTACK_MIN = 90
  MATTACK_MAX = 110

  # ------------------------------------------------------------
  # Хранилище для данных о типах передвижения (из movetypes.json)
  # ------------------------------------------------------------
  @movetypes = {}

  def self.movetypes=(data)
    @movetypes = data
  end

  # ------------------------------------------------------------
  #  ФИЗИЧЕСКИЙ УРОН
  # ------------------------------------------------------------

  def self.calculate_physical(attack, defense)
    attack_mod  = rand(ATTACK_MIN..ATTACK_MAX) / 100.0
    defense_mod = rand(DEFENSE_MIN..DEFENSE_MAX) / 100.0

    effective_attack  = (attack * attack_mod).to_i
    effective_defense = (defense * defense_mod).to_i

    raw = effective_attack - effective_defense
    [raw, 1].max
  end

  def self.physical_for_units(attacker, defender)
    atk = attacker[:atk] || attacker[:attack] || 0
    def_val = defender[:def] || defender[:defense] || 0
    calculate_physical(atk, def_val)
  end

  def self.physical_damage_range(attack, defense)
    min_dmg = (attack * ATTACK_MIN / 100.0).to_i - (defense * DEFENSE_MAX / 100.0).to_i
    max_dmg = (attack * ATTACK_MAX / 100.0).to_i - (defense * DEFENSE_MIN / 100.0).to_i
    [[min_dmg, 1].max, [max_dmg, 1].max]
  end

  # ------------------------------------------------------------
  #  МАГИЧЕСКИЙ УРОН
  # ------------------------------------------------------------

  # Проверка уклонения от магии – теперь данные из movetypes.json
  def self.magic_dodge?(movetype_key)
    chance = @movetypes.dig(movetype_key.to_s, "magic_dodge") || 0
    rand(1..100) <= chance
  end

  def self.calculate_magic(matk, defender_movetype)
    return 0 if magic_dodge?(defender_movetype)

    matk_mod = rand(MATTACK_MIN..MATTACK_MAX) / 100.0
    effective_matk = (matk * matk_mod).to_i
    [effective_matk, 1].max
  end

  def self.magic_for_units(attacker, defender)
    matk  = attacker[:matk] || attacker[:magic_attack] || 0
    mtype = defender[:movetype] || "regular"   # теперь строка, не символ
    calculate_magic(matk, mtype)
  end

  def self.magic_damage_range(matk)
    min_dmg = (matk * MATTACK_MIN / 100.0).to_i
    max_dmg = (matk * MATTACK_MAX / 100.0).to_i
    [[min_dmg, 1].max, [max_dmg, 1].max]
  end

  def self.magic_hit_chance(movetype)
    chance = @movetypes.dig(movetype.to_s, "magic_dodge") || 0
    100 - chance
  end
end