# lib/battleManager/camera_battle.rb

class BattleCamera
  attr_accessor :target_x, :target_y   # цель в пикселях мировых
  attr_reader   :x, :y                # текущее положение камеры (левый верхний угол)

  def initialize(screen_width, screen_height, map_width, map_height)
    @screen_width = screen_width
    @screen_height = screen_height
    @map_width = map_width
    @map_height = map_height

    @x = 0.0
    @y = 0.0
    @target_x = 0.0
    @target_y = 0.0

    # Параметры сглаживания (чем меньше, тем плавнее)
    @smooth_factor = 0.1
  end

  # Мгновенно установить камеру в центр указанной клетки (используется при инициализации)
  def snap_to(x, y)
    @target_x = x * 48 + 24 - @screen_width / 2.0
    @target_y = y * 48 + 24 - @screen_height / 2.0
    @x = @target_x
    @y = @target_y
    clamp_target!
  end

  # Плавное следование за юнитом
  def follow_unit(unit)
    desired_x = unit[:x] * 48 + 24 - @screen_width / 2.0
    desired_y = unit[:y] * 48 + 24 - @screen_height / 2.0
    speed = 0.1   # чем меньше, тем плавнее
    @target_x += (desired_x - @target_x) * speed
    @target_y += (desired_y - @target_y) * speed
    clamp_target!
  end

  # Плавное следование за произвольной точкой (например, курсор)
  def follow_point(px, py)
    desired_x = px - @screen_width / 2.0
    desired_y = py - @screen_height / 2.0
    speed = 0.1
    @target_x += (desired_x - @target_x) * speed
    @target_y += (desired_y - @target_y) * speed
    clamp_target!
  end

  def update
    # Плавное движение к цели
    @x += (@target_x - @x) * @smooth_factor
    @y += (@target_y - @y) * @smooth_factor
  end

  # Возвращает смещение для отрисовки (можно использовать в BeginMode2D)
  def offset
    Raylib::Vector2.create(@x, @y)
  end

  # Возвращает объект камеры Raylib (удобно для BeginMode2D)
  def to_camera2d
    Raylib::Camera2D.new
             .with_target(@x + @screen_width / 2.0, @y + @screen_height / 2.0)
             .with_offset(@screen_width / 2.0, @screen_height / 2.0)
             .with_rotation(0.0)
             .with_zoom(1.0)
  end

  private

  def clamp_target!
    max_x = @map_width * 48 - @screen_width
    max_y = @map_height * 48 - @screen_height
    @target_x = @target_x.clamp(0, max_x > 0 ? max_x : 0)
    @target_y = @target_y.clamp(0, max_y > 0 ? max_y : 0)
  end
end