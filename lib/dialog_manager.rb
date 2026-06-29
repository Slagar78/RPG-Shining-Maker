# lib/dialog_manager.rb
# Менеджер диалогов с NPC. Выполняет скрипты из NPC_script.json,
# используя локальный text.txt карты и глобальный gamescript.txt.
# Поддерживает команды: say, choice, turn_to_player, turn_player_to_npc, end.

require 'raylib'
include Raylib

class DialogManager
  # @return [Boolean] завершён ли диалог
  attr_reader :finished

  # @param script [Array] массив команд из NPC_script.json
  # @param npc [NPC] объект NPC, с которым идёт диалог
  # @param player [Player] объект игрока
  # @param game_map [GameMap] текущая карта (для получения локального текста)
  # @param local_text [Hash] локальный текст карты (ID => строка)
  # @param global_text [Hash] глобальный текст игры (ID => строка)
  # @param message_panel_tex [Texture] текстура панели сообщений
  # @param font [Font] шрифт для отрисовки текста
  # @param leader_name [String] имя лидера (первого персонажа в партии)
  def initialize(script, npc, player, game_map, local_text, global_text, message_panel_tex, font, leader_name)
    @script = script
    @npc = npc
    @player = player
    @game_map = game_map
    @local_text = local_text || {}
    @global_text = global_text || {}
    @message_panel_tex = message_panel_tex
    @font = font
    @leader_name = leader_name || ''

    @index = 0                     # текущая команда в скрипте
    @finished = false
    @waiting_for_input = false     # ожидание ввода пользователя (для say/choice)
    @current_text = nil            # текст, который сейчас выводится (для say)
    @choices = nil                 # массив вариантов для choice
    @selected_choice = 0           # выбранный вариант
    @reveal_index = 0              # сколько символов уже показано
    @reveal_timer = 0              # таймер для посимвольного вывода
    @full_text_shown = false       # весь ли текст показан (для say)
    @panel_w = 480
    @panel_h = 128
    @panel_x = (576 - @panel_w) / 2
    @panel_y = 480 - @panel_h - 24
  end

  # Обновление диалога – вызывается каждый кадр в состоянии :dialog
  def update
    return if @finished

    # Если не ждём ввода, выполняем команды подряд
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
        @player.direction = direction_to_player(@player, @npc)  # конвертация в число
        @index += 1
      when 'end'
        @finished = true
        break
      else
        # неизвестная команда – пропускаем
        @index += 1
      end
    end

    # Обработка ввода, если ждём
    handle_input if @waiting_for_input

    # Если команды закончились и не ждём ввода, завершаем диалог
    if @index >= @script.length && !@waiting_for_input
      @finished = true
    end
  end

  # Отрисовка диалогового окна
  def draw
    return unless @waiting_for_input || @finished
    return if @finished

    # Рисуем фон панели
    if @message_panel_tex
      dst = Rectangle.create(@panel_x, @panel_y, @panel_w, @panel_h)
      src = Rectangle.create(0, 0, @message_panel_tex.width, @message_panel_tex.height)
      DrawTexturePro(@message_panel_tex, src, dst, Vector2.create(0,0), 0, WHITE)
    else
      DrawRectangle(@panel_x, @panel_y, @panel_w, @panel_h, GRAY)
      DrawRectangleLines(@panel_x, @panel_y, @panel_w, @panel_h, DARKGRAY)
    end

    if @current_text && !@choices
      # Вывод текста (say)
      display = @current_text[0, @reveal_index]
      lines = display.split('{N}', -1)
      y_offset = @panel_y + 10
      lines.each do |line|
        DrawTextEx(@font, line, Vector2.create(@panel_x + 20, y_offset), 30, 1, WHITE)
        y_offset += 38
      end
    elsif @choices
      # Вывод вариантов выбора
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

  # Завершён ли диалог
  def finished?
    @finished
  end

  private

  # Обработка команды say
  def handle_say(cmd)
    text_id = cmd['text_id']
    text = get_text(text_id)
    return unless text

    # Подстановка плейсхолдеров
    text = text.gsub('{LEADER}', @leader_name)
    text = text.gsub('{NPC_NAME}', @npc.id || 'NPC')

    @current_text = text
    @reveal_index = 0
    @reveal_timer = 0
    @full_text_shown = false
    @waiting_for_input = true
  end

  # Обработка команды choice
  def handle_choice(cmd)
    @choices = cmd['options']
    @selected_choice = 0
    @waiting_for_input = true
  end

  # Обработка ввода пользователя (вызывается, когда @waiting_for_input == true)
  def handle_input
    if @current_text && !@choices
      # Режим say – посимвольный вывод
      unless @full_text_shown
        # Постепенное раскрытие текста
        if @reveal_index < @current_text.length
          @reveal_timer += 1
          if @reveal_timer >= 2  # скорость вывода (каждые 2 кадра +1 символ)
            @reveal_timer = 0
            @reveal_index += 1
          end
        else
          @full_text_shown = true
        end
      end

      # Пропуск печатания (только пробел, можно убрать)
      if IsKeyPressed(KEY_SPACE) && !@full_text_shown
        @reveal_index = @current_text.length
        @full_text_shown = true
      end

      # Продолжение – только когда текст полностью показан
      if @full_text_shown && (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
        @waiting_for_input = false
        @index += 1
        @current_text = nil
      end
    elsif @choices
      # Режим choice – выбор варианта
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

  # Получить текст по ID: сначала локальный, затем глобальный
  def get_text(text_id)
    @local_text[text_id] || @global_text[text_id]
  end

  # Определить направление от объекта A к объекту B (возвращает строку)
  # Используется для NPC (у него direction – строка)
  def direction_to(from_obj, to_obj)
    dx = to_obj.x - from_obj.x
    dy = to_obj.y - from_obj.y
    if dx.abs > dy.abs
      dx > 0 ? 'right' : 'left'
    else
      dy > 0 ? 'down' : 'up'
    end
  end

  # Определить направление от игрока к NPC, но возвращает числовую константу
  # (у игрока direction – число: DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_UP)
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