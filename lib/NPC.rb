# lib/NPC.rb

class NPC
  attr_reader :x, :y, :id

  TILE_SIZE = 48
  PIXEL_SPEED = 4
  ANIM_SPEED = 12

  DIR_MAP = {
    'up'    => 0,
    'left'  => 1,
    'right' => 1,   # та же строка, что и left
    'down'  => 2
  }

  def initialize(data)
    @id = data['id']
    @x = data['x']
    @y = data['y']
    @sprite_name = data['sprite']
    @behavior = data['behavior'] || 'static'
    @direction = data['direction'] || 'down'

    @home_x = data['home_x'] || @x
    @home_y = data['home_y'] || @y
    @radius = data['radius'] || 3

    @moving = false
    @move_dir = nil
    @pixel_offset = 0
    @wait_timer = rand(40..120)
    @pattern = 0
    @anim_timer = 0

    # Текстуры: левая (обычная) и правая (зеркальная)
    @tex_left = nil
    @tex_right = nil
    load_sprite
  end

  def load_sprite
    path = "assets/mapsprites_NPC/#{@sprite_name}.png"
    return unless File.exist?(path)

    img = LoadImage(path)
    img_mirror = ImageCopy(img)
    ImageFlipHorizontal(img_mirror)

    @tex_left  = LoadTextureFromImage(img)
    @tex_right = LoadTextureFromImage(img_mirror)

    SetTextureFilter(@tex_left,  TEXTURE_FILTER_POINT)
    SetTextureFilter(@tex_right, TEXTURE_FILTER_POINT)

    UnloadImage(img)
    UnloadImage(img_mirror)
  end

  def update(map, player)
    case @behavior
    when 'wander' then update_wander(map, player)
    end
    update_animation
    update_movement if @moving
  end

  def update_animation
    @anim_timer += 1
    if @anim_timer >= ANIM_SPEED
      @anim_timer = 0
      @pattern = (@pattern + 1) % 2
    end
  end

  def update_movement
    @pixel_offset += PIXEL_SPEED
    if @pixel_offset >= TILE_SIZE
      case @move_dir
      when :right then @x += 1
      when :left  then @x -= 1
      when :down  then @y += 1
      when :up    then @y -= 1
      end
      @pixel_offset = 0
      @moving = false
      @move_dir = nil
    end
  end

  def visual_x
    base = @x * TILE_SIZE
    if @moving
      case @move_dir
      when :right then base + @pixel_offset
      when :left  then base - @pixel_offset
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
      when :down then base + @pixel_offset
      when :up   then base - @pixel_offset
      else base
      end
    else
      base
    end
  end

  def draw(camera)
    # Выбираем текстуру в зависимости от направления
    texture = (@direction == 'right') ? @tex_right : @tex_left
    return unless texture

    dir_row = DIR_MAP[@direction] || 0
    frame_w = 48
    frame_h = 48
    col = @pattern % 2

    src = Raylib::Rectangle.create(col * frame_w, dir_row * frame_h, frame_w, frame_h)
    dest = Raylib::Rectangle.create(visual_x, visual_y - 16, frame_w, frame_h)
    Raylib.DrawTexturePro(texture, src, dest, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
  end

  private

  def update_wander(map, player)
    return if @moving
    if @wait_timer > 0
      @wait_timer -= 1
      return
    end

    dirs = [:down, :left, :right, :up].shuffle
    dirs.each do |dir|
      new_x = @x
      new_y = @y
      case dir
      when :down  then new_y += 1
      when :up    then new_y -= 1
      when :left  then new_x -= 1
      when :right then new_x += 1
      end

      next if (new_x - @home_x).abs > @radius
      next if (new_y - @home_y).abs > @radius
      next if map && !map.passable?(new_x, new_y)
      next if player && new_x == player.x && new_y == player.y

      @direction = dir.to_s
      @move_dir = dir
      @moving = true
      @pixel_offset = 0
      @wait_timer = rand(40..120)
      return
    end

    @wait_timer = rand(40..120)
  end
end