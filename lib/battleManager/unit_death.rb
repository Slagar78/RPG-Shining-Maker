# lib/battleManager/unit_death.rb
# Анимация смерти юнита: быстрое вращение на месте (смена направлений по часовой стрелке, два круга)

class UnitDeath
  attr_reader :unit, :finished

  def initialize(unit, tile_size = 48)
    @unit = unit
    @tile_size = tile_size
    @timer = 0
    @switch_speed = 5         # кадров на одно направление (чем меньше, тем быстрее вращение)
    @current_step = 0         # общее количество смен направлений
    @max_steps = 8            # два круга по 4 направления = 8 смен
    @finished = false

    # Начинаем с направления вправо (по часовой стрелке)
    @dir_index = 1            # 0=вниз, 1=вправо, 2=вверх, 3=влево
    @anim_frame = 0
    @anim_timer = 0
    @anim_speed = 5           # скорость смены кадров внутри одного направления
  end

  def update
    return if @finished

    @timer += 1
    # Переключение направления каждые @switch_speed кадров
    if @timer >= @switch_speed
      @timer = 0
      @current_step += 1
      if @current_step >= @max_steps
        @finished = true
        return
      end
      # По часовой стрелке: 1 -> 2 -> 3 -> 0 -> 1 ...
      @dir_index = (@dir_index + 1) % 4
    end

    # Анимация кадров (ноги) для живости
    @anim_timer += 1
    if @anim_timer >= @anim_speed
      @anim_timer = 0
      @anim_frame = (@anim_frame + 1) % 2
    end
  end

  def draw(camera, highlight_tex = nil)
    return if @finished
    tex = @unit[:tex]
    return unless tex

    # Ряд и флип по текущему направлению
    row, flip = case @dir_index
                when 0  # вниз
                  [2, false]
                when 1  # вправо
                  [1, true]
                when 2  # вверх
                  [0, false]
                when 3  # влево
                  [1, false]
                end

    src_width = @tile_size.to_f
    src_width = -src_width if flip   # отражаем текстуру по горизонтали для вправо

    src = Raylib::Rectangle.create(
      @anim_frame * @tile_size.to_f,
      row * @tile_size.to_f,
      src_width,
      @tile_size.to_f
    )

    # Рисуем по центру клетки
    screen_x = @unit[:x] * @tile_size - camera.x + @tile_size / 2.0
    screen_y = @unit[:y] * @tile_size - camera.y + @tile_size / 2.0
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