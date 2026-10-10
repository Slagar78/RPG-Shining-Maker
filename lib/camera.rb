# lib/camera.rb
require 'raylib'
include Raylib

class Camera
  def initialize
    @camera = Camera2D.new
    @camera.zoom = 1.0
    @camera.offset = Vector2.create(288, 240)
    @camera.target = Vector2.create(0, 0)

    @snapped = Camera2D.new
    @snapped.zoom = 1.0
    @snapped.offset = Vector2.create(288, 240)

    @target_vec = Vector2.create(0, 0)
    @half_w = 288.0
    @half_h = 240.0
  end

	def update(player, game_map)
	  tile_size = game_map.tile_size
	  target_x = player.visual_x + tile_size / 2.0
	  target_y = player.visual_y + tile_size / 2.0

	  screen_w = 576.0
	  screen_h = 480.0

	  if (bounds = game_map.area_bounds_for(player.x, player.y))
		area_w = bounds[:right]  - bounds[:left]
		area_h = bounds[:bottom] - bounds[:top]

		# По X
		if area_w <= screen_w
		  # Area уже экрана — центрируем её
		  target_x = (bounds[:left] + bounds[:right]) / 2.0
		else
		  min_x = bounds[:left]  + screen_w / 2.0
		  max_x = bounds[:right] - screen_w / 2.0
		  target_x = clamp(target_x, min_x, max_x)
		end

		# По Y
		if area_h <= screen_h
		  target_y = (bounds[:top] + bounds[:bottom]) / 2.0
		else
		  min_y = bounds[:top]    + screen_h / 2.0
		  max_y = bounds[:bottom] - screen_h / 2.0
		  target_y = clamp(target_y, min_y, max_y)
		end
	  end

	  @target_vec.x = target_x
	  @target_vec.y = target_y
	  @camera.target = @target_vec
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

# не используется для камеры не удалять
def lerp(a, b, t)
  a + (b - a) * t
end