# lib/camera.rb
require 'raylib'
include Raylib

class Camera
  def initialize
    @camera = Camera2D.new
    @camera.zoom = 1.0
    @camera.offset = Vector2.create(288, 240) # Центр экрана 576x480
    @camera.target = Vector2.create(0, 0)

    @snapped = Camera2D.new
    @snapped.zoom = 1.0
    @snapped.offset = Vector2.create(288, 240)

    # Внутренние переменные (ЛЕВЫЙ ВЕРХНИЙ УГОЛ камеры в мировых координатах, как в Фортране)
    @cam_x = 0.0
    @cam_y = 0.0
    
    @cam_speed = 4.0 
    @initialized = false
  end

  def update(player, game_map)
    tile_size = game_map.tile_size
    
    # Координаты центра игрока в мире
    px = player.visual_x + tile_size / 2.0
    py = player.visual_y + tile_size / 2.0

    # Инициализация при старте (центрируем игрока)
    unless @initialized
      @cam_x = px - 288.0
      @cam_y = py - 240.0
      @initialized = true
    end

    is_moving = player.moving 
    
    # 3 тайла при движении, 4 тайла при остановке
    dz_move = 3 * tile_size
    dz_stop = 4 * tile_size

    # Позиция игрока на экране относительно левого верхнего угла
    screen_x = px - @cam_x
    screen_y = py - @cam_y

    target_x = @cam_x
    target_y = @cam_y

    # Направления из player.rb: DIR_DOWN=2, DIR_LEFT=4, DIR_RIGHT=6, DIR_UP=8
    dir = player.direction
    is_horizontal = (dir == 4 || dir == 6)
    is_vertical = (dir == 2 || dir == 8)

    if is_moving
      if is_horizontal
        # По горизонтали используем границу движения (2 тайла)
        if screen_x < dz_move
          target_x = px - dz_move
        elsif screen_x > 576 - dz_move
          target_x = px - (576 - dz_move)
        end
        
        # По вертикали используем границу остановки (3 тайла), чтобы отъехать от края
        if screen_y < dz_stop
          target_y = py - dz_stop
        elsif screen_y > 480 - dz_stop
          target_y = py - (480 - dz_stop)
        end

      elsif is_vertical
        # По вертикали используем границу движения (2 тайла)
        if screen_y < dz_move
          target_y = py - dz_move
        elsif screen_y > 480 - dz_move
          target_y = py - (480 - dz_move)
        end
        
        # По горизонтали используем границу остановки (3 тайла)
        if screen_x < dz_stop
          target_x = px - dz_stop
        elsif screen_x > 576 - dz_stop
          target_x = px - (576 - dz_stop)
        end
      end
    else
      # Если стоим, обе оси выравниваются по 3 тайла
      if screen_x < dz_stop
        target_x = px - dz_stop
      elsif screen_x > 576 - dz_stop
        target_x = px - (576 - dz_stop)
      end
      
      if screen_y < dz_stop
        target_y = py - dz_stop
      elsif screen_y > 480 - dz_stop
        target_y = py - (480 - dz_stop)
      end
    end

    # Ограничения карты (чтобы не было черных полос)
    if (bounds = game_map.area_bounds_for(player.x, player.y))
      area_w = bounds[:right]  - bounds[:left]
      area_h = bounds[:bottom] - bounds[:top]

      if area_w <= 576.0
        target_x = (bounds[:left] + bounds[:right]) / 2.0 - 288.0
      else
        target_x = clamp(target_x, bounds[:left], bounds[:right] - 576.0)
      end

      if area_h <= 480.0
        target_y = (bounds[:top] + bounds[:bottom]) / 2.0 - 240.0
      else
        target_y = clamp(target_y, bounds[:top], bounds[:bottom] - 480.0)
      end
    end

    # Плавное движение камеры (постоянная скорость, как в Фортране)
    if (target_x - @cam_x).abs > @cam_speed
      @cam_x += (target_x > @cam_x) ? @cam_speed : -@cam_speed
    else
      @cam_x = target_x
    end

    if (target_y - @cam_y).abs > @cam_speed
      @cam_y += (target_y > @cam_y) ? @cam_speed : -@cam_speed
    else
      @cam_y = target_y
    end

    # Передаем в Raylib ЦЕНТР экрана
    @camera.target = Vector2.create(@cam_x + 288.0, @cam_y + 240.0)
  end

  # Мгновенно переносит камеру на игрока (для варпов)
  def snap_to(player, game_map)
    tile_size = game_map.tile_size
    px = player.visual_x + tile_size / 2.0
    py = player.visual_y + tile_size / 2.0

    @cam_x = px - 288.0
    @cam_y = py - 240.0

    if (bounds = game_map.area_bounds_for(player.x, player.y))
      area_w = bounds[:right]  - bounds[:left]
      area_h = bounds[:bottom] - bounds[:top]

      if area_w <= 576.0
        @cam_x = (bounds[:left] + bounds[:right]) / 2.0 - 288.0
      else
        @cam_x = clamp(@cam_x, bounds[:left], bounds[:right] - 576.0)
      end

      if area_h <= 480.0
        @cam_y = (bounds[:top] + bounds[:bottom]) / 2.0 - 240.0
      else
        @cam_y = clamp(@cam_y, bounds[:top], bounds[:bottom] - 480.0)
      end
    end

    @camera.target = Vector2.create(@cam_x + 288.0, @cam_y + 240.0)
    @initialized = true
  end

  def render_camera
    @snapped.target = @camera.target
    @snapped.zoom = @camera.zoom
    @snapped
  end

  private

  def clamp(value, min, max)
    return min if value < min
    return max if value > max
    value
  end
end