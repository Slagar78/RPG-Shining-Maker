# lib/battleManager/battle_utils.rb
module BattleUtils
  TILE_SIZE = 48  # если нужно, можно использовать BattleManager::TILE_SIZE, но проще объявить

  def calculate_move_range(unit)
    start_x = unit[:x]
    start_y = unit[:y]
    move   = unit[:mov]
    w      = @battle_w
    h      = @battle_h

    visited = Array.new(h) { Array.new(w, false) }
    queue = []
    visited[start_y][start_x] = true
    queue.push([start_x, start_y, move])

    is_ally = @allies.include?(unit)
    if is_ally
      blocking_positions = @enemies.select { |e| e[:hp] > 0 }.map { |e| [e[:x], e[:y]] }
    else
      blocking_positions = @allies.select { |a| a[:hp] > 0 }.map { |a| [a[:x], a[:y]] }
    end

    passable = lambda do |nx, ny|
      return false if nx < 0 || nx >= w || ny < 0 || ny >= h
      gx = nx + @battle_x
      gy = ny + @battle_y
      return false if gy < 0 || gy >= @terrain.size || gx < 0 || gx >= @terrain[gy].size
      return false if @terrain[gy][gx] == -1
      return false if blocking_positions.include?([nx, ny])
      true
    end

    while queue.any?
      x, y, steps = queue.shift
      next if steps <= 0
      [[0,1],[0,-1],[1,0],[-1,0]].each do |dx, dy|
        nx = x + dx
        ny = y + dy
        # Сначала проверяем проходимость и границы (passable включает проверку границ)
        next unless passable.call(nx, ny)
        # Теперь безопасно обращаемся к visited
        next if visited[ny][nx]
        visited[ny][nx] = true
        queue.push([nx, ny, steps - 1])
      end
    end

    result = []
    h.times do |y|
      w.times do |x|
        result << [x, y] if visited[y][x]
      end
    end
    result
  end

  # Можешь сразу добавить другие методы, которые не зависят от состояний боя:
  def cell_free?(x, y, except_unit = nil)
    if except_unit && except_unit[:enemy]
      (@allies + @enemies).none? { |u| u != except_unit && u[:x] == x && u[:y] == y && u[:actor] }
    else
      (@allies + @enemies).none? { |u| u != except_unit && u[:x] == x && u[:y] == y }
    end
  end

  def occupied_tiles(except_unit = nil)
    occupied = []
    (@allies + @enemies).each do |u|
      next if u.equal?(except_unit)
      occupied << [u[:x], u[:y]]
    end
    occupied
  end

  def adjacent_enemy?(unit)
    !find_adjacent_enemies(unit).empty?
  end

  def find_adjacent_enemies(unit)
    ux, uy = unit[:x], unit[:y]
    @enemies.select do |e|
      ex, ey = e[:x], e[:y]
      dist = (ex - ux).abs + (ey - uy).abs
      dist == 1 && e[:hp] > 0
    end
  end

  def sort_targets_by_angle(targets, from_x, from_y)
    targets.sort_by do |t|
      rad = Math.atan2(t[:y] - from_y, t[:x] - from_x)
      rad >= 0 ? rad : rad + 2 * Math::PI
    end
  end
end