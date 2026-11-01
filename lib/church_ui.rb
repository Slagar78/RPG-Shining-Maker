# lib/church_ui.rb
require 'raylib'

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

  def initialize(font, large_font, db, party, start_inventory, game_text = {})
    @font            = font
    @large_font      = large_font || font
    @db              = db
    @party           = party || []
    @start_inventory = start_inventory || []
    @game_text       = game_text || {}

    @visible        = false
    @selected_index = 0
    @sub_state      = :main
    @anim_timer     = 0.0

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
        @sub_state         = :sub
        @panel_anim_y      = PANEL_MSG_HIDDEN_Y
        @text_reveal_index = 0
        @text_reveal_timer = 0
        @text_full_shown   = false
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
    end
  end

  def update(dt = 1.0 / 60.0)
    return unless @visible
    @anim_timer += dt * 60.0
    speed = PANEL_MSG_SPEED * dt * 60.0

    if @sub_state == :sub
      if @panel_anim_y > PANEL_MSG_TARGET_Y
        @panel_anim_y -= speed
        @panel_anim_y = PANEL_MSG_TARGET_Y if @panel_anim_y < PANEL_MSG_TARGET_Y
      elsif !@text_full_shown
        full = message_for_action
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
  end

  def message_for_action
    case @tiles[@selected_index]["name"]
    when "raise" then @game_text["0030"] || "Whom do you want to raise?{N}(Not implemented yet)"
    when "cure"  then @game_text["0031"] || "Whom do you want to cure?{N}(Not implemented yet)"
    when "promo" then @game_text["0032"] || "Whom do you want to promote?{N}(Not implemented yet)"
    when "save"  then @game_text["0033"] || "Save your progress?{N}(Not implemented yet)"
    else "What do you want to do?"
    end
  end

  def draw_message_panel
    return unless @message_panel_tex
    dst = Raylib::Rectangle.create(PANEL_MSG_X, @panel_anim_y, PANEL_MSG_W, PANEL_MSG_H)
    src = Raylib::Rectangle.create(0, 0, @message_panel_tex.width, @message_panel_tex.height)
    Raylib.DrawTexturePro(@message_panel_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

    full    = message_for_action
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

  def unload
    @textures.each do |t|
      Raylib.UnloadTexture(t[:normal]) if t[:normal]
      Raylib.UnloadTexture(t[:anim])   if t[:anim]
    end
    @textures = []
    Raylib.UnloadTexture(@message_panel_tex) if @message_panel_tex
    Raylib.UnloadTexture(@commands_tex)      if @commands_tex
  end
end