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
  attr_accessor :x, :y, :direction, :pattern, :last_x, :last_y
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
	
	# поля для предыдущей позиции
    @last_x = @x
    @last_y = @y
	
	@stairs_event  = nil
    @stairs_dx     = 0
    @stairs_dy     = 0
    @target_x      = 0
    @target_y      = 0
			
	@reserved_x = nil
    @reserved_y = nil

    init_render_objects
    load_textures

  end

  def moving_to?(tx, ty)
    @moving && @reserved_x == tx && @reserved_y == ty
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
  return if @stairs_event

  # === Ручной старт движения по лестнице с любой клетки ===
  if @map
    ev = @map.stairs_at(@x, @y)
    if ev
      input_dir = nil
      if    IsKeyDown(KEY_LEFT)  then input_dir = [-1, 0]
      elsif IsKeyDown(KEY_RIGHT) then input_dir = [1, 0]
      elsif IsKeyDown(KEY_UP)    then input_dir = [0, -1]
      elsif IsKeyDown(KEY_DOWN)  then input_dir = [0, 1]
      end

      if input_dir
        info = @map.stairs_direction(@x, @y, *input_dir)
        if info
          @stairs_event = ev
          @stairs_dx, @stairs_dy, @target_x, @target_y = info
          @moving = true
          @pixel_offset = 0
          if @stairs_dx == 1
            @direction = DIR_RIGHT
          elsif @stairs_dx == -1
            @direction = DIR_LEFT
          end
          return
        end
      end
    end
  end
  # === Конец блока лестниц ===

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
    return unless @map.passable?(new_x, new_y, @x, @y) && @map.inside_area?(new_x, new_y)
    return if @map.npc_at?(new_x, new_y)                                          # ← стоящие NPC
    return if @map.npcs.any? { |npc| npc.moving_to?(new_x, new_y) }              # ← движущиеся NPC
    else
      return if new_x < 0 || new_x >= DEFAULT_GRID_W
      return if new_y < 0 || new_y >= DEFAULT_GRID_H
    end

    @slide_dir = dir
    @move_dir  = dir
	@reserved_x = new_x
    @reserved_y = new_y
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
    # === Движение по лестнице ===
    if @stairs_event
      @pixel_offset += PIXEL_SPEED
      if @pixel_offset >= TILE_SIZE
        @pixel_offset = 0
        @x += @stairs_dx
        @y += @stairs_dy
        @last_x = @x - @stairs_dx   # предыдущая позиция
        @last_y = @y - @stairs_dy
        if @x == @target_x && @y == @target_y
          @moving = false
          @stairs_event = nil
        end
      end
      return
    end

    return unless @moving

    @pixel_offset += PIXEL_SPEED
    if @pixel_offset >= TILE_SIZE
      old_x = @x
      old_y = @y

      case @move_dir
      when DIR_RIGHT then @x += 1
      when DIR_LEFT  then @x -= 1
      when DIR_DOWN  then @y += 1
      when DIR_UP    then @y -= 1
      end

      @last_x = old_x
      @last_y = old_y

      # Снимаем резервирование, т.к. шаг выполнен
      @reserved_x = nil
      @reserved_y = nil

      # Проверка на вход в лестницу после каждого шага
      maybe_start_stairs

      # Обработка льда
      if @map && @map.tile_type_at(@x, @y) == 2
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
           @map.passable?(next_x, next_y, @x, @y)
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

def maybe_start_stairs
  return unless @map
  ev = @map.stairs_at(@x, @y)
  return unless ev

  x1 = ev['start_x']; y1 = ev['start_y']
  x2 = ev['end_x'];   y2 = ev['end_y']

  # не запускаем, если уже стоим на конечной клетке
  return if @x == x2 && @y == y2

  dx = (x2 - x1) <=> 0
  dy = (y2 - y1) <=> 0

  @stairs_event = ev
  @stairs_dx = dx
  @stairs_dy = dy
  @target_x = x2
  @target_y = y2

  if dx == 1
    @direction = DIR_RIGHT
  elsif dx == -1
    @direction = DIR_LEFT
  end

  @moving = true
  @pixel_offset = 0
end

  def update
    update_animation
    update_movement
  end

  # ====================== DRAW ======================
  def draw
    px = visual_x
    py = visual_y - 16

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
  if @stairs_event
    case @stairs_dx
    when 1  then base + @pixel_offset
    when -1 then base - @pixel_offset
    else base
    end
  elsif @moving
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
  if @stairs_event
    case @stairs_dy
    when 1  then base + @pixel_offset
    when -1 then base - @pixel_offset
    else base
    end
  elsif @moving
    case @move_dir
    when DIR_DOWN then base + @pixel_offset
    when DIR_UP   then base - @pixel_offset
    else base
    end
  else
    base
  end
end

end