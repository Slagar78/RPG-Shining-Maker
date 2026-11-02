# lib/intro/title_screen.rb
require 'raylib'
include Raylib

class TitleScreen
  def initialize
    @bg_tex = LoadTexture("assets/intro/title_bg.png")
    SetTextureFilter(@bg_tex, TEXTURE_FILTER_POINT) if @bg_tex
    
    @menu_index = 0
    @options = ["New Game", "Continue"]
    
    @win_x = 320
    @win_y = 340
    @win_w = 220
    @win_h = 90
    
    @timer = 0.0
  end

  def update(dt)
    @timer += dt

    if IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)
      @menu_index = (@menu_index + 1) % @options.size
    elsif IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)
      @menu_index = (@menu_index - 1) % @options.size
    end

    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_ENTER)
      if @menu_index == 0
        return :save_slot
      else
        return :continue 
      end
    end
    nil
  end

  def draw
    DrawTexture(@bg_tex, 0, 0, WHITE) if @bg_tex

    draw_mz_window(@win_x, @win_y, @win_w, @win_h)

    @options.each_with_index do |opt, i|
      y = @win_y + 15 + i * 30
      
      # ИСПРАВЛЕНО: создаем цвет через присваивание
      if i == @menu_index
        highlight_color = Color.new
        highlight_color.r = 60
        highlight_color.g = 100
        highlight_color.b = 200
        highlight_color.a = 150
        DrawRectangle(@win_x + 8, y - 2, @win_w - 16, 26, highlight_color)
      end
      
      DrawText(opt, @win_x + 20, y, 20, WHITE)
    end
  end

  def draw_mz_window(x, y, w, h)
    bg_color = Color.new
    bg_color.r = 20
    bg_color.g = 30
    bg_color.b = 70
    bg_color.a = 220
    DrawRectangle(x, y, w, h, bg_color)
    
    outer_line = Color.new
    outer_line.r = 200
    outer_line.g = 200
    outer_line.b = 220
    outer_line.a = 255
    DrawRectangleLinesEx(Rectangle.create(x, y, w, h), 2, outer_line)
    
    inner_line = Color.new
    inner_line.r = 100
    inner_line.g = 150
    inner_line.b = 255
    inner_line.a = 150
    DrawRectangleLinesEx(Rectangle.create(x+4, y+4, w-8, h-8), 1, inner_line)
  end
  
  def unload
    UnloadTexture(@bg_tex) if @bg_tex
  end
end