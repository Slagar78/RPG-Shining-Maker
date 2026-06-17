# lib/battleManager/unit_death.rb
class UnitDeath
  attr_reader :unit, :finished

  def initialize(unit, tile_size = 48)
    @unit = unit
    @tile_size = tile_size
    @timer = 0
    @switch_speed = 5
    @current_step = 0
    @max_steps = 8          # 2 полных оборота (4 направления × 2 = 8 смен)
    @finished = false
    @dir_index = 1
    @anim_frame = 0
    @anim_timer = 0
    @anim_speed = 5

    # Взрыв
    @explosion_tex = Raylib.LoadTexture("assets/mapsprites_NPC/explosion.png")
    Raylib.SetTextureFilter(@explosion_tex, Raylib::TEXTURE_FILTER_POINT)
    @explosion_frame = 0
    @explosion_timer = 0.0
    @explosion_speed = 0.1   # секунд на кадр (можно подогнать под FPS)

    # Состояния
    @spinning_completed = false
    @playing_explosion = false
  end

  def update
    return if @finished

    if @spinning_completed
      # Проигрываем взрыв
      unless @playing_explosion
        @playing_explosion = true
        @explosion_frame = 0
        @explosion_timer = 0.0
      end

      @explosion_timer += Raylib.GetFrameTime()
      if @explosion_timer >= @explosion_speed
        @explosion_timer = 0.0
        @explosion_frame += 1
        if @explosion_frame >= 3
          @finished = true
        end
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
    return if @finished
    return unless @unit

    if @spinning_completed && @playing_explosion && @explosion_frame < 3
      # Рисуем взрыв
      src = Raylib::Rectangle.create(
        0,
        @explosion_frame * 48,
        48,
        48
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
      Raylib.DrawTexturePro(@explosion_tex, src, dest, origin, 0, Raylib::WHITE)
      return
    end

    # Иначе рисуем вращающегося юнита (как было)
    tex = @unit[:tex]
    return unless tex

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

  def unload
    Raylib.UnloadTexture(@explosion_tex) if @explosion_tex
  end
end