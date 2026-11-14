# RPG-Shining-Maker\lib\battleManager\ai.rb

require_relative 'ai/controller'

class EnemyAI
  def self.decide_moves(unit, allies, enemies, highlight_tiles)
    AiController.new(unit, allies, enemies, highlight_tiles).decide_moves
  end
end