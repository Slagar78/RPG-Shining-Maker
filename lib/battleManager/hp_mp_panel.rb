# lib/battleManager/hp_mp_panel.rb

class HpMpPanel
  BASE_W = 168
  BASE_H = 88
  PADDING_LEFT = 15
  PADDING_TOP = 12
  LINE_HEIGHT = 22
  FONT_SIZE = 20

  def initialize(font = nil)
    @font = font
    @texture = load_texture("assets/ui/HpMpPanel.png")
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

    # Имя и уровень
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

    # HP/MP из общих полей юнита
    hp     = unit[:hp]     || 0
    max_hp = unit[:max_hp] || 0
    mp     = unit[:mp]     || 0
    max_mp = unit[:max_mp] || 0

    name_line = "#{name}  LV #{lvl}"
    hp_line   = "HP  #{hp}/#{max_hp}"
    mp_line   = "MP  #{mp}/#{max_mp}"

    max_text_width = [name_line, hp_line, mp_line].map { |txt| measure_text(txt) }.max
    width = [BASE_W, max_text_width + PADDING_LEFT + 20].max

    x = 576 - width - 8
    y = 8

    if @texture
      src = Rectangle.create(0, 0, @texture.width, @texture.height)
      dst = Rectangle.create(x, y, width, BASE_H)
      DrawTexturePro(@texture, src, dst, Vector2.create(0, 0), 0, WHITE)
    else
      DrawRectangle(x, y, width, BASE_H, Fade(BLACK, 0.8))
      DrawRectangleLines(x, y, width, BASE_H, WHITE)
    end

    tx = x + PADDING_LEFT
    ty = y + PADDING_TOP
    draw_text(name_line, tx, ty, WHITE)
    draw_text(hp_line,   tx, ty + LINE_HEIGHT, WHITE)
    draw_text(mp_line,   tx, ty + LINE_HEIGHT * 2, WHITE)
  end

  private

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