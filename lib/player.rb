# lib/player.rb
require 'raylib'

TILE_SIZE = 48
DEFAULT_GRID_W = 12
DEFAULT_GRID_H = 10

DIR_DOWN  = 2
DIR_LEFT  = 4
DIR_RIGHT = 6
DIR_UP    = 8

PIXEL_SPEED = 4         # пикселей за кадр (целое число)
ANIM_SPEED  = 12

class Player
  attr_accessor :x, :y, :direction, :pattern
  attr_accessor :moving, :move_dir, :pixel_offset
  attr_accessor :anim_frame
  attr_accessor :can_move
  attr_accessor :map

  def initialize(map = nil)
    @map = map

    if @map
      @x = @map.width / 2
      @y = @map.height / 2
    else
      @x = 6
      @y = 5
    end

    @direction     = DIR_DOWN
    @pattern       = 0
    @moving        = false
    @move_dir      = DIR_DOWN
    @pixel_offset  = 0
    @anim_frame    = 0
    @can_move      = true
    @just_turned   = false

    @sliding       = false
    @slide_dir     = DIR_DOWN

    init_render_objects
    load_textures
  end

  def init_render_objects
    @src_rect    = Rectangle.create(0, 0, TILE_SIZE, TILE_SIZE)
    @dst_rect    = Rectangle.create(0, 0, TILE_SIZE, TILE_SIZE)
    @draw_origin = Vector2.create(0, 0)
  end

  def load_textures
    img = LoadImage("assets/mapsprites/hero.png")
    img_mirror = ImageCopy(img)
    ImageFlipHorizontal(img_mirror)

    @tex_left  = LoadTextureFromImage(img)
    @tex_right = LoadTextureFromImage(img_mirror)

    SetTextureFilter(@tex_left,  TEXTURE_FILTER_POINT)
    SetTextureFilter(@tex_right, TEXTURE_FILTER_POINT)

    UnloadImage(img)
    UnloadImage(img_mirror)
  end

  # ====================== INPUT ======================
  def handle_input
    return unless @can_move
    return if @sliding
    return if @moving

    @just_turned = false

    if IsKeyDown(KEY_RIGHT)
      try_move_or_turn(DIR_RIGHT)
    elsif IsKeyDown(KEY_LEFT)
      try_move_or_turn(DIR_LEFT)
    elsif IsKeyDown(KEY_DOWN)
      try_move_or_turn(DIR_DOWN)
    elsif IsKeyDown(KEY_UP)
      try_move_or_turn(DIR_UP)
    end
  end

  def try_move_or_turn(dir)
    if @direction != dir
      @direction = dir
      @just_turned = true
      return
    end

    return if @just_turned

    new_x = @x
    new_y = @y

    case dir
    when DIR_RIGHT then new_x += 1
    when DIR_LEFT  then new_x -= 1
    when DIR_DOWN  then new_y += 1
    when DIR_UP    then new_y -= 1
    end

    if @map
      return if new_x < 0 || new_x >= @map.width
      return if new_y < 0 || new_y >= @map.height
      return unless @map.passable?(new_x, new_y)
    else
      return if new_x < 0 || new_x >= DEFAULT_GRID_W
      return if new_y < 0 || new_y >= DEFAULT_GRID_H
    end

    @slide_dir = dir
    @move_dir  = dir
    @moving    = true
    @pixel_offset = 0
  end

  # ====================== UPDATE ======================
  def update_animation
    return if @sliding               # на льду не меняем кадр анимации
    @anim_frame += 1
    if @anim_frame >= ANIM_SPEED
      @anim_frame = 0
      @pattern = (@pattern + 1) % 2
    end
end

  def update_movement
    return unless @moving

    @pixel_offset += PIXEL_SPEED
    if @pixel_offset >= TILE_SIZE
      case @move_dir
      when DIR_RIGHT then @x += 1
      when DIR_LEFT  then @x -= 1
      when DIR_DOWN  then @y += 1
      when DIR_UP    then @y -= 1
      end

      if @map && @map.tile_type_at(@x, @y) == 2   # лёд
        next_x = @x
        next_y = @y
        case @move_dir
        when DIR_RIGHT then next_x += 1
        when DIR_LEFT  then next_x -= 1
        when DIR_DOWN  then next_y += 1
        when DIR_UP    then next_y -= 1
        end

        if next_x >= 0 && next_x < @map.width &&
           next_y >= 0 && next_y < @map.height &&
           @map.passable?(next_x, next_y)
          @sliding = true
          @moving  = true
          @pixel_offset = 0
        else
          @sliding = false
          @moving  = false
          @pixel_offset = 0
        end
      else
        @sliding = false
        @moving  = false
        @pixel_offset = 0
      end
    end
  end

  def update
    update_animation
    update_movement
  end

  # ====================== DRAW ======================
  def draw
    px = visual_x
    py = (@y * TILE_SIZE - 16) + (@moving ? pixel_offset_y : 0)

    texture = (@direction == DIR_RIGHT) ? @tex_right : @tex_left

    row = case @direction
          when DIR_UP    then 0
          when DIR_LEFT, DIR_RIGHT then 1
          else 2
          end

    @src_rect.x = @pattern * TILE_SIZE
    @src_rect.y = row * TILE_SIZE

    @dst_rect.x = px
    @dst_rect.y = py

    # Для простоты используем WHITE (без прозрачности)
    DrawTexturePro(texture, @src_rect, @dst_rect, @draw_origin, 0, WHITE)
  end

  # ====================== VISUAL POSITION ======================
  def visual_x
    base = @x * TILE_SIZE
    if @moving
      case @move_dir
      when DIR_RIGHT then base + @pixel_offset
      when DIR_LEFT  then base - @pixel_offset
      else base
      end
    else
      base
    end
  end

  def visual_y
    base = @y * TILE_SIZE
    if @moving
      case @move_dir
      when DIR_DOWN then base + @pixel_offset
      when DIR_UP   then base - @pixel_offset
      else base
      end
    else
      base
    end
  end

  private

  def pixel_offset_y
    case @move_dir
    when DIR_DOWN then @pixel_offset
    when DIR_UP   then -@pixel_offset
    else 0
    end
  end
end