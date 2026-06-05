# lib\battleManager\camera_battle.rb

class BattleCamera
  attr_accessor :target_x, :target_y
  attr_reader   :x, :y

  def initialize(screen_width, screen_height, map_width, map_height)
    @screen_width = screen_width
    @screen_height = screen_height
    @map_width = map_width
    @map_height = map_height

    @x = 0.0
    @y = 0.0
    @target_x = 0.0
    @target_y = 0.0
  end

  def snap_to(x, y)
    @x = x * 48 + 24 - @screen_width / 2
    @y = y * 48 + 24 - @screen_height / 2
    clamp!
  end

  def follow_unit(unit)
    @x = unit[:x] * 48 + 24 - @screen_width / 2
    @y = unit[:y] * 48 + 24 - @screen_height / 2
    clamp!
  end

  def follow_point(px, py)
    @x = px - @screen_width / 2
    @y = py - @screen_height / 2
    clamp!
  end

  def update
    # Камера мгновенно на месте, без инерции
  end

  def offset
    # Возвращаем только целые координаты, чтобы не было субпиксельного дрожания
    Raylib::Vector2.create(@x.round, @y.round)
  end

  private

  def clamp!
    max_x = @map_width - @screen_width
    max_y = @map_height - @screen_height
    @x = @x.clamp(0, max_x > 0 ? max_x : 0).round
    @y = @y.clamp(0, max_y > 0 ? max_y : 0).round
  end
end