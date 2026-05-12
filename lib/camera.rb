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

    max_x = game_map.width * tile_size - @half_w
    max_y = game_map.height * tile_size - @half_h

    target_x = clamp(target_x, @half_w, max_x) if max_x > @half_w
    target_y = clamp(target_y, @half_h, max_y) if max_y > @half_h

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