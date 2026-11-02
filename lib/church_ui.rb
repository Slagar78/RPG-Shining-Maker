# lib/church_ui.rb
require 'raylib'
require 'json'

class ChurchMenu
  TILE_SIZE      = 48
  OFFSET         = 48
  ANIM_INTERVAL  = 24

  PANEL_MSG_W        = 480
  PANEL_MSG_H        = 128
  PANEL_MSG_X        = (576 - PANEL_MSG_W) / 2
  PANEL_MSG_TARGET_Y = 480 - PANEL_MSG_H - 24
  PANEL_MSG_HIDDEN_Y = 480
  PANEL_MSG_SPEED    = 38

  TEXT_REVEAL_SPEED = 2

  attr_reader :visible, :selected_index, :sub_state

  # ЧТОБЫ ИЗ ИГРЫ МОЖНО БЫЛО ОБНОВЛЯТЬ ДАННЫЕ
  attr_accessor :current_slot, :current_map_id, :player_name

  # ДОБАВЛЕН ПАРАМЕТР current_slot
  def initialize(font, large_font, db, party, start_inventory, game_text = {}, player = nil, map_id = 0, player_name = "Bowie", current_slot = 1)
    @font            = font
    @large_font      = large_font || font
    @db              = db
    @party           = party || []
    @start_inventory = start_inventory || []
    @game_text       = game_text || {}
    
    @player          = player
    @current_map_id  = map_id
    @player_name     = player_name
    @current_slot    = current_slot # <--- Сохраняем номер слота

    @visible        = false
    @selected_index = 0
    @sub_state      = :main
    @anim_timer     = 0.0
    
    @save_confirm_index = 0
    @confirm_anim_timer = 0.0
    @save_text_stage    = 0 # 0: ask, 1: light, 2: continue, 3: rest

    @tiles = [
      { "id" => 0, "name" => "raise", "icon" => "assets/ui/menu/Raise.png", "icon_anim" => "assets/ui/menu/Raise_Anim.png" },
      { "id" => 1, "name" => "cure",  "icon" => "assets/ui/menu/Cure.png",  "icon_anim" => "assets/ui/menu/Cure_Anim.png"  },
      { "id" => 2, "name" => "promo", "icon" => "assets/ui/menu/Promo.png", "icon_anim" => "assets/ui/menu/Promo_Anim.png" },
      { "id" => 3, "name" => "save",  "icon" => "assets/ui/menu/Save.png",  "icon_anim" => "assets/ui/menu/Save_Anim.png"  }
    ]

    @panel_anim_y       = PANEL_MSG_HIDDEN_Y
    @text_reveal_index  = 0
    @text_reveal_timer  = 0
    @text_full_shown    = false

    @commands_tex = nil
    @commands_w   = 0
    @commands_h   = 0

    load_textures
  end

  def load_textures
    @textures = []
    @tiles.each do |tile|
      normal = File.exist?(tile["icon"])      ? Raylib.LoadTexture(tile["icon"])      : nil
      anim   = File.exist?(tile["icon_anim"]) ? Raylib.LoadTexture(tile["icon_anim"]) : nil
      Raylib.SetTextureFilter(normal, Raylib::TEXTURE_FILTER_POINT) if normal
      Raylib.SetTextureFilter(anim,   Raylib::TEXTURE_FILTER_POINT) if anim
      @textures << { normal: normal, anim: anim }
    end

    @message_panel_tex = nil
    mp = "assets/ui/message_panel.png"
    if File.exist?(mp)
      @message_panel_tex = Raylib.LoadTexture(mp)
      Raylib.SetTextureFilter(@message_panel_tex, Raylib::TEXTURE_FILTER_POINT)
    end

    if File.exist?("assets/ui/Commands.png")
      @commands_tex = Raylib.LoadTexture("assets/ui/Commands.png")
      Raylib.SetTextureFilter(@commands_tex, Raylib::TEXTURE_FILTER_POINT)
      @commands_w = @commands_tex.width
      @commands_h = @commands_tex.height
    end

    @yes_tex = nil; @no_tex = nil; @yes_anim_tex = nil; @no_anim_tex = nil; @yes_no_panel_tex = nil
    @yes_tex = Raylib.LoadTexture("assets/ui/menu/Yes.png") if File.exist?("assets/ui/menu/Yes.png")
    @no_tex = Raylib.LoadTexture("assets/ui/menu/No.png") if File.exist?("assets/ui/menu/No.png")
    @yes_anim_tex = Raylib.LoadTexture("assets/ui/menu/Yes_Anim.png") if File.exist?("assets/ui/menu/Yes_Anim.png")
    @no_anim_tex = Raylib.LoadTexture("assets/ui/menu/No_Anim.png") if File.exist?("assets/ui/menu/No_Anim.png")
    @yes_no_panel_tex = Raylib.LoadTexture("assets/ui/yes-no_panel.png") if File.exist?("assets/ui/yes-no_panel.png")
    
    [@yes_tex, @no_tex, @yes_anim_tex, @no_anim_tex, @yes_no_panel_tex].each do |t|
      Raylib.SetTextureFilter(t, Raylib::TEXTURE_FILTER_POINT) if t
    end
  end

  def open
    @visible            = true
    @selected_index     = 0
    @anim_timer         = 0.0
    @sub_state          = :main
    @panel_anim_y       = PANEL_MSG_HIDDEN_Y
    @text_reveal_index  = 0
    @text_reveal_timer  = 0
    @text_full_shown    = false
    @save_text_stage    = 0
  end

  def close
    @visible            = false
    @sub_state          = :main
    @panel_anim_y       = PANEL_MSG_HIDDEN_Y
    @text_reveal_index  = 0
    @text_reveal_timer  = 0
    @text_full_shown    = false
  end

  def handle_input(dt = 1.0 / 60.0)
    return unless @visible

    case @sub_state
    when :main
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        close
        return
      end
      old_index = @selected_index
      if Raylib.IsKeyPressed(Raylib::KEY_UP)
        @selected_index = 0
      elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
        @selected_index = 1
      elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @selected_index = 2
      elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
        @selected_index = 3
      end
      @anim_timer = 0 if @selected_index != old_index

      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
        if @tiles[@selected_index]["name"] == "save"
          @sub_state = :save_ask
          @save_text_stage = 0
          @text_reveal_index = 0
          @text_full_shown = false
          @panel_anim_y = PANEL_MSG_HIDDEN_Y
          @save_confirm_index = 0
          @confirm_anim_timer = 0.0
        else
          @sub_state         = :sub
          @panel_anim_y      = PANEL_MSG_HIDDEN_Y
          @text_reveal_index = 0
          @text_reveal_timer = 0
          @text_full_shown   = false
        end
      end

    when :sub
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        @sub_state = :main
        return
      end
      if !@text_full_shown && (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D))
        @text_reveal_index = message_for_action.length
        @text_full_shown   = true
        return
      end
      return unless @text_full_shown
      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
        @sub_state = :main
      end

    when :save_ask
      if !@text_full_shown && (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D))
        @text_reveal_index = get_save_text.length
        @text_full_shown = true
        return
      end
      return unless @text_full_shown

      if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @save_confirm_index = 1 - @save_confirm_index
      elsif Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D) || Raylib.IsKeyPressed(Raylib::KEY_ENTER)
        if @save_confirm_index == 0 # YES
          save_game_to_file
          @sub_state = :save_light
          @save_text_stage = 1
          @text_reveal_index = 0
          @text_full_shown = false
        else # NO
          @sub_state = :main
          @panel_anim_y = PANEL_MSG_HIDDEN_Y
        end
      end

    when :save_light
      if !@text_full_shown && (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D))
        @text_reveal_index = get_save_text.length
        @text_full_shown = true
        return
      end
      return unless @text_full_shown

      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D) || Raylib.IsKeyPressed(Raylib::KEY_ENTER)
        @sub_state = :save_continue
        @save_text_stage = 2
        @text_reveal_index = 0
        @text_full_shown = false
        @save_confirm_index = 0
      end

    when :save_continue
      if !@text_full_shown && (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D))
        @text_reveal_index = get_save_text.length
        @text_full_shown = true
        return
      end
      return unless @text_full_shown

      if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @save_confirm_index = 1 - @save_confirm_index
      elsif Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D) || Raylib.IsKeyPressed(Raylib::KEY_ENTER)
        if @save_confirm_index == 0 # YES
          close
        else # NO
          @sub_state = :save_rest
          @save_text_stage = 3
          @text_reveal_index = 0
          @text_full_shown = false
        end
      end

    when :save_rest
      if !@text_full_shown && (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D))
        @text_reveal_index = get_save_text.length
        @text_full_shown = true
        return
      end
      return unless @text_full_shown

      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D) || Raylib.IsKeyPressed(Raylib::KEY_ENTER)
        close
        return :reset_to_title
      end
    end
  end

  def update(dt = 1.0 / 60.0)
    return unless @visible
    @anim_timer += dt * 60.0
    @confirm_anim_timer += dt * 60.0
    speed = PANEL_MSG_SPEED * dt * 60.0

    is_save_state = [:save_ask, :save_light, :save_continue, :save_rest].include?(@sub_state)

    if @sub_state == :sub || is_save_state
      if @panel_anim_y > PANEL_MSG_TARGET_Y
        @panel_anim_y -= speed
        @panel_anim_y = PANEL_MSG_TARGET_Y if @panel_anim_y < PANEL_MSG_TARGET_Y
      elsif !@text_full_shown
        full = is_save_state ? get_save_text : message_for_action
        @text_reveal_timer += dt * 60.0
        while @text_reveal_timer >= TEXT_REVEAL_SPEED && @text_reveal_index < full.length
          @text_reveal_timer -= TEXT_REVEAL_SPEED
          @text_reveal_index += 1
          while full[@text_reveal_index, 3] == "{N}"
            @text_reveal_index += 3
          end
        end
        @text_full_shown = true if @text_reveal_index >= full.length
      end
    else
      if @panel_anim_y < PANEL_MSG_HIDDEN_Y
        @panel_anim_y += speed
        @panel_anim_y = PANEL_MSG_HIDDEN_Y if @panel_anim_y > PANEL_MSG_HIDDEN_Y
      end
    end
  end

  def draw
    return unless @visible
    draw_message_panel if @panel_anim_y < PANEL_MSG_HIDDEN_Y
    draw_tiles if @sub_state == :main && @panel_anim_y >= PANEL_MSG_HIDDEN_Y
    
    if [:save_ask, :save_continue].include?(@sub_state) && @text_full_shown
      draw_yes_no_icons
    end
  end

  def get_save_text
    case @save_text_stage
    when 0 then @game_text["0022"] || "May I record your adventure{N}now?"
    when 1 then @game_text["0023"] || "The light allows you to{N}resume your adventure!"
    when 2 then @game_text["0024"] || "Will you continue your{N}adventure?"
    when 3 then @game_text["0025"] || "Then, take a rest before{N}you continue."
    end
  end

  def message_for_action
    case @tiles[@selected_index]["name"]
    when "raise" then @game_text["0030"] || "Whom do you want to raise?{N}(Not implemented yet)"
    when "cure"  then @game_text["0031"] || "Whom do you want to cure?{N}(Not implemented yet)"
    when "promo" then @game_text["0032"] || "Whom do you want to promote?{N}(Not implemented yet)"
    else "What do you want to do?"
    end
  end

  def draw_message_panel
    return unless @message_panel_tex
    dst = Raylib::Rectangle.create(PANEL_MSG_X, @panel_anim_y, PANEL_MSG_W, PANEL_MSG_H)
    src = Raylib::Rectangle.create(0, 0, @message_panel_tex.width, @message_panel_tex.height)
    Raylib.DrawTexturePro(@message_panel_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

    is_save_state = [:save_ask, :save_light, :save_continue, :save_rest].include?(@sub_state)
    full = is_save_state ? get_save_text : message_for_action
    display = full[0, @text_reveal_index] || ""
    return if display.empty?

    if @large_font
      y_offset = @panel_anim_y + 10
      display.split("{N}", -1).each do |line|
        Raylib.DrawTextEx(@large_font, line, Raylib::Vector2.create(PANEL_MSG_X + 20, y_offset), 30, 1, Raylib::WHITE)
        y_offset += 38
      end
    end
  end

  def draw_yes_no_icons
    return unless @yes_tex && @no_tex
    icon_size = 48
    spacing = 36
    panel_w = 64
    panel_h = 48
    gap_to_panel = 18
    
    icons_total_width = (icon_size * 2) + spacing
    start_x = PANEL_MSG_X + (PANEL_MSG_W - icons_total_width) / 2
    icon_y = @panel_anim_y - icon_size + 10

    tex_to_draw = @yes_tex
    if @save_confirm_index == 0 && @yes_anim_tex
      tex_to_draw = @yes_anim_tex if (@confirm_anim_timer % 24) < 12
    end
    src = Raylib::Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
    dst = Raylib::Rectangle.create(start_x, icon_y, icon_size, icon_size)
    color = (@save_confirm_index == 0) ? Raylib::WHITE : Raylib::GRAY
    Raylib.DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0,0), 0, color)

    no_icon_x = start_x + icon_size + spacing
    tex_to_draw = @no_tex
    if @save_confirm_index == 1 && @no_anim_tex
      tex_to_draw = @no_anim_tex if (@confirm_anim_timer % 24) < 12
    end
    src = Raylib::Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
    dst = Raylib::Rectangle.create(no_icon_x, icon_y, icon_size, icon_size)
    color = (@save_confirm_index == 1) ? Raylib::WHITE : Raylib::GRAY
    Raylib.DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0,0), 0, color)

    if @yes_no_panel_tex
      panel_x = no_icon_x + icon_size + gap_to_panel
      src = Raylib::Rectangle.create(0, 0, @yes_no_panel_tex.width, @yes_no_panel_tex.height)
      dst = Raylib::Rectangle.create(panel_x, icon_y, panel_w, panel_h)
      Raylib.DrawTexturePro(@yes_no_panel_tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

      text = (@save_confirm_index == 0) ? "Yes" : "No"
      if @font
        font_size = 20
        text_size = Raylib.MeasureTextEx(@font, text, font_size, 1)
        text_x = panel_x + (panel_w - text_size.x) / 2
        text_y = icon_y + (panel_h - text_size.y) / 2
        Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(text_x, text_y), font_size, 1, Raylib::WHITE)
      end
    end
  end

  def draw_tiles
    center_x = 576 / 2
    center_y = 480 - 80
    positions = [
      { x: center_x,          y: center_y - OFFSET + 24 },
      { x: center_x - OFFSET, y: center_y },
      { x: center_x + OFFSET, y: center_y },
      { x: center_x,          y: center_y + OFFSET - 24 }
    ]

    (0..3).each do |i|
      tex = @textures[i]
      pos = positions[i]
      texture = nil
      if tex
        if i == @selected_index
          use_anim = (@anim_timer % ANIM_INTERVAL) < (ANIM_INTERVAL / 2) && tex[:anim]
          texture  = use_anim ? tex[:anim] : tex[:normal]
        else
          texture = tex[:normal]
        end
      end
      dst = Raylib::Rectangle.create(pos[:x] - TILE_SIZE / 2, pos[:y] - TILE_SIZE / 2, TILE_SIZE, TILE_SIZE)
      src = Raylib::Rectangle.create(0, 0, TILE_SIZE, TILE_SIZE)
      if texture
        Raylib.DrawTexturePro(texture, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      else
        Raylib.DrawRectangle(pos[:x] - TILE_SIZE / 2, pos[:y] - TILE_SIZE / 2, TILE_SIZE, TILE_SIZE, Raylib::GRAY)
        Raylib.DrawRectangleLines(pos[:x] - TILE_SIZE / 2, pos[:y] - TILE_SIZE / 2, TILE_SIZE, TILE_SIZE, Raylib::DARKGRAY)
      end
    end

    if @commands_tex
      panel_x = center_x + OFFSET + TILE_SIZE / 2 + 16
      panel_y = center_y - @commands_h / 2 + 24
      src = Raylib::Rectangle.create(0, 0, @commands_w, @commands_h)
      dst = Raylib::Rectangle.create(panel_x, panel_y, @commands_w, @commands_h)
      Raylib.DrawTexturePro(@commands_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

      if @large_font
        name = @tiles[@selected_index]["name"].capitalize
        fs   = 30
        sz   = Raylib.MeasureTextEx(@large_font, name, fs, 1)
        Raylib.DrawTextEx(@large_font, name,
          Raylib::Vector2.create(panel_x + 20, panel_y + (@commands_h - sz.y) / 2),
          fs, 1, Raylib::WHITE)
      end
    end
  end

  def save_game_to_file
    p_x = @player ? @player.x : 12
    p_y = @player ? @player.y : 14
    
    save_data = {
      "player_name" => @player_name, 
      "gold" => @db&.globals ? (@db.globals["gold"] || 0) : 0,
      "map_id" => @current_map_id, 
      "x" => p_x,
      "y" => p_y,
      "start_inventory" => @start_inventory
    }
    
    Dir.mkdir("data/save") unless Dir.exist?("data/save")
    # СОХРАНЯЕМ В ВЫБРАННЫЙ СЛОТ
    File.write("data/save/save_#{@current_slot}.json", JSON.pretty_generate(save_data))
    puts "✅ Игра успешно сохранена в data/save/save_#{@current_slot}.json!"
  end

  def unload
    @textures.each do |t|
      Raylib.UnloadTexture(t[:normal]) if t[:normal]
      Raylib.UnloadTexture(t[:anim])   if t[:anim]
    end
    @textures = []
    Raylib.UnloadTexture(@message_panel_tex) if @message_panel_tex
    Raylib.UnloadTexture(@commands_tex)      if @commands_tex
    Raylib.UnloadTexture(@yes_tex) if @yes_tex
    Raylib.UnloadTexture(@no_tex) if @no_tex
    Raylib.UnloadTexture(@yes_anim_tex) if @yes_anim_tex
    Raylib.UnloadTexture(@no_anim_tex) if @no_anim_tex
    Raylib.UnloadTexture(@yes_no_panel_tex) if @yes_no_panel_tex
  end
end