# lib/battleManager/battle_give.rb
module BattleGive
  # Поиск соседних союзников
  def adjacent_allies(unit)
    ux, uy = unit[:x], unit[:y]
    @allies.select do |a|
      ax, ay = a[:x], a[:y]
      dist = (ax - ux).abs + (ay - uy).abs
      alive = a[:hp] > 0
      dist == 1 && alive && a != unit
    end
  end

  # Начать передачу выбранному союзнику
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
      item_to_give = donor_items[idx].dup
      item_to_give["equipped"] = false
      donor_items[idx] = { "item" => "NOTHING", "equipped" => false }
      target_items[free_slot] = item_to_give
      start_give_message("0001", {
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

    start_give_message("0002", {
      '{DONOR}' => donor_actor["name"],
      '{ITEM}' => @pending_give_item["item"],
      '{RECEIVED_ITEM}' => chosen_target_item["item"],
      '{TARGET}' => target_actor["name"]
    })
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

  # Начало сообщения о передаче с посимвольным выводом
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
  
end