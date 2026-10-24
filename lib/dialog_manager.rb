# lib/dialog_manager.rb
require 'raylib'
include Raylib

class DialogManager
  attr_reader :finished

  REVEAL_SPEED = 1.0 / 30.0   # секунд на символ (было 2 кадра при 60 FPS)

  def initialize(script, npc, player, game_map, local_text, global_text,
                 message_panel_tex, font, leader_name, original_npc_direction,
                 flags = nil, shop_menu = nil, shops = nil)
    @script = script
    @npc = npc
    @player = player
    @game_map = game_map
    @local_text = local_text || {}
    @global_text = global_text || {}
    @message_panel_tex = message_panel_tex
    @font = font
    @leader_name = leader_name || ''
    @original_npc_direction = original_npc_direction
    @flags = flags

    # ── магазины ──
    @shop_menu = shop_menu
    @shops     = shops
    @waiting_for_shop = false

    @index = 0
    @finished = false
    @waiting_for_input = false
    @current_text = nil
    @choices = nil
    @selected_choice = 0
    @reveal_index = 0
    @reveal_timer = 0
    @full_text_shown = false

    @arrow_timer = 0.0
    @arrow_visible = false
    @next_text_delay = 0.0

    @panel_w = 480
    @panel_h = 128
    @panel_x = (576 - @panel_w) / 2
    @panel_y = 480 - @panel_h - 24
  end

  def update(dt = 1.0 / 60.0)
    return if @finished

    # ── Ждём закрытия магазина ──
    # Пока магазин открыт, диалог не двигается и не перехватывает ввод.
    if @waiting_for_shop
      if @shop_menu && !@shop_menu.visible
        @waiting_for_shop = false
        @index += 1
      end
      return
    end

    # ЖДЕМ ЗАДЕРЖКУ ПЕРЕД СЛЕДУЮЩИМ ТЕКСТОМ
    if @next_text_delay > 0
      @next_text_delay -= dt
      return if @next_text_delay > 0
    end

    while @index < @script.length && !@waiting_for_input
      cmd = @script[@index]
      case cmd['type']
      when 'say'
        handle_say(cmd)
      when 'choice'
        handle_choice(cmd)
      when 'turn_to_player'
        @npc.direction = direction_to(@npc, @player)
        @index += 1
      when 'turn_player_to_npc'
        @player.direction = direction_to_player(@player, @npc)
        @index += 1
      when 'unlock_battle'
        @flags&.unlock_battle(cmd['battle'])
        @index += 1
      when 'complete_battle'
        @flags&.complete_battle(cmd['battle'])
        @index += 1
      when 'open_shop'
        handle_shop(cmd)
        break if @waiting_for_shop   # выходим из цикла, дальше — ждём магазин
      when 'end'
        @finished = true
        break
      when 'start_battle'
        @flags&.set("__start_battle__", cmd['battle'])
        @finished = true
        break
      else
        @index += 1
      end
    end

    handle_input(dt) if @waiting_for_input

    if @index >= @script.length && !@waiting_for_input && !@waiting_for_shop
      @finished = true
    end

    if @finished && @npc.direction != @original_npc_direction
      @npc.direction = @original_npc_direction
    end

    # ★ АНИМАЦИЯ МИГАНИЯ СТРЕЛОЧКИ ★
    if @waiting_for_input && @full_text_shown && !@choices
      @arrow_timer += dt
      if @arrow_timer >= 0.4  # 0.4 секунды на одно состояние (видно/не видно)
        @arrow_timer = 0.0
        @arrow_visible = !@arrow_visible
      end
    else
      # Если текст не показан или это выбор (choice), стрелку прячем
      @arrow_visible = false
      @arrow_timer = 0.0
    end
  end

  def draw
    # Пока магазин открыт — диалог не рисуется, магазин сам себя рисует.
    return if @waiting_for_shop
    return unless @waiting_for_input || @finished || @next_text_delay > 0
    return if @finished

    # Отрисовка фона панели
    if @message_panel_tex
      dst = Rectangle.create(@panel_x, @panel_y, @panel_w, @panel_h)
      src = Rectangle.create(0, 0, @message_panel_tex.width, @message_panel_tex.height)
      DrawTexturePro(@message_panel_tex, src, dst, Vector2.create(0,0), 0, WHITE)
    else
      DrawRectangle(@panel_x, @panel_y, @panel_w, @panel_h, GRAY)
      DrawRectangleLines(@panel_x, @panel_y, @panel_w, @panel_h, DARKGRAY)
    end

    if @current_text && !@choices
      # Отрисовка текста (посимвольно)
      raw_display = @current_text[0, @reveal_index]
      lines = split_display_lines(raw_display)

      y_offset = @panel_y + 10
      lines.each do |line|
        DrawTextEx(@font, line, Vector2.create(@panel_x + 20, y_offset), 30, 1, WHITE)
        y_offset += 38
      end
      
      # ★ ОТРИСОВКА МИГАЮЩЕЙ СТРЕЛОЧКИ ★
      # Проверяем, есть ли СЛЕДУЮЩИЙ текст в скрипте
      next_cmd = @script[@index + 1]
      has_next_text = next_cmd && next_cmd['type'] == 'say'

      # Рисуем стрелочку ТОЛЬКО если есть следующий текст
      if @full_text_shown && @arrow_visible && has_next_text
        arrow_x = @panel_x + @panel_w - 30
        arrow_y = @panel_y + @panel_h - 25
        
        DrawTriangle(
          Vector2.create(arrow_x, arrow_y),
          Vector2.create(arrow_x + 8, arrow_y + 12),
          Vector2.create(arrow_x + 16, arrow_y),
          YELLOW
        )
      end

    elsif @choices
      # Отрисовка вариантов выбора (если это выбор)
      y_offset = @panel_y + 20
      @choices.each_with_index do |option, i|
        text = option['text']
        color = (i == @selected_choice) ? YELLOW : WHITE
        prefix = (i == @selected_choice) ? '> ' : '  '
        DrawTextEx(@font, prefix + text, Vector2.create(@panel_x + 20, y_offset), 30, 1, color)
        y_offset += 38
      end
    end
  end

  def finished?
    @finished
  end

  # Публичный геттер — Game по нему понимает, рисовать ли магазин.
  def waiting_for_shop?
    @waiting_for_shop
  end

  private

  def handle_shop(cmd)
    shop_id = cmd['shop_id']

    if @shop_menu.nil? || @shops.nil?
      puts "WARNING: open_shop, но shop_menu/shops не переданы в DialogManager."
      @index += 1
      return
    end

    if @shop_menu.open_by_id(@shops, shop_id)
      @waiting_for_shop = true
    else
      # Магазин не найден — пропускаем команду, чтобы не застрять.
      @index += 1
    end
  end

  def handle_say(cmd)
    text_id = cmd['text_id']
    
    # Проверяем, есть ли запятая в ID
    if text_id.is_a?(String) && text_id.include?(',')
      ids = text_id.split(',').map(&:strip)
      
      # Берем первый ID для текущего окна
      cmd['text_id'] = ids.shift
      
      # Остальные вставляем в скрипт сразу ПОСЛЕ текущего шага.
      # Делаем reverse, чтобы порядок текстов не перепутался.
      ids.reverse.each do |id|
        @script.insert(@index + 1, { 'type' => 'say', 'text_id' => id })
      end
    end
    
    # Дальше стандартный код (он возьмет уже измененный cmd['text_id'])
    text = get_text(cmd['text_id'])
    return unless text

    text = text.gsub('{LEADER}', @leader_name)
    text = text.gsub('{NPC_NAME}', @npc.id || 'NPC')

    @current_text = text
    @reveal_index = 0
    @reveal_timer = 0
    @full_text_shown = false
    @waiting_for_input = true
  end

  def handle_choice(cmd)
    @choices = cmd['options']
    @selected_choice = 0
    @waiting_for_input = true
  end

  def handle_input(dt = 1.0 / 60.0)
    if @current_text && !@choices
      unless @full_text_shown
        if @reveal_index < @current_text.length
          if @current_text[@reveal_index] == '{'
            closing = @current_text.index('}', @reveal_index)
            if closing
              @reveal_index = closing + 1
            else
              @reveal_index += 1
            end
          else
            @reveal_timer += dt
            if @reveal_timer >= REVEAL_SPEED
              @reveal_timer -= REVEAL_SPEED
              @reveal_index += 1
            end
          end
        else
          @full_text_shown = true
        end
      end

      if IsKeyPressed(KEY_SPACE) && !@full_text_shown
        @reveal_index = @current_text.length
        @full_text_shown = true
      end

      if @full_text_shown && (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
        @waiting_for_input = false
        @index += 1
        @current_text = nil
        @next_text_delay = 0.1 # 0.1 секунды паузы
      end
    elsif @choices
      if IsKeyPressed(KEY_UP)
        @selected_choice = (@selected_choice - 1) % @choices.length
      elsif IsKeyPressed(KEY_DOWN)
        @selected_choice = (@selected_choice + 1) % @choices.length
      elsif IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)
        next_index = @choices[@selected_choice]['next']
        @choices = nil
        @waiting_for_input = false
        @index = next_index
      end
    end
  end

  def split_display_lines(raw)
    lines = []
    current_line = ""
    i = 0
    while i < raw.length
      if raw[i] == '{'
        closing = raw.index('}', i)
        if closing
          tag = raw[i..closing]
          if tag == '{N}'
            lines << current_line
            current_line = ""
          end
          i = closing + 1
        else
          current_line << raw[i]
          i += 1
        end
      else
        current_line << raw[i]
        i += 1
      end
    end
    lines << current_line unless current_line.empty? && lines.any?
    lines
  end

  def get_text(text_id)
    @local_text[text_id] || @global_text[text_id]
  end

  def direction_to(from_obj, to_obj)
    dx = to_obj.x - from_obj.x
    dy = to_obj.y - from_obj.y
    if dx.abs > dy.abs
      dx > 0 ? 'right' : 'left'
    else
      dy > 0 ? 'down' : 'up'
    end
  end

  def direction_to_player(player, npc)
    dx = npc.x - player.x
    dy = npc.y - player.y
    if dx.abs > dy.abs
      dx > 0 ? DIR_RIGHT : DIR_LEFT
    else
      dy > 0 ? DIR_DOWN : DIR_UP
    end
  end
end