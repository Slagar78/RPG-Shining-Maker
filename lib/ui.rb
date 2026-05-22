# lib/ui.rb
require 'json'

# ============================================
# МЕНЮ 4 ПЛИТКИ (Bottom Menu)
# ============================================
class BottomMenu
  attr_reader :selected_index

  def initialize(tiles_data = nil)
    load_tiles(tiles_data)
    @visible = false
    @selected_index = 0
    @anim_timer = 0
    @tile_size = 48
    @offset = 48
    load_textures
  end

  def load_tiles(tiles_data = nil)
    if tiles_data
      @tiles = tiles_data
    elsif File.exist?("data/menu.json")
      data = JSON.parse(File.read("data/menu.json"))
      @tiles = data["tiles"]
    else
      @tiles = [
        { "id" => 0, "name" => "status", "icon" => "assets/ui/menu/status.png", "icon_anim" => "assets/ui/menu/status_anim.png" },
        { "id" => 1, "name" => "magic",  "icon" => "assets/ui/menu/magic.png",  "icon_anim" => "assets/ui/menu/magic_anim.png" },
        { "id" => 2, "name" => "items",  "icon" => "assets/ui/menu/items.png",  "icon_anim" => "assets/ui/menu/items_anim.png" },
        { "id" => 3, "name" => "event",  "icon" => "assets/ui/menu/event.png",  "icon_anim" => "assets/ui/menu/event_anim.png" }
      ]
    end
  end
  
  def load_textures
    @textures = []
    @tiles.each do |tile|
      normal = Raylib.LoadTexture(tile["icon"])
      anim = Raylib.LoadTexture(tile["icon_anim"])
      Raylib.SetTextureFilter(normal, 0)
      Raylib.SetTextureFilter(anim, 0)
      @textures << { normal: normal, anim: anim }
    end
  end
  
  def open
    @visible = true
    @selected_index = 0
    @anim_timer = 0
  end
  
  def close
    @visible = false
  end
  
  def handle_input
    return unless @visible
    
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
  end
end

# ============================================
# StatusOverlay (обновлён под общую базу)
# ============================================
class StatusOverlay
  VISIBLE_ROWS = 5
  attr_reader :current_actor

  def initialize(font = nil, db = nil, start_inventory = nil, party = nil, classes_data = nil, class_names = nil)
    @font = font
    @db = db

    # Используем переданные общие объекты
    @start_inventory = start_inventory
    @party = party
    @classes_data = classes_data
    @class_names = class_names

    # Если что-то не передано – загружаем из JSON (обратная совместимость)
    if @start_inventory.nil?
      @start_inventory = []
      if File.exist?("data/actors/start_inventory.json")
        data = JSON.parse(File.read("data/actors/start_inventory.json"))
        @start_inventory = data["start_inventory"] || []
      end
    end

    if @party.nil?
      @party = []
      if File.exist?("data/actors/actors.json")
        data = JSON.parse(File.read("data/actors/actors.json"))
        @party = data["actors"] || []
      end
    end

    if @classes_data.nil? || @class_names.nil?
      @classes_data = [] if @classes_data.nil?
      @class_names = {} if @class_names.nil?
      if File.exist?("data/actors/classes.json")
        data = JSON.parse(File.read("data/actors/classes.json"))
        @classes_data = data["classes"] || []
        @classes_data.each { |c| @class_names[c["id"]] = c["name"] }
      end
    end

    # Заклинания – берём из базы, если она передана, иначе загружаем
    @all_spells = []
    if @db && @db.spells
      @all_spells = @db.spells
    elsif File.exist?("data/spells/spells.json")
      data = JSON.parse(File.read("data/spells/spells.json"))
      @all_spells = data["spells"] || []
    end

    # ----- Остальная инициализация -----
    @visible = false
    @anim_phase = 0
    @anim_timer = 0
    @ready_to_close = false
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    @portrait_cache = {}

    @upper_target_x = 182
    @upper_target_y = 16
    @lower_target_x = 48
    @lower_target_y = 224
    @portrait_target_x = 48
    @portrait_target_y = 16
    @frame_target_x = 48
    @frame_target_y = 16

    @upper_start_x = 576 + 220
    @lower_start_y = 480 + 220
    @portrait_start_x = -220
    @frame_start_x = -220

    @upper_x = @upper_start_x
    @upper_y = @upper_target_y
    @lower_x = @lower_target_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @portrait_y = @portrait_target_y
    @frame_x = @frame_start_x
    @frame_y = @frame_target_y

    @upper_w = 346
    @upper_h = 208
    @lower_w = 480
    @lower_h = 240
    @portrait_w = 134
    @portrait_h = 208
    @frame_w = 134
    @frame_h = 208

    @selection_blink_timer = 0
    @status_view_mode = 0
    @list_top_index = 0
    @selected_actor_index = 0
    @input_timer_up = 0
    @input_timer_down = 0

    load_textures
  end

  # -----------------------------------------------------------------
  # Вспомогательные методы поиска
  # -----------------------------------------------------------------
  def get_actor_stats(actor_name)
    @party.each { |actor| return actor if actor["name"] == actor_name }
    nil
  end

  def find_actor_items(actor_name)
    actor = @party.find { |a| a["name"] == actor_name }
    return [] unless actor
    entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    entry ? entry["items"] || [] : []
  end

  def find_item_by_name(name)
    @db ? @db.find_by_name(name) : nil
  end

  def calculate_equip_bonuses(actor)
    return { attack: 0, defense: 0 } unless actor
    inv_entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    return { attack: 0, defense: 0 } unless inv_entry

    total_attack = 0
    total_defense = 0
    inv_entry["items"].each do |item_entry|
      next unless item_entry["equipped"]
      next if item_entry["item"] == "NOTHING"
      item_data = find_item_by_name(item_entry["item"])
      next unless item_data
      total_attack += (item_data["attack"] || 0).to_i
      total_defense += (item_data["defense"] || 0).to_i
    end
    { attack: total_attack, defense: total_defense }
  end

  # -----------------------------------------------------------------
  # Загрузка текстур и актёров
  # -----------------------------------------------------------------
  def load_textures
    @upper_tex = Raylib.LoadTexture("assets/ui/upper_panel.png")
    @lower_tex = Raylib.LoadTexture("assets/ui/lower_panel.png")
    @frame_tex = Raylib.LoadTexture("assets/ui/portrait_frame.png")
    @ruby_tex  = Raylib.LoadTexture("assets/ui/ruby_icon.png")
    Raylib.SetTextureFilter(@upper_tex, 0) if @upper_tex
    Raylib.SetTextureFilter(@lower_tex, 0) if @lower_tex
    Raylib.SetTextureFilter(@frame_tex, 0) if @frame_tex
    Raylib.SetTextureFilter(@ruby_tex, 0)  if @ruby_tex
  end

  # -----------------------------------------------------------------
  # Отрисовка текста и предметов
  # -----------------------------------------------------------------
  def draw_text_custom(text, x, y, size, color)
    if @font
      Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
    else
      Raylib.DrawText(text, x, y, size, color)
    end
  end

  def draw_text_centered_h(text, cx, y, size, color)
    return unless @font
    vec = Raylib.MeasureTextEx(@font, text, size, 1)
    x = cx - vec.x / 2
    Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
  end

  def draw_item_name(text, x, y, size, color)
    if text.include?(' ')
      first_word = text[0...text.index(' ')].strip
      second_word = text[text.index(' ') + 1..-1].strip
      draw_text_custom(first_word, x, y, size, color)
      draw_text_custom(second_word, x + 14, y + 15, size, color)
    else
      draw_text_custom(text, x, y, size, color)
    end
  end

  def load_portrait(name)
    return nil unless name
    return @portrait_cache[name] if @portrait_cache.key?(name)
    path = "assets/ui/portraits/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @portrait_cache[name] = tex
  end

  def load_blink_portrait(name)
    return nil unless name
    cache_key = "#{name}_blink"
    return @portrait_cache[cache_key] if @portrait_cache.key?(cache_key)
    path = "assets/ui/portraits/#{name}_blink.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @portrait_cache[cache_key] = tex
  end

  # -----------------------------------------------------------------
  # Смена выбранного персонажа
  # -----------------------------------------------------------------
  def change_selected_actor(delta)
    return unless @party.any?
    new_index = @selected_actor_index + delta
    return if new_index < 0 || new_index >= @party.length

    @selected_actor_index = new_index
    if @selected_actor_index < @list_top_index
      @list_top_index = @selected_actor_index
    elsif @selected_actor_index >= @list_top_index + VISIBLE_ROWS
      @list_top_index = @selected_actor_index - VISIBLE_ROWS + 1
    end

    @current_actor = @party[@selected_actor_index]["name"]
    @current_items = find_actor_items(@current_actor)
    actor = @party[@selected_actor_index]
    portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
    @portrait_tex = load_portrait(portrait_name)
    @blink_tex = load_blink_portrait(portrait_name)

    if actor
      klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
      spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
      @current_spells = spell_list.select { |spell| spell["level"] <= actor["level"] }
    end
  end

  # -----------------------------------------------------------------
  # Анимация открытия / закрытия
  # -----------------------------------------------------------------
  def open(player = nil)
    return if @visible
    @visible = true
    @anim_phase = 1
    @ready_to_close = false

    @upper_x = @upper_start_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x

    if @party.any?
      if @selected_actor_index >= @party.length
        @selected_actor_index = @party.length - 1
      end
      if @selected_actor_index < @list_top_index
        @list_top_index = @selected_actor_index
      elsif @selected_actor_index >= @list_top_index + VISIBLE_ROWS
        @list_top_index = @selected_actor_index - VISIBLE_ROWS + 1
      end
      @current_actor = @party[@selected_actor_index]["name"]
      actor = @party.find { |a| a["name"] == @current_actor }
      if actor
        klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
        spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
        @current_spells = spell_list.select { |spell| spell["level"] <= actor["level"] }
      else
        @current_spells = []
      end
      @current_items = find_actor_items(@current_actor)
      portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
      @portrait_tex = load_portrait(portrait_name)
      @blink_tex = load_blink_portrait(portrait_name)
    else
      @current_actor = nil
      @current_spells = []
      @current_items = []
    end
    @blink_timer = 0
    @blink_duration = 0
    @selection_blink_timer = 0
  end

  def close
    return unless @visible && @anim_phase == 2
    @anim_phase = 3
  end

  def force_close
    @visible = false
    @anim_phase = 0
  end

  def handle_input
    return unless @visible && @anim_phase == 2
    if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
      close
      return
    end
    if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
      @status_view_mode = 1 - @status_view_mode
    end
    if Raylib.IsKeyDown(Raylib::KEY_UP)
      @input_timer_up += 1
      if @input_timer_up == 1 || (@input_timer_up > 20 && (@input_timer_up - 20) % 5 == 0)
        change_selected_actor(-1)
      end
    else
      @input_timer_up = 0
    end
    if Raylib.IsKeyDown(Raylib::KEY_DOWN)
      @input_timer_down += 1
      if @input_timer_down == 1 || (@input_timer_down > 20 && (@input_timer_down - 20) % 5 == 0)
        change_selected_actor(1)
      end
    else
      @input_timer_down = 0
    end
  end

  def update
    return unless @visible
    speed = 38
    case @anim_phase
    when 1
      @portrait_x += speed; @portrait_x = @portrait_target_x if @portrait_x > @portrait_target_x
      @frame_x += speed; @frame_x = @frame_target_x if @frame_x > @frame_target_x
      @upper_x -= speed; @upper_x = @upper_target_x if @upper_x < @upper_target_x
      @lower_y -= speed; @lower_y = @lower_target_y if @lower_y < @lower_target_y
      if @portrait_x >= @portrait_target_x && @frame_x >= @frame_target_x &&
         @upper_x <= @upper_target_x && @lower_y <= @lower_target_y
        @anim_phase = 2
      end
    when 3
      @portrait_x -= speed; @portrait_x = @portrait_start_x if @portrait_x < @portrait_start_x
      @frame_x -= speed; @frame_x = @frame_start_x if @frame_x < @frame_start_x
      @upper_x += speed; @upper_x = @upper_start_x if @upper_x > @upper_start_x
      @lower_y += speed; @lower_y = @lower_start_y if @lower_y > @lower_start_y
      if @portrait_x <= @portrait_start_x
        @visible = false
        @anim_phase = 0
      end
    end
    if @anim_phase == 2
      @blink_timer += 1
      if @blink_duration > 0
        @blink_duration -= 1
      elsif @blink_timer >= @blink_interval
        @blink_duration = 8
        @blink_timer = 0
        @blink_interval = 100 + rand(50)
      end
      @selection_blink_timer += 1
    end
  end

  def draw
    return unless @visible
    origin = Raylib::Vector2.create(0, 0)

    dst = Raylib::Rectangle.create(@lower_x, @lower_y, @lower_w, @lower_h)
    Raylib.DrawTexturePro(@lower_tex, Raylib::Rectangle.create(0,0,@lower_w,@lower_h), dst, origin, 0, Raylib::WHITE)
    dst = Raylib::Rectangle.create(@upper_x, @upper_y, @upper_w, @upper_h)
    Raylib.DrawTexturePro(@upper_tex, Raylib::Rectangle.create(0,0,@upper_w,@upper_h), dst, origin, 0, Raylib::WHITE)

    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, 134, 208)
      Raylib.DrawTexturePro(portrait, Raylib::Rectangle.create(0,0,134,208), dst, origin, 0, Raylib::WHITE)
    end
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, Raylib::Rectangle.create(0,0,@frame_w,@frame_h), dst, origin, 0, Raylib::WHITE)

    # Верхняя панель
    actor_data = @party.find { |a| a["name"] == @current_actor }
    if actor_data
      class_id   = actor_data["class_id"]
      class_name = @class_names[class_id] || "???"
      level      = actor_data["level"]
      header     = "#{actor_data["name"].slice(0,10)}  #{class_name.slice(0,10)}  LV #{level}"
      draw_text_custom(header, @upper_x + 25, @upper_y + 12, 20, WHITE)
    else
      draw_text_custom("NO DATA", @upper_x + 25, @upper_y + 12, 20, WHITE)
    end
    draw_text_custom("Магия", @upper_x + 25, @upper_y + 38, 20, WHITE)
    draw_text_custom("Предметы", @upper_x + 195, @upper_y + 38, 20, WHITE)

    if @current_spells && @current_spells.any?
      @current_spells.each_with_index do |spell, i|
        y = @upper_y + 72 + i * 34
        draw_text_custom("#{spell["spell"]} Lv#{spell["spell_level"]}", @upper_x + 25, y, 20, WHITE)
      end
    else
      draw_text_custom("Nothing", @upper_x + 25, @upper_y + 72, 20, ORANGE)
    end

    has_any_item = @current_items && @current_items.any? { |entry| entry["item"] != "NOTHING" }
    if has_any_item
      @current_items.each_with_index do |item_entry, i|
        next if item_entry["item"] == "NOTHING"
        y = @upper_y + 64 + i * 33
        draw_item_name(item_entry["item"], @upper_x + 195, y, 18, WHITE)
        if item_entry["equipped"]
          draw_text_custom("E", @upper_x + 180, y, 18, YELLOW)
        end
      end
    else
      draw_text_custom("Nothing", @upper_x + 195, @upper_y + 64, 18, ORANGE)
    end

    # Нижняя панель
    header_y = @lower_y + 28
    if @status_view_mode == 0
      draw_text_custom("Имя",   @lower_x + 44,  header_y, 20, WHITE)
      draw_text_custom("Класс", @lower_x + 187, header_y, 20, WHITE)
      level_header_center_x = @lower_x + 290 + Raylib.MeasureTextEx(@font, "Уровень", 20, 1).x / 2
      exp_header_center_x   = @lower_x + 395 + Raylib.MeasureTextEx(@font, "Опыт", 20, 1).x / 2
      draw_text_custom("Уровень", @lower_x + 290, header_y, 20, WHITE)
      draw_text_custom("Опыт",    @lower_x + 395, header_y, 20, WHITE)
    else
      draw_text_custom("Имя", @lower_x + 44, header_y, 20, WHITE)
      stat_headers = ["HP", "MP", "AT", "DF", "AGI", "MV"]
      stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]
      stat_headers.each_with_index do |head, idx|
        cx = stat_centers[idx]
        w = Raylib.MeasureTextEx(@font, head, 20, 1).x
        draw_text_custom(head, cx - w / 2, header_y, 20, WHITE)
      end
    end

    VISIBLE_ROWS.times do |i|
      list_index = @list_top_index + i
      break if list_index >= @party.length
      member = @party[list_index]
      y = @lower_y + 71 + i * 34

      if member["name"] == @current_actor
        pulse = Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6
        alpha = (pulse * 255).to_i
        highlight = Raylib.Fade(Raylib::BLUE, alpha / 255.0)
        Raylib.DrawRectangle(@lower_x + 38, y - 4, 138, 28, highlight)
      end

      if @ruby_tex
        ruby_src = Raylib::Rectangle.create(0, 0, @ruby_tex.width, @ruby_tex.height)
        ruby_dst = Raylib::Rectangle.create(@lower_x + 15, y - 3, 24, 24)
        Raylib.DrawTexturePro(@ruby_tex, ruby_src, ruby_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
      end

      if i == 0 && @list_top_index > 0
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        ax = @lower_x + 27; ay = y + 12
        Raylib.DrawTriangle(Raylib::Vector2.create(ax, ay - 6),
                            Raylib::Vector2.create(ax - 6, ay + 4),
                            Raylib::Vector2.create(ax + 6, ay + 4), color)
      end
      if i == VISIBLE_ROWS - 1 && @list_top_index + VISIBLE_ROWS < @party.length
        ax = @lower_x + 27; ay = y + 12
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        Raylib.DrawTriangle(Raylib::Vector2.create(ax - 6, ay - 4),
                            Raylib::Vector2.create(ax, ay + 6),
                            Raylib::Vector2.create(ax + 6, ay - 4), color)
      end

      name_display = member["name"].slice(0, 10)
      draw_text_custom(name_display, @lower_x + 44, y, 18, WHITE)

      if @status_view_mode == 0
        class_name = @class_names[member["class_id"]] || "???"
        class_display = class_name.slice(0, 10)
        draw_text_custom(class_display, @lower_x + 187, y, 18, WHITE)
        draw_text_centered_h(member["level"].to_s, level_header_center_x, y, 18, WHITE)
        draw_text_centered_h(member["exp"].to_s,    exp_header_center_x,   y, 18, WHITE)
      else
        klass = @classes_data.find { |c| c["id"] == member["class_id"] }
        if klass && @db
        lv = [(member["level"] || 1), 1].max
        hp_val  = [@db.stat_at_level(klass["hp_growth"],  lv), 0].max
        mp_val  = [@db.stat_at_level(klass["mp_growth"],  lv), 0].max
        atk_val = [@db.stat_at_level(klass["attack_growth"], lv), 0].max
        def_val = [@db.stat_at_level(klass["defense_growth"], lv), 0].max
        agi_val = [@db.stat_at_level(klass["agility_growth"], lv), 0].max
        mov_val = [(klass["move"] || 0), 0].max
      else
        hp_val = mp_val = atk_val = def_val = agi_val = mov_val = 0
      end
        bonuses = calculate_equip_bonuses(member)
        eff_atk = [atk_val + bonuses[:attack], 0].max
        eff_def = [def_val + bonuses[:defense], 0].max
		
        stat_values = [hp_val, mp_val, eff_atk, eff_def, agi_val, mov_val]
        stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]
        stat_values.each_with_index do |val, idx|
          draw_text_centered_h(val.to_s, stat_centers[idx], y, 18, WHITE)
        end
      end
    end
  end
end

# ============================================
# ОКНО ПРОФАЙЛА (Character Profile)
# ============================================
class Profile
  def initialize(font = nil, db = nil, start_inventory = nil)
    @font = font
    @db = db
    @start_inventory = start_inventory   # общий массив

    # Загружаем, если не передан (обратная совместимость)
    if @start_inventory.nil?
      @start_inventory = []
      if File.exist?("data/actors/start_inventory.json")
        data = JSON.parse(File.read("data/actors/start_inventory.json"))
        @start_inventory = data["start_inventory"] || []
      end
    end

    # ----- Остальная инициализация (без изменений) -----
    @visible = false
    @anim_phase = 0
    @anim_timer = 0
    @ready_to_close = false
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    @portrait_cache = {}
    @icon_cache = {}
    @mapsprite_cache = {}
    @sprite_frame = 0
    @sprite_timer = 0
    @sprite_speed = 14

    @right_panel_target_x = 182
    @right_panel_target_y = 16
    @sub_panel_target_x = 48
    @sub_panel_target_y = 224
    @right_panel_start_x = 576 + 220
    @sub_panel_start_y = 480 + 220
    @right_panel_x = @right_panel_start_x
    @right_panel_y = @right_panel_target_y
    @sub_panel_x = @sub_panel_target_x
    @sub_panel_y = @sub_panel_start_y
    @right_panel_w = 346
    @right_panel_h = 448
    @sub_panel_w = 134
    @sub_panel_h = 240

    @portrait_target_x = 48
    @portrait_target_y = 16
    @frame_target_x = 48
    @frame_target_y = 16
    @portrait_start_x = -220
    @frame_start_x = -220
    @portrait_x = @portrait_start_x
    @portrait_y = @portrait_target_y
    @frame_x = @frame_start_x
    @frame_y = @frame_target_y

    load_textures
  end

  def load_textures
    @right_panel_tex = Raylib.LoadTexture("assets/ui/right_panel.png")
    @sub_panel_tex   = Raylib.LoadTexture("assets/ui/sub_panel.png")
    @frame_tex       = Raylib.LoadTexture("assets/ui/portrait_frame.png")
    Raylib.SetTextureFilter(@right_panel_tex, 0) if @right_panel_tex
    Raylib.SetTextureFilter(@sub_panel_tex, 0)   if @sub_panel_tex
    Raylib.SetTextureFilter(@frame_tex, 0)       if @frame_tex
  end

  def open(actor_name, party, class_names, classes_data, portrait_cache, start_inventory)
    return if @visible
    @visible = true
    @anim_phase = 1
    @ready_to_close = false
    @current_actor = actor_name
    @party = party
    @class_names = class_names
    @classes_data = classes_data
    @portrait_cache = portrait_cache
    @start_inventory = start_inventory
    @gold = @db&.globals ? (@db.globals["gold"] || 0) : 0
    actor = @party.find { |a| a["name"] == actor_name }
    if actor
      portrait_name = actor["portrait"] || actor["name"]
      @portrait_tex = load_portrait(portrait_name)
      @blink_tex = load_blink_portrait(portrait_name)
    end
    @blink_timer = 0
    @blink_duration = 0
    @sprite_timer = 0
    @sprite_frame = 0
    mapsprite_name = actor ? actor["mapsprite"] : nil
    @mapsprite_tex = mapsprite_name ? load_mapsprite(mapsprite_name) : nil
    @right_panel_x = @right_panel_start_x
    @sub_panel_y = @sub_panel_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x
  end

  # Теперь поиск предметов идёт через центральную базу @db
  def find_item_by_name(name)
    @db ? @db.find_by_name(name) : nil
  end

  def calculate_equip_bonuses(actor)
    return { attack: 0, defense: 0 } unless actor
    inv_entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    return { attack: 0, defense: 0 } unless inv_entry
    total_attack = 0
    total_defense = 0
    inv_entry["items"].each do |item_entry|
      next unless item_entry["equipped"]
      next if item_entry["item"] == "NOTHING"
      item_data = find_item_by_name(item_entry["item"])
      next unless item_data
      total_attack += (item_data["attack"] || 0).to_i
      total_defense += (item_data["defense"] || 0).to_i
    end
    { attack: total_attack, defense: total_defense }
  end

  # ---------- портреты и кеш ----------
  def load_portrait(name)
    return nil unless name
    return @portrait_cache[name] if @portrait_cache.key?(name)
    path = "assets/ui/portraits/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @portrait_cache[name] = tex
  end

  def load_blink_portrait(name)
    return nil unless name
    cache_key = "#{name}_blink"
    return @portrait_cache[cache_key] if @portrait_cache.key?(cache_key)
    path = "assets/ui/portraits/#{name}_blink.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @portrait_cache[cache_key] = tex
  end

  # ---------- обновление ----------
  def update
    return unless @visible
    speed = 38

    case @anim_phase
    when 1  # сборка
      @portrait_x += speed
      @portrait_x = @portrait_target_x if @portrait_x > @portrait_target_x
      @frame_x += speed
      @frame_x = @frame_target_x if @frame_x > @frame_target_x

      @right_panel_x -= speed
      @right_panel_x = @right_panel_target_x if @right_panel_x < @right_panel_target_x

      @sub_panel_y -= speed
      @sub_panel_y = @sub_panel_target_y if @sub_panel_y < @sub_panel_target_y

      if @portrait_x >= @portrait_target_x &&
         @frame_x >= @frame_target_x &&
         @right_panel_x <= @right_panel_target_x &&
         @sub_panel_y <= @sub_panel_target_y
        @anim_phase = 2
      end

    when 3  # разборка
      @portrait_x -= speed
      @portrait_x = @portrait_start_x if @portrait_x < @portrait_start_x
      @frame_x -= speed
      @frame_x = @frame_start_x if @frame_x < @frame_start_x

      @right_panel_x += speed
      @right_panel_x = @right_panel_start_x if @right_panel_x > @right_panel_start_x

      @sub_panel_y += speed
      @sub_panel_y = @sub_panel_start_y if @sub_panel_y > @sub_panel_start_y

      if @portrait_x <= @portrait_start_x
        @visible = false
        @anim_phase = 0
        @ready_to_close = true
      end
    end

    # Моргание и анимация спрайта
    if @anim_phase == 2
      @blink_timer += 1
      if @blink_duration > 0
        @blink_duration -= 1
      elsif @blink_timer >= @blink_interval
        @blink_duration = 8
        @blink_timer = 0
        @blink_interval = 100 + rand(50)
      end
      @sprite_timer += 1
      if @sprite_timer >= @sprite_speed
        @sprite_timer = 0
        @sprite_frame = (@sprite_frame + 1) % 2
      end
    end
  end

  def close
    return unless @visible && @anim_phase == 2
    @anim_phase = 3
  end

  def force_close
    @visible = false
    @anim_phase = 0
  end

  # ---------- отрисовка ----------
  def draw
    return unless @visible
    origin = Raylib::Vector2.create(0, 0)

    # Правая панель
    dst = Raylib::Rectangle.create(@right_panel_x, @right_panel_y, @right_panel_w, @right_panel_h)
    src = Raylib::Rectangle.create(0, 0, @right_panel_w, @right_panel_h)
    Raylib.DrawTexturePro(@right_panel_tex, src, dst, origin, 0, Raylib::WHITE)

    # Маленькая панель под портретом
    dst = Raylib::Rectangle.create(@sub_panel_x, @sub_panel_y, @sub_panel_w, @sub_panel_h)
    src = Raylib::Rectangle.create(0, 0, @sub_panel_w, @sub_panel_h)
    Raylib.DrawTexturePro(@sub_panel_tex, src, dst, origin, 0, Raylib::WHITE)

    # Портрет
    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, 134, 208)
      src = Raylib::Rectangle.create(0, 0, 134, 208)
      Raylib.DrawTexturePro(portrait, src, dst, origin, 0, Raylib::WHITE)
    end

    # Рамка
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, 134, 208)
    src = Raylib::Rectangle.create(0, 0, 134, 208)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)

    # ===== ТЕКСТ НА ПРАВОЙ ПАНЕЛИ =====
    actor = @party.find { |a| a["name"] == @current_actor } if @current_actor
    if actor
      class_id = actor["class_id"]
      klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
	  class_full_name = klass ? (klass["full_name"] || klass["name"]) : "???"
      class_full_name = class_full_name.slice(0, 16)
      actor_name = actor["name"].slice(0, 10)

      # Класс – золотистым
      draw_text_custom(class_full_name, @right_panel_x + 25, @right_panel_y + 12, 20, GOLD)
      class_width = Raylib.MeasureTextEx(@font, class_full_name, 20, 1).x
      space_width = 12
      name_x = @right_panel_x + 25 + class_width + space_width
      # Имя – белым
      draw_text_custom(actor_name, name_x, @right_panel_y + 12, 20, WHITE)
      if klass && @db
        lv = [(actor["level"] || 1), 1].max
        hp_val  = [@db.stat_at_level(klass["hp_growth"],  lv), 0].max
        mp_val  = [@db.stat_at_level(klass["mp_growth"],  lv), 0].max
        atk_val = [@db.stat_at_level(klass["attack_growth"], lv), 0].max
        def_val = [@db.stat_at_level(klass["defense_growth"], lv), 0].max
        agi_val = [@db.stat_at_level(klass["agility_growth"], lv), 0].max
        mov_val = [(klass["move"] || 0), 0].max
      else
        hp_val = mp_val = atk_val = def_val = agi_val = mov_val = 0
      end

      lv = actor["level"]
      exp = actor["exp"] || 0

      left_x = @right_panel_x + 55    # для «Magic»
      right_x = @right_panel_x + 210  # для «Items»
      stats_left_x = @right_panel_x + 25    # левый столбец статов (LV, HP, MP, EXP)
      stats_right_x = @right_panel_x + 130  # правый столбец статов (ATT, DEF, AGI, MOV)
      y_base = @right_panel_y + 55
      line_h = 28

      draw_text_custom("LV    #{lv}", stats_left_x, y_base, 20, WHITE)
      draw_text_custom("HP    #{hp_val}", stats_left_x, y_base + line_h, 20, WHITE)
      draw_text_custom("MP    #{mp_val}", stats_left_x, y_base + line_h * 2, 20, WHITE)
      draw_text_custom("EXP   #{exp}", stats_left_x, y_base + line_h * 3, 20, WHITE)

      bonuses = calculate_equip_bonuses(actor)
      eff_atk = [atk_val + bonuses[:attack], 0].max
      eff_def = [def_val + bonuses[:defense], 0].max

      draw_text_custom("ATT   #{eff_atk}", stats_right_x, y_base, 20, WHITE)
      draw_text_custom("DEF   #{eff_def}", stats_right_x, y_base + line_h, 20, WHITE)
      draw_text_custom("AGI   #{agi_val}", stats_right_x, y_base + line_h * 2, 20, WHITE)
      draw_text_custom("MOV   #{mov_val}", stats_right_x, y_base + line_h * 3, 20, WHITE)

      # Магия и Предметы
      klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
      spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
      spells = spell_list.select { |s| s["level"] <= actor["level"] }

      section_y = @right_panel_y + 200
      draw_text_custom("Magic", left_x - 30, section_y, 20, WHITE)
      draw_text_custom("Items", right_x - 50, section_y, 20, WHITE)

      # Магия (левый столбец)
      if spells.any?
        spells.first(4).each_with_index do |spell, i|
          y = section_y + 30 + i * 52
          spell_icon = load_icon(find_spell_icon(spell["spell"], spell["spell_level"]))
          if spell_icon
            src = Raylib::Rectangle.create(0, 0, 32, 48)
            dst = Raylib::Rectangle.create(left_x - 30, y, 32, 48)
            Raylib.DrawTexturePro(spell_icon, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
          end
          draw_text_custom(spell["spell"], left_x + 10, y + 6, 18, WHITE)
          draw_text_custom("Lv #{spell["spell_level"]}", left_x + 10, y + 26, 18, WHITE)
        end
      else
        draw_text_custom("Nothing", left_x, section_y + 30, 18, ORANGE)
      end

      # Предметы (правый столбец)
      inv_entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
      items = inv_entry ? inv_entry["items"] : []
      has_any_item = items.any? { |entry| entry["item"] != "NOTHING" }
      if has_any_item
        items.first(4).each_with_index do |item_entry, i|
          next if item_entry["item"] == "NOTHING"
          y = section_y + 30 + i * 52
          item_data = find_item_by_name(item_entry["item"])
          item_icon = item_data ? load_icon(item_data["icon"]) : nil
          if item_icon
            src = Raylib::Rectangle.create(0, 0, 32, 48)
            dst = Raylib::Rectangle.create(right_x - 50, y, 32, 48)
            Raylib.DrawTexturePro(item_icon, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
          end
          name = item_entry["item"]
          if item_entry["equipped"]
            draw_text_custom("E", right_x - 15, y + 12, 18, YELLOW)
          end
          draw_item_name(name, right_x + 5, y + 12, 18, WHITE)
        end
      else
        draw_text_custom("Nothing", right_x - 50, section_y + 30, 18, ORANGE)
      end
    end

    # Анимированный спрайт
    if @mapsprite_tex
      src = Raylib::Rectangle.create(@sprite_frame * 48, 2 * 48, 48, 48)
      dst = Raylib::Rectangle.create(@sub_panel_x + (134 - 48) / 2, @sub_panel_y + 48, 48, 48)
      Raylib.DrawTexturePro(@mapsprite_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    end

    if actor
      kills = actor["kills"] || 0
      defeats = actor["defeats"] || 0
      text_x = @sub_panel_x + 15
      text_y = @sub_panel_y + 106
      draw_text_custom("KILLS", text_x, text_y, 18, WHITE)
      draw_text_custom([kills, 9999].min.to_s.rjust(4), text_x + 70, text_y, 18, GREEN)
      draw_text_custom("DEFEAT", text_x, text_y + 25, 18, WHITE)
      draw_text_custom([defeats, 9999].min.to_s.rjust(4), text_x + 70, text_y + 25, 18, RED)

      gold_y = text_y + 76
      draw_text_custom("GOLD", text_x + 28, gold_y, 20, WHITE)
      gold_digits = [@gold, 9999999999].min.to_s.chars.join(' ')
      gold_header_width = Raylib.MeasureTextEx(@font, "GOLD", 18, 1).x
      gold_center_x = (text_x + 28) + gold_header_width / 2
      gold_val_width = Raylib.MeasureTextEx(@font, gold_digits, 20, 1).x
      val_x = gold_center_x - gold_val_width / 2
      draw_text_custom(gold_digits, val_x, gold_y + 20, 20, YELLOW)
    end
  end

  # Вспомогательные рисовальные методы
  def draw_text_custom(text, x, y, size, color)
    if @font
      Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
    else
      Raylib.DrawText(text, x, y, size, color)
    end
  end

  def draw_item_name(text, x, y, size, color)
    if text.include?(' ')
      first_word = text[0...text.index(' ')].strip
      second_word = text[text.index(' ') + 1..-1].strip
      draw_text_custom(first_word, x, y, size, color)
      draw_text_custom(second_word, x + 14, y + 15, size, color)
    else
      draw_text_custom(text, x, y, size, color)
    end
  end

  # Загрузка mapsprite
  def load_mapsprite(name)
    return nil unless name
    return @mapsprite_cache[name] if @mapsprite_cache.key?(name)
    path = "assets/mapsprites/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @mapsprite_cache[name] = tex
  end

  # Загрузка иконки
  def load_icon(path)
    return nil unless path && !path.empty?
    return @icon_cache[path] if @icon_cache.key?(path)
    tex = nil
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(tex, 0)
    end
    @icon_cache[path] = tex
  end

  # Поиск иконки заклинания (теперь через @db)
  def find_spell_icon(name, level)
    spells = @db ? @db.spells : nil
    spells ||= begin
      if File.exist?("data/spells/spells.json")
        JSON.parse(File.read("data/spells/spells.json"))["spells"] || []
      else
        []
      end
    end
    spell = spells.find { |s| s["name"].casecmp?(name) && s["level"] == level }
    spell ? spell["icon"] : nil
  end
end

# ============================================
# MagicOverlay
# ============================================
class MagicOverlay
  VISIBLE_ROWS = 5
  attr_reader :current_actor

  def initialize(font = nil, db = nil, start_inventory = nil, party = nil, classes_data = nil, class_names = nil)
    @font = font
    @db = db

    # Общие данные
    @start_inventory = start_inventory
    @party = party
    @classes_data = classes_data
    @class_names = class_names

    # Загрузка по умолчанию, если не переданы
    if @start_inventory.nil?
      @start_inventory = []
      if File.exist?("data/actors/start_inventory.json")
        data = JSON.parse(File.read("data/actors/start_inventory.json"))
        @start_inventory = data["start_inventory"] || []
      end
    end

    if @party.nil?
      @party = []
      if File.exist?("data/actors/actors.json")
        data = JSON.parse(File.read("data/actors/actors.json"))
        @party = data["actors"] || []
      end
    end

    if @classes_data.nil? || @class_names.nil?
      @classes_data = [] if @classes_data.nil?
      @class_names = {} if @class_names.nil?
      if File.exist?("data/actors/classes.json")
        data = JSON.parse(File.read("data/actors/classes.json"))
        @classes_data = data["classes"] || []
        @classes_data.each { |c| @class_names[c["id"]] = c["name"] }
      end
    end

    # Заклинания – берём из базы, если она передана, иначе загружаем
    @all_spells = []
    if @db && @db.spells
      @all_spells = @db.spells
    elsif File.exist?("data/spells/spells.json")
      data = JSON.parse(File.read("data/spells/spells.json"))
      @all_spells = data["spells"] || []
    end

    # ----- Остальная инициализация -----
    @visible = false
    @anim_phase = 0
    @anim_timer = 0
    @ready_to_close = false
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    @portrait_cache = {}

    @upper_target_x = 182
    @upper_target_y = 16
    @lower_target_x = 48
    @lower_target_y = 224
    @portrait_target_x = 48
    @portrait_target_y = 16
    @frame_target_x = 48
    @frame_target_y = 16

    @upper_start_x = 576 + 220
    @lower_start_y = 480 + 220
    @portrait_start_x = -220
    @frame_start_x = -220

    @upper_x = @upper_start_x
    @upper_y = @upper_target_y
    @lower_x = @lower_target_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @portrait_y = @portrait_target_y
    @frame_x = @frame_start_x
    @frame_y = @frame_target_y

    @upper_w = 346
    @upper_h = 208
    @lower_w = 480
    @lower_h = 240
    @portrait_w = 134
    @portrait_h = 208
    @frame_w = 134
    @frame_h = 208

    @selection_blink_timer = 0
    @status_view_mode = 0
    @list_top_index = 0
    @selected_actor_index = 0
    @input_timer_up = 0
    @input_timer_down = 0

    @empty_magic_tex = nil
    @empty_magic_tex_loaded = false
    @icon_cache = {}

    load_textures
  end

  # -----------------------------------------------------------------
  # Вспомогательные методы поиска
  # -----------------------------------------------------------------
  def get_actor_stats(actor_name)
    @party.each { |actor| return actor if actor["name"] == actor_name }
    nil
  end

  def find_actor_items(actor_name)
    actor = @party.find { |a| a["name"] == actor_name }
    return [] unless actor
    entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    entry ? entry["items"] || [] : []
  end

  def find_item_by_name(name)
    @db ? @db.find_by_name(name) : nil
  end

  def calculate_equip_bonuses(actor)
    return { attack: 0, defense: 0 } unless actor
    inv_entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    return { attack: 0, defense: 0 } unless inv_entry

    total_attack = 0
    total_defense = 0
    inv_entry["items"].each do |item_entry|
      next unless item_entry["equipped"]
      next if item_entry["item"] == "NOTHING"
      item_data = find_item_by_name(item_entry["item"])
      next unless item_data
      total_attack += (item_data["attack"] || 0).to_i
      total_defense += (item_data["defense"] || 0).to_i
    end
    { attack: total_attack, defense: total_defense }
  end

  # -----------------------------------------------------------------
  # Загрузка текстур и актёров
  # -----------------------------------------------------------------
  def load_textures
    @upper_tex = Raylib.LoadTexture("assets/ui/upper_panel.png")
    @lower_tex = Raylib.LoadTexture("assets/ui/lower_panel.png")
    @frame_tex = Raylib.LoadTexture("assets/ui/portrait_frame.png")
    @ruby_tex  = Raylib.LoadTexture("assets/ui/ruby_icon.png")
    Raylib.SetTextureFilter(@upper_tex, 0) if @upper_tex
    Raylib.SetTextureFilter(@lower_tex, 0) if @lower_tex
    Raylib.SetTextureFilter(@frame_tex, 0) if @frame_tex
    Raylib.SetTextureFilter(@ruby_tex, 0)  if @ruby_tex

    if File.exist?("assets/items/item_empty.png")
      @empty_magic_tex = Raylib.LoadTexture("assets/items/item_empty.png")
      Raylib.SetTextureFilter(@empty_magic_tex, 0)
    end
  end

  # -----------------------------------------------------------------
  # Отрисовка текста и предметов
  # -----------------------------------------------------------------
  def draw_text_custom(text, x, y, size, color)
    if @font
      Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
    else
      Raylib.DrawText(text, x, y, size, color)
    end
  end

  def draw_text_centered_h(text, cx, y, size, color)
    return unless @font
    vec = Raylib.MeasureTextEx(@font, text, size, 1)
    x = cx - vec.x / 2
    Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(x, y), size, 1, color)
  end

  def draw_item_name(text, x, y, size, color)
    if text.include?(' ')
      first_word = text[0...text.index(' ')].strip
      second_word = text[text.index(' ') + 1..-1].strip
      draw_text_custom(first_word, x, y, size, color)
      draw_text_custom(second_word, x + 14, y + 15, size, color)
    else
      draw_text_custom(text, x, y, size, color)
    end
  end

  def load_portrait(name)
    return nil unless name
    return @portrait_cache[name] if @portrait_cache.key?(name)
    path = "assets/ui/portraits/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @portrait_cache[name] = tex
  end

  def load_blink_portrait(name)
    return nil unless name
    cache_key = "#{name}_blink"
    return @portrait_cache[cache_key] if @portrait_cache.key?(cache_key)
    path = "assets/ui/portraits/#{name}_blink.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @portrait_cache[cache_key] = tex
  end

  # -----------------------------------------------------------------
  # Смена выбранного персонажа
  # -----------------------------------------------------------------
  def change_selected_actor(delta)
    return unless @party.any?
    new_index = @selected_actor_index + delta
    return if new_index < 0 || new_index >= @party.length

    @selected_actor_index = new_index
    if @selected_actor_index < @list_top_index
      @list_top_index = @selected_actor_index
    elsif @selected_actor_index >= @list_top_index + VISIBLE_ROWS
      @list_top_index = @selected_actor_index - VISIBLE_ROWS + 1
    end

    @current_actor = @party[@selected_actor_index]["name"]
    @current_items = find_actor_items(@current_actor)
    actor = @party[@selected_actor_index]
    portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
    @portrait_tex = load_portrait(portrait_name)
    @blink_tex = load_blink_portrait(portrait_name)

    if actor
      klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
      spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
      @current_spells = spell_list.select { |spell| spell["level"] <= actor["level"] }
    end
  end

  # -----------------------------------------------------------------
  # Анимация открытия / закрытия
  # -----------------------------------------------------------------
  def open(player = nil)
    return if @visible
    @visible = true
    @anim_phase = 1
    @ready_to_close = false

    @upper_x = @upper_start_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x

    if @party.any?
      @selected_actor_index = @selected_actor_index.clamp(0, @party.length - 1)
      if @selected_actor_index < @list_top_index
        @list_top_index = @selected_actor_index
      elsif @selected_actor_index >= @list_top_index + VISIBLE_ROWS
        @list_top_index = @selected_actor_index - VISIBLE_ROWS + 1
      end
      @current_actor = @party[@selected_actor_index]["name"]
      actor = @party.find { |a| a["name"] == @current_actor }
      if actor
        klass = @classes_data.find { |c| c["id"] == actor["class_id"] }
        spell_list = (klass && klass["spell_list"]) ? klass["spell_list"] : []
        @current_spells = spell_list.select { |spell| spell["level"] <= actor["level"] }
      else
        @current_spells = []
      end
      @current_items = find_actor_items(@current_actor)
      portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
      @portrait_tex = load_portrait(portrait_name)
      @blink_tex = load_blink_portrait(portrait_name)
    else
      @current_actor = nil
      @current_spells = []
      @current_items = []
    end
    @blink_timer = 0
    @blink_duration = 0
    @selection_blink_timer = 0
  end

  def close
    return unless @visible && @anim_phase == 2
    @anim_phase = 3
  end

  def force_close
    @visible = false
    @anim_phase = 0
  end

  def handle_input
    return unless @visible && @anim_phase == 2

    if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
      close
      return
    end
    if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
      @status_view_mode = 1 - @status_view_mode
    end
    if Raylib.IsKeyDown(Raylib::KEY_UP)
      @input_timer_up += 1
      if @input_timer_up == 1 || (@input_timer_up > 20 && (@input_timer_up - 20) % 5 == 0)
        change_selected_actor(-1)
      end
    else
      @input_timer_up = 0
    end
    if Raylib.IsKeyDown(Raylib::KEY_DOWN)
      @input_timer_down += 1
      if @input_timer_down == 1 || (@input_timer_down > 20 && (@input_timer_down - 20) % 5 == 0)
        change_selected_actor(1)
      end
    else
      @input_timer_down = 0
    end
  end

  def update
    return unless @visible
    speed = 38

    case @anim_phase
    when 1
      @portrait_x += speed; @portrait_x = @portrait_target_x if @portrait_x > @portrait_target_x
      @frame_x += speed; @frame_x = @frame_target_x if @frame_x > @frame_target_x
      @upper_x -= speed; @upper_x = @upper_target_x if @upper_x < @upper_target_x
      @lower_y -= speed; @lower_y = @lower_target_y if @lower_y < @lower_target_y
      if @portrait_x >= @portrait_target_x && @frame_x >= @frame_target_x &&
         @upper_x <= @upper_target_x && @lower_y <= @lower_target_y
        @anim_phase = 2
      end
    when 3
      @portrait_x -= speed; @portrait_x = @portrait_start_x if @portrait_x < @portrait_start_x
      @frame_x -= speed; @frame_x = @frame_start_x if @frame_x < @frame_start_x
      @upper_x += speed; @upper_x = @upper_start_x if @upper_x > @upper_start_x
      @lower_y += speed; @lower_y = @lower_start_y if @lower_y > @lower_start_y
      if @portrait_x <= @portrait_start_x
        @visible = false
        @anim_phase = 0
      end
    end

    if @anim_phase == 2
      @blink_timer += 1
      if @blink_duration > 0
        @blink_duration -= 1
      elsif @blink_timer >= @blink_interval
        @blink_duration = 8
        @blink_timer = 0
        @blink_interval = 100 + rand(50)
      end
      @selection_blink_timer += 1
    end
  end

  # -----------------------------------------------------------------
  # Отрисовка
  # -----------------------------------------------------------------
  def draw
    return unless @visible
    origin = Raylib::Vector2.create(0, 0)

    # Нижняя панель
    dst = Raylib::Rectangle.create(@lower_x, @lower_y, @lower_w, @lower_h)
    Raylib.DrawTexturePro(@lower_tex, Raylib::Rectangle.create(0,0,@lower_w,@lower_h), dst, origin, 0, Raylib::WHITE)

    # Верхняя панель
    dst = Raylib::Rectangle.create(@upper_x, @upper_y, @upper_w, @upper_h)
    Raylib.DrawTexturePro(@upper_tex, Raylib::Rectangle.create(0,0,@upper_w,@upper_h), dst, origin, 0, Raylib::WHITE)

    # Портрет
    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, 134, 208)
      Raylib.DrawTexturePro(portrait, Raylib::Rectangle.create(0,0,134,208), dst, origin, 0, Raylib::WHITE)
    end

    # Рамка
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, Raylib::Rectangle.create(0,0,@frame_w,@frame_h), dst, origin, 0, Raylib::WHITE)

    # ===== ВЕРХНЯЯ ПАНЕЛЬ: МАГИЯ =====
    actor_data = @party.find { |a| a["name"] == @current_actor }
    if actor_data
      class_id   = actor_data["class_id"]
      class_name = @class_names[class_id] || "???"
      level      = actor_data["level"]
      header     = "#{actor_data["name"]}  #{class_name}  LV #{level}"
      draw_text_custom(header, @upper_x + 25, @upper_y + 12, 20, WHITE)
    else
      draw_text_custom("NO DATA", @upper_x + 25, @upper_y + 12, 20, WHITE)
    end

    draw_text_custom("-- MAGIC --", @upper_x + 47, @upper_y + 35, 20, WHITE)

    # Крест иконок
    base_x = @upper_x + 40
    base_y = @upper_y + 60
    offset_x = 44
    offset_y = 42

    icon_positions = [
      { x: base_x + offset_x, y: base_y + 8 },               # верхняя
      { x: base_x,            y: base_y + offset_y },         # левая
      { x: base_x + offset_x * 2, y: base_y + offset_y },    # правая
      { x: base_x + offset_x, y: base_y + offset_y * 2 - 8 } # нижняя
    ]

    text_x = @upper_x + 195
    text_y = @upper_y + 48
    text_line_h = 36

    spells = @current_spells ? @current_spells.first(4) : []

    # Загрузка пустой текстуры (один раз)
    unless @empty_magic_tex_loaded
      if File.exist?("assets/spells/magic_empty.png")
        @empty_magic_tex = Raylib.LoadTexture("assets/spells/magic_empty.png")
        Raylib.SetTextureFilter(@empty_magic_tex, 0) if @empty_magic_tex
      end
      @empty_magic_tex_loaded = true
    end

    # Рисуем иконки
    4.times do |i|
      ipos = icon_positions[i]
      spell = spells[i]

      if spell
        icon = load_icon(find_spell_icon(spell["spell"], spell["spell_level"]))
        if icon
          src = Raylib::Rectangle.create(0, 0, 32, 48)
          dst = Raylib::Rectangle.create(ipos[:x], ipos[:y], 32, 48)
          Raylib.DrawTexturePro(icon, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
        end
      else
        if @empty_magic_tex
          dst = Raylib::Rectangle.create(ipos[:x], ipos[:y], 32, 48)
          Raylib.DrawTexturePro(@empty_magic_tex, Raylib::Rectangle.create(0,0,32,48), dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
        else
          Raylib.DrawRectangle(ipos[:x], ipos[:y], 32, 48, Raylib::GRAY)
        end
      end
    end

    # Текст столбиком (или Nothing, если заклинаний нет)
    if spells.any?
      4.times do |i|
        spell = spells[i]
        next unless spell
        y = text_y + i * text_line_h
        draw_text_custom(spell["spell"], text_x, y, 20, PURPLE)
        draw_text_custom("level #{spell["spell_level"]}", text_x + 14, y + 18, 20, LIME)
      end
    else
      draw_text_custom("Nothing", text_x, text_y, 20, ORANGE)
    end

    # ===== НИЖНЯЯ ПАНЕЛЬ: ЗАГОЛОВКИ =====
    header_y = @lower_y + 28
    if @status_view_mode == 0
      draw_text_custom("Имя",    @lower_x + 44,  header_y, 20, WHITE)
      draw_text_custom("Класс",  @lower_x + 187, header_y, 20, WHITE)
      level_header_center_x = @lower_x + 290 + Raylib.MeasureTextEx(@font, "Уровень", 20, 1).x / 2
      exp_header_center_x   = @lower_x + 395 + Raylib.MeasureTextEx(@font, "Опыт", 20, 1).x / 2
      draw_text_custom("Уровень", @lower_x + 290, header_y, 20, WHITE)
      draw_text_custom("Опыт",    @lower_x + 395, header_y, 20, WHITE)
    else
      draw_text_custom("Имя", @lower_x + 44, header_y, 20, WHITE)
      stat_headers = ["HP", "MP", "AT", "DF", "AGI", "MV"]
      stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]
      stat_headers.each_with_index do |head, idx|
        cx = stat_centers[idx]
        w = Raylib.MeasureTextEx(@font, head, 20, 1).x
        draw_text_custom(head, cx - w / 2, header_y, 20, WHITE)
      end
    end

    # Список персонажей
    VISIBLE_ROWS.times do |i|
      list_index = @list_top_index + i
      break if list_index >= @party.length
      member = @party[list_index]
      y = @lower_y + 71 + i * 34

      if member["name"] == @current_actor
        pulse = Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6
        alpha = (pulse * 255).to_i
        highlight = Raylib.Fade(Raylib::BLUE, alpha / 255.0)
        Raylib.DrawRectangle(@lower_x + 38, y - 4, 138, 28, highlight)
      end

      if @ruby_tex
        ruby_src = Raylib::Rectangle.create(0, 0, @ruby_tex.width, @ruby_tex.height)
        ruby_dst = Raylib::Rectangle.create(@lower_x + 15, y - 3, 24, 24)
        Raylib.DrawTexturePro(@ruby_tex, ruby_src, ruby_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
      end

      if i == 0 && @list_top_index > 0
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        ax = @lower_x + 27; ay = y + 12
        Raylib.DrawTriangle(Raylib::Vector2.create(ax, ay - 6),
                            Raylib::Vector2.create(ax - 6, ay + 4),
                            Raylib::Vector2.create(ax + 6, ay + 4), color)
      end
      if i == VISIBLE_ROWS - 1 && @list_top_index + VISIBLE_ROWS < @party.length
        ax = @lower_x + 27; ay = y + 12
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        Raylib.DrawTriangle(Raylib::Vector2.create(ax - 6, ay - 4),
                            Raylib::Vector2.create(ax, ay + 6),
                            Raylib::Vector2.create(ax + 6, ay - 4), color)
      end

      name_display = member["name"].slice(0, 10)
      draw_text_custom(name_display, @lower_x + 44, y, 18, WHITE)

      if @status_view_mode == 0
        class_name = @class_names[member["class_id"]] || "???"
        class_display = class_name.slice(0, 10)
        draw_text_custom(class_display, @lower_x + 187, y, 18, WHITE)
        draw_text_centered_h(member["level"].to_s, level_header_center_x, y, 18, LIME)
        draw_text_centered_h(member["exp"].to_s,    exp_header_center_x,   y, 18, LIME)
      else
        klass = @classes_data.find { |c| c["id"] == member["class_id"] }
        if klass && @db
          lv = [(member["level"] || 1), 1].max
          hp_val  = [@db.stat_at_level(klass["hp_growth"],  lv), 0].max
          mp_val  = [@db.stat_at_level(klass["mp_growth"],  lv), 0].max
          atk_val = [@db.stat_at_level(klass["attack_growth"], lv), 0].max
          def_val = [@db.stat_at_level(klass["defense_growth"], lv), 0].max
          agi_val = [@db.stat_at_level(klass["agility_growth"], lv), 0].max
          mov_val = [(klass["move"] || 0), 0].max
        else
          hp_val = mp_val = atk_val = def_val = agi_val = mov_val = 0
        end

        bonuses = calculate_equip_bonuses(member)
        eff_atk = [atk_val + bonuses[:attack], 0].max
        eff_def = [def_val + bonuses[:defense], 0].max
		
        stat_values = [hp_val, mp_val, eff_atk, eff_def, agi_val, mov_val]
        stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]
        stat_values.each_with_index do |val, idx|
          draw_text_centered_h(val.to_s, stat_centers[idx], y, 18, LIME)
        end
      end
    end
  end

  # -----------------------------------------------------------------
  # Загрузка иконок и поиск иконок заклинаний
  # -----------------------------------------------------------------
  def load_icon(path)
    return nil unless path && !path.empty?
    return @icon_cache[path] if @icon_cache.key?(path)
    tex = nil
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(tex, 0)
    end
    @icon_cache[path] = tex
    tex
  end

  def find_spell_icon(name, level)
    spells = @db ? @db.spells : nil
    spells ||= begin
      if File.exist?("data/spells/spells.json")
        JSON.parse(File.read("data/spells/spells.json"))["spells"] || []
      else
        []
      end
    end
    spell = spells.find { |s| s["name"].casecmp?(name) && s["level"] == level }
    spell ? spell["icon"] : nil
  end
end
# ============================================
# SearchOverlay (диалоговое окно для плитки "Поиск местности")
# ============================================
class SearchOverlay
  def initialize(large_font = nil, game_text = nil, party = nil)
    @large_font = large_font
    @game_text = game_text
    @party = party
    @visible = false
    @message_panel_tex = nil
    load_message_texture
  end

  def load_message_texture
    path = "assets/ui/message_panel.png"
    if File.exist?(path)
      @message_panel_tex = Raylib.LoadTexture(path)
      Raylib.SetTextureFilter(@message_panel_tex, 0) if @message_panel_tex
    end
  end

  def open
    @visible = true
  end

  def close
    @visible = false
  end

  def visible
    @visible
  end

  def handle_input
    return unless @visible
    if Raylib.IsKeyPressed(Raylib::KEY_A) || 
       Raylib.IsKeyPressed(Raylib::KEY_D) ||
       Raylib.IsKeyPressed(Raylib::KEY_S)
      close
    end
  end

  def update
    # nothing to update
  end

  def draw
  return unless @visible
  template = @game_text ? (@game_text["0005"] || "Ничего интересного не найдено.") : "Ничего интересного не найдено."

  # Подстановка имени лидера
  if @party && @party[0]
    leader_name = @party[0]["name"] || "???"
    template = template.gsub("{LEADER}", leader_name)
  end

  lines = template.split("{N}")

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
  lines.each do |line_text|
    Raylib.DrawTextEx(@large_font, line_text, Raylib::Vector2.create(panel_x + 20, y_offset), 30, 1, Raylib::WHITE)
    y_offset += 38
  end
end
end