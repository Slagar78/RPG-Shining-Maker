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

  # Ширина неизменных краёв панели (левого и правого, с закруглениями)
  LEFT_EDGE_W = 8
  RIGHT_EDGE_W = 8

  def initialize(font = nil)
    @font = font
    @texture = load_texture("assets/ui/HpMpPanel.png")
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

    # Вычисляем требуемую ширину панели (после порога – по 3px за единицу)
    max_val = [max_hp, max_mp].max
    extra = max_val > THRESHOLD ? max_val - THRESHOLD : 0
    panel_width = BASE_W + extra * STICK_W

    name_line = "#{name}  LV #{lvl}"

    # Правый край фиксирован, левый сдвигается влево
    x = 576 - panel_width - 8
    y = 8

    # Отрисовка фона панели из трёх частей (только если текстура загружена)
    if @texture
      # 1. Левый край (скруглённый) – фиксированной ширины
      left_src = Rectangle.create(0, 0, LEFT_EDGE_W, @tex_height)
      left_dst = Rectangle.create(x, y, LEFT_EDGE_W, BASE_H)
      DrawTexturePro(@texture, left_src, left_dst, Vector2.create(0, 0), 0, WHITE)

      # 2. Правый край (скруглённый) – фиксированной ширины
      right_src = Rectangle.create(@tex_width - RIGHT_EDGE_W, 0, RIGHT_EDGE_W, @tex_height)
      right_dst = Rectangle.create(x + panel_width - RIGHT_EDGE_W, y, RIGHT_EDGE_W, BASE_H)
      DrawTexturePro(@texture, right_src, right_dst, Vector2.create(0, 0), 0, WHITE)

      # 3. Середина – растягивается на всю оставшуюся ширину
      middle_src = Rectangle.create(LEFT_EDGE_W, 0, @tex_width - LEFT_EDGE_W - RIGHT_EDGE_W, @tex_height)
      middle_dst = Rectangle.create(x + LEFT_EDGE_W, y, panel_width - LEFT_EDGE_W - RIGHT_EDGE_W, BASE_H)
      DrawTexturePro(@texture, middle_src, middle_dst, Vector2.create(0, 0), 0, WHITE)
    else
      # Запасной вариант, если текстура не загружена
      DrawRectangle(x, y, panel_width, BASE_H, Fade(BLACK, 0.8))
      DrawRectangleLines(x, y, panel_width, BASE_H, WHITE)
    end

    tx = x + PADDING_LEFT
    ty = y + PADDING_TOP
    draw_text(name_line, tx, ty, WHITE)

    # Фиксированная ширина подписей «HP» / «MP» для выравнивания палочек
    label_w = [measure_text("HP"), measure_text("MP")].max

    draw_hpmp_bar(x, y, panel_width, tx, ty + LINE_HEIGHT, "HP", hp, max_hp, label_w)
    draw_hpmp_bar(x, y, panel_width, tx, ty + LINE_HEIGHT * 2, "MP", mp, max_mp, label_w)
  end

  private

  def draw_hpmp_bar(panel_x, panel_y, panel_width, base_x, base_y, label, current, maximum, label_w)
    draw_text(label, base_x, base_y, WHITE)

    bar_start_x = base_x + label_w + STICK_GAP
    max_possible_sticks = ((panel_x + panel_width - PADDING_LEFT - bar_start_x) / STICK_W).floor
    sticks = [current, maximum].min
    sticks = [sticks, max_possible_sticks].min

    color = Color.new
    color.r = 194; color.g = 178; color.b = 128; color.a = 255
    stick_y = base_y + (FONT_SIZE - STICK_H) / 2

    sticks.times do |i|
      stick_x = bar_start_x + i * STICK_W
      DrawRectangle(stick_x, stick_y, STICK_W, STICK_H, color)
    end

    number_str = "#{current}/#{maximum}"
    number_w = measure_text(number_str)

    if sticks > 0
      numbers_x = bar_start_x + sticks * STICK_W + STICK_GAP
    else
      numbers_x = bar_start_x
    end

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