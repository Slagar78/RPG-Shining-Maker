# lib/battleManager/battle_give.rb
module BattleGive
  # ---------- переменные анимации ----------
  attr_accessor :give_anim_active, :give_anim_timer,
                :give_anim_start_x, :give_anim_start_y,
                :give_anim_end_x, :give_anim_end_y,
                :give_anim_item_tex, :give_anim_donor_items,
                :give_anim_target_items, :give_anim_donor_slot,
                :give_anim_target_slot,
                :give_anim_message_id, :give_anim_message_params,
                :item_icon_cache,
                :give_anim_swap_item_tex, :give_anim_swap_donor_slot, :give_anim_swap_target_slot

  def init_give_animation_vars
    @give_anim_active = false
    @give_anim_timer = 0
    @give_anim_start_x = 0.0
    @give_anim_start_y = 0.0
    @give_anim_end_x = 0.0
    @give_anim_end_y = 0.0
    @give_anim_item_tex = nil
    @give_anim_donor_items = []
    @give_anim_target_items = []
    @give_anim_donor_slot = 0
    @give_anim_target_slot = 0
    @give_anim_message_id = nil
    @give_anim_message_params = {}
    @item_icon_cache = {}

    @give_anim_swap_item_tex = nil
    @give_anim_swap_donor_slot = -1
    @give_anim_swap_target_slot = -1

    # Загружаем или создаём текстуру пустого слота
    path = "assets/items/item_empty.png"
    if File.exist?(path)
      img = LoadImage(path)
      @empty_item_tex = LoadTextureFromImage(img)
      UnloadImage(img)
      SetTextureFilter(@empty_item_tex, TEXTURE_FILTER_POINT)
    else
      # Программно создаём текстуру 32x48 (тёмный фон, рамка)
      img = GenImageColor(32, 48, DARKGRAY)
      ImageDrawRectangleLines(img, Rectangle.create(0, 0, 32, 48), GRAY)
      @empty_item_tex = LoadTextureFromImage(img)
      UnloadImage(img)
      SetTextureFilter(@empty_item_tex, TEXTURE_FILTER_POINT)
    end
  end

  # Поиск соседних союзников
  def adjacent_allies(unit)
    ux, uy = unit[:x], unit[:y]
    @allies.select do |a|
      ax, ay = a[:x], a[:y]
      dist = (ax - ux).abs + (ay - uy).abs
      alive = a[:hp] > 0
      dist <= 1 && alive && a != unit
    end
  end

  # Начать передачу выбранному союзнику (изменённая версия)
  def execute_give_to(target_unit)
    return unless @pending_give_item && target_unit
    donor = @current_unit
    donor_actor = donor[:actor]
    target_actor = target_unit[:actor]
    return unless donor_actor && target_actor

    donor_entry = @start_inventory.find { |inv| inv["actor_id"] == donor_actor["id"] }
    target_entry = @start_inventory.find { |inv| inv["actor_id"] == target_actor["id"] }
    return unless donor_entry && target_entry

    donor_items = donor_entry["items"]
    target_items = target_entry["items"]

    idx = donor_items.index { |entry| entry["item"] == @pending_give_item["item"] }
    return unless idx

    free_slot = target_items.index { |entry| entry["item"] == "NOTHING" }

    if free_slot
      # Простая передача
      item_to_give = donor_items[idx].dup
      item_to_give["equipped"] = false
      donor_items[idx] = { "item" => "NOTHING", "equipped" => false }
      target_items[free_slot] = item_to_give
      # Запуск анимации
      start_give_animation(donor, target_unit, @pending_give_item["item"],
                           idx, free_slot, "0001", {
        '{DONOR}' => donor_actor["name"],
        '{ITEM}' => @pending_give_item["item"],
        '{TARGET}' => target_actor["name"]
      })
    else
      @give_swap_target_unit = target_unit
      target_items_list = target_items.map do |item_entry|
        item_name = item_entry["item"]
        next nil if item_name == "NOTHING"
        item_data = @db.find_by_name(item_name)
        icon = item_data ? item_data["icon"] : nil
        { "item" => item_name, "icon" => icon }
      end.compact
      @battle_menu.open_item_grid(:give_swap, target_items_list)
      @battle_state = :item_grid_select
      @target_highlight = nil
      @give_targets = []
    end
  end

  # Обмен предметами при заполненном инвентаре цели
  def perform_give_swap(chosen_target_item)
    donor = @current_unit
    target = @give_swap_target_unit
    return unless donor && target && @pending_give_item
    donor_actor = donor[:actor]
    target_actor = target[:actor]
    return unless donor_actor && target_actor

    donor_entry = @start_inventory.find { |inv| inv["actor_id"] == donor_actor["id"] }
    target_entry = @start_inventory.find { |inv| inv["actor_id"] == target_actor["id"] }
    return unless donor_entry && target_entry

    donor_items = donor_entry["items"]
    target_items = target_entry["items"]

    donor_idx = donor_items.index { |e| e["item"] == @pending_give_item["item"] }
    target_idx = target_items.index { |e| e["item"] == chosen_target_item["item"] }
    return unless donor_idx && target_idx

    donor_items[donor_idx], target_items[target_idx] = target_items[target_idx], donor_items[donor_idx]
    donor_items[donor_idx]["equipped"] = false
    target_items[target_idx]["equipped"] = false

    # Запуск анимации обмена
    donor_item_name = @pending_give_item["item"]
    target_item_name = chosen_target_item["item"]
    start_give_animation(donor, target, donor_item_name,
                         donor_idx, target_idx, "0002", {
      '{DONOR}' => donor_actor["name"],
      '{ITEM}' => donor_item_name,
      '{RECEIVED_ITEM}' => target_item_name,
      '{TARGET}' => target_actor["name"]
    }, target_item_name, donor_idx, target_idx)
    @pending_give_item = nil
    @give_targets = []
    @target_highlight = nil
    @give_swap_target_unit = nil
  end

  # Завершить ход после передачи
  def finish_give
    @pending_give_item = nil
    @give_targets = []
    @target_highlight = nil
    @give_swap_target_unit = nil
    end_current_turn
  end

  # Обработка ввода в состоянии выбора цели для передачи
  def handle_give_targeting_input
    return unless @give_targets.any?
    if IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)
      @give_target_index = (@give_target_index - 1) % @give_targets.size
      @target_highlight = @give_targets[@give_target_index]
      @audio.play_sfx("cursor") if @audio
    elsif IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)
      @give_target_index = (@give_target_index + 1) % @give_targets.size
      @target_highlight = @give_targets[@give_target_index]
      @audio.play_sfx("cursor") if @audio
    elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      execute_give_to(@give_targets[@give_target_index])
    elsif IsKeyPressed(KEY_S)
      @target_highlight = nil
      @give_targets = []
      @pending_give_item = nil
      @highlight_tiles = @saved_highlight_tiles.dup
      @battle_menu.open_item_menu
      @battle_state = :item_select
      @audio.play_sfx("cancel_menu") if @audio
    end
  end

  # Начало сообщения с посимвольным выводом
  def start_give_message(id, params)
    @give_message_id = id
    @give_message_params = params
    @battle_state = :give_message
    @highlight_tiles = []
    @target_highlight = nil
    @give_targets = []

    template = @game_text[id] || ""
    text = template.dup
    params.each { |key, val| text.gsub!(key, val.to_s) }
    @give_msg_full_lines = text.split('{N}')
    @give_msg_char_index = 0
    @give_msg_char_timer = 0
    @give_msg_char_speed = 3
    @give_msg_finished = false
  end

  # Обновление анимации печати символов
  def update_give_message
    return if @give_msg_finished
    @give_msg_char_timer += 1
    if @give_msg_char_timer >= @give_msg_char_speed
      @give_msg_char_timer = 0
      total_chars = @give_msg_full_lines.sum(&:length)
      @give_msg_char_index += 1
      if @give_msg_char_index >= total_chars
        @give_msg_char_index = total_chars
        @give_msg_finished = true
      end
    end
  end

  # Ввод в режиме сообщения: пропуск печати или завершение
  def handle_give_message_input
    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      if @give_msg_finished
        finish_give
      else
        total_chars = @give_msg_full_lines.sum(&:length)
        @give_msg_char_index = total_chars
        @give_msg_finished = true
      end
    end
  end

  # Отрисовка панели с печатаемым текстом
  def draw_give_message
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

    remaining = @give_msg_char_index
    y_offset = panel_y + 10
    font = @battle_scene.message_font || @font

    @give_msg_full_lines.each do |line|
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

  # ---------- новые методы анимации ----------
  def get_item_texture(item_name)
    return @empty_item_tex if item_name.nil? || item_name == "NOTHING"
    return @item_icon_cache[item_name] if @item_icon_cache.key?(item_name)
    item_data = @db.find_by_name(item_name)
    path = item_data ? item_data["icon"] : nil
    tex = nil
    if path && File.exist?(path)
      img = LoadImage(path)
      tex = LoadTextureFromImage(img)
      UnloadImage(img)
      SetTextureFilter(tex, TEXTURE_FILTER_POINT)
    end
    @item_icon_cache[item_name] = tex
  end

  def actor_items_array(unit)
    actor = unit[:actor]
    return [] unless actor
    entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    return [] unless entry
    entry["items"].map do |e|
      name = e["item"]
      icon_path = nil
      if name != "NOTHING"
        data = @db.find_by_name(name)
        icon_path = data ? data["icon"] : nil
      end
      { "item" => name, "icon" => icon_path }
    end
  end

  def start_give_animation(donor, target, item_name, donor_slot, target_slot, msg_id, msg_params,
                           swap_item_name = nil, swap_donor_slot = -1, swap_target_slot = -1)
    @give_anim_active = true
    @give_anim_timer = 0
    @give_anim_donor_slot = donor_slot
    @give_anim_target_slot = target_slot
    @give_anim_item_tex = get_item_texture(item_name)
    @give_anim_donor_items = actor_items_array(donor)
    @give_anim_target_items = actor_items_array(target)
    @give_anim_message_id = msg_id
    @give_anim_message_params = msg_params

    if swap_item_name
      @give_anim_swap_item_tex = get_item_texture(swap_item_name)
      @give_anim_swap_donor_slot = swap_donor_slot
      @give_anim_swap_target_slot = swap_target_slot
    else
      @give_anim_swap_item_tex = nil
      @give_anim_swap_donor_slot = -1
      @give_anim_swap_target_slot = -1
    end

    @battle_state = :give_animation
    @highlight_tiles = []
    @target_highlight = nil
  end

  def update_give_animation
    @give_anim_timer += 1
    if @give_anim_timer >= 150
      @give_anim_active = false
      start_give_message(@give_anim_message_id, @give_anim_message_params)
    end
  end

  def draw_give_animation
    t = @give_anim_timer
    period = 30

    # --- Основной предмет (дающий -> получатель) ---
    donor_alpha = if t < 75
                    ((t / (period/2)) % 2 == 0) ? 255 : 0
                  else
                    0
                  end
    target_alpha = if t < 75
                     0
                   elsif t < 120
                     (((t - 75) / (period/2)) % 2 == 0) ? 255 : 0
                   else
                     255
                   end

    # --- Второй предмет (если обмен: получатель -> дающий) ---
    swap_donor_alpha = 0
    swap_target_alpha = 0
    if @give_anim_swap_item_tex
      swap_donor_alpha = target_alpha   # появляется у дающего
      swap_target_alpha = donor_alpha   # исчезает у получателя
    end

    # Рисуем крест дающего (основной предмет + возможный второй предмет)
    draw_item_cross(288, 400, @give_anim_donor_items, @give_anim_donor_slot, "Giver",
                    @give_anim_item_tex, donor_alpha,
                    @give_anim_swap_item_tex, swap_donor_alpha)
    # Рисуем крест получателя
    draw_item_cross(400, 400, @give_anim_target_items, @give_anim_target_slot, "Receiver",
                    @give_anim_item_tex, target_alpha,
                    @give_anim_swap_item_tex, swap_target_alpha)
  end

  def draw_item_cross(cx, cy, items, selected_slot, title,
                      main_tex, main_alpha,
                      swap_tex = nil, swap_alpha = 0)
    tw = MeasureText(title, 14)
    DrawText(title, cx - tw/2, cy - 120, 14, WHITE)

    positions = [
      { x: cx,        y: cy - 24 },
      { x: cx - 32,   y: cy },
      { x: cx + 32,   y: cy },
      { x: cx,        y: cy + 24 }
    ]

    4.times do |i|
      entry = items[i]
      slot_x = positions[i][:x] - 16
      slot_y = positions[i][:y] - 24
      slot_w = 32
      slot_h = 48

      # Фон слота
      DrawRectangle(slot_x, slot_y, slot_w, slot_h, DARKGRAY)
      DrawRectangleLines(slot_x, slot_y, slot_w, slot_h, GRAY)

      tex = nil
      alpha = 0

      if i == selected_slot
        # Выбираем, что показывать: основной предмет, обменный или пустую иконку
        # Приоритет: тот, у кого альфа выше (если оба > 0)
        if swap_tex && swap_alpha > main_alpha
          tex = swap_tex
          alpha = swap_alpha
        elsif main_alpha > 0
          tex = main_tex
          alpha = main_alpha
        else
          tex = @empty_item_tex
          alpha = 255
        end
      else
        # Обычные слоты
        tex = get_item_texture(entry ? entry["item"] : "NOTHING")
        alpha = 255
      end

      if tex && alpha > 0
        color = Fade(WHITE, alpha / 255.0)
        src = Rectangle.create(0, 0, 32, 48)
        dst = Rectangle.create(slot_x, slot_y, slot_w, slot_h)
        DrawTexturePro(tex, src, dst, Vector2.create(0,0), 0, color)
      end

      if i == selected_slot
        DrawRectangleLinesEx(Rectangle.create(slot_x - 1, slot_y - 1, slot_w + 2, slot_h + 2), 2, YELLOW)
      end
    end
  end
  
end