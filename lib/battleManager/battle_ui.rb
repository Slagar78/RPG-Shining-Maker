# lib/battleManager/battle_ui.rb

class BattleMenu
  attr_reader :selected_index, :visible
  attr_reader :magic_mode, :item_menu_mode
  attr_reader :selected_spell, :selected_item_action

  def initialize
    @tiles = [
      { "id" => 0, "name" => "attack", "icon" => "assets/ui/menu/Attack.png", "icon_anim" => "assets/ui/menu/Attack_Anim.png" },
      { "id" => 1, "name" => "magic",  "icon" => "assets/ui/menu/Menu2.png",  "icon_anim" => "assets/ui/menu/Menu2_Anim.png" },
      { "id" => 2, "name" => "item",   "icon" => "assets/ui/menu/Menu3.png",  "icon_anim" => "assets/ui/menu/Menu3_Anim.png" },
      { "id" => 3, "name" => "stay",   "icon" => "assets/ui/menu/Stay.png",   "icon_anim" => "assets/ui/menu/Stay_Anim.png" }
    ]

    # Плитки подменю предметов
    @item_tiles = [
      { "id" => 0, "name" => "use",   "icon" => "assets/ui/menu/Use.png",   "icon_anim" => "assets/ui/menu/Use_Anim.png" },
      { "id" => 1, "name" => "give",  "icon" => "assets/ui/menu/Give.png",  "icon_anim" => "assets/ui/menu/Give_Anim.png" },
      { "id" => 2, "name" => "equip", "icon" => "assets/ui/menu/Equip.png", "icon_anim" => "assets/ui/menu/Equip_Anim.png" },
      { "id" => 3, "name" => "drop",  "icon" => "assets/ui/menu/Drop.png",  "icon_anim" => "assets/ui/menu/Drop_Anim.png" }
    ]

    @visible = false
    @selected_index = 0
    @anim_timer = 0
    @tile_size = 48
    @offset = 48
    @font = nil

    @magic_mode = false
    @spells = []
    @magic_selected = 0
    @empty_magic_tex = nil
	@empty_item_tex = nil
    @magic_icon_cache = {}

    # --- универсальный грид предметов (Use / Give / Drop) ---
    @item_grid_mode = nil        # :use, :give, :drop или nil
    @items = []
    @item_selected = 0
    @item_icons = {}
    @pending_grid_item = nil    # будет [item, mode]

    @item_menu_mode = false
    @item_menu_selected = 0

    load_textures
    load_commands_panel
    load_magic_panel
	load_empty_item_tex
  end

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

    @item_textures = []
    @item_tiles.each do |tile|
      normal = anim = nil
      if File.exist?(tile["icon"])
        normal = Raylib.LoadTexture(tile["icon"])
        Raylib.SetTextureFilter(normal, 0)
      end
      if File.exist?(tile["icon_anim"])
        anim = Raylib.LoadTexture(tile["icon_anim"])
        Raylib.SetTextureFilter(anim, 0)
      end
      @item_textures << { normal: normal, anim: anim }
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

  def load_magic_panel
    path = "assets/ui/commands_names.png"
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      @magic_panel_tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(@magic_panel_tex, Raylib::TEXTURE_FILTER_POINT)
      @magic_panel_w = @magic_panel_tex.width
      @magic_panel_h = @magic_panel_tex.height
    else
      puts "WARNING: #{path} not found."
      @magic_panel_tex = nil
    end
  end

  def load_empty_item_tex
    path = "assets/items/item_empty.png"
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      @empty_item_tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(@empty_item_tex, Raylib::TEXTURE_FILTER_POINT)
    else
      @empty_item_tex = nil
    end
  end

  def open(can_attack = false)
    @visible = true
    @selected_index = can_attack ? 0 : 3
    @anim_timer = 0
    @magic_mode = false
    @item_menu_mode = false
    @item_grid_mode = nil   # закрываем и грид, если был открыт
  end

  def close
    @visible = false
    @magic_mode = false
    @item_menu_mode = false
    @item_grid_mode = nil
  end

  # --- Магия ---
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

  # --- Подменю предметов (4 плитки) ---
  def open_item_menu
    @item_menu_mode = true
    @item_menu_selected = 0
    @visible = true
    @anim_timer = 0
  end

  def close_item_menu
    @item_menu_mode = false
  end

  # --- Универсальный грид предметов (крест 4 иконки) ---
  def open_item_grid(mode, items)
    @item_grid_mode = mode
    @items = items.first(4)
    @item_selected = 0
    @visible = true
    @pending_grid_item = nil
  end

  def close_item_grid
    @item_grid_mode = nil
    @items = []
    @item_selected = 0
    @pending_grid_item = nil
	@visible = false
  end

  def selected_item
    @items[@item_selected]
  end

  # Возвращает [item, mode] или nil, если ничего не выбрано
  def fetch_pending_grid_item
    result = @pending_grid_item
    @pending_grid_item = nil
    result
  end

  def load_item_icon(item)
    return nil unless item && item["icon"]
    path = item["icon"]
    return @item_icons[path] if @item_icons.key?(path)

    if File.exist?(path)
      img = Raylib.LoadImage(path)
      tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(tex, Raylib::TEXTURE_FILTER_POINT)
      @item_icons[path] = tex
    else
      @item_icons[path] = nil
    end
  end

  def selected_item_action
    @item_menu_selected   # 0=Use, 1=Give, 2=Equip, 3=Drop
  end

  # --- Обработка ввода ---
  def handle_input
    return unless @visible

    if @item_menu_mode
      if Raylib.IsKeyPressed(Raylib::KEY_UP)
        @item_menu_selected = 0
      elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
        @item_menu_selected = 1
      elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @item_menu_selected = 2
      elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
        @item_menu_selected = 3
      end
      return
    end

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

    if @item_grid_mode
      # Стрелки: переключение с пропуском пустых слотов
      if Raylib.IsKeyPressed(Raylib::KEY_UP) || Raylib.IsKeyPressed(Raylib::KEY_LEFT)
        new_idx = @item_selected
        loop do
          new_idx = (new_idx - 1) % 4
          break if @items[new_idx]&.dig("item") != "NOTHING" || new_idx == @item_selected
        end
        @item_selected = new_idx
      elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT) || Raylib.IsKeyPressed(Raylib::KEY_DOWN)
        new_idx = @item_selected
        loop do
          new_idx = (new_idx + 1) % 4
          break if @items[new_idx]&.dig("item") != "NOTHING" || new_idx == @item_selected
        end
        @item_selected = new_idx
      end

      # Подтверждение – только если слот не пустой
      if (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)) &&
         @items[@item_selected]&.dig("item") != "NOTHING"
        @pending_grid_item = [selected_item, @item_grid_mode]
      end
      return
    end

    # Основное меню
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

    center_x = 576 / 2
    center_y = 480 - 80

    if @item_grid_mode
      draw_item_grid_icons(center_x, center_y, @offset)
      return
    end

    if @item_menu_mode
      draw_tile_menu(@item_textures, @item_tiles, @item_menu_selected, center_x, center_y)
      return
    end

    if @magic_mode
      draw_magic_icons(center_x, center_y, @offset)
      return
    end

    draw_tile_menu(@textures, @tiles, @selected_index, center_x, center_y)
  end

  # Универсальная отрисовка 4 плиток
  def draw_tile_menu(textures, tiles, selected_index, center_x, center_y)
    positions = [
      { x: center_x,           y: center_y - @offset + 24 },
      { x: center_x - @offset, y: center_y },
      { x: center_x + @offset, y: center_y },
      { x: center_x,           y: center_y + @offset - 24 }
    ]

    (0..3).each do |i|
      tex = textures[i]
      pos = positions[i]

      if tex
        if i == selected_index
          use_anim = (@anim_timer % 24) < 12 && tex[:anim]
          texture = use_anim ? tex[:anim] : tex[:normal]
        else
          texture = tex[:normal]
        end
      end

      if texture
        dst = Raylib::Rectangle.create(pos[:x] - @tile_size/2, pos[:y] - @tile_size/2, @tile_size, @tile_size)
        src = Raylib::Rectangle.create(0, 0, @tile_size, @tile_size)
        Raylib.DrawTexturePro(texture, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      else
        Raylib.DrawRectangle(pos[:x] - @tile_size/2, pos[:y] - @tile_size/2, @tile_size, @tile_size, Raylib::GRAY)
        Raylib.DrawRectangleLines(pos[:x] - @tile_size/2, pos[:y] - @tile_size/2, @tile_size, @tile_size, Raylib::DARKGRAY)
      end
    end

    # Панель с названием команды справа
    if @commands_tex
      panel_x = center_x + @offset + @tile_size/2 + 16
      panel_y = center_y - @commands_h/2 + 24
      src = Raylib::Rectangle.create(0, 0, @commands_w, @commands_h)
      dst = Raylib::Rectangle.create(panel_x, panel_y, @commands_w, @commands_h)
      Raylib.DrawTexturePro(@commands_tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

      if @font
        name = tiles[selected_index]["name"].capitalize
        font_size = 30
        text_size = Raylib.MeasureTextEx(@font, name, font_size, 1)
        text_x = panel_x + 20
        text_y = panel_y + (@commands_h - text_size.y) / 2
        Raylib.DrawTextEx(@font, name, Raylib::Vector2.create(text_x, text_y), font_size, 1, Raylib::WHITE)
      end
    end
  end

  # Отрисовка иконок магии
  def draw_magic_icons(cx, cy, offset)
    positions = [
      { x: cx,       y: cy - 24 },
      { x: cx - 32,  y: cy },
      { x: cx + 32,  y: cy },
      { x: cx,       y: cy + 24 }
    ]

    unless @empty_magic_tex
      path = "assets/spells/magic_empty.png"
      if File.exist?(path)
        @empty_magic_tex = Raylib.LoadTexture(path)
        Raylib.SetTextureFilter(@empty_magic_tex, Raylib::TEXTURE_FILTER_POINT)
      end
    end

    4.times do |i|
      next if i == @magic_selected
      spell = @spells[i]
      pos = positions[i]

      icon = if spell
               load_magic_icon(find_spell_icon(spell["spell"], spell["spell_level"]))
             else
               @empty_magic_tex
             end

      base_w = 32
      base_h = 48
      base_x = pos[:x] - base_w / 2
      base_y = pos[:y] - base_h / 2

      src = Raylib::Rectangle.create(0, 0, base_w, base_h)
      dst = Raylib::Rectangle.create(base_x, base_y, base_w, base_h)

      if icon
        Raylib.DrawTexturePro(icon, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      else
        Raylib.DrawRectangle(base_x, base_y, base_w, base_h, Raylib::GRAY)
      end
    end

    selected_i = @magic_selected
    selected_pos = positions[selected_i]
    selected_spell = @spells[selected_i]

    icon = if selected_spell
             load_magic_icon(find_spell_icon(selected_spell["spell"], selected_spell["spell_level"]))
           else
             @empty_magic_tex
           end

    base_w = 32
    base_h = 48
    scale = 1.2
    new_w = base_w * scale
    new_h = base_h * scale
    new_x = selected_pos[:x] - new_w / 2
    new_y = selected_pos[:y] - new_h / 2

    src = Raylib::Rectangle.create(0, 0, base_w, base_h)
    dst = Raylib::Rectangle.create(new_x, new_y, new_w, new_h)

    if icon
      Raylib.DrawTexturePro(icon, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    else
      Raylib.DrawRectangle(new_x, new_y, new_w, new_h, Raylib::GRAY)
    end

    if @magic_panel_tex && @spells.any?
      right_pos = positions[2]
      panel_x = right_pos[:x] + 16 + 16
      panel_y = right_pos[:y] - @magic_panel_h / 2

      src = Raylib::Rectangle.create(0, 0, @magic_panel_w, @magic_panel_h)
      dst = Raylib::Rectangle.create(panel_x, panel_y, @magic_panel_w, @magic_panel_h)
      Raylib.DrawTexturePro(@magic_panel_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

      if @font && @magic_selected < @spells.length
        name = @spells[@magic_selected]["spell"] || ""
        font_size = 25
        text_size = Raylib.MeasureTextEx(@font, name, font_size, 1)
        text_x = panel_x + (@magic_panel_w - text_size.x) / 2 - 42
        text_y = panel_y + (@magic_panel_h - text_size.y) / 2 - 20
        Raylib.DrawTextEx(@font, name, Raylib::Vector2.create(text_x, text_y), font_size, 1, Raylib::WHITE)
      end
    end
  end

  # Отрисовка грида иконок предметов (Use/Give/Drop)
def draw_item_grid_icons(cx, cy, offset)
  positions = [
    { x: cx,       y: cy - 24 },
    { x: cx - 32,  y: cy },
    { x: cx + 32,  y: cy },
    { x: cx,       y: cy + 24 }
  ]

  # Невыбранные иконки
  4.times do |i|
    next if i == @item_selected
    item = @items[i]
    pos = positions[i]
    icon = load_item_icon(item)
    tex = icon || @empty_item_tex
    base_w = 32
    base_h = 48
    dst = Raylib::Rectangle.create(pos[:x] - base_w/2, pos[:y] - base_h/2, base_w, base_h)
    src = Raylib::Rectangle.create(0, 0, base_w, base_h)
    if tex
      Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
    else
      Raylib.DrawRectangle(dst.x, dst.y, base_w, base_h, Raylib::GRAY)
    end
  end

  # Выбранная (увеличенная)
  sel_item = @items[@item_selected]
  sel_pos = positions[@item_selected]
  icon = load_item_icon(sel_item)
  tex = icon || @empty_item_tex
  scale = 1.2
  new_w = 32 * scale
  new_h = 48 * scale
  dst = Raylib::Rectangle.create(sel_pos[:x] - new_w/2, sel_pos[:y] - new_h/2, new_w, new_h)
  src = Raylib::Rectangle.create(0, 0, 32, 48)
  if tex
    Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
  else
    Raylib.DrawRectangle(dst.x, dst.y, new_w, new_h, Raylib::GRAY)
  end
end

  def load_magic_icon(path)
    return nil unless path && !path.empty?
    return @magic_icon_cache[path] if @magic_icon_cache.key?(path)
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(tex, Raylib::TEXTURE_FILTER_POINT)
      @magic_icon_cache[path] = tex
    else
      @magic_icon_cache[path] = nil
    end
  end

  def find_spell_icon(name, level)
    spells = $spells || []
    spell = spells.find { |s| s["name"].casecmp?(name) && s["level"] == level }
    spell ? spell["icon"] : nil
  end
end