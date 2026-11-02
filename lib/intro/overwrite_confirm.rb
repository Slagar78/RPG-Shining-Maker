# lib/intro/overwrite_confirm.rb
require 'raylib'
include Raylib

class OverwriteConfirm
  attr_reader :slot_index

  def initialize(font, large_font, slot_index)
    @font       = font
    @large_font = large_font
    @slot_index = slot_index

    @confirm_index      = 0
    @confirm_anim_timer = 0.0

    @yes_tex = @no_tex = @yes_anim_tex = @no_anim_tex = @yes_no_panel_tex = nil
    load_textures
  end

  def load_textures
    @yes_tex          = LoadTexture("assets/ui/menu/Yes.png")           if File.exist?("assets/ui/menu/Yes.png")
    @no_tex           = LoadTexture("assets/ui/menu/No.png")            if File.exist?("assets/ui/menu/No.png")
    @yes_anim_tex     = LoadTexture("assets/ui/menu/Yes_Anim.png")      if File.exist?("assets/ui/menu/Yes_Anim.png")
    @no_anim_tex      = LoadTexture("assets/ui/menu/No_Anim.png")       if File.exist?("assets/ui/menu/No_Anim.png")
    @yes_no_panel_tex = LoadTexture("assets/ui/yes-no_panel.png")       if File.exist?("assets/ui/yes-no_panel.png")

    [@yes_tex, @no_tex, @yes_anim_tex, @no_anim_tex, @yes_no_panel_tex].each do |t|
      SetTextureFilter(t, TEXTURE_FILTER_POINT) if t
    end
  end

  # Возвращает :yes / :no / nil
  def update(dt)
    @confirm_anim_timer += dt * 60.0

    if IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)
      @confirm_index = 1 - @confirm_index
    elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_ENTER)
      return (@confirm_index == 0) ? :yes : :no
    elsif IsKeyPressed(KEY_S) || IsKeyPressed(KEY_BACKSPACE)
      return :no
    end
    nil
  end

  def draw
    bg = Color.new
    bg.r = 0; bg.g = 0; bg.b = 0; bg.a = 180
    DrawRectangle(0, 0, 576, 480, bg)

    w, h = 360, 160
    x = (576 - w) / 2
    y = (480 - h) / 2

    win = Color.new
    win.r = 20; win.g = 30; win.b = 70; win.a = 240
    DrawRectangle(x, y, w, h, win)
    DrawRectangleLinesEx(Rectangle.create(x, y, w, h), 2, WHITE)

    DrawText("Overwrite this save?", x + 20, y + 20, 18, LIGHTGRAY)
    DrawText("(Slot #{@slot_index})",  x + 20, y + 44, 14, GRAY)

    icon_size    = 48
    spacing      = 36
    panel_w      = 64
    panel_h      = 48
    gap_to_panel = 18

    icons_total_width = (icon_size * 2) + spacing
    start_x = x + (w - icons_total_width) / 2
    icon_y  = y + 80

    # YES
    if @yes_tex
      tex_to_draw = @yes_tex
      if @confirm_index == 0 && @yes_anim_tex
        tex_to_draw = @yes_anim_tex if (@confirm_anim_timer % 24) < 12
      end
      src = Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
      dst = Rectangle.create(start_x, icon_y, icon_size, icon_size)
      color = (@confirm_index == 0) ? WHITE : GRAY
      DrawTexturePro(tex_to_draw, src, dst, Vector2.create(0, 0), 0, color)
    end

    # NO
    no_icon_x = start_x + icon_size + spacing
    if @no_tex
      tex_to_draw = @no_tex
      if @confirm_index == 1 && @no_anim_tex
        tex_to_draw = @no_anim_tex if (@confirm_anim_timer % 24) < 12
      end
      src = Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
      dst = Rectangle.create(no_icon_x, icon_y, icon_size, icon_size)
      color = (@confirm_index == 1) ? WHITE : GRAY
      DrawTexturePro(tex_to_draw, src, dst, Vector2.create(0, 0), 0, color)
    end

    # Панель с текстом Yes / No
    if @yes_no_panel_tex
      panel_x = no_icon_x + icon_size + gap_to_panel
      src = Rectangle.create(0, 0, @yes_no_panel_tex.width, @yes_no_panel_tex.height)
      dst = Rectangle.create(panel_x, icon_y, panel_w, panel_h)
      DrawTexturePro(@yes_no_panel_tex, src, dst, Vector2.create(0, 0), 0, WHITE)

      text = (@confirm_index == 0) ? "Yes" : "No"
      if @font
        fs = 20
        ts = MeasureTextEx(@font, text, fs, 1)
        tx = panel_x + (panel_w - ts.x) / 2
        ty = icon_y  + (panel_h - ts.y) / 2
        DrawTextEx(@font, text, Vector2.create(tx, ty), fs, 1, WHITE)
      end
    end
  end

  def unload
    UnloadTexture(@yes_tex)          if @yes_tex
    UnloadTexture(@no_tex)           if @no_tex
    UnloadTexture(@yes_anim_tex)     if @yes_anim_tex
    UnloadTexture(@no_anim_tex)      if @no_anim_tex
    UnloadTexture(@yes_no_panel_tex) if @yes_no_panel_tex
  end
end