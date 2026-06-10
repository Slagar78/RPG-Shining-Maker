# lib/battleManager/unit_death.rb
class UnitDeath
  attr_reader :unit, :finished

  def initialize(unit, tile_size = 48)
    @unit = unit
    @tile_size = tile_size
    @timer = 0
    @switch_speed = 5
    @current_step = 0
    @max_steps = 8
    @finished = false
    @dir_index = 1
    @anim_frame = 0
    @anim_timer = 0
    @anim_speed = 5
    # Задержка после вращения (в секундах)
    @delay = 0.7
    @post_spin_timer = 0.0
    @spinning_completed = false
  end

  def update
    return if @finished
    if @spinning_completed
      # Ждём, пока пройдёт задержка
      @post_spin_timer += Raylib.GetFrameTime()
      if @post_spin_timer >= @delay
        @finished = true
      end
      return
    end
    # Фаза вращения
    @timer += 1
    if @timer >= @switch_speed
      @timer = 0
      @current_step += 1
      if @current_step >= @max_steps
        @spinning_completed = true
        return
      end
      @dir_index = (@dir_index + 1) % 4
    end
    @anim_timer += 1
    if @anim_timer >= @anim_speed
      @anim_timer = 0
      @anim_frame = (@anim_frame + 1) % 2
    end
  end

  def draw(camera, highlight_tex = nil)
    return if @finished || @spinning_completed
    return unless @unit && @unit[:tex]   # ← Добавил защиту (главный фикс)

    tex = @unit[:tex]
    
    row, flip = case @dir_index
                when 0 then [2, false]
                when 1 then [1, true]
                when 2 then [0, false]
                when 3 then [1, false]
                end

    src_width = @tile_size.to_f
    src_width = -src_width if flip

    src = Raylib::Rectangle.create(
      @anim_frame * @tile_size.to_f,
      row * @tile_size.to_f,
      src_width,
      @tile_size.to_f
    )

    screen_x = @unit[:x] * @tile_size - camera.x + @tile_size / 2.0
    screen_y = @unit[:y] * @tile_size - camera.y - 16 + @tile_size / 2.0

    dest = Raylib::Rectangle.create(
      screen_x - @tile_size / 2.0,
      screen_y - @tile_size / 2.0,
      @tile_size.to_f,
      @tile_size.to_f
    )

    origin = Raylib::Vector2.create(0, 0)
    Raylib.DrawTexturePro(tex, src, dest, origin, 0, Raylib::WHITE)
  end
end