# lib/battleManager/camera_battle.rb

class BattleCamera
  attr_accessor :target_x, :target_y, :smooth_factor
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
    @smooth_factor = 10.0
  end

  def snap_to(x, y)
    @target_x = (x * 48 + 24 - @screen_width / 2.0).round
    @target_y = (y * 48 + 24 - @screen_height / 2.0).round
    @x = @target_x
    @y = @target_y
    clamp_target!
  end

  def follow_unit(unit)
    desired_x = unit[:x] * 48 + 24 - @screen_width / 2.0
    desired_y = unit[:y] * 48 + 24 - @screen_height / 2.0
    @target_x = desired_x.round
    @target_y = desired_y.round
    clamp_target!
  end

  def follow_point(px, py)
    @target_x = (px - @screen_width / 2.0).round
    @target_y = (py - @screen_height / 2.0).round
    clamp_target!
  end

  # Синхронное движение с курсором: камера мгновенно на точке,
  # без догона. Нужно для :cursor_moving / :cursor_returning,
  # где цель движется непрерывно и с постоянной скоростью.
  def lock_to(px, py)
    @target_x = (px - @screen_width / 2.0).round
    @target_y = (py - @screen_height / 2.0).round
    clamp_target!
    @x = @target_x
    @y = @target_y
  end

	def update(dt = 1.0 / 60.0)
	  tx = @target_x.round
	  ty = @target_y.round

	  t = 1.0 - Math.exp(-@smooth_factor * dt)
	  @x += (tx - @x) * t
	  @y += (ty - @y) * t

	  @x = tx if (tx - @x).abs < 0.5
	  @y = ty if (ty - @y).abs < 0.5
	end

  # Для рендеринга – только целые пиксели, чтобы убрать субпиксельное дрожание
  def integer_offset
    Raylib::Vector2.create(@x.round, @y.round)
  end

  def offset
    Raylib::Vector2.create(@x.round, @y.round)
  end

  def to_camera2d
    Raylib::Camera2D.new
             .with_target(@x + @screen_width / 2.0, @y + @screen_height / 2.0)
             .with_offset(@screen_width / 2.0, @screen_height / 2.0)
             .with_rotation(0.0)
             .with_zoom(1.0)
  end

  private

  def clamp_target!
    max_x = @map_width - @screen_width
    max_y = @map_height - @screen_height
    min_y = -16   # высота чёрной полосы над картой
    @target_x = @target_x.clamp(0, max_x > 0 ? max_x : 0)
    @target_y = @target_y.clamp(min_y, max_y > min_y ? max_y : min_y)
  end
end