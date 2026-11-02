# lib/intro/name_input.rb
require 'raylib'
include Raylib

class NameInput
  attr_reader :name

  def initialize(font, large_font)
    @font = font
    @large_font = large_font
    @name = ""
    @max_length = 20
    
    @state = :typing # :typing -> :ok_selected -> :confirm
    @confirm_index = 0
    @confirm_anim_timer = 0.0
    
    @yes_tex = nil
    @no_tex = nil
    @yes_anim_tex = nil
    @no_anim_tex = nil
    @yes_no_panel_tex = nil

    load_textures
  end

  def load_textures
    if File.exist?("assets/ui/menu/Yes.png")
      @yes_tex = LoadTexture("assets/ui/menu/Yes.png")
      SetTextureFilter(@yes_tex, TEXTURE_FILTER_POINT)
    end
    if File.exist?("assets/ui/menu/No.png")
      @no_tex = LoadTexture("assets/ui/menu/No.png")
      SetTextureFilter(@no_tex, TEXTURE_FILTER_POINT)
    end
    if File.exist?("assets/ui/menu/Yes_Anim.png")
      @yes_anim_tex = LoadTexture("assets/ui/menu/Yes_Anim.png")
      SetTextureFilter(@yes_anim_tex, TEXTURE_FILTER_POINT)
    end
    if File.exist?("assets/ui/menu/No_Anim.png")
      @no_anim_tex = LoadTexture("assets/ui/menu/No_Anim.png")
      SetTextureFilter(@no_anim_tex, TEXTURE_FILTER_POINT)
    end
    if File.exist?("assets/ui/yes-no_panel.png")
      @yes_no_panel_tex = LoadTexture("assets/ui/yes-no_panel.png")
      SetTextureFilter(@yes_no_panel_tex, TEXTURE_FILTER_POINT)
    end
  end

  def update(dt)
    if @state == :typing
      # 1. Печатаем буквы (включая A, D, S)
      while (char = GetCharPressed()) > 0
        if @name.length < @max_length && char >= 32 && char <= 125 # Только печатные символы
          @name += char.chr(Encoding::UTF_8) rescue @name += char.chr
        end
      end

      # 2. Удаляем символ
      if IsKeyPressed(KEY_BACKSPACE) && @name.length > 0
        @name = @name[0...-1]
      end

      # 3. Переход к кнопке OK (по Enter, Tab или Стрелке Вниз)
      if IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_DOWN)
        @name = "Mushra" if @name.strip.empty?  # <--- ИСПРАВЛЕНО ТУТ
        @state = :ok_selected
      end
      
    elsif @state == :ok_selected
      # На кнопке OK нажимаем A, D или Enter для перехода к подтверждению
      if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_ENTER)
        @state = :confirm
        @confirm_index = 0
        @confirm_anim_timer = 0.0
      # Или возвращаемся к печати по S / Вверх
      elsif IsKeyPressed(KEY_S) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_BACKSPACE)
        @state = :typing
      end
      
    elsif @state == :confirm
      @confirm_anim_timer += dt * 60.0
      
      # Переключение Yes/No
      if IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)
        @confirm_index = 1 - @confirm_index
      elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_ENTER)
        if @confirm_index == 0
          return :gameplay
        else
          @state = :ok_selected # Если выбрали NO, возвращаемся к кнопке OK
        end
      elsif IsKeyPressed(KEY_S) || IsKeyPressed(KEY_BACKSPACE)
        @state = :ok_selected
      end
    end
    nil
  end

  def draw
    bg_color = Color.new
    bg_color.r = 0
    bg_color.g = 0
    bg_color.b = 0
    bg_color.a = 180
    DrawRectangle(0, 0, 576, 480, bg_color)
    
    w, h = 350, 180 # Увеличили высоту окна, чтобы влезла кнопка OK
    x = (576 - w) / 2
    y = (480 - h) / 2
    
    win_color = Color.new
    win_color.r = 20
    win_color.g = 30
    win_color.b = 70
    win_color.a = 240
    DrawRectangle(x, y, w, h, win_color)
    DrawRectangleLinesEx(Rectangle.create(x, y, w, h), 2, WHITE)
    
    if @state == :typing || @state == :ok_selected
      DrawText("Enter your name:", x + 20, y + 20, 18, LIGHTGRAY)
      
      # Поле ввода
      DrawRectangle(x + 20, y + 50, w - 40, 35, BLACK)
      DrawRectangleLinesEx(Rectangle.create(x + 20, y + 50, w - 40, 35), 1, GRAY)
      
      # Курсор мигает только когда мы печатаем
      cursor = (@state == :typing && (GetTime() * 2).to_i % 2 == 0) ? "_" : ""
      DrawText("#{@name}#{cursor}", x + 30, y + 58, 22, WHITE)
      
      # Отрисовка кнопки OK
      ok_w, ok_h = 80, 30
      ok_x = x + (w - ok_w) / 2
      ok_y = y + 100
      
      ok_rect = Rectangle.create(ok_x, ok_y, ok_w, ok_h)
      
      if @state == :ok_selected
        # Подсветка выбранной кнопки OK
        highlight_color = Color.new
        highlight_color.r = 60
        highlight_color.g = 100
        highlight_color.b = 200
        highlight_color.a = 255
        DrawRectangleRec(ok_rect, highlight_color)
      end
      
      DrawRectangleLinesEx(ok_rect, 2, WHITE)
      
      # Текст "OK" по центру
      if @large_font
        text_size = MeasureTextEx(@large_font, "OK", 20, 1)
        text_x = ok_x + (ok_w - text_size.x) / 2
        text_y = ok_y + (ok_h - text_size.y) / 2
        DrawTextEx(@large_font, "OK", Vector2.create(text_x, text_y), 20, 1, WHITE)
      end
      
      # Подсказки внизу
      if @state == :typing
        DrawText("Press Enter to select [OK]", x + 20, y + 140, 14, GREEN)
      else
        DrawText("Press A/D to confirm", x + 20, y + 140, 14, GREEN)
        DrawText("Press S to type", x + 20, y + 160, 14, GRAY)
      end
      
    elsif @state == :confirm
      DrawText("Is this name okay?", x + 20, y + 20, 18, LIGHTGRAY)
      
      if @large_font
        name_size = MeasureTextEx(@large_font, @name, 30, 1)
        name_x = x + (w - name_size.x) / 2
        DrawTextEx(@large_font, @name, Vector2.create(name_x, y + 45), 30, 1, YELLOW)
      end

      # ═══ РИСУЕМ YES / NO ═══
      icon_size = 48
      spacing = 36
      panel_w = 64
      panel_h = 48
      gap_to_panel = 18
      
      icons_total_width = (icon_size * 2) + spacing
      start_x = x + (w - icons_total_width) / 2
      icon_y = y + 90

      # 1. YES
      if @yes_tex
        tex_to_draw = @yes_tex
        if @confirm_index == 0 && @yes_anim_tex
          tex_to_draw = @yes_anim_tex if (@confirm_anim_timer % 24) < 12
        end
        src = Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
        dst = Rectangle.create(start_x, icon_y, icon_size, icon_size)
        color = (@confirm_index == 0) ? WHITE : GRAY
        DrawTexturePro(tex_to_draw, src, dst, Vector2.create(0,0), 0, color)
      end

      # 2. NO
      no_icon_x = start_x + icon_size + spacing
      if @no_tex
        tex_to_draw = @no_tex
        if @confirm_index == 1 && @no_anim_tex
          tex_to_draw = @no_anim_tex if (@confirm_anim_timer % 24) < 12
        end
        src = Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
        dst = Rectangle.create(no_icon_x, icon_y, icon_size, icon_size)
        color = (@confirm_index == 1) ? WHITE : GRAY
        DrawTexturePro(tex_to_draw, src, dst, Vector2.create(0,0), 0, color)
      end

      # 3. Панель с текстом
      if @yes_no_panel_tex
        panel_x = no_icon_x + icon_size + gap_to_panel
        src = Rectangle.create(0, 0, @yes_no_panel_tex.width, @yes_no_panel_tex.height)
        dst = Rectangle.create(panel_x, icon_y, panel_w, panel_h)
        DrawTexturePro(@yes_no_panel_tex, src, dst, Vector2.create(0,0), 0, WHITE)

        text = (@confirm_index == 0) ? "Yes" : "No"
        if @font
          font_size = 20
          text_size = MeasureTextEx(@font, text, font_size, 1)
          text_x = panel_x + (panel_w - text_size.x) / 2
          text_y = icon_y + (panel_h - text_size.y) / 2
          DrawTextEx(@font, text, Vector2.create(text_x, text_y), font_size, 1, WHITE)
        end
      end
    end
  end

  def unload
    UnloadTexture(@yes_tex) if @yes_tex
    UnloadTexture(@no_tex) if @no_tex
    UnloadTexture(@yes_anim_tex) if @yes_anim_tex
    UnloadTexture(@no_anim_tex) if @no_anim_tex
    UnloadTexture(@yes_no_panel_tex) if @yes_no_panel_tex
  end
end