# lib/enemy_profile.rb

class EnemyProfile
  def initialize(font = nil, db = nil)
    @font = font
    @db = db

    @visible = false
    @anim_phase = 0
    @ready_to_close = false
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    @portrait_cache = {}
    @icon_cache = {}
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
    @sub_panel_h = 168

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
    @sub_panel_tex   = Raylib.LoadTexture("assets/ui/sub_panel_enemy.png")
    @frame_tex       = Raylib.LoadTexture("assets/ui/portrait_frame.png")
    Raylib.SetTextureFilter(@right_panel_tex, 0) if @right_panel_tex
    Raylib.SetTextureFilter(@sub_panel_tex, 0)   if @sub_panel_tex
    Raylib.SetTextureFilter(@frame_tex, 0)       if @frame_tex
  end

  def open(enemy_data, portrait_cache = {})
    return if @visible
    @visible = true
    @anim_phase = 1
    @ready_to_close = false
    @enemy = enemy_data
    @portrait_cache = portrait_cache

    portrait_name = @enemy["portrait"] || @enemy["name"]
    @portrait_tex = load_portrait(portrait_name)
    @blink_tex = load_blink_portrait(portrait_name)

    @blink_timer = 0
    @blink_duration = 0
    @sprite_timer = 0
    @sprite_frame = 0
    mapsprite_name = @enemy["mapsprite"]
    @mapsprite_tex = mapsprite_name ? load_mapsprite_enemy(mapsprite_name) : nil

    @right_panel_x = @right_panel_start_x
    @sub_panel_y = @sub_panel_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x
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

  def load_mapsprite_enemy(name)
    return nil unless name
    cache_key = "enemy_map_#{name}"
    return @mapsprite_cache[cache_key] if @mapsprite_cache && @mapsprite_cache.key?(cache_key)
    @mapsprite_cache ||= {}
    path = "assets/mapsprites_enemy/#{name}.png"
    return nil unless File.exist?(path)
    img = Raylib.LoadImage(path)
    tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(tex, 0)
    @mapsprite_cache[cache_key] = tex
  end

  def find_item_by_name(name)
    @db ? @db.find_by_name(name) : nil
  end

  def calculate_equip_bonuses
    return { attack: 0, defense: 0, hp: 0, mp: 0, agility: 0, mov: 0 } unless @enemy
    items = @enemy["items"] || []
    total_attack = 0
    total_defense = 0
    total_hp = 0
    total_mp = 0
    total_agility = 0
    total_mov = 0

    items.each do |item_entry|
      next unless item_entry["equipped"]
      next if item_entry["item"] == "Nothing"
      item_data = find_item_by_name(item_entry["item"])
      next unless item_data
      total_attack  += (item_data["attack"]  || 0).to_i
      total_defense += (item_data["defense"] || 0).to_i
      total_hp      += (item_data["hp"]      || 0).to_i
      total_mp      += (item_data["mp"]      || 0).to_i
      total_agility += (item_data["agility"] || 0).to_i
      total_mov     += (item_data["mov"]     || 0).to_i
    end
    { attack: total_attack, defense: total_defense,
      hp: total_hp, mp: total_mp, agility: total_agility, mov: total_mov }
  end

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

  def find_spell_icon(name, level)
    spells = @db ? @db.spells : nil
    spells ||= []
    spell = spells.find { |s| s["name"].casecmp?(name) && s["level"] == level }
    spell ? spell["icon"] : nil
  end

  def update
    return unless @visible
    speed = 38

    case @anim_phase
    when 1  # открытие
      @portrait_x += speed; @portrait_x = @portrait_target_x if @portrait_x > @portrait_target_x
      @frame_x += speed; @frame_x = @frame_target_x if @frame_x > @frame_target_x
      @right_panel_x -= speed; @right_panel_x = @right_panel_target_x if @right_panel_x < @right_panel_target_x
      @sub_panel_y -= speed; @sub_panel_y = @sub_panel_target_y if @sub_panel_y < @sub_panel_target_y

      if @portrait_x >= @portrait_target_x && @frame_x >= @frame_target_x &&
         @right_panel_x <= @right_panel_target_x && @sub_panel_y <= @sub_panel_target_y
        @anim_phase = 2
      end

    when 3  # закрытие
      @portrait_x -= speed; @portrait_x = @portrait_start_x if @portrait_x < @portrait_start_x
      @frame_x -= speed; @frame_x = @frame_start_x if @frame_x < @frame_start_x
      @right_panel_x += speed; @right_panel_x = @right_panel_start_x if @right_panel_x > @right_panel_start_x
      @sub_panel_y += speed; @sub_panel_y = @sub_panel_start_y if @sub_panel_y > @sub_panel_start_y

      if @portrait_x <= @portrait_start_x
        @visible = false
        @anim_phase = 0
        @ready_to_close = true
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

  def draw
    return unless @visible
    origin = Raylib::Vector2.create(0, 0)

    # Правая панель
    dst = Raylib::Rectangle.create(@right_panel_x, @right_panel_y, @right_panel_w, @right_panel_h)
    Raylib.DrawTexturePro(@right_panel_tex, Raylib::Rectangle.create(0, 0, @right_panel_w, @right_panel_h), dst, origin, 0, Raylib::WHITE)

    # Нижняя подпанель (под портретом)
    dst = Raylib::Rectangle.create(@sub_panel_x, @sub_panel_y, @sub_panel_w, @sub_panel_h)
    Raylib.DrawTexturePro(@sub_panel_tex, Raylib::Rectangle.create(0, 0, @sub_panel_w, @sub_panel_h), dst, origin, 0, Raylib::WHITE)

    # Портрет
    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, 134, 208)
      Raylib.DrawTexturePro(portrait, Raylib::Rectangle.create(0, 0, 134, 208), dst, origin, 0, Raylib::WHITE)
    end

    # Рамка портрета
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, 134, 208)
    Raylib.DrawTexturePro(@frame_tex, Raylib::Rectangle.create(0, 0, 134, 208), dst, origin, 0, Raylib::WHITE)

    return unless @enemy

    name = @enemy["name"] || "???"
    race = @enemy["race"] || "???"
    status = @enemy["status"] || "???"
    move_type = @enemy["move_type"] || "???"
    level = @enemy["level"] || 1

    # Заголовок: имя и раса
    header_y = @right_panel_y + 12
    draw_text_custom(name, @right_panel_x + 25, header_y, 20, WHITE)
    race_x = @right_panel_x + 25 + Raylib.MeasureTextEx(@font, name + "  ", 20, 1).x
    draw_text_custom(race, race_x, header_y, 20, GOLD)

    # Статы врага
    stats = @enemy["stats"] || {}
    base_hp  = stats["max_hp"] || 0
    base_mp  = stats["max_mp"] || 0
    base_atk = stats["base_att"] || 0
    base_def = stats["base_def"] || 0
    base_agi = stats["base_agi"] || 0
    base_mov = stats["base_mov"] || 0

    bonuses = calculate_equip_bonuses
    eff_hp  = [base_hp  + bonuses[:hp],      0].max
    eff_mp  = [base_mp  + bonuses[:mp],      0].max
    eff_atk = [base_atk + bonuses[:attack],  0].max
    eff_def = [base_def + bonuses[:defense], 0].max
    eff_agi = [base_agi + bonuses[:agility], 0].max
    eff_mov = [base_mov + bonuses[:mov],     0].max

    left_x = @right_panel_x + 25
    right_x = @right_panel_x + 130
    y_base = @right_panel_y + 55
    line_h = 28

    # Левый столбец: LV, HP, MP, ??? (вместо EXP)
    draw_text_custom("LV    #{level}", left_x, y_base, 20, WHITE)
    draw_text_custom("HP    #{eff_hp}", left_x, y_base + line_h, 20, WHITE)
    draw_text_custom("MP    #{eff_mp}", left_x, y_base + line_h * 2, 20, WHITE)
    draw_text_custom("EXP   ???", left_x, y_base + line_h * 3, 20, WHITE)

    # Правый столбец: ATT, DEF, AGI, MOV
    draw_text_custom("ATT   #{eff_atk}", right_x, y_base, 20, WHITE)
    draw_text_custom("DEF   #{eff_def}", right_x, y_base + line_h, 20, WHITE)
    draw_text_custom("AGI   #{eff_agi}", right_x, y_base + line_h * 2, 20, WHITE)
    draw_text_custom("MOV   #{eff_mov}", right_x, y_base + line_h * 3, 20, WHITE)

    # Дополнительно: RACE, STATUS, MOVE TYPE
    extra_x = right_x + 100
    small_line = 20
    draw_text_custom("RACE", extra_x, y_base, 20, GOLD)
    draw_text_custom(race, extra_x, y_base + small_line, 15, WHITE)
    draw_text_custom("STATUS", extra_x, y_base + small_line * 2 + 4, 20, GOLD)
    draw_text_custom(status, extra_x, y_base + small_line * 3 + 4, 15, WHITE)
    draw_text_custom("MOVE TYPE", extra_x, y_base + small_line * 4 + 8, 20, GOLD)
    draw_text_custom(move_type, extra_x, y_base + small_line * 5 + 8, 15, WHITE)

    # Магия и Предметы
    spells_list = @enemy["spells"] || []
    spell_levels = @enemy["spell_levels"] || []
    valid_spells = spells_list.each_with_index.reject { |name, i| name == "Nothing" }.map { |name, i| { "spell" => name, "level" => spell_levels[i] || 1 } }
    items = @enemy["items"] || []

    magic_x = @right_panel_x + 55
    items_x = @right_panel_x + 210

    section_y = @right_panel_y + 200
    draw_text_custom("Magic", magic_x - 30, section_y, 20, WHITE)
    draw_text_custom("Items", items_x - 50, section_y, 20, WHITE)

    # Магия
    if valid_spells.any?
      valid_spells.first(4).each_with_index do |spell, i|
        y = section_y + 30 + i * 52
        icon = load_icon(find_spell_icon(spell["spell"], spell["level"]))
        if icon
          src = Raylib::Rectangle.create(0, 0, 32, 48)
          dst = Raylib::Rectangle.create(magic_x - 30, y, 32, 48)
          Raylib.DrawTexturePro(icon, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
        end
        draw_text_custom(spell["spell"], magic_x + 10, y + 6, 18, WHITE)
        draw_text_custom("Lv #{spell["level"]}", magic_x + 10, y + 26, 18, WHITE)
      end
    else
      draw_text_custom("Nothing", magic_x, section_y + 30, 18, ORANGE)
    end

    # Предметы
    has_any_item = items.any? { |entry| entry["item"] != "Nothing" }
    if has_any_item
      items.first(4).each_with_index do |item_entry, i|
        next if item_entry["item"] == "Nothing"
        y = section_y + 30 + i * 52
        item_data = find_item_by_name(item_entry["item"])
        item_icon = item_data ? load_icon(item_data["icon"]) : nil
        if item_icon
          src = Raylib::Rectangle.create(0, 0, 32, 48)
          dst = Raylib::Rectangle.create(items_x - 50, y, 32, 48)
          Raylib.DrawTexturePro(item_icon, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
        end
        name = item_entry["item"]
        if item_entry["equipped"]
          draw_text_custom("E", items_x - 15, y + 12, 18, YELLOW)
        end
        draw_item_name(name, items_x + 5, y + 12, 18, WHITE)
      end
    else
      draw_text_custom("Nothing", items_x - 50, section_y + 30, 18, ORANGE)
    end

    # Анимированный спрайт
    if @mapsprite_tex
      src = Raylib::Rectangle.create(@sprite_frame * 48, 2 * 48, 48, 48)
      dst = Raylib::Rectangle.create(@sub_panel_x + (134 - 48) / 2, @sub_panel_y + 48, 48, 48)
      Raylib.DrawTexturePro(@mapsprite_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    end

    # KILLS / DEFEATS → ???
    text_x = @sub_panel_x + 15
    text_y = @sub_panel_y + 106
    draw_text_custom("KILLS", text_x, text_y, 18, WHITE)
    draw_text_custom("???", text_x + 70, text_y, 18, GREEN)
    draw_text_custom("DEFEAT", text_x, text_y + 25, 18, WHITE)
    draw_text_custom("???", text_x + 70, text_y + 25, 18, RED)
  end

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
end