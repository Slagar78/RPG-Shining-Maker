# lib/dialog_manager.rb
require 'raylib'
include Raylib

class DialogManager
  attr_reader :finished

  def initialize(script, npc, player, game_map, local_text, global_text, message_panel_tex, font, leader_name, original_npc_direction)
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

    @index = 0
    @finished = false
    @waiting_for_input = false
    @current_text = nil
    @choices = nil
    @selected_choice = 0
    @reveal_index = 0
    @reveal_timer = 0
    @full_text_shown = false

    @panel_w = 480
    @panel_h = 128
    @panel_x = (576 - @panel_w) / 2
    @panel_y = 480 - @panel_h - 24
  end

  def update
    return if @finished

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
      when 'end'
        @finished = true
        break
      else
        @index += 1
      end
    end

    handle_input if @waiting_for_input

    if @index >= @script.length && !@waiting_for_input
      @finished = true
    end

    if @finished && @npc.direction != @original_npc_direction
      @npc.direction = @original_npc_direction
    end
  end

  def draw
    return unless @waiting_for_input || @finished
    return if @finished

    if @message_panel_tex
      dst = Rectangle.create(@panel_x, @panel_y, @panel_w, @panel_h)
      src = Rectangle.create(0, 0, @message_panel_tex.width, @message_panel_tex.height)
      DrawTexturePro(@message_panel_tex, src, dst, Vector2.create(0,0), 0, WHITE)
    else
      DrawRectangle(@panel_x, @panel_y, @panel_w, @panel_h, GRAY)
      DrawRectangleLines(@panel_x, @panel_y, @panel_w, @panel_h, DARKGRAY)
    end

    if @current_text && !@choices
      # Разбор видимого текста с тегами
      raw_display = @current_text[0, @reveal_index]
      lines = split_display_lines(raw_display)

      y_offset = @panel_y + 10
      lines.each do |line|
        DrawTextEx(@font, line, Vector2.create(@panel_x + 20, y_offset), 30, 1, WHITE)
        y_offset += 38
      end
    elsif @choices
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

  private

  def handle_say(cmd)
    text_id = cmd['text_id']
    text = get_text(text_id)
    return unless text

    # Заменяем только служебные плейсхолдеры, теги управления ({N}, {WAIT}...) оставляем
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

  def handle_input
    if @current_text && !@choices
      # Режим say – посимвольный вывод с пропуском тегов
      unless @full_text_shown
        if @reveal_index < @current_text.length
          # Если на текущей позиции начинается тег {XXX}
          if @current_text[@reveal_index] == '{'
            closing = @current_text.index('}', @reveal_index)
            if closing
              # Пропускаем весь тег целиком, ничего не показывая
              @reveal_index = closing + 1
            else
              # Некорректный тег (нет закрывающей скобки) – пропускаем один символ
              @reveal_index += 1
            end
          else
            @reveal_timer += 1
            if @reveal_timer >= 2  # скорость вывода
              @reveal_timer = 0
              @reveal_index += 1
            end
          end
        else
          @full_text_shown = true
        end
      end

      # Пропуск всего текста по пробелу
      if IsKeyPressed(KEY_SPACE) && !@full_text_shown
        @reveal_index = @current_text.length
        @full_text_shown = true
      end

      # Переход к следующей команде после полного показа текста
      if @full_text_shown && (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
        @waiting_for_input = false
        @index += 1
        @current_text = nil
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

  # Разбивает сырую строку с тегами на массив видимых строк.
  # {N} – перенос строки, остальные теги (напр. {WAIT}) – просто удаляются.
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
            # Перенос строки
            lines << current_line
            current_line = ""
          else
            # Любой другой тег игнорируется (не добавляется к тексту)
          end
          i = closing + 1
        else
          # Нет закрывающей скобки – считаем как обычный символ
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