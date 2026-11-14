# RPG-Shining-Maker\lib\battleManager\ai\types\aggressive.rb

module AiTypes
  class Aggressive
    def initialize(unit, allies, enemies, highlight_tiles)
      @unit = unit
      @allies = allies
      @enemies = enemies
      @highlight_tiles = highlight_tiles
    end

    def decide_moves
      return [] if @allies.empty?

      target = find_closest_target
      return [] unless target

      # Считаем дистанцию по прямой
      distance = (@unit[:x] - target[:x]).abs + (@unit[:y] - target[:y]).abs

      # Передаем ВСЕХ юнитов, чтобы команды видели актуальную обстановку
      ctx = { unit: @unit, highlight_tiles: @highlight_tiles, allies: @allies, enemies: @enemies }

      # Если мы уже вплотную, вызываем команду АТАКИ
      if distance <= 1
        result = AiCommands.execute(:attack, ctx, { target: target })
        return result[:path] # Вернет пустой массив []
      end

      # Если цель далеко — идем к ней
      result = AiCommands.execute(:move, ctx, { target: target })
      
      if result[:success]
        result[:path]
      else
        []
      end
    end

    private

    def find_closest_target
      @allies.min_by { |t| (@unit[:x] - t[:x]).abs + (@unit[:y] - t[:y]).abs }
    end
  end
end