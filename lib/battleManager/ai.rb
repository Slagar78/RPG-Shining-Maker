class EnemyAI
  def self.decide_moves(unit, allies, enemies, highlight_tiles)
    decide_moves_aggressive(unit, allies, enemies, highlight_tiles)
  end

  def self.decide_moves_aggressive(unit, allies, enemies, highlight_tiles)
    return [] if allies.empty?

    target = find_closest_target(unit, allies)
    return [] unless target
    tx, ty = target[:x], target[:y]

    # 1. Собираем все доступные клетки рядом с целью (соседние 4 клетки)
    attack_positions = []
    [[1,0],[-1,0],[0,1],[0,-1]].each do |dx, dy|
      nx = tx + dx
      ny = ty + dy
      attack_positions << [nx, ny] if highlight_tiles.include?([nx, ny])
    end

    # 2. Из них выбираем только свободные (не занятые другими врагами)
    free_attack_positions = attack_positions.select do |pos|
      (allies + enemies).none? { |u| u != unit && u[:x] == pos[0] && u[:y] == pos[1] }
    end

    # 3. Если есть свободная атакующая позиция, строим путь к ближайшей из них
    unless free_attack_positions.empty?
      target_cell = free_attack_positions.min_by { |pos| (pos[0] - unit[:x]).abs + (pos[1] - unit[:y]).abs }
      return build_path_to(unit, target_cell, highlight_tiles)
    end

    # 4. Иначе идём к любой свободной клетке (дальше от цели, но чтобы приблизиться)
    free_tiles = highlight_tiles.select do |pos|
      (allies + enemies).none? { |u| u != unit && u[:x] == pos[0] && u[:y] == pos[1] }
    end
    return [] if free_tiles.empty?

    best = free_tiles.min_by { |pos| (pos[0] - tx).abs + (pos[1] - ty).abs }
    build_path_to(unit, best, highlight_tiles)
  end

  def self.build_path_to(unit, goal, highlight_tiles)
    path = []
    cx, cy = unit[:x], unit[:y]
    gx, gy = goal[0], goal[1]

    (unit[:mov] || 3).times do
      candidates = []
      [[1,0],[-1,0],[0,1],[0,-1]].each do |dx, dy|
        nx = cx + dx
        ny = cy + dy
        candidates << [nx, ny] if highlight_tiles.include?([nx, ny])
      end
      break if candidates.empty?

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