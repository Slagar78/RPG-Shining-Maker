# lib/battleManager/battle_player.rb

TILE_SIZE = 48
PIXEL_SPEED = 4
ANIM_SPEED  = 12

DIR_DOWN  = 2
DIR_LEFT  = 4
DIR_RIGHT = 6
DIR_UP    = 8

class BattlePlayer
  attr_reader :direction, :moving, :remaining_moves
  attr_accessor :blinking; attr_reader :blink_alpha
  attr_accessor :x, :y, :map_width, :map_height

  def initialize(unit, highlight_tiles, tex, tile_size = TILE_SIZE)
    @unit = unit
    @x = unit[:x]
    @y = unit[:y]
    @tex = tex
    @tile_size = tile_size
    @direction = DIR_DOWN
    @pattern = 0
    @anim_frame = 0
    @moving = false
    @move_dir = DIR_DOWN
    @pixel_offset = 0
    @highlight_tiles = highlight_tiles
    @remaining_moves = unit[:mov]
	
	@blinking = false
    @blink_timer = 0
    @blink_alpha = 255   # начальная непрозрачность
	
    init_render_objects
  end

  def init_render_objects
    @src_rect    = Rectangle.create(0, 0, @tile_size, @tile_size)
    @dst_rect    = Rectangle.create(0, 0, @tile_size, @tile_size)
    @draw_origin = Vector2.create(0, 0)
  end

  # Новый метод: обновить разрешённые для хода клетки
  def update_highlight_tiles(new_tiles)
    @highlight_tiles = new_tiles
  end

def try_move(dir, battle_manager = nil)
  return if @moving

  # 1. Сразу разрешаем поворот на месте, если направление изменилось
  if @direction != dir
    @direction = dir
    return
  end

  # 2. Только потом проверяем, можно ли сделать шаг
  new_x = @x
  new_y = @y
  case dir
  when DIR_RIGHT then new_x += 1
  when DIR_LEFT  then new_x -= 1
  when DIR_DOWN  then new_y += 1
  when DIR_UP    then new_y -= 1
  end

  return unless @highlight_tiles.include?([new_x, new_y])
  return if new_x < 0 || new_x >= @map_width || new_y < 0 || new_y >= @map_height

  @move_dir = dir
  @moving = true
  @pixel_offset = 0
end

  def move_towards(target_x, target_y, battle_manager = nil)
    dx = target_x - @x
    dy = target_y - @y
    dir = nil
    dir = DIR_RIGHT if dx > 0
    dir = DIR_LEFT  if dx < 0
    dir = DIR_DOWN  if dy > 0
    dir = DIR_UP    if dy < 0
    return unless dir
    try_move(dir, battle_manager)
  end

  def teleport(new_x, new_y)
    @x = new_x
    @y = new_y
    @pixel_offset = 0
    @moving = false
    @move_dir = DIR_DOWN   # значение по умолчанию
  end

  def face_target(target_x, target_y)
    dx = target_x - @x
    dy = target_y - @y
    if dx > 0
      @direction = DIR_RIGHT
    elsif dx < 0
      @direction = DIR_LEFT
    elsif dy > 0
      @direction = DIR_DOWN
    elsif dy < 0
      @direction = DIR_UP
    end
  end

  def handle_input(battle_manager = nil)
    return if @moving
    dir = nil
    dir = DIR_RIGHT if IsKeyDown(KEY_RIGHT)
    dir = DIR_LEFT  if IsKeyDown(KEY_LEFT)
    dir = DIR_DOWN  if IsKeyDown(KEY_DOWN)
    dir = DIR_UP    if IsKeyDown(KEY_UP)
    return unless dir
    try_move(dir, battle_manager)
  end

  def update
    update_movement
    update_animation
	update_blinking
  end

  def update_movement
    return unless @moving
    @pixel_offset += PIXEL_SPEED

    if @pixel_offset >= @tile_size
      case @move_dir
      when DIR_RIGHT then @x += 1
      when DIR_LEFT  then @x -= 1
      when DIR_DOWN  then @y += 1
      when DIR_UP    then @y -= 1
      end
      @pixel_offset = 0
      @moving = false
    end
  end

  def update_animation
    @anim_frame += 1
    if @anim_frame >= ANIM_SPEED
      @anim_frame = 0
      @pattern = (@pattern + 1) % 2
    end
  end

  def update_blinking
    return unless @blinking
    @blink_timer += 1
    # Плавное изменение альфы от 0 до 255, период – примерно 40 кадров (0.67 сек при 60 FPS)
    # Значение sin возвращает -1..1, преобразуем в диапазон 0..255
    raw = Math.sin(@blink_timer * 0.25)   # – скорость мерцания (меньше = медленнее)
    @blink_alpha = ((raw + 1) * 127.5).to_i.clamp(0, 255)
  end

  def draw
    return unless @tex
	
    px = visual_x.round
    py = (visual_y - 16).round

    row = case @direction
          when DIR_UP    then 0
          when DIR_LEFT, DIR_RIGHT then 1
          else 2
          end

    @src_rect.x = @pattern * @tile_size
    @src_rect.y = row * @tile_size

    flip = (@direction == DIR_RIGHT)
    @src_rect.width = flip ? -@tile_size : @tile_size

    @dst_rect.x = px
    @dst_rect.y = py
    # Определяем прозрачность: если мигание включено и фаза невидимая — делаем полупрозрачным
    tint = if @blinking
         Raylib::Fade(Raylib::WHITE, @blink_alpha / 255.0)
       else
         Raylib::WHITE
       end

    DrawTexturePro(@tex, @src_rect, @dst_rect, @draw_origin, 0, tint)

    @src_rect.width = @tile_size
  end

  def visual_x
    base = @x * @tile_size
    case @move_dir
    when DIR_RIGHT then base + @pixel_offset
    when DIR_LEFT  then base - @pixel_offset
    else base
    end
  end

  def visual_y
    base = @y * @tile_size
    case @move_dir
    when DIR_DOWN then base + @pixel_offset
    when DIR_UP   then base - @pixel_offset
    else base
    end
  end
end