# lib/intro/save_slot_menu.rb
require 'raylib'
include Raylib

class SaveSlotMenu
  def initialize(font)
    @font = font
    @frame_tex = LoadTexture("assets/intro/ui_save_slot_frame.png")
    SetTextureFilter(@frame_tex, TEXTURE_FILTER_POINT) if @frame_tex
    @slot_index = 0
  end

  def update(dt)
    if IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)
      @slot_index = (@slot_index + 1) % 4
    elsif IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)
      @slot_index = (@slot_index - 1) % 4
    end

    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_ENTER)
      return :name_input
    elsif IsKeyPressed(KEY_BACKSPACE)
      return :title
    end
    nil
  end

  def draw
    bg_color = Color.new
    bg_color.r = 0
    bg_color.g = 0
    bg_color.b = 0
    bg_color.a = 150
    DrawRectangle(0, 0, 576, 480, bg_color)
    
    (0..3).each do |i|
      x = 370
      y = 20 + i * 74
      
      dst = Rectangle.create(x, y, 192, 64)
      src = Rectangle.create(0, 0, @frame_tex.width, @frame_tex.height)
      
      tint = (i == @slot_index) ? WHITE : GRAY
      
      DrawTexturePro(@frame_tex, src, dst, Vector2.create(0, 0), 0, tint)
      
      # ═══ ЦЕНТРИРОВАНИЕ ТЕКСТА ═══
      text = "Slot #{i + 1} - Empty"
      text_color = (i == @slot_index) ? DARKBLUE : WHITE
      font_size = 18
      
      if @font
        # Измеряем размер текста
        text_size = MeasureTextEx(@font, text, font_size, 1)
        
        # Вычисляем координаты для идеального центра
        text_x = x + (192 - text_size.x) / 2
        text_y = y + (64 - text_size.y) / 2
        
        # ═══ РИСУЕМ ОБВОДКУ (ЧЕРНУЮ) ═══
        outline_color = BLACK
        offset = 1.0 # Сдвиг на 1 пиксель

        # Рисуем текст 8 раз со сдвигами (создаем контур)
        DrawTextEx(@font, text, Vector2.create(text_x - offset, text_y), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x + offset, text_y), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x, text_y - offset), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x, text_y + offset), font_size, 1, outline_color)
        
        # Диагонали (для еще более плотной обводки)
        DrawTextEx(@font, text, Vector2.create(text_x - offset, text_y - offset), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x + offset, text_y - offset), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x - offset, text_y + offset), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x + offset, text_y + offset), font_size, 1, outline_color)

        # ═══ РИСУЕМ ОСНОВНОЙ ТЕКСТ ПОВЕРХ ОБВОДКИ ═══
        DrawTextEx(@font, text, Vector2.create(text_x, text_y), font_size, 1, text_color)
      else
        # Если шрифта нет (на всякий случай), рисуем хоть как-то
        DrawText(text, x + 35, y + 22, font_size, text_color)
      end
    end
  end
  
  def unload
    UnloadTexture(@frame_tex) if @frame_tex
  end
end