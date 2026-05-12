# lib/battleManager/cursor.rb

class BattleCursor
  attr_accessor :x, :y, :visible   # тайловые координаты
  attr_accessor :px, :py           # пиксельные координаты (верхний левый угол)
  attr_reader :tile_size

  def initialize(tile_size = 48)
    @x = 0
    @y = 0
    @px = 0.0
    @py = 0.0
    @tile_size = tile_size
    @visible = false
    @tex = Raylib.LoadTexture("assets/ui/menu/Cursor.png")
    Raylib.SetTextureFilter(@tex, Raylib::TEXTURE_FILTER_POINT)
  end

  def update
    # ничего не делаем
  end

  def draw(cam_x, cam_y)
    return unless @visible && @tex
    # Если заданы пиксельные координаты (px, py), используем их.
    # Иначе переводим тайловые.
    draw_px = @px > 0 || @py > 0 ? @px : @x * @tile_size
    draw_py = @px > 0 || @py > 0 ? @py : @y * @tile_size
    dst = Raylib::Rectangle.create(draw_px + cam_x, draw_py + cam_y, @tile_size, @tile_size)
    src = Raylib::Rectangle.create(0, 0, @tex.width, @tex.height)
    Raylib.DrawTexturePro(@tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end

  # Переместить в тайловые координаты (и сбросить пиксельные)
  def move_to(tile_x, tile_y)
    @x = tile_x
    @y = tile_y
    @px = 0.0
    @py = 0.0
  end

  # Переместить в пиксельные координаты (для плавного движения)
  def move_to_pixel(pixel_x, pixel_y)
    @px = pixel_x
    @py = pixel_y
    @x = (pixel_x / @tile_size).floor
    @y = (pixel_y / @tile_size).floor
  end
end