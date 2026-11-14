# RPG-Shining-Maker\lib\battleManager\ai\controller.rb

require_relative 'commands'
require_relative 'types/aggressive'
require_relative 'types/defensive'

class AiController
  # Словарь: И цифры, и строки ведут к нужному классу поведения.
  AI_TYPES = {
    0 => AiTypes::Aggressive,  "aggressive" => AiTypes::Aggressive,
    1 => AiTypes::Defensive,   "defensive"  => AiTypes::Defensive,
    
    # Сюда будем добавлять новые типы (например: "healer" => AiTypes::Healer)
  }.freeze

  DEFAULT_AI = AiTypes::Aggressive

  def initialize(unit, allies, enemies, highlight_tiles)
    @unit = unit
    @allies = allies
    @enemies = enemies
    @highlight_tiles = highlight_tiles
    
    # Читаем ai_type. Если в JSON пусто, по умолчанию будет 0.
    @ai_type = unit[:ai_type] || unit[:ai] || 0
  end

  # Главный метод, который вызывает battle.rb
  def decide_moves
    # Находим нужный класс (например, Aggressive) и запускаем его
    handler_class.new(@unit, @allies, @enemies, @highlight_tiles).decide_moves
  end

  private

  def handler_class
    AI_TYPES[@ai_type] || DEFAULT_AI
  end
end