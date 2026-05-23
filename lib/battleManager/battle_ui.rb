# lib/battleManager/battle_ui.rb

class BattleMenu
  attr_reader :selected_index
  
  attr_reader :magic_mode         # позволяет узнать, включён ли режим магии
  attr_reader :selected_spell     # выбранное заклинание (хеш)

  def initialize
    @tiles = [
      { "id" => 0, "name" => "attack", "icon" => "assets/ui/menu/Attack.png", "icon_anim" => "assets/ui/menu/Attack_Anim.png" },
      { "id" => 1, "name" => "magic",  "icon" => "assets/ui/menu/Menu2.png",  "icon_anim" => "assets/ui/menu/Menu2_Anim.png" },
      { "id" => 2, "name" => "item",   "icon" => "assets/ui/menu/Menu3.png",  "icon_anim" => "assets/ui/menu/Menu3_Anim.png" },
      { "id" => 3, "name" => "stay",   "icon" => "assets/ui/menu/Stay.png",   "icon_anim" => "assets/ui/menu/Stay_Anim.png" }
    ]

    @visible = false
    @selected_index = 0
    @anim_timer = 0
    @tile_size = 48
    @offset = 48
    @font = nil   # будет установлен из BattleManager
	
	@magic_mode = false
    @spells = []
    @magic_selected = 0
    @empty_magic_tex = nil
    @magic_icon_cache = {}

    load_textures
    load_commands_panel
  end

  # Сеттер для шрифта
  def font=(font)
    @font = font
  end

  def load_textures
    @textures = []
    @tiles.each do |tile|
      normal = Raylib.LoadTexture(tile["icon"])
      anim   = Raylib.LoadTexture(tile["icon_anim"])
      Raylib.SetTextureFilter(normal, 0)
      Raylib.SetTextureFilter(anim, 0)
      @textures << { normal: normal, anim: anim }
    end
  end

  def load_commands_panel
    path = "assets/ui/Commands.png"
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      @commands_tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(@commands_tex, Raylib::TEXTURE_FILTER_POINT)
      @commands_w = @commands_tex.width
      @commands_h = @commands_tex.height
    else
      puts "WARNING: #{path} not found."
      @commands_tex = nil
    end
  end

  def open(can_attack = false)
    @visible = true
    @selected_index = can_attack ? 0 : 3   # 0 = Attack, 3 = Stay
    @anim_timer = 0
  end

  def close
    @visible = false
  end

  # ---------- МАГИЧЕСКИЙ РЕЖИМ ----------
  def open_magic(spells)
    @magic_mode = true
    @spells = spells.first(4)
    @magic_selected = 0
    @visible = true
  end

  def close_magic
    @magic_mode = false
    @spells = []
    @magic_selected = 0
  end

  def selected_spell
    @spells[@magic_selected]
  end

  def draw_magic_icons
    cx = 576 / 2
    cy = 480 / 2

    positions = [
      { x: cx,        y: cy - 42 },
      { x: cx - 44,   y: cy },
      { x: cx + 44,   y: cy },
      { x: cx,        y: cy + 42 }
    ]

    unless @empty_magic_tex
      path = "assets/spells/magic_empty.png"
      if File.exist?(path)
        @empty_magic_tex = Raylib.LoadTexture(path)
        Raylib.SetTextureFilter(@empty_magic_tex, Raylib::TEXTURE_FILTER_POINT)
      end
    end

    4.times do |i|
      spell = @spells[i]
      pos = positions[i]

      if spell
        icon = load_magic_icon(spell["icon"])
      else
        icon = @empty_magic_tex
      end

      if icon
        src = Raylib::Rectangle.create(0, 0, 32, 48)
        dst = Raylib::Rectangle.create(pos[:x] - 16, pos[:y] - 24, 32, 48)
        Raylib.DrawTexturePro(icon, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      else
        Raylib.DrawRectangle(pos[:x] - 16, pos[:y] - 24, 32, 48, Raylib::GRAY)
      end

      if i == @magic_selected
        Raylib.DrawRectangleLines(pos[:x] - 18, pos[:y] - 26, 36, 52, Raylib::YELLOW)
      end
    end
  end

  def load_magic_icon(path)
    return nil unless path && !path.empty?
    @magic_icon_cache[path] ||= begin
      if File.exist?(path)
        img = Raylib.LoadImage(path)
        tex = Raylib.LoadTextureFromImage(img)
        Raylib.UnloadImage(img)
        Raylib.SetTextureFilter(tex, Raylib::TEXTURE_FILTER_POINT)
        tex
      end
    end
  end

  def handle_input
    return unless @visible

    if @magic_mode
      if Raylib.IsKeyPressed(Raylib::KEY_UP)
        @magic_selected = 0
      elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
        @magic_selected = 1
      elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @magic_selected = 2
      elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
        @magic_selected = 3
      end
      @magic_selected = @magic_selected.clamp(0, @spells.size - 1)
      return
    end

    if Raylib.IsKeyPressed(Raylib::KEY_UP)
      @selected_index = 0
    elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
      @selected_index = 1
    elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
      @selected_index = 2
    elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
      @selected_index = 3
    end
  end

  def update
    return unless @visible
    @anim_timer += 1
  end

    def draw
    return unless @visible

    if @magic_mode
      draw_magic_icons
      return
    end

    center_x = 576 / 2
    center_y = 480 - 80

    positions = [
      { x: center_x,           y: center_y - @offset + 24 },
      { x: center_x - @offset, y: center_y },
      { x: center_x + @offset, y: center_y },
      { x: center_x,           y: center_y + @offset - 24 }
    ]

    (0..3).each do |i|
      tex = @textures[i]
      pos = positions[i]

      if i == @selected_index
        use_anim = (@anim_timer % 24) < 12 && tex[:anim]
        texture = use_anim ? tex[:anim] : tex[:normal]
      else
        texture = tex[:normal]
      end

      dst = Raylib::Rectangle.create(pos[:x] - @tile_size/2, pos[:y] - @tile_size/2, @tile_size, @tile_size)
      src = Raylib::Rectangle.create(0, 0, @tile_size, @tile_size)
      Raylib.DrawTexturePro(texture, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    end

    if @commands_tex
      panel_x = center_x + @offset + @tile_size/2 + 16
      panel_y = center_y - @commands_h/2 + 24

      src = Raylib::Rectangle.create(0, 0, @commands_w, @commands_h)
      dst = Raylib::Rectangle.create(panel_x, panel_y, @commands_w, @commands_h)
      Raylib.DrawTexturePro(@commands_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

      if @font
        name = @tiles[@selected_index]["name"].capitalize
        font_size = 30
        text_size = Raylib.MeasureTextEx(@font, name, font_size, 1)
        text_x = panel_x + 20
        text_y = panel_y + (@commands_h - text_size.y) / 2
        Raylib.DrawTextEx(@font, name, Raylib::Vector2.create(text_x, text_y), font_size, 1, Raylib::WHITE)
      end
    end
  end
  
end