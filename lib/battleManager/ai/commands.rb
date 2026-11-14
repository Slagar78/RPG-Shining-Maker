module AiCommands
  COMMANDS = {
    move: :command_move,
    stay: :command_stay,
    attack: :command_attack,  # <--- ДОБАВИЛИ КОМАНДУ АТАКИ
  }.freeze

  def self.execute(command, ctx, params = {})
    method = COMMANDS[command.to_sym]
    raise ArgumentError, "Unknown AI command: #{command}" unless method
    send(method, ctx, params)
  end

  # ── КОМАНДА: СТОЯТЬ НА МЕСТЕ ──────────────────────────────
  def self.command_stay(_ctx, _params = {})
    { success: true, action: :stay, path: [] }
  end

  # ── КОМАНДА: АТАКОВАТЬ ────────────────────────────────────
  def self.command_attack(ctx, params = {})
    target = params[:target]
    return { success: false, action: :stay, path: [] } unless target

    # Запоминаем цель в юните, чтобы battle.rb знал, кого бить
    ctx[:unit][:target_to_attack] = target
    { success: true, action: :attack, path: [] }
  end

  # ── КОМАНДА: ИДТИ К ЦЕЛИ ──────────────────────────────────
  def self.command_move(ctx, params = {})
    target = params[:target]
    return { success: false, action: :stay, path: [] } unless target

    goal_x = target[:x]
    goal_y = target[:y]
    tiles = ctx[:highlight_tiles]
    start = [ctx[:unit][:x], ctx[:unit][:y]]

    # Волновой алгоритм (BFS) с запоминанием дистанции
    queue = [[start, 0]]
    visited = { start => nil }
    distances = { start => 0 }
    
    while !queue.empty?
      curr, dist = queue.shift
      cx, cy = curr
      
      [[1,0],[-1,0],[0,1],[0,-1]].each do |dx, dy|
        neighbor = [cx + dx, cy + dy]
        if tiles.include?(neighbor) && !visited.key?(neighbor)
          visited[neighbor] = curr
          distances[neighbor] = dist + 1
          queue << [neighbor, dist + 1]
        end
      end
    end

    reachable_tiles = visited.keys - [start]
    return { success: true, action: :stay, path: [] } if reachable_tiles.empty?

    # Собираем ВСЕ занятые клетки (кроме себя самого)
    occupied = (ctx[:allies] + ctx[:enemies]).reject { |u| u.equal?(ctx[:unit]) }.map { |u| [u[:x], u[:y]] }

    # Клетки вплотную к цели
    adjacent_tiles = [[1,0],[-1,0],[0,1],[0,-1]].map { |dx, dy| [goal_x + dx, goal_y + dy] }
    
    # Ищем свободные клетки вплотную к цели
    valid_attack_positions = (reachable_tiles & adjacent_tiles).reject { |pos| occupied.include?(pos) }

    chosen_tile = nil

    if valid_attack_positions.any?
      # 1. ЕСЛИ МОЖЕМ ДОСТАТЬ ЦЕЛЬ: Идем на полную мощность.
      chosen_tile = valid_attack_positions.min_by { |pos| distances[pos] }
    else
      # 2. ЕСЛИ ДАЛЕКО: "Прогуливаемся"
      step_limits = [2, 2, 2, 3, 3, 3, 1, 4] 
      max_steps = step_limits.sample
      
      valid_slow_tiles = reachable_tiles.select { |pos| distances[pos] <= max_steps }
      
      if valid_slow_tiles.empty?
        return { success: true, action: :stay, path: [] }
      end

      min_dist = valid_slow_tiles.map { |x, y| (x - goal_x).abs + (y - goal_y).abs }.min
      
      # Выбираем лучшие СВОБОДНЫЕ клетки на минимальном расстоянии
      best_tiles = valid_slow_tiles.select { |x, y| 
        (x - goal_x).abs + (y - goal_y).abs == min_dist && !occupied.include?([x, y]) 
      }
      
      if best_tiles.empty?
        return { success: true, action: :stay, path: [] }
      end

      chosen_tile = best_tiles.sample
    end

    # Восстанавливаем пройденный путь
    path = []
    curr = chosen_tile
    while curr != start
      path.unshift(curr)
      curr = visited[curr]
    end

    { success: true, action: :move, path: path }
  end
end