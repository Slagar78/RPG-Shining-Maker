# lib/battleManager/ai.rb

class EnemyAI
  def self.decide_moves(unit, allies, enemies, highlight_tiles)
    case unit[:ai_type] || unit[:ai] || 0
    when 1
      []  # защитный – стоит на месте
    else
      decide_moves_aggressive(unit, allies, enemies, highlight_tiles)
    end
  end

  def self.decide_moves_aggressive(unit, allies, enemies, highlight_tiles)
    return [] if allies.empty?

    target = find_closest_target(unit, allies)
    return [] unless target
    tx, ty = target[:x], target[:y]

    # Все клетки, занятые любыми юнитами (кроме самого себя)
    occupied = (allies + enemies)
      .reject { |u| u.equal?(unit) }
      .map { |u| [u[:x], u[:y]] }

    # Свободные клетки – только те, где никого нет
    free_tiles = highlight_tiles.select { |pos| !occupied.include?(pos) }

    # 1. Пытаемся найти свободную клетку рядом с целью для атаки
    attack_positions = []
    [[1,0],[-1,0],[0,1],[0,-1]].each do |dx, dy|
      nx = tx + dx
      ny = ty + dy
      attack_positions << [nx, ny] if free_tiles.include?([nx, ny])
    end

    if attack_positions.any?
      goal = attack_positions.min_by { |pos|
        (pos[0] - unit[:x]).abs + (pos[1] - unit[:y]).abs
      }
    elsif free_tiles.any?
      # 2. Иначе просто идём к ближайшей свободной клетке
      goal = free_tiles.min_by { |pos|
        (pos[0] - tx).abs + (pos[1] - ty).abs
      }
    else
      # Вообще нет свободных клеток – стоим на месте, без метаний
      return []
    end

    # Строим жадный путь (разрешено ходить по любым highlight_tiles)
    path = build_greedy_path(unit, goal, highlight_tiles)

    # Обрезаем хвост из занятых клеток – чтобы последний шаг был свободным
    final_occupied = (allies + enemies)
      .reject { |u| u.equal?(unit) }
      .map { |u| [u[:x], u[:y]] }
    while !path.empty? && final_occupied.include?(path.last)
      path.pop
    end

    path
  end

  # Жадный путь: можно ходить по всем клеткам из highlight_tiles (включая занятые врагами)
  def self.build_greedy_path(unit, goal, highlight_tiles)
    path = []
    cx, cy = unit[:x], unit[:y]
    gx, gy = goal[0], goal[1]
    move = unit[:mov] || 3

    move.times do
      candidates = []
      [[1,0],[-1,0],[0,1],[0,-1]].each do |dx, dy|
        nx = cx + dx
        ny = cy + dy
        candidates << [nx, ny] if highlight_tiles.include?([nx, ny])
      end
      break if candidates.empty?

      # Выбираем ближайшую к цели
      candidates.sort_by! { |x, y| (x - gx).abs + (y - gy).abs }
      cx, cy = candidates.first
      path << [cx, cy]
      break if cx == gx && cy == gy
    end
    path
  end

  def self.find_closest_target(unit, targets)
    return nil if targets.empty?
    targets.min_by { |t| (unit[:x] - t[:x]).abs + (unit[:y] - t[:y]).abs }
  end
end