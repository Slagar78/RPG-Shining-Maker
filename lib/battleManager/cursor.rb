# lib/battleManager/cursor.rb

class BattleCursor
  attr_accessor :x, :y, :visible   # тайловые координаты (для информации)
  attr_accessor :px, :py           # пиксельные координаты ЦЕНТРА курсора
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
    # здесь можно добавить плавное движение, если нужно
  end

  def draw(cam_x, cam_y)
    return unless @visible && @tex

    # Центрируем текстуру на точке (px, py)
    draw_x = @px - @tex.width / 2.0
    draw_y = @py - @tex.height / 2.0

    dst = Raylib::Rectangle.create(
      draw_x + cam_x,
      draw_y + cam_y,
      @tex.width,
      @tex.height
    )
    src = Raylib::Rectangle.create(0, 0, @tex.width, @tex.height)
    Raylib.DrawTexturePro(@tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end

  # Переместить на тайл (центр тайла становится точкой px, py)
  def move_to(tile_x, tile_y)
    @x = tile_x
    @y = tile_y
    @px = tile_x * @tile_size + @tile_size / 2.0
    @py = tile_y * @tile_size + @tile_size / 2.0
    @visible = true
  end

  # Переместить в произвольную пиксельную точку (например, для интерполяции)
  def move_to_pixel(pixel_x, pixel_y)
    @px = pixel_x
    @py = pixel_y
    @x = (pixel_x / @tile_size).floor
    @y = (pixel_y / @tile_size).floor
    @visible = true
  end
end