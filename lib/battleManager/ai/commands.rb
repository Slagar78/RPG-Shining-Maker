# RPG-Shining-Maker\lib\battleManager\ai\commands.rb

module AiCommands
  COMMANDS = {
    move: :command_move,
    stay: :command_stay,
  }.freeze

  def self.execute(command, ctx, params = {})
    method = COMMANDS[command.to_sym]
    raise ArgumentError, "Unknown AI command: #{command}" unless method
    send(method, ctx, params)
  end

  def self.command_stay(_ctx, _params = {})
    { success: true, action: :stay, path: [] }
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

    # Клетки вплотную к цели
    adjacent_tiles = [[1,0],[-1,0],[0,1],[0,-1]].map { |dx, dy| [goal_x + dx, goal_y + dy] }
    valid_attack_positions = reachable_tiles & adjacent_tiles

    chosen_tile = nil

    if valid_attack_positions.any?
      # 1. ЕСЛИ МОЖЕМ ДОСТАТЬ ЦЕЛЬ: 
      # Идем на полную мощность. Выбираем ближайшую точку вплотную к цели.
      chosen_tile = valid_attack_positions.min_by { |pos| distances[pos] }
    else
      # 2. ЕСЛИ ДАЛЕКО: 
      # Враг "прогуливается". Выбираем случайный лимит шагов (чаще 2 или 3).
      step_limits = [2, 2, 2, 3, 3, 3, 1, 4] 
      max_steps = step_limits.sample
      
      # Находим все клетки, до которых можно дойти за выбранное количество шагов
      valid_slow_tiles = reachable_tiles.select { |pos| distances[pos] <= max_steps }
      
      if valid_slow_tiles.empty?
        return { success: true, action: :stay, path: [] }
      end

      # Из этих клеток выбираем ту, которая максимально приближает к цели
      min_dist = valid_slow_tiles.map { |x, y| (x - goal_x).abs + (y - goal_y).abs }.min
      best_tiles = valid_slow_tiles.select { |x, y| (x - goal_x).abs + (y - goal_y).abs == min_dist }
      
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