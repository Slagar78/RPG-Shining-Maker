# lib/battleManager/hp_mp_panel.rb
class HpMpPanel
  BASE_W = 168
  BASE_H = 88
  PADDING_LEFT = 12
  PADDING_TOP = 8
  LINE_HEIGHT = 22
  FONT_SIZE = 20

  STICK_W = 3
  STICK_H = 18
  STICK_GAP = 4
  THRESHOLD = 20
  MAX_DISPLAY_STICKS = 100

  LEFT_EDGE_W = 10
  RIGHT_EDGE_W = 10
  MID_TILE_W = 8

  def initialize(font = nil)
    @font = font
    @texture = load_texture("assets/ui/HpMpPanel.png")
    @stick_tex      = load_texture("assets/ui/Hp_Mp_Points.png")   # уровень 0 (жёлтый)
    @stick_tex_lvl1 = load_texture("assets/ui/Hp_Mp_Points_2.png") # уровень 1 (>100)
    @stick_tex_lvl2 = load_texture("assets/ui/Hp_Mp_Points_3.png") # уровень 2 (>200)
    @stick_tex_lvl3 = load_texture("assets/ui/Hp_Mp_Points_4.png") # уровень 3 (>300)
    if @texture
      @tex_width = @texture.width
      @tex_height = @texture.height
    else
      @tex_width = 0
      @tex_height = 0
    end
  end

  def load_texture(path)
    return nil unless File.exist?(path)
    img = LoadImage(path)
    tex = LoadTextureFromImage(img)
    UnloadImage(img)
    SetTextureFilter(tex, TEXTURE_FILTER_POINT)
    tex
  end

  def draw(unit, db)
    return unless unit

    if unit[:actor]
      name = unit[:actor]["name"] || "???"
      lvl  = unit[:actor]["level"] || 1
    elsif unit[:enemy]
      e = unit[:enemy]
      name = e.respond_to?(:name) ? e.name : e["name"] || "???"
      lvl  = e.respond_to?(:level) ? e.level : e["level"] || 1
    else
      return
    end

    hp     = unit[:hp]     || 0
    max_hp = unit[:max_hp] || 0
    mp     = unit[:mp]     || 0
    max_mp = unit[:max_mp] || 0

    # ---------- 1. Вычисление ширины панели ----------
    label_w = [measure_text("HP"), measure_text("MP")].max
    max_number_str_hp = "#{max_hp}/#{max_hp}"
    max_number_str_mp = "#{max_mp}/#{max_mp}"
    max_number_w = [measure_text(max_number_str_hp), measure_text(max_number_str_mp)].max

    max_sticks = [[max_hp, max_mp].max, MAX_DISPLAY_STICKS].min
    sticks_width = max_sticks * STICK_W

    content_width = PADDING_LEFT + label_w + STICK_GAP + sticks_width + STICK_GAP + max_number_w + PADDING_LEFT
    raw_width = [BASE_W, content_width].max

    mid_area = raw_width - LEFT_EDGE_W - RIGHT_EDGE_W
    tiles = (mid_area.to_f / MID_TILE_W).ceil
    panel_width = LEFT_EDGE_W + RIGHT_EDGE_W + tiles * MID_TILE_W

    name_line = "#{name}  LV #{lvl}"

    x = 576 - panel_width - 8
    y = 8

    # ---------- 2. Фон панели (рамка + тайлинг середины) ----------
    if @texture
      left_src = Rectangle.create(0, 0, LEFT_EDGE_W, @tex_height)
      left_dst = Rectangle.create(x, y, LEFT_EDGE_W, BASE_H)
      DrawTexturePro(@texture, left_src, left_dst, Vector2.create(0, 0), 0, WHITE)

      right_src = Rectangle.create(@tex_width - RIGHT_EDGE_W, 0, RIGHT_EDGE_W, @tex_height)
      right_dst = Rectangle.create(x + panel_width - RIGHT_EDGE_W, y, RIGHT_EDGE_W, BASE_H)
      DrawTexturePro(@texture, right_src, right_dst, Vector2.create(0, 0), 0, WHITE)

      tile_src = Rectangle.create(LEFT_EDGE_W + 2, 0, MID_TILE_W, @tex_height)
      tiles.times do |i|
        tile_x = x + LEFT_EDGE_W + i * MID_TILE_W
        tile_dst = Rectangle.create(tile_x, y, MID_TILE_W, BASE_H)
        DrawTexturePro(@texture, tile_src, tile_dst, Vector2.create(0, 0), 0, WHITE)
      end
    else
      DrawRectangle(x, y, panel_width, BASE_H, Fade(BLACK, 0.8))
      DrawRectangleLines(x, y, panel_width, BASE_H, WHITE)
    end

    # ---------- 3. Текст и палочки ----------
    tx = x + PADDING_LEFT
    ty = y + PADDING_TOP
    draw_text(name_line, tx, ty, WHITE)

    draw_hpmp_bar(x, y, panel_width, tx, ty + LINE_HEIGHT, "HP", hp, max_hp, label_w)
    draw_hpmp_bar(x, y, panel_width, tx, ty + LINE_HEIGHT * 2, "MP", mp, max_mp, label_w)
  end

  private

  def draw_hpmp_bar(panel_x, panel_y, panel_width, base_x, base_y, label, current, maximum, label_w)
    draw_text(label, base_x, base_y, WHITE)

    bar_start_x = base_x + label_w + STICK_GAP

    # Число
    number_str = "#{current}/#{maximum}"
    number_w = measure_text(number_str)

    # Сколько палочек физически поместится
    available = panel_x + panel_width - PADDING_LEFT - bar_start_x - STICK_GAP - number_w
    total_positions = [(available / STICK_W).floor, MAX_DISPLAY_STICKS].min
    total_positions = 0 if total_positions < 0

    # Сколько палочек надо нарисовать (не больше current и maximum)
    sticks_to_draw = [current, maximum].min
    sticks_to_draw = [sticks_to_draw, total_positions].min

    stick_y = base_y + (FONT_SIZE - STICK_H) / 2

    # Массив слоёв: [порог, текстура]
    layers = [
      { threshold: 0,   tex: @stick_tex },       # жёлтый, рисуется всегда поверх предыдущего, если current > threshold
      { threshold: 100, tex: @stick_tex_lvl1 },
      { threshold: 200, tex: @stick_tex_lvl2 },
      { threshold: 300, tex: @stick_tex_lvl3 }
    ]

    # Рисуем только нужное количество палочек, начиная с самого высокого слоя, который перекрывает
    # Идём от последнего слоя к первому (чтобы верхние слои рисовались последними и перекрывали нижние)
    # Но в рамках Shining Force порядок: сначала рисуются все палочки нижнего слоя, потом верхние накладываются.
    # Поэтому сначала определяем для каждого слоя, сколько он должен занять, и рисуем последовательно.

    # Для каждого слоя вычисляем, сколько позиций он занимает (с учётом того, что следующий слой перекрывает правую часть)
    # Начинаем с самого высокого слоя (threshold = 300)
    drawn = 0
    (layers.length - 1).downto(1) do |i|
      threshold = layers[i][:threshold]
      if current > threshold
        count = [current - threshold, total_positions - drawn].min
        if count > 0
          tex = layers[i][:tex]
          start_idx = total_positions - drawn - count
          count.times do |j|
            stick_x = bar_start_x + (start_idx + j) * STICK_W
            if tex
              src = Rectangle.create(0, 0, tex.width, tex.height)
              dst = Rectangle.create(stick_x, stick_y, STICK_W, STICK_H)
              DrawTexturePro(tex, src, dst, Vector2.create(0, 0), 0, WHITE)
            end
          end
          drawn += count
        end
      end
    end

    # Оставшееся место рисуем базовым жёлтым, если нужно
    remaining = sticks_to_draw - drawn
    if remaining > 0
      tex = layers[0][:tex]
      remaining.times do |j|
        stick_x = bar_start_x + j * STICK_W   # жёлтые всегда слева
        if tex
          src = Rectangle.create(0, 0, tex.width, tex.height)
          dst = Rectangle.create(stick_x, stick_y, STICK_W, STICK_H)
          DrawTexturePro(tex, src, dst, Vector2.create(0, 0), 0, WHITE)
        else
          color = Color.new; color.r = 194; color.g = 178; color.b = 128; color.a = 255
          DrawRectangle(stick_x, stick_y, STICK_W, STICK_H, color)
        end
      end
    end

    # Число (прижимаем к правому краю, если не влезает)
    numbers_x = bar_start_x + sticks_to_draw * STICK_W + STICK_GAP
    if numbers_x + number_w > panel_x + panel_width - PADDING_LEFT
      numbers_x = panel_x + panel_width - PADDING_LEFT - number_w
    end
    draw_text(number_str, numbers_x, base_y, WHITE)
  end

  def measure_text(text)
    if @font
      vec = MeasureTextEx(@font, text, FONT_SIZE, 1)
      vec.x
    else
      MeasureText(text, FONT_SIZE)
    end
  end

  def draw_text(text, x, y, color)
    if @font
      DrawTextEx(@font, text, Vector2.create(x, y), FONT_SIZE, 1, color)
    else
      DrawText(text, x, y, FONT_SIZE, color)
    end
  end
end