# lib/battleManager/ai.rb

class EnemyAI
  def self.decide_moves(unit, allies, enemies, highlight_tiles)
    # Только агрессивный AI (тип 0), но улучшенный
    decide_moves_aggressive(unit, allies, enemies, highlight_tiles)
  end

  # Агрессивный ИИ – идёт к ближайшему герою, но останавливается на свободной клетке
  def self.decide_moves_aggressive(unit, allies, enemies, highlight_tiles)
    return [] if allies.empty?

    max_mov = unit[:mov] || 3
    cx, cy = unit[:x], unit[:y]

    # Находим ближайшего героя
    target = find_closest_target(unit, allies)
    return [] unless target
    tx, ty = target[:x], target[:y]

    # Строим жадный путь (без учёта занятости промежуточных клеток)
    path = []
    current_x, current_y = cx, cy

    max_mov.times do
      candidates = []
      [[1,0],[-1,0],[0,1],[0,-1]].each do |dx, dy|
        nx = current_x + dx
        ny = current_y + dy
        candidates << [nx, ny] if highlight_tiles.include?([nx, ny])
      end
      break if candidates.empty?

      # Сортируем по близости к цели (манхэттенское расстояние)
      candidates.sort_by! { |x, y| (x - tx).abs + (y - ty).abs }
      chosen = candidates.first
      path << chosen
      current_x, current_y = chosen
    end

    # Обрезаем путь до первой свободной клетки (включая последнюю, если свободна)
    path = trim_to_free_cell(path, unit, allies, enemies)

    # Если путь стал пустым, остаёмся на месте
    path
  end

  # Обрезает путь так, чтобы последняя клетка была свободна
  def self.trim_to_free_cell(path, unit, allies, enemies)
    return [] if path.empty?
    # Идём с конца, ищем первую свободную клетку
    (path.length - 1).downto(0) do |i|
      cell = path[i]
      if cell_is_really_free?(cell, unit, allies, enemies)
        return path[0..i]
      end
    end
    # Если ни одной свободной нет (все клетки заняты) – возвращаем пустой путь
    []
  end

  def self.find_closest_target(unit, targets)
    return nil if targets.empty?
    targets.min_by { |t| (unit[:x] - t[:x]).abs + (unit[:y] - t[:y]).abs }
  end

  def self.cell_is_really_free?(pos, current_unit, allies, enemies)
    (allies + enemies).none? do |u|
      u != current_unit && u[:x] == pos[0] && u[:y] == pos[1]
    end
  end
end