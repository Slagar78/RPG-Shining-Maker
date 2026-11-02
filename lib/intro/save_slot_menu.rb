# lib/intro/save_slot_menu.rb
require 'raylib'
require 'json'
include Raylib

class SaveSlotMenu
  def initialize(font, mode = :new_game)
    @font = font
    @mode = mode # :new_game или :load_game
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
      if @mode == :new_game
        return :name_input
      else
        # Проверяем, существует ли файл сохранения
        if File.exist?("data/save/save_#{@slot_index + 1}.json")
          return { action: :load, slot: @slot_index }
        else
          # Если файла нет, звучит ошибка и ничего не происходит
          $audio.play_sfx("error") if $audio
          return nil
        end
      end
    elsif IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_S)
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
      
      # ═══ ЧИТАЕМ ФАЙЛ, ЧТОБЫ ПОКАЗАТЬ ИМЯ ═══
      file_path = "data/save/save_#{i + 1}.json"
      text = "Slot #{i + 1} - Empty"
      
      if File.exist?(file_path)
        begin
          save_data = JSON.parse(File.read(file_path))
          p_name = save_data["player_name"] || "Unknown"
          text = "#{p_name} - Lv 1" # Пока уровень по умолчанию 1
        rescue
          text = "Slot #{i + 1} - Corrupted"
        end
      end

      text_color = (i == @slot_index) ? WHITE : LIGHTGRAY
      font_size = 18
      
      if @font
        text_size = MeasureTextEx(@font, text, font_size, 1)
        text_x = x + (192 - text_size.x) / 2
        text_y = y + (64 - text_size.y) / 2
        
        # Обводка текста
        outline_color = BLACK
        offset = 1.0
        DrawTextEx(@font, text, Vector2.create(text_x - offset, text_y), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x + offset, text_y), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x, text_y - offset), font_size, 1, outline_color)
        DrawTextEx(@font, text, Vector2.create(text_x, text_y + offset), font_size, 1, outline_color)
        
        DrawTextEx(@font, text, Vector2.create(text_x, text_y), font_size, 1, text_color)
      else
        DrawText(text, x + 35, y + 22, font_size, text_color)
      end
    end
  end
  
  def unload
    UnloadTexture(@frame_tex) if @frame_tex
  end
end