# lib/battleManager/battle_drop.rb
module BattleDrop
  attr_accessor :drop_confirm_index, :drop_confirm_item, :drop_confirm_anim_timer,
                :drop_yes_tex, :drop_no_tex, :drop_yes_anim_tex, :drop_no_anim_tex,
                :drop_msg_full_lines, :drop_msg_char_index, :drop_msg_char_timer,
                :drop_msg_char_speed, :drop_msg_finished

  def init_drop_vars
    @drop_confirm_index = 0
    @drop_confirm_item = nil
    @drop_confirm_anim_timer = 0

    @drop_yes_tex = nil
    @drop_no_tex = nil
    if File.exist?("assets/ui/menu/Yes.png")
      @drop_yes_tex = Raylib.LoadTexture("assets/ui/menu/Yes.png")
      Raylib.SetTextureFilter(@drop_yes_tex, Raylib::TEXTURE_FILTER_POINT)
    end
    if File.exist?("assets/ui/menu/No.png")
      @drop_no_tex = Raylib.LoadTexture("assets/ui/menu/No.png")
      Raylib.SetTextureFilter(@drop_no_tex, Raylib::TEXTURE_FILTER_POINT)
    end

    @drop_yes_anim_tex = nil
    @drop_no_anim_tex = nil
    if File.exist?("assets/ui/menu/Yes_Anim.png")
      @drop_yes_anim_tex = Raylib.LoadTexture("assets/ui/menu/Yes_Anim.png")
      Raylib.SetTextureFilter(@drop_yes_anim_tex, Raylib::TEXTURE_FILTER_POINT)
    end
    if File.exist?("assets/ui/menu/No_Anim.png")
      @drop_no_anim_tex = Raylib.LoadTexture("assets/ui/menu/No_Anim.png")
      Raylib.SetTextureFilter(@drop_no_anim_tex, Raylib::TEXTURE_FILTER_POINT)
    end

    @drop_msg_full_lines = []
    @drop_msg_char_index = 0
    @drop_msg_char_timer = 0
    @drop_msg_char_speed = 3
    @drop_msg_finished = false
  end

  def start_drop_confirm(item)
    @drop_confirm_item = item
    @drop_confirm_index = 0
    @drop_confirm_anim_timer = 0
    @battle_state = :drop_confirm
    @highlight_tiles = []
    @target_highlight = nil
  end

  def handle_drop_confirm_input
    if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
      @drop_confirm_index = 1 - @drop_confirm_index
    elsif Raylib.IsKeyPressed(Raylib::KEY_S)
      @drop_confirm_item = nil
      @battle_menu.open_item_menu
      @battle_state = :item_select
      @audio.play_sfx("cancel_menu") if @audio
    elsif Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
      if @drop_confirm_index == 0   # Yes
        perform_drop
      else                          # No
        @drop_confirm_item = nil
        @battle_menu.open_item_menu
        @battle_state = :item_select
        @audio.play_sfx("cancel_menu") if @audio
      end
    end
  end

  def update_drop_confirm
    @battle_player&.update_animation
    @drop_confirm_anim_timer += 1
  end

def perform_drop
  return unless @drop_confirm_item && @current_unit && @current_unit[:actor]
  actor = @current_unit[:actor]
  entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
  return unless entry

  items = entry["items"]
  idx = items.index { |e| e["item"] == @drop_confirm_item["item"] }
  if idx
    # Снимаем экипировку, если была
    if items[idx]["equipped"]
      items[idx]["equipped"] = false
    end
    # Заменяем предмет на пустой слот (без сдвига)
    items[idx] = { "item" => "NOTHING", "equipped" => false }
  end

  start_drop_result
end

  def start_drop_result
    template = @game_text["0004"] || "The {ITEM} {N}is discarded."
    text = template.gsub("{ITEM}", @drop_confirm_item["item"])
    @drop_msg_full_lines = text.split('{N}')
    @drop_msg_char_index = 0
    @drop_msg_char_timer = 0
    @drop_msg_char_speed = 3
    @drop_msg_finished = false
    @battle_state = :drop_message
    @drop_confirm_item = nil
  end

  def update_drop_message
    return if @drop_msg_finished
    @drop_msg_char_timer += 1
    if @drop_msg_char_timer >= @drop_msg_char_speed
      @drop_msg_char_timer = 0
      total_chars = @drop_msg_full_lines.sum(&:length)
      @drop_msg_char_index += 1
      if @drop_msg_char_index >= total_chars
        @drop_msg_char_index = total_chars
        @drop_msg_finished = true
      end
    end
  end

  def handle_drop_message_input
    if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
      if @drop_msg_finished
        end_current_turn
      else
        total_chars = @drop_msg_full_lines.sum(&:length)
        @drop_msg_char_index = total_chars
        @drop_msg_finished = true
      end
    end
  end

  def draw_drop_confirm
    template = @game_text["0003"] || "The {ITEM} {N}will be discarded. OK ?"
    text = template.gsub("{ITEM}", @drop_confirm_item ? @drop_confirm_item["item"] : "???")
    lines = text.split('{N}')

    panel_w = 480
    panel_h = 128
    panel_x = (576 - panel_w) / 2
    panel_y = 480 - panel_h - 24

    if @message_panel_tex
      dst = Raylib::Rectangle.create(panel_x, panel_y, panel_w, panel_h)
      src = Raylib::Rectangle.create(0, 0, @message_panel_tex.width, @message_panel_tex.height)
      Raylib.DrawTexturePro(@message_panel_tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
    else
      Raylib.DrawRectangle(panel_x, panel_y, panel_w, panel_h, Raylib::GRAY)
      Raylib.DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, Raylib::DARKGRAY)
    end

    y_offset = panel_y + 10
    font = @battle_scene.message_font || @font
    lines.each do |line_text|
      Raylib.DrawTextEx(font, line_text, Raylib::Vector2.create(panel_x + 20, y_offset), 30, 1, Raylib::WHITE)
      y_offset += 38
    end

    icon_size = 48
    spacing = 20
    total_width = icon_size * 2 + spacing
    start_x = panel_x + (panel_w - total_width) / 2
    icon_y = panel_y - icon_size - 10

    if @drop_yes_tex
      tex_to_draw = @drop_yes_tex
      if @drop_confirm_index == 0 && @drop_yes_anim_tex
        if (@drop_confirm_anim_timer % 24) < 12
          tex_to_draw = @drop_yes_anim_tex
        end
      end
      src = Raylib::Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
      dst = Raylib::Rectangle.create(start_x, icon_y, icon_size, icon_size)
      color = (@drop_confirm_index == 0) ? Raylib::WHITE : Raylib::GRAY
      Raylib.DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0,0), 0, color)
    end

    if @drop_no_tex
      tex_to_draw = @drop_no_tex
      if @drop_confirm_index == 1 && @drop_no_anim_tex
        if (@drop_confirm_anim_timer % 24) < 12
          tex_to_draw = @drop_no_anim_tex
        end
      end
      src = Raylib::Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
      dst = Raylib::Rectangle.create(start_x + icon_size + spacing, icon_y, icon_size, icon_size)
      color = (@drop_confirm_index == 1) ? Raylib::WHITE : Raylib::GRAY
      Raylib.DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0,0), 0, color)
    end
  end

  def draw_drop_message
    panel_w = 480
    panel_h = 128
    panel_x = (576 - panel_w) / 2
    panel_y = 480 - panel_h - 24

    if @message_panel_tex
      dst = Raylib::Rectangle.create(panel_x, panel_y, panel_w, panel_h)
      src = Raylib::Rectangle.create(0, 0, @message_panel_tex.width, @message_panel_tex.height)
      Raylib.DrawTexturePro(@message_panel_tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
    else
      Raylib.DrawRectangle(panel_x, panel_y, panel_w, panel_h, Raylib::GRAY)
      Raylib.DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, Raylib::DARKGRAY)
    end

    remaining = @drop_msg_char_index
    y_offset = panel_y + 10
    font = @battle_scene.message_font || @font

    @drop_msg_full_lines.each do |line|
      if remaining > 0
        slice = line[0, remaining]
        Raylib.DrawTextEx(font, slice, Raylib::Vector2.create(panel_x + 20, y_offset), 30, 1, Raylib::WHITE)
        remaining -= line.length
        y_offset += 38
      else
        break
      end
    end
  end
end