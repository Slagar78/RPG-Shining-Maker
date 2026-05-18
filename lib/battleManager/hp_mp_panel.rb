# lib/battleManager/hp_mp_panel.rb
class HpMpPanel
  BASE_W = 120
  BASE_H = 88
  PADDING_LEFT = 12
  PADDING_TOP = 8
  LINE_HEIGHT = 22
  FONT_SIZE = 20

  STICK_W = 2
  STICK_H = 18
  STICK_GAP = 4
  THRESHOLD = 20
  MAX_DISPLAY_STICKS = 100   # ширина шкалы в палочках

  LEFT_EDGE_W = 10
  RIGHT_EDGE_W = 10
  MID_TILE_W = 8

  def initialize(font = nil)
    @font = font
    @texture = load_texture("assets/ui/HpMpPanel.png")
    @stick_tex      = load_texture("assets/ui/Hp_Mp_Points.png")   # жёлтый
    @stick_tex_lvl1 = load_texture("assets/ui/Hp_Mp_Points_2.png") # зелёный (>100)
    @stick_tex_lvl2 = load_texture("assets/ui/Hp_Mp_Points_3.png") # фиолетовый (>200)
    @stick_tex_lvl3 = load_texture("assets/ui/Hp_Mp_Points_4.png") # чёрный (>300)
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

    # 1. Расчёт ширины панели
    label_w = [measure_text("HP"), measure_text("MP")].max
    max_number_str_hp = "#{max_hp}/#{max_hp}"
    max_number_str_mp = "#{max_mp}/#{max_mp}"
    max_number_w = [measure_text(max_number_str_hp), measure_text(max_number_str_mp)].max

    max_sticks = [[max_hp, max_mp].max, MAX_DISPLAY_STICKS].min
    sticks_width = max_sticks * STICK_W

    content_width = PADDING_LEFT + label_w + STICK_GAP + sticks_width + STICK_GAP + max_number_w + PADDING_LEFT

    name_line = "#{name}  LV #{lvl}"
    name_line_w = measure_text(name_line)
    name_content_width = name_line_w + PADDING_LEFT * 2

    raw_width = [BASE_W, content_width, name_content_width].max

    mid_area = raw_width - LEFT_EDGE_W - RIGHT_EDGE_W
    tiles = (mid_area.to_f / MID_TILE_W).ceil
    panel_width = LEFT_EDGE_W + RIGHT_EDGE_W + tiles * MID_TILE_W

    x = 576 - panel_width - 8
    y = 8

    # 2. Фон панели
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

    # 3. Текст и палочки
    tx = x + PADDING_LEFT
    ty = y + PADDING_TOP
    draw_text(name_line, tx, ty, WHITE)

    draw_hpmp_bar(x, y, panel_width, tx, ty + LINE_HEIGHT, "HP", hp, max_hp, label_w)
    draw_hpmp_bar(x, y, panel_width, tx, ty + LINE_HEIGHT * 2, "MP", mp, max_mp, label_w)
  end

  # Рисует панель в правом нижнем углу (для цели атаки)
  def draw_bottom_right(unit, db)
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

    # 1. Расчёт ширины панели (аналогично обычному draw)
    label_w = [measure_text("HP"), measure_text("MP")].max
    max_number_str_hp = "#{max_hp}/#{max_hp}"
    max_number_str_mp = "#{max_mp}/#{max_mp}"
    max_number_w = [measure_text(max_number_str_hp), measure_text(max_number_str_mp)].max

    max_sticks = [[max_hp, max_mp].max, MAX_DISPLAY_STICKS].min
    sticks_width = max_sticks * STICK_W

    content_width = PADDING_LEFT + label_w + STICK_GAP + sticks_width + STICK_GAP + max_number_w + PADDING_LEFT

    name_line = "#{name}  LV #{lvl}"
    name_line_w = measure_text(name_line)
    name_content_width = name_line_w + PADDING_LEFT * 2

    raw_width = [BASE_W, content_width, name_content_width].max

    mid_area = raw_width - LEFT_EDGE_W - RIGHT_EDGE_W
    tiles = (mid_area.to_f / MID_TILE_W).ceil
    panel_width = LEFT_EDGE_W + RIGHT_EDGE_W + tiles * MID_TILE_W

    # === ОТЛИЧИЕ: координаты для правого нижнего угла ===
    x = 576 - panel_width - 8
    y = 480 - BASE_H - 8

    # 2. Фон панели (рисуем точно так же)
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

    # 3. Текст и палочки
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
    number_str = "#{current}/#{maximum}"
    number_w = measure_text(number_str)

    # Определяем, сколько палочек влезает физически
    available = panel_x + panel_width - PADDING_LEFT - bar_start_x - STICK_GAP - number_w
    total_positions = [(available / STICK_W).floor, MAX_DISPLAY_STICKS].min
    total_positions = 0 if total_positions < 0

    stick_y = base_y + (FONT_SIZE - STICK_H) / 2

    # Красные палочки ТОЛЬКО для потерянного HP (от current до maximum, но не больше total_positions)
    max_visible = [maximum, total_positions].min
    (current...max_visible).each do |i|
      stick_x = bar_start_x + i * STICK_W
      red = Color.new
      red.r = 220; red.g = 40; red.b = 40; red.a = 200
      DrawRectangle(stick_x, stick_y, STICK_W, STICK_H, red)
    end

    # Затем рисуем цветные палочки текущего здоровья (поверх красных)
    layers = [
      { tex: @stick_tex,      count: [current, 100].min },
      { tex: @stick_tex_lvl1, count: [current - 100, 100].min },
      { tex: @stick_tex_lvl2, count: [current - 200, 100].min },
      { tex: @stick_tex_lvl3, count: [current - 300, 100].min }
    ]

    layers.each do |layer|
      count = layer[:count]
      next if count <= 0
      tex = layer[:tex]
      (0...count).each do |i|
        stick_x = bar_start_x + i * STICK_W
        if tex
          src = Rectangle.create(0, 0, tex.width, tex.height)
          dst = Rectangle.create(stick_x, stick_y, STICK_W, STICK_H)
          DrawTexturePro(tex, src, dst, Vector2.create(0, 0), 0, WHITE)
        else
          fallback = Color.new
          fallback.r = 194; fallback.g = 178; fallback.b = 128; fallback.a = 255
          DrawRectangle(stick_x, stick_y, STICK_W, STICK_H, fallback)
        end
      end
    end

    # Число справа
    number_x = bar_start_x + total_positions * STICK_W + STICK_GAP
    if number_x + number_w > panel_x + panel_width - PADDING_LEFT
      number_x = panel_x + panel_width - PADDING_LEFT - number_w
    end
    draw_text(number_str, number_x, base_y, WHITE)
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