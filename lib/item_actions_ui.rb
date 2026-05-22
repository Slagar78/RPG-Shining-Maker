# item_actions_ui.rb
# Общая база для окон работы с предметами (Use / Give / Drop / Equip)
require 'json'

# ============================================================
# Вспомогательный модуль (не влияет на другие окна)
# ============================================================
module ItemUIHelpers
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

  # Рисует название предмета с переносом, если оно из двух слов
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

  def find_actor_items(actor_name)
    actor = @party.find { |a| a["name"] == actor_name }
    return [] unless actor
    entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    return [] unless entry
    entry["items"] || []
  end

  def find_item_by_name(name)
    @db.find_by_name(name) if @db
  end
end

# ============================================================
# Базовый класс для всех подменю предметов
# ============================================================
class ItemSubMenuBase
  include ItemUIHelpers
   
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

  attr_reader :visible, :current_actor, :anim_phase

  def initialize(mode, font, db, party, classes_data, class_names, start_inventory)
    @mode = mode
    @font = font
    @db = db
    @party = party
    @classes_data = classes_data
    @class_names = class_names
    @start_inventory = start_inventory

    # Кеши
    @portrait_cache = {}
    @icon_cache = {}
    @items_data = nil

    # Видимость и анимация
    @visible = false
    @anim_phase = 0
    @ready_to_close = false

    # Позиции панелей (как в StatusOverlay)
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

    # Размеры
    @upper_w = 346
    @upper_h = 208
    @lower_w = 480
    @lower_h = 240
    @portrait_w = 134
    @portrait_h = 208
    @frame_w = 134
    @frame_h = 208

    # Таймеры моргания, пульсации
    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    @selection_blink_timer = 0
    @status_view_mode = 0     # 0 = класс/уровень/опыт, 1 = статы

    # Индексы для выбора персонажа и предмета
    @selected_actor_index = 0
    @list_top_index = 0
    @selected_item_index = 0
    @item_scroll = 0

    # Автоповтор для персонажа
    @input_timer_up = 0
    @input_timer_down = 0

    # Фокус: :party (нижняя панель) или :items (верхняя панель)
    @focus = :party

    # Текстуры панелей
    load_common_textures
  end

  # ----------------------------------------------------------
  # Загрузка общих текстур
  # ----------------------------------------------------------
  def load_common_textures
  @upper_tex = Raylib.LoadTexture("assets/ui/upper_panel.png")
  @lower_tex = Raylib.LoadTexture("assets/ui/lower_panel.png")
  @frame_tex = Raylib.LoadTexture("assets/ui/portrait_frame.png")
  @ruby_tex  = Raylib.LoadTexture("assets/ui/ruby_icon.png")

  Raylib.SetTextureFilter(@upper_tex, 0) if @upper_tex
  Raylib.SetTextureFilter(@lower_tex, 0) if @lower_tex
  Raylib.SetTextureFilter(@frame_tex, 0) if @frame_tex
  Raylib.SetTextureFilter(@ruby_tex, 0)  if @ruby_tex

  # Дефолтная иконка предмета (если файла нет, останется nil – тогда рисовать не будем)
  if File.exist?("assets/items/item_empty.png")
    @empty_item_tex = Raylib.LoadTexture("assets/items/item_empty.png")
    Raylib.SetTextureFilter(@empty_item_tex, 0) if @empty_item_tex
  else
    @empty_item_tex = nil
  end
end

  # ----------------------------------------------------------
  # Анимация открытия
  # ----------------------------------------------------------
  def open(actor_name = nil)
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
      elsif @selected_actor_index >= @list_top_index + 5
        @list_top_index = @selected_actor_index - 4
      end
      update_current_actor
    else
      @current_actor = nil
      @current_items = []
    end

    @blink_timer = 0
    @blink_duration = 0
    @blink_interval = 120
    @selection_blink_timer = 0
    @selected_item_index = 0
    @item_scroll = 0
    @focus = :party   # начинаем с выбора персонажа
	@status_view_mode = 0
  end

  # ----------------------------------------------------------
  # Закрытие с анимацией
  # ----------------------------------------------------------
  def close
    return unless @visible && @anim_phase == 2
    @anim_phase = 3
  end

  def force_close
    @visible = false
    @anim_phase = 0
  end

  # ----------------------------------------------------------
  # Обновление состояния текущего актора
  # ----------------------------------------------------------
  def update_current_actor
  @current_actor = @party[@selected_actor_index]["name"]
  @current_items = filter_items(find_actor_items(@current_actor))

  actor = @party[@selected_actor_index]
  portrait_name = actor ? (actor["portrait"] || actor["name"]) : @current_actor
  @portrait_tex = load_portrait(portrait_name)
  @blink_tex = load_blink_portrait(portrait_name)

  @selected_item_index = 0
  @item_scroll = 0  
  if @donor_actor && @party[@selected_actor_index]["name"] == @donor_actor["name"]
  @donor_items = fill_to_four(find_actor_items(@donor_actor["name"]))
    end
end

  # ----------------------------------------------------------
  # Изменение выбранного персонажа (со скроллингом)
  # ----------------------------------------------------------
  def change_selected_actor(delta)
    return unless @party.any?
	$audio.play_sfx(:confirm) if $audio
    new_index = @selected_actor_index + delta
    return if new_index < 0 || new_index >= @party.length

    @selected_actor_index = new_index
    if @selected_actor_index < @list_top_index
      @list_top_index = @selected_actor_index
    elsif @selected_actor_index >= @list_top_index + 5
      @list_top_index = @selected_actor_index - 4
    end
    update_current_actor
  end

  # ----------------------------------------------------------
  # Обновление (движение панелей, моргание)
  # ----------------------------------------------------------
  def update
    return unless @visible
    speed = 38

    case @anim_phase
    when 1
      @portrait_x += speed
      @portrait_x = @portrait_target_x if @portrait_x > @portrait_target_x
      @frame_x += speed
      @frame_x = @frame_target_x if @frame_x > @frame_target_x
      @upper_x -= speed
      @upper_x = @upper_target_x if @upper_x < @upper_target_x
      @lower_y -= speed
      @lower_y = @lower_target_y if @lower_y < @lower_target_y

      if @portrait_x >= @portrait_target_x &&
         @frame_x >= @frame_target_x &&
         @upper_x <= @upper_target_x &&
         @lower_y <= @lower_target_y
        @anim_phase = 2
      end

    when 3
      @portrait_x -= speed
      @portrait_x = @portrait_start_x if @portrait_x < @portrait_start_x
      @frame_x -= speed
      @frame_x = @frame_start_x if @frame_x < @frame_start_x
      @upper_x += speed
      @upper_x = @upper_start_x if @upper_x > @upper_start_x
      @lower_y += speed
      @lower_y = @lower_start_y if @lower_y > @lower_start_y

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

  # ----------------------------------------------------------
  # Обработка ввода
  # ----------------------------------------------------------
  def handle_input
    return unless @visible && @anim_phase == 2

    case @focus
    when :party
      # ----- Фокус на нижней панели (персонажи) -----
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        close
        return
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

      if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @status_view_mode = 1 - @status_view_mode
		$audio.play_sfx(:confirm) if $audio
      end

      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
        @focus = :items
        @selected_item_index = 0
      end

    when :items
      # ----- Фокус на верхней панели (предметы) -----
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        @focus = :party
        return
      end

      if Raylib.IsKeyPressed(Raylib::KEY_UP)
        @selected_item_index = 0
      elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
        @selected_item_index = 1
      elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @selected_item_index = 2
      elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
        @selected_item_index = 3
      end

      if Raylib.IsKeyPressed(Raylib::KEY_A)
        if @current_items && @selected_item_index < @current_items.length
          item_entry = @current_items[@selected_item_index]
          actor = @party[@selected_actor_index]
          if actor && item_entry && item_entry["item"] != "NOTHING"
            confirm_action(item_entry, actor)
          end
        end
      end
    end
  end

  # ----------------------------------------------------------
  # Отрисовка всего окна
  # ----------------------------------------------------------
  def draw
    return unless @visible
    origin = Raylib::Vector2.create(0, 0)

    # Верхняя панель
    dst = Raylib::Rectangle.create(@upper_x, @upper_y, @upper_w, @upper_h)
    src = Raylib::Rectangle.create(0, 0, @upper_w, @upper_h)
    Raylib.DrawTexturePro(@upper_tex, src, dst, origin, 0, Raylib::WHITE)

    # Нижняя панель
    dst = Raylib::Rectangle.create(@lower_x, @lower_y, @lower_w, @lower_h)
    src = Raylib::Rectangle.create(0, 0, @lower_w, @lower_h)
    Raylib.DrawTexturePro(@lower_tex, src, dst, origin, 0, Raylib::WHITE)

    # Портрет и рамка
    if @portrait_tex
      portrait = (@blink_duration > 0 && @blink_tex) ? @blink_tex : @portrait_tex
      dst = Raylib::Rectangle.create(@portrait_x, @portrait_y, 134, 208)
      src = Raylib::Rectangle.create(0, 0, 134, 208)
      Raylib.DrawTexturePro(portrait, src, dst, origin, 0, Raylib::WHITE)
    end
    dst = Raylib::Rectangle.create(@frame_x, @frame_y, @frame_w, @frame_h)
    src = Raylib::Rectangle.create(0, 0, @frame_w, @frame_h)
    Raylib.DrawTexturePro(@frame_tex, src, dst, origin, 0, Raylib::WHITE)

    # Контент панелей
    draw_upper_content
    draw_lower_content
  end

  # ----------------------------------------------------------
  # Верхняя панель: предметы текущего персонажа
  # ----------------------------------------------------------
 def draw_upper_content
  # Заголовок
  actor_data = @party[@selected_actor_index]
  if actor_data
    class_id   = actor_data["class_id"]
    class_name = @class_names[class_id] || "???"
    level = [(actor_data["level"] || 1), 1].max
    header     = "#{actor_data["name"]}  #{class_name}  LV #{level}"
    draw_text_custom(header, @upper_x + 25, @upper_y + 12, 20, WHITE)
  else
    draw_text_custom("NO DATA", @upper_x + 25, @upper_y + 12, 20, WHITE)
  end

  draw_text_custom("-- ITEMS --", @upper_x + 47, @upper_y + 35, 20, WHITE)

  # Крест иконок (как в MagicOverlay)
  base_x = @upper_x + 40
  base_y = @upper_y + 60
  offset_x = 44
  offset_y = 42
  icon_positions = [
    { x: base_x + offset_x, y: base_y + 8 },               # верхняя (индекс 0)
    { x: base_x,            y: base_y + offset_y },         # левая  (индекс 1)
    { x: base_x + offset_x * 2, y: base_y + offset_y },    # правая (индекс 2)
    { x: base_x + offset_x, y: base_y + offset_y * 2 - 8 } # нижняя (индекс 3)
  ]

  # Если все слоты пустые – показываем одну надпись NOTHING
  if @current_items.nil? || @current_items.none? { |entry| entry && entry["item"] != "NOTHING" }
    draw_text_custom("NOTHING", @upper_x + 195, @upper_y + 48, 18, ORANGE)
  end

  # Текст справа (такой же столбец, как у магии)
  text_x = @upper_x + 195
  text_y = @upper_y + 48
  text_line_h = 36

  4.times do |i|
    ipos = icon_positions[i]
    item_entry = @current_items ? @current_items[i] : nil

    # Определяем, какую иконку рисовать
    if item_entry && item_entry["item"] != "NOTHING"
      # Есть предмет: пробуем загрузить его иконку
      item_data = find_item_by_name(item_entry["item"])
      icon_path = item_data ? item_data["icon"] : nil
      icon_tex = load_icon(icon_path)  # вернёт nil, если файл не найден
    else
      icon_tex = nil
    end

    # Если иконка не найдена – используем дефолтную (empty_item_tex)
    tex_to_draw = icon_tex || @empty_item_tex

    if tex_to_draw
      src = Raylib::Rectangle.create(0, 0, 32, 48)
      dst = Raylib::Rectangle.create(ipos[:x], ipos[:y], 32, 48)
      Raylib.DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    else
      # Если совсем ничего нет – оставляем серый квадрат (на всякий случай)
      Raylib.DrawRectangle(ipos[:x], ipos[:y], 32, 48, Raylib::GRAY)
      Raylib.DrawRectangleLines(ipos[:x], ipos[:y], 32, 48, Raylib::DARKGRAY)
    end

    # Название предмета (справа) – только для заполненных слотов
    if item_entry && item_entry["item"] != "NOTHING"
      y_text = text_y + i * text_line_h
      text_color = (i == @selected_item_index && @focus == :items) ? Raylib::LIME : Raylib::WHITE
      draw_item_name(item_entry["item"], text_x, y_text, 18, text_color)
      if item_entry["equipped"]
        draw_text_custom("E", text_x - 15, y_text + 4, 18, YELLOW)
      end
    end

    # Подсветка текущего выбранного слота (только в режиме выбора предметов)
    if i == @selected_item_index && @focus == :items
      alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
      color = Raylib.Fade(Raylib::GREEN, alpha / 255.0)
          Raylib.DrawRectangleLinesEx(
          Raylib::Rectangle.create(ipos[:x] - 1, ipos[:y] - 1, 34, 50),
          2.0,    # толщина рамки в пикселях (можешь поставить 3.0, если хочешь ещё толще)
          color
        )
    end
  end
end
  # ----------------------------------------------------------
  # Нижняя панель: список персонажей (как в StatusOverlay)
  # ----------------------------------------------------------
    def draw_lower_content
       draw_character_list(mode: @status_view_mode)
    end
   # общий метод
    def draw_stats_comparison(member, item_bonus_atk, item_bonus_df, y, blink_visible)
    klass = @classes_data.find { |c| c["id"] == member["class_id"] }
    if klass && @db
      lv = [(member["level"] || 1), 1].max
      atk = [@db.stat_at_level(klass["attack_growth"], lv), 0].max
      df  = [@db.stat_at_level(klass["defense_growth"], lv), 0].max
    else
      atk = 0; df = 0
    end
    bonuses = calculate_equip_bonuses(member)
    atk = [atk + bonuses[:attack], 0].max
    df  = [df  + bonuses[:defense], 0].max
    future_atk = [atk + item_bonus_atk, 0].max
    future_df  = [df  + item_bonus_df,  0].max

    arrow_str = " > "
    gap = 6

    # ATTACK
    atk_str_cur = atk.to_s
    atk_str_fut = future_atk.to_s
    cur_w   = Raylib.MeasureTextEx(@font, atk_str_cur, 18, 1).x
    arrow_w = Raylib.MeasureTextEx(@font, arrow_str, 18, 1).x
    fut_w   = Raylib.MeasureTextEx(@font, atk_str_fut, 18, 1).x
    total_w = cur_w + gap + arrow_w + gap + fut_w
    start_x = (@lower_x + 220) - total_w / 2
    draw_text_custom(atk_str_cur, start_x, y, 18, WHITE)
    draw_text_custom(arrow_str, start_x + cur_w + gap, y, 18, WHITE)
    fut_color = if future_atk > atk
                  blink_visible ? GREEN : WHITE
                elsif future_atk < atk
                  blink_visible ? RED : WHITE
                else
                  WHITE
                end
    draw_text_custom(atk_str_fut, start_x + cur_w + gap + arrow_w + gap, y, 18, fut_color)

    # DEFENSE
    df_str_cur = df.to_s
    df_str_fut = future_df.to_s
    cur_w  = Raylib.MeasureTextEx(@font, df_str_cur, 18, 1).x
    fut_w  = Raylib.MeasureTextEx(@font, df_str_fut, 18, 1).x
    total_w = cur_w + gap + arrow_w + gap + fut_w
    start_x = (@lower_x + 330) - total_w / 2
    draw_text_custom(df_str_cur, start_x, y, 18, WHITE)
    draw_text_custom(arrow_str, start_x + cur_w + gap, y, 18, WHITE)
    fut_color = if future_df > df
                  blink_visible ? GREEN : WHITE
                elsif future_df < df
                  blink_visible ? RED : WHITE
                else
                  WHITE
                end
    draw_text_custom(df_str_fut, start_x + cur_w + gap + arrow_w + gap, y, 18, fut_color)
  end

def draw_character_list(mode: 0, item_bonus_atk: 0, item_bonus_df: 0)
  header_y = @lower_y + 28
  if mode == 0
    draw_text_custom("Имя",    @lower_x + 44,  header_y, 20, WHITE)
    draw_text_custom("Класс",  @lower_x + 187, header_y, 20, WHITE)
    level_header_center_x = @lower_x + 290 + Raylib.MeasureTextEx(@font, "Уровень", 20, 1).x / 2
    exp_header_center_x   = @lower_x + 395 + Raylib.MeasureTextEx(@font, "Опыт", 20, 1).x / 2
    draw_text_custom("Уровень", @lower_x + 290, header_y, 20, WHITE)
    draw_text_custom("Опыт",    @lower_x + 395, header_y, 20, WHITE)
  elsif mode == 1
    draw_text_custom("Имя", @lower_x + 44, header_y, 20, WHITE)
    stat_headers = ["HP", "MP", "AT", "DF", "AGI", "MV"]
    stat_centers = [@lower_x + 200, @lower_x + 250, @lower_x + 300, @lower_x + 350, @lower_x + 400, @lower_x + 445]
    stat_headers.each_with_index do |head, idx|
      cx = stat_centers[idx]
      w = Raylib.MeasureTextEx(@font, head, 20, 1).x
      draw_text_custom(head, cx - w / 2, header_y, 20, WHITE)
    end
  elsif mode == 2
    draw_text_custom("Имя",    @lower_x + 44,  header_y, 20, WHITE)
    draw_text_custom("ATTACK",  @lower_x + 187, header_y, 20, WHITE)
    draw_text_custom("DEFENSE", @lower_x + 290, header_y, 20, WHITE)
  end

  blink_visible = ((@selection_blink_timer / 15) % 2 == 0) if mode == 2

  5.times do |i|
    list_index = @list_top_index + i
    break if list_index >= @party.length
    member = @party[list_index]
    y = @lower_y + 71 + i * 34

    # highlight
    if member["name"] == @current_actor
      if @focus == :items
        highlight = Raylib.Fade(Raylib::BLUE, 0.5)
        Raylib.DrawRectangle(@lower_x + 38, y - 4, 138, 28, highlight)
      else
        pulse = Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6
        alpha = (pulse * 255).to_i
        highlight = Raylib.Fade(Raylib::BLUE, alpha / 255.0)
        Raylib.DrawRectangle(@lower_x + 38, y - 4, 138, 28, highlight)
      end
    end

    # ruby icon
    if @ruby_tex
      ruby_src = Raylib::Rectangle.create(0, 0, @ruby_tex.width, @ruby_tex.height)
      ruby_dst = Raylib::Rectangle.create(@lower_x + 15, y - 3, 24, 24)
      Raylib.DrawTexturePro(@ruby_tex, ruby_src, ruby_dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    end

    name_display = member["name"].slice(0, 10)
    draw_text_custom(name_display, @lower_x + 44, y, 18, WHITE)

    case mode
    when 0
      class_name = @class_names[member["class_id"]] || "???"
      class_display = class_name.slice(0, 10)
      draw_text_custom(class_display, @lower_x + 187, y, 18, WHITE)
      lv_display = [(member["level"] || 1), 1].max
      draw_text_centered_h(lv_display.to_s, level_header_center_x, y, 18, WHITE)
      draw_text_centered_h(member["exp"].to_s,    exp_header_center_x,   y, 18, WHITE)
    when 1
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
    when 2
      draw_stats_comparison(member, item_bonus_atk, item_bonus_df, y, blink_visible)
    end
  end

  # arrows (scroll indicators) — они одинаковые
  if @list_top_index > 0
    alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
    color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
    ax = @lower_x + 27
    ay = @lower_y + 71 + 12
    Raylib.DrawTriangle(
      Raylib::Vector2.create(ax, ay - 6),
      Raylib::Vector2.create(ax - 6, ay + 4),
      Raylib::Vector2.create(ax + 6, ay + 4),
      color
    )
  end
  if @list_top_index + 5 < @party.length
    alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
    color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
    ax = @lower_x + 27
    ay = @lower_y + 71 + 4*34 + 12
    Raylib.DrawTriangle(
      Raylib::Vector2.create(ax - 6, ay - 4),
      Raylib::Vector2.create(ax, ay + 6),
      Raylib::Vector2.create(ax + 6, ay - 4),
      color
    )
  end
end

  # ----------------------------------------------------------
  # Методы для переопределения в наследниках
  # ----------------------------------------------------------
  def filter_items(items)
    items
  end

  def confirm_action(item_entry, actor)
  end
end

# ============================================================
# Конкретные меню (Use / Give / Drop / Equip)
# ============================================================

class UseMenu < ItemSubMenuBase
  def initialize(font, db, party, classes_data, class_names, start_inventory)
    super(:use, font, db, party, classes_data, class_names, start_inventory)
  end

  def filter_items(items)
    items.reject { |entry| entry["item"] == "NOTHING" }
  end

  def confirm_action(item_entry, actor)
    puts "Использован предмет #{item_entry["item"]} на #{actor["name"]}"
  end
end

# ============================================================
# class GiveMenu
# ============================================================
class GiveMenu < ItemSubMenuBase
  GIVE_MESSAGE_DURATION = 180
  RESULT_MESSAGE_DURATION = 120

  def initialize(font, large_font, db, party, classes_data, class_names, start_inventory, game_text)
    super(:give, font, db, party, classes_data, class_names, start_inventory)
    @large_font = large_font
    @game_text = game_text
    @give_state = :select_item
    @give_message_timer = 0
    @selected_give_item = nil
    @give_selected_item_index = nil
    @donor_actor = nil
    @donor_items = nil
    @message_panel_tex = nil
    @result_message_id = nil
    @result_params = {}
    @result_message_timer = 0
    load_give_textures
  end

  def load_give_textures
    @message_panel_tex = Raylib.LoadTexture("assets/ui/message_panel.png")
    Raylib.SetTextureFilter(@message_panel_tex, 0) if @message_panel_tex
  end

  def open(actor_name = nil)
    super
    @give_state = :select_item
    @give_message_timer = 0
    @selected_give_item = nil
    @give_selected_item_index = nil
    @donor_actor = nil
    @donor_items = nil
    @result_message_id = nil
    @result_params = {}
    @result_message_timer = 0
    @focus = :party
  end

  def filter_items(items)
    # Всегда показываем ровно 4 слота, пустые остаются пустыми
    fill_to_four(items)
  end

  def handle_input
    case @give_state
    when :select_item
      super
    when :show_message, :result_message
      # ничего не делаем
    when :select_target
      return unless @visible && @anim_phase == 2
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        @give_state = :select_item
        @focus = :party
        return
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
      if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        max_modes = item_affects_attack_defense? ? 3 : 2
        @status_view_mode = (@status_view_mode + 1) % max_modes
      end
      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
        give_item_to(@party[@selected_actor_index])
      end
    end
  end

  def update
    super
    case @give_state
    when :show_message
      if @visible == false && @anim_phase == 0
        @give_message_timer += 1
        if @give_message_timer >= GIVE_MESSAGE_DURATION
          @give_state = :select_target
          @give_message_timer = 0
          open_target_selection
        end
      end
    when :result_message
      @result_message_timer += 1
      if @result_message_timer >= RESULT_MESSAGE_DURATION
        @give_state = :select_item
        donor_index = @party.index { |a| a["id"] == @donor_actor["id"] } || 0
        @selected_actor_index = donor_index
        if donor_index < @list_top_index
          @list_top_index = donor_index
        elsif donor_index >= @list_top_index + 5
          @list_top_index = donor_index - 4
        end
        @focus = :party
        @selected_item_index = 0
        @selected_give_item = nil
        @give_selected_item_index = nil
        update_current_actor
        reopen_for_selection
      end
    end
  end

  def open_target_selection
    @visible = true
    @anim_phase = 1
    @upper_x = @upper_start_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x
    @focus = :party
    @blink_timer = 0
    @blink_duration = 0
    update_current_actor
	@status_view_mode = 2 if item_affects_attack_defense?
  end

  def draw
    case @give_state
    when :select_item then super
    when :show_message then draw_message_only
    when :select_target then super
    when :result_message then draw_result_message
    end
  end

  def draw_upper_content
    super
    if @give_state == :select_target && @donor_actor
      if @party[@selected_actor_index] == @donor_actor
        current_donor_items = fill_to_four(find_actor_items(@donor_actor["name"]))
        idx = @give_selected_item_index
        return unless idx && idx >= 0 && idx < 4 && current_donor_items[idx]
        return if current_donor_items[idx]["item"] == "NOTHING"

        base_x = @upper_x + 40
        base_y = @upper_y + 60
        offset_x = 44
        offset_y = 42
        positions = [
          { x: base_x + offset_x, y: base_y + 8 },
          { x: base_x,            y: base_y + offset_y },
          { x: base_x + offset_x * 2, y: base_y + offset_y },
          { x: base_x + offset_x, y: base_y + offset_y * 2 - 8 }
        ]
        pos = positions[idx]
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 180
        color = Raylib.Fade(Raylib::RED, alpha / 255.0)
        Raylib.DrawRectangle(pos[:x], pos[:y], 32, 48, color)
      end
    end
  end

  def draw_message_only
    template = @game_text["0000"] || "Pass the {ITEM}{N}to whom?"
    item_name = @selected_give_item ? @selected_give_item["item"] : "???"
    lines = template.gsub('{ITEM}', item_name).split('{N}')

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
      Raylib.DrawTextEx(@large_font, line_text, Raylib::Vector2.create(panel_x + 40, y_offset), 30, 1, Raylib::WHITE)
      y_offset += 38
    end
  end

  def draw_result_message
    template = @game_text[@result_message_id] || "Transfer complete."
    text = template.dup
    @result_params.each { |key, val| text.gsub!(key, val.to_s) }
    lines = text.split('{N}')

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
      Raylib.DrawTextEx(@large_font, line_text, Raylib::Vector2.create(panel_x + 40, y_offset), 30, 1, Raylib::WHITE)
      y_offset += 38
    end
  end

  def item_affects_attack_defense?
    return false unless @selected_give_item
    item_data = find_item_by_name(@selected_give_item["item"])
    return false unless item_data
    %w[Weapon Ring].include?(item_data["category"])
  end

  def fill_to_four(arr)
    arr = arr.dup
    while arr.length < 4
      arr << { "item" => "NOTHING", "equipped" => false }
    end
    arr
  end

  def give_item_to(actor)
    return unless @selected_give_item && actor && @donor_actor
    return if actor == @donor_actor

    donor_items = fill_to_four(find_actor_items(@donor_actor["name"]))
    target_items = fill_to_four(find_actor_items(actor["name"]))

    idx = @give_selected_item_index
    return unless idx && idx >= 0 && idx < 4 && donor_items[idx] && donor_items[idx]["item"] == @selected_give_item["item"]

    empty_slot = target_items.index { |entry| entry["item"] == "NOTHING" }

    if empty_slot
      item_to_give = donor_items[idx].dup
      item_to_give["equipped"] = false       # <-- снимаем экипировку
      donor_items[idx] = { "item" => "NOTHING", "equipped" => false }
      target_items[empty_slot] = item_to_give
      update_inventory(@donor_actor["id"], donor_items)
      update_inventory(actor["id"], target_items)

      @result_message_id = "0001"
      @result_params = {
        '{DONOR}' => @donor_actor["name"],
        '{ITEM}' => item_to_give["item"],
        '{TARGET}' => actor["name"]
      }
    else
      donor_item = donor_items[idx]
      target_item = target_items[idx] || { "item" => "NOTHING", "equipped" => false }
      donor_item["equipped"] = false
      target_item["equipped"] = false if target_item["item"] != "NOTHING"
      donor_items[idx] = target_item.dup
      target_items[idx] = donor_item.dup
      update_inventory(@donor_actor["id"], donor_items)
      update_inventory(actor["id"], target_items)

      @result_message_id = "0002"
      @result_params = {
        '{DONOR}' => @donor_actor["name"],
        '{ITEM}' => donor_item["item"],
        '{RECEIVED_ITEM}' => target_item["item"] != "NOTHING" ? target_item["item"] : "Nothing",
        '{TARGET}' => actor["name"]
      }
    end

    @give_state = :result_message
    @result_message_timer = 0
  end

  def update_inventory(actor_id, items)
    inv = @start_inventory.find { |inv| inv["actor_id"] == actor_id }
    if inv
      inv["items"] = items
    end
  end

    def draw_lower_content
    if @give_state == :select_target && @status_view_mode == 2
      give_atk = 0
      give_def = 0
      if @selected_give_item
        item_data = find_item_by_name(@selected_give_item["item"])
        if item_data
          give_atk = (item_data["attack"] || 0).to_i
          give_def = (item_data["defense"] || 0).to_i
        end
      end

      header_y = @lower_y + 28
      draw_text_custom("Имя",    @lower_x + 44,  header_y, 20, WHITE)
      draw_text_custom("ATTACK",  @lower_x + 187, header_y, 20, WHITE)
      draw_text_custom("DEFENSE", @lower_x + 290, header_y, 20, WHITE)

      blink_visible = ((@selection_blink_timer / 15) % 2 == 0)

      5.times do |i|
        list_index = @list_top_index + i
        break if list_index >= @party.length
        member = @party[list_index]
        y = @lower_y + 71 + i * 34

        if member["name"] == @current_actor
          highlight = Raylib.Fade(Raylib::BLUE, 0.5)
          Raylib.DrawRectangle(@lower_x + 38, y - 4, 138, 28, highlight)
        end
        if @ruby_tex
          ruby_src = Raylib::Rectangle.create(0, 0, @ruby_tex.width, @ruby_tex.height)
          ruby_dst = Raylib::Rectangle.create(@lower_x + 15, y - 3, 24, 24)
          Raylib.DrawTexturePro(@ruby_tex, ruby_src, ruby_dst,
                                Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
        end

        name_display = member["name"].slice(0, 10)
        draw_text_custom(name_display, @lower_x + 44, y, 18, WHITE)

        cur_bonuses = calculate_equip_bonuses(member)
        cur_atk_bonus = cur_bonuses[:attack]
        cur_def_bonus = cur_bonuses[:defense]

        delta_atk = give_atk - cur_atk_bonus
        delta_def = give_def - cur_def_bonus

        draw_stats_comparison(member, delta_atk, delta_def, y, blink_visible)
      end

      if @list_top_index > 0
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        ax = @lower_x + 27
        ay = @lower_y + 71 + 12
        Raylib.DrawTriangle(
          Raylib::Vector2.create(ax, ay - 6),
          Raylib::Vector2.create(ax - 6, ay + 4),
          Raylib::Vector2.create(ax + 6, ay + 4),
          color
        )
      end
      if @list_top_index + 5 < @party.length
        alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
        color = Raylib.Fade(Raylib::WHITE, alpha / 255.0)
        ax = @lower_x + 27
        ay = @lower_y + 71 + 4*34 + 12
        Raylib.DrawTriangle(
          Raylib::Vector2.create(ax - 6, ay - 4),
          Raylib::Vector2.create(ax, ay + 6),
          Raylib::Vector2.create(ax + 6, ay - 4),
          color
        )
      end
    else
      super
    end
  end
  
  def confirm_action(item_entry, actor)
    return if item_entry["item"] == "NOTHING"
    @selected_give_item = item_entry
    @give_selected_item_index = @selected_item_index
    @donor_actor = actor
    @donor_items = fill_to_four(find_actor_items(actor["name"]))
    @give_state = :show_message
    force_close
  end

  def reset_give_state
    @selected_give_item = nil
    @give_selected_item_index = nil
    @donor_items = nil
  end

  def reopen_for_selection
    @visible = true
    @anim_phase = 1
    @upper_x = @upper_start_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x
    @blink_timer = 0
    @blink_duration = 0
    @selection_blink_timer = 0
	@status_view_mode = 0   # сброс режима отображения
    update_current_actor
  end
end
# ----------------------------------------------------------------
# class EquipMenu
# ----------------------------------------------------------------
class EquipMenu < ItemSubMenuBase
  def initialize(font, db, party, classes_data, class_names, start_inventory)
    super(:equip, font, db, party, classes_data, class_names, start_inventory)
    @equip_state = :select_actor
    @selected_slot_index = 0
    @available_equipment = []
    @slot_categories = ["Weapon", "Ring"]

    @navigable_indices = []

    @panel_swap_active = false
    @panel_swap_phase  = 0
    @swap_target_state = nil
    @swap_speed = 60

    @pending_ring = false
    @selected_ring_index = 0
    @ring_navigable_indices = []
    @ring_items = []

    @equippable_icon = nil
    if File.exist?("assets/items/Equippable.png")
      @equippable_icon = Raylib.LoadTexture("assets/items/Equippable.png")
      Raylib.SetTextureFilter(@equippable_icon, 0)
    end
  end

  def filter_items(items)
    items
  end

  def weapon_available?
    items = fill_to_four(find_actor_items(@current_actor))
    items.any? do |entry|
      entry["item"] != "NOTHING" &&
      (data = find_item_by_name(entry["item"])) &&
      data["category"] == "Weapon"
    end
  end

  def selecting_actor?
    @equip_state == :select_actor
  end

  def ring_available?
    items = find_actor_items(@current_actor)
    items.any? do |entry|
      entry["item"] != "NOTHING" &&
      (data = find_item_by_name(entry["item"])) &&
      data["category"] == "Ring"
    end
  end

  def open(actor_name = nil)
    super
    @equip_state = :select_actor
    @selected_slot_index = 0
    @focus = :party
    @panel_swap_active = false
    @navigable_indices = []
    @pending_ring = false
    @selected_ring_index = 0
    @ring_navigable_indices = []
    @ring_items = []
  end

  def start_panel_swap(new_state)
    return if @panel_swap_active
    @swap_target_state = new_state
    @panel_swap_active = true
    @panel_swap_phase = 1
    @swap_old_x = @upper_x
    @swap_new_x = 576 + @upper_w
  end

  def build_navigable_indices
    items = fill_to_four(find_actor_items(@current_actor))
    weapon_slots = []
    (0..3).each do |i|
      entry = items[i]
      next if entry["item"] == "NOTHING"
      data = find_item_by_name(entry["item"])
      if data && data["category"] == "Weapon"
        weapon_slots << i
      end
    end
    @navigable_indices = weapon_slots + [4]
    unless @navigable_indices.include?(@selected_slot_index)
      @selected_slot_index = @navigable_indices.first || 4
    end
  end

  # Строим список колец, СОХРАНЯЯ их реальные индексы в инвентаре (0–3)
  def build_ring_navigable_indices
    all_items = find_actor_items(@current_actor)
    @ring_items = Array.new(4) { { "item" => "NOTHING", "equipped" => false } }
    ring_slot_indices = []

    all_items.each_with_index do |entry, idx|
      next if idx >= 4 || entry["item"] == "NOTHING"
      data = find_item_by_name(entry["item"])
      if data && data["category"] == "Ring"
        @ring_items[idx] = entry.merge("original_index" => idx)
        ring_slot_indices << idx
      end
    end

    @ring_navigable_indices = ring_slot_indices + [4]   # 4 = No Ring
    @selected_ring_index = @ring_navigable_indices.first || 4
  end

  def return_to_actor_selection
    @equip_state = :select_actor
    @focus = :party
    @upper_x = @upper_target_x
    @panel_swap_active = false
    @panel_swap_phase = 0
    @pending_ring = false
    update_current_actor
  end

  def proceed_after_weapon
    if @pending_ring && ring_available?
      @pending_ring = false
      build_ring_navigable_indices
      start_panel_swap(:select_ring)
    else
      return_to_actor_selection
    end
  end

  def handle_input
    return unless @visible && @anim_phase == 2
    return if @panel_swap_active

    case @equip_state
    when :select_actor
      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
        has_weapon = weapon_available?
        has_ring = ring_available?

        if has_weapon || has_ring
          @pending_ring = has_ring && has_weapon
          if has_weapon
            $audio.play_sfx(:confirm)
            build_navigable_indices
            start_panel_swap(:select_slot)
          else
            $audio.play_sfx(:confirm)
            build_ring_navigable_indices
            start_panel_swap(:select_ring)
          end
        else
          $audio.play_sfx(:block)
        end
        return
      end
      super

    when :select_slot
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        $audio.play_sfx(:cancel)
        @pending_ring = false
        start_panel_swap(:select_actor)
        return
      end

      if @navigable_indices.any?
        idx = @navigable_indices.index(@selected_slot_index) || 0
        if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_UP)
          new_idx = (idx - 1) % @navigable_indices.size
          @selected_slot_index = @navigable_indices[new_idx]
        elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT) || Raylib.IsKeyPressed(Raylib::KEY_DOWN)
          new_idx = (idx + 1) % @navigable_indices.size
          @selected_slot_index = @navigable_indices[new_idx]
        end
      end

      if Raylib.IsKeyPressed(Raylib::KEY_A)
        actor = @party[@selected_actor_index]
        items = fill_to_four(find_actor_items(@current_actor))

        if @selected_slot_index == 4
          (0..3).each do |i|
            entry = items[i]
            next if entry["item"] == "NOTHING"
            data = find_item_by_name(entry["item"])
            if data && data["category"] == "Weapon"
              entry["equipped"] = false
            end
          end
          update_inventory(actor["id"], items)
          @current_items = items
          $audio.play_sfx(:confirm)
          proceed_after_weapon
        else
          slot = @selected_slot_index
          entry = items[slot]
          if entry && entry["item"] != "NOTHING"
            data = find_item_by_name(entry["item"])
            if data && data["category"] == "Weapon"
              (0..3).each do |i|
                next if i == slot
                e = items[i]
                next if e["item"] == "NOTHING"
                d = find_item_by_name(e["item"])
                if d && d["category"] == "Weapon"
                  e["equipped"] = false
                end
              end
              entry["equipped"] = true
              update_inventory(actor["id"], items)
              @current_items = items
              $audio.play_sfx(:confirm)
              proceed_after_weapon
            else
              proceed_after_weapon
            end
          else
            proceed_after_weapon
          end
        end
      end

    when :select_ring
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        $audio.play_sfx(:cancel)
        return_to_actor_selection
        return
      end

      if @ring_navigable_indices.any?
        idx = @ring_navigable_indices.index(@selected_ring_index) || 0
        if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_UP)
          new_idx = (idx - 1) % @ring_navigable_indices.size
          @selected_ring_index = @ring_navigable_indices[new_idx]
        elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT) || Raylib.IsKeyPressed(Raylib::KEY_DOWN)
          new_idx = (idx + 1) % @ring_navigable_indices.size
          @selected_ring_index = @ring_navigable_indices[new_idx]
        end
      end

      if Raylib.IsKeyPressed(Raylib::KEY_A)
        actor = @party[@selected_actor_index]
        if @selected_ring_index == 4   # No Ring
          items = find_actor_items(@current_actor)
          items.each do |entry|
            next if entry["item"] == "NOTHING"
            data = find_item_by_name(entry["item"])
            entry["equipped"] = false if data && data["category"] == "Ring"
          end
          update_inventory(actor["id"], items)
          @current_items = items
        else
          chosen_entry = @ring_items[@selected_ring_index]
          if chosen_entry && chosen_entry["item"] != "NOTHING"
            items = find_actor_items(@current_actor)
            # Снимаем все кольца
            items.each do |entry|
              next if entry["item"] == "NOTHING"
              data = find_item_by_name(entry["item"])
              entry["equipped"] = false if data && data["category"] == "Ring"
            end
            # Надеваем выбранное, используя его оригинальный индекс
            original_idx = chosen_entry["original_index"]
            if original_idx && items[original_idx]
              items[original_idx]["equipped"] = true
            else
              real_item = items.find { |e| e["item"] == chosen_entry["item"] }
              real_item["equipped"] = true if real_item
            end
            update_inventory(actor["id"], items)
            @current_items = items
          end
        end
        $audio.play_sfx(:confirm)
        return_to_actor_selection
      end

    when :select_item
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        @equip_state = :select_slot
        @focus = :items
        return
      end
      if Raylib.IsKeyPressed(Raylib::KEY_UP)
        @selected_item_index = 0
      elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
        @selected_item_index = 1
      elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @selected_item_index = 2
      elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
        @selected_item_index = 3
      end
      if Raylib.IsKeyPressed(Raylib::KEY_A)
        if @available_equipment && @selected_item_index < @available_equipment.length
          item_entry = @available_equipment[@selected_item_index]
          if item_entry && item_entry["item"] != "NOTHING"
            actor = @party[@selected_actor_index]
            items = fill_to_four(find_actor_items(@current_actor))
            (0..3).each do |i|
              e = items[i]
              next if e["item"] == "NOTHING"
              d = find_item_by_name(e["item"])
              if d && d["category"] == "Weapon"
                e["equipped"] = false
              end
            end
            items[@selected_slot_index] = item_entry.dup
            items[@selected_slot_index]["equipped"] = true
            update_inventory(actor["id"], items)
            @current_items = items
            $audio.play_sfx(:confirm)
            proceed_after_weapon
          end
        end
        @equip_state = :select_slot
        @focus = :items
      end
    end
  end

  def update
    super
    if @panel_swap_active
      case @panel_swap_phase
      when 1
        @upper_x += @swap_speed
        if @upper_x >= @upper_start_x
          @upper_x = @upper_start_x
          @equip_state = @swap_target_state
          @focus = (@equip_state == :select_actor ? :party : :items)
          @upper_x = @swap_new_x
          @panel_swap_phase = 2
        end
      when 2
        @upper_x -= @swap_speed
        if @upper_x <= @upper_target_x
          @upper_x = @upper_target_x
          @panel_swap_active = false
          @panel_swap_phase = 0
        end
      end
    end
  end

  def draw_upper_content
    if @equip_state == :select_slot
      actor_data = @party[@selected_actor_index]
      return unless actor_data
      items = fill_to_four(find_actor_items(@current_actor))

      klass = @classes_data.find { |c| c["id"] == actor_data["class_id"] }
      if klass && @db
        lv = actor_data["level"] || 1
        base_atk = [@db.stat_at_level(klass["attack_growth"], lv), 0].max
        base_def = [@db.stat_at_level(klass["defense_growth"], lv), 0].max
        base_agi = [@db.stat_at_level(klass["agility_growth"], lv), 0].max
        base_mov = [(klass["move"] || 0), 0].max
      else
        base_atk = base_def = base_agi = base_mov = 0
      end

      if @selected_slot_index == 4
        future_atk = base_atk
        future_def = base_def
      else
        entry = items[@selected_slot_index]
        bonus_atk = 0
        bonus_def = 0
        if entry && entry["item"] != "NOTHING"
          data = find_item_by_name(entry["item"])
          if data && data["category"] == "Weapon"
            bonus_atk = (data["attack"] || 0).to_i
            bonus_def = (data["defense"] || 0).to_i
          end
        end
        future_atk = base_atk + bonus_atk
        future_def = base_def + bonus_def
      end

      future_atk = [future_atk, 0].max
      future_def = [future_def, 0].max

      class_id   = actor_data["class_id"]
      class_name = @class_names[class_id] || "???"
      level = [(actor_data["level"] || 1), 1].max
      header     = "#{actor_data["name"]}  #{class_name}  LV #{level}"
      draw_text_custom(header, @upper_x + 25, @upper_y + 12, 20, WHITE)

      if @selected_slot_index == 4
        draw_text_custom("Unarmed", @upper_x + 25, @upper_y + 35, 18, YELLOW)
      else
        current_item = items[@selected_slot_index]
        eq_text = current_item && current_item["item"] != "NOTHING" ? "Equipped: #{current_item["item"]}" : "Empty"
        draw_text_custom(eq_text, @upper_x + 25, @upper_y + 35, 18, YELLOW)
      end

      base_x = @upper_x + 40
      base_y = @upper_y + 60
      offset_x = 44
      offset_y = 42
      icon_positions = [
        { x: base_x + offset_x, y: base_y + 8 },
        { x: base_x,            y: base_y + offset_y },
        { x: base_x + offset_x * 2, y: base_y + offset_y },
        { x: base_x + offset_x, y: base_y + offset_y * 2 - 8 },
        { x: base_x + offset_x * 3, y: base_y + offset_y }
      ]

      5.times do |i|
        ipos = icon_positions[i]
        if i == 4
          icon_tex = @equippable_icon || @empty_item_tex
        else
          item_entry = items[i]
          if item_entry && item_entry["item"] != "NOTHING"
            item_data = find_item_by_name(item_entry["item"])
            if item_data && item_data["category"] == "Weapon"
              icon_path = item_data["icon"]
              icon_tex = load_icon(icon_path) || @empty_item_tex
            else
              icon_tex = @empty_item_tex
            end
          else
            icon_tex = @empty_item_tex
          end
        end
        if icon_tex
          src = Raylib::Rectangle.create(0, 0, 32, 48)
          dst = Raylib::Rectangle.create(ipos[:x], ipos[:y], 32, 48)
          Raylib.DrawTexturePro(icon_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
        end

        draw_frame = (i == @selected_slot_index)
        if draw_frame
          alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
          color = Raylib.Fade(Raylib::GREEN, alpha / 255.0)
          Raylib.DrawRectangleLinesEx(
            Raylib::Rectangle.create(ipos[:x] - 1, ipos[:y] - 1, 34, 50),
            2.0, color
          )
        end
      end

      text_x = @upper_x + 240
      text_y = @upper_y + 58
      line_h = 36

      # Бонусы от выбранного оружия (если слот не "Unarmed")
      if @selected_slot_index == 4
        bonus_att = 0
        bonus_def = 0
        bonus_agi = 0
        bonus_mov = 0
      else
        entry = items[@selected_slot_index]
        bonus_att = 0
        bonus_def = 0
        bonus_agi = 0
        bonus_mov = 0
        if entry && entry["item"] != "NOTHING"
          data = find_item_by_name(entry["item"])
          if data && data["category"] == "Weapon"
            bonus_att = (data["attack"]  || 0).to_i
            bonus_def = (data["defense"] || 0).to_i
            bonus_agi = (data["agility"] || 0).to_i
            bonus_mov = (data["move"]    || 0).to_i
          end
        end
      end

      future_att = [base_atk + bonus_att, 0].max
      future_def = [base_def + bonus_def, 0].max
      future_agi = [base_agi + bonus_agi, 0].max
      future_mov = [base_mov + bonus_mov, 0].max

      draw_text_custom("ATT #{future_att}", text_x, text_y, 18, WHITE)
      draw_text_custom("DEF #{future_def}", text_x, text_y + line_h, 18, WHITE)
      draw_text_custom("AGI #{future_agi}", text_x, text_y + line_h*2, 18, WHITE)
      draw_text_custom("MOV #{future_mov}", text_x, text_y + line_h*3, 18, WHITE)

    elsif @equip_state == :select_ring
      actor_data = @party[@selected_actor_index]
      return unless actor_data

      klass = @classes_data.find { |c| c["id"] == actor_data["class_id"] }
      if klass && @db
        lv = actor_data["level"] || 1
        base_atk = [@db.stat_at_level(klass["attack_growth"], lv), 0].max
        base_def = [@db.stat_at_level(klass["defense_growth"], lv), 0].max
        base_agi = [@db.stat_at_level(klass["agility_growth"], lv), 0].max
        base_mov = [(klass["move"] || 0), 0].max
      else
        base_atk = base_def = base_agi = base_mov = 0
      end

      items = find_actor_items(@current_actor)
      equipped_ring = items.find do |entry|
        next false if entry["item"] == "NOTHING"
        data = find_item_by_name(entry["item"])
        data && data["category"] == "Ring" && entry["equipped"]
      end
      ring_name = equipped_ring ? equipped_ring["item"] : "(Empty)"

      class_name = @class_names[actor_data["class_id"]] || "???"
      lv = [(actor_data["level"] || 1), 1].max
      header = "#{actor_data["name"]}  #{class_name}  LV #{lv}"
      draw_text_custom(header, @upper_x + 25, @upper_y + 12, 20, WHITE)
      draw_text_custom("Ring : #{ring_name}", @upper_x + 25, @upper_y + 35, 18, YELLOW)

      # Иконки колец (4 фиксированных слота, кольца остаются на своих реальных индексах!)
      base_x = @upper_x + 40
      base_y = @upper_y + 60
      offset_x = 44
      offset_y = 42
      icon_positions = [
        { x: base_x + offset_x, y: base_y + 8 },
        { x: base_x,            y: base_y + offset_y },
        { x: base_x + offset_x * 2, y: base_y + offset_y },
        { x: base_x + offset_x, y: base_y + offset_y * 2 - 8 },
        { x: base_x + offset_x * 3, y: base_y + offset_y }
      ]

      5.times do |i|
        ipos = icon_positions[i]
        if i == 4
          icon_tex = @equippable_icon || @empty_item_tex
        else
          entry = @ring_items[i]   # i от 0 до 3, берём из подготовленного массива
          if entry && entry["item"] != "NOTHING"
            item_data = find_item_by_name(entry["item"])
            icon_path = item_data ? item_data["icon"] : nil
            icon_tex = load_icon(icon_path) || @empty_item_tex
          else
            icon_tex = @empty_item_tex
          end
        end
        if icon_tex
          src = Raylib::Rectangle.create(0, 0, 32, 48)
          dst = Raylib::Rectangle.create(ipos[:x], ipos[:y], 32, 48)
          Raylib.DrawTexturePro(icon_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
        end

        if i == @selected_ring_index
          alpha = (Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6) * 255
          color = Raylib.Fade(Raylib::GREEN, alpha / 255.0)
          Raylib.DrawRectangleLinesEx(
            Raylib::Rectangle.create(ipos[:x] - 1, ipos[:y] - 1, 34, 50),
            2.0, color
          )
        end
      end

      # Статы в правой колонке (ATT, DEF, AGI, MOV)
      text_x = @upper_x + 240
      text_y = @upper_y + 58
      line_h = 36

      # Текущие бонусы только от оружия (без колец)
      w_bonus = weapon_bonuses(actor_data)
      weapon_att = base_atk + w_bonus[:attack]
      weapon_def = base_def + w_bonus[:defense]
      weapon_agi = base_agi   # оружие не даёт AGI
      weapon_mov = base_mov   # оружие не даёт MOV

      # Бонусы от выбранного кольца
      if @selected_ring_index == 4
        ring_bonus_att = 0
        ring_bonus_def = 0
        ring_bonus_agi = 0
        ring_bonus_mov = 0
      else
        chosen = @ring_items[@selected_ring_index]
        ring_bonus_att = 0
        ring_bonus_def = 0
        ring_bonus_agi = 0
        ring_bonus_mov = 0
        if chosen && chosen["item"] != "NOTHING"
          data = find_item_by_name(chosen["item"])
          if data
            ring_bonus_att = (data["attack"]  || 0).to_i
            ring_bonus_def = (data["defense"]  || 0).to_i
            ring_bonus_agi = (data["agility"]  || 0).to_i
            ring_bonus_mov = (data["move"]     || 0).to_i
          end
        end
      end

      future_att = [weapon_att + ring_bonus_att, 0].max
      future_def = [weapon_def + ring_bonus_def, 0].max
      future_agi = [weapon_agi + ring_bonus_agi, 0].max
      future_mov = [weapon_mov + ring_bonus_mov, 0].max

      draw_text_custom("ATT #{future_att}", text_x, text_y, 18, WHITE)
      draw_text_custom("DEF #{future_def}", text_x, text_y + line_h, 18, WHITE)
      draw_text_custom("AGI #{future_agi}", text_x, text_y + line_h*2, 18, WHITE)
      draw_text_custom("MOV #{future_mov}", text_x, text_y + line_h*3, 18, WHITE)

    else
      super
    end
  end

  private

  def update_equipped_info
  end

# Бонусы только от экипированного оружия
  def weapon_bonuses(actor)
    items = find_actor_items(actor["name"])
    atk = 0
    defn = 0
    agi = 0
    mov = 0
    items.each do |entry|
      next if entry["item"] == "NOTHING"
      next unless entry["equipped"]
      data = find_item_by_name(entry["item"])
      next unless data && data["category"] == "Weapon"
      atk += (data["attack"]  || 0).to_i
      defn += (data["defense"] || 0).to_i
      agi  += (data["agility"] || 0).to_i
      mov  += (data["move"]    || 0).to_i
    end
    { attack: atk, defense: defn, agility: agi, move: mov }
  end

  def update_inventory(actor_id, items)
    inv = @start_inventory.find { |inv| inv["actor_id"] == actor_id }
    if inv
      inv["items"] = items
    end
  end

  def fill_to_four(arr)
    arr = arr.dup
    while arr.length < 4
      arr << { "item" => "NOTHING", "equipped" => false }
    end
    arr
  end
end
# ----------------------------------------------------------------
# class DropMenu
# ----------------------------------------------------------------
class DropMenu < ItemSubMenuBase
  SHOW_DURATION = 180   # больше не используется для автоудаления
  RESULT_DURATION = 120

  def initialize(font, large_font, db, party, classes_data, class_names, start_inventory, game_text)
    super(:drop, font, db, party, classes_data, class_names, start_inventory)
    @large_font = large_font
    @game_text = game_text
    @drop_state = :select_item
    @selected_drop_item = nil
    @selected_drop_actor = nil
    @message_timer = 0
    @message_panel_tex = nil
    @confirm_index = 0
    @confirm_anim_timer = 0

    # Загружаем статические текстуры подтверждения
    @yes_tex = nil
    @no_tex = nil
    if File.exist?("assets/ui/menu/Yes.png")
      @yes_tex = Raylib.LoadTexture("assets/ui/menu/Yes.png")
      Raylib.SetTextureFilter(@yes_tex, 0) if @yes_tex
    end
    if File.exist?("assets/ui/menu/No.png")
      @no_tex = Raylib.LoadTexture("assets/ui/menu/No.png")
      Raylib.SetTextureFilter(@no_tex, 0) if @no_tex
    end

    # Загружаем анимированные текстуры (если есть)
    @yes_anim_tex = nil
    @no_anim_tex = nil
    if File.exist?("assets/ui/menu/Yes_Anim.png")
      @yes_anim_tex = Raylib.LoadTexture("assets/ui/menu/Yes_Anim.png")
      Raylib.SetTextureFilter(@yes_anim_tex, 0) if @yes_anim_tex
    end
    if File.exist?("assets/ui/menu/No_Anim.png")
      @no_anim_tex = Raylib.LoadTexture("assets/ui/menu/No_Anim.png")
      Raylib.SetTextureFilter(@no_anim_tex, 0) if @no_anim_tex
    end

    load_message_texture
  end

  def load_message_texture
    @message_panel_tex = Raylib.LoadTexture("assets/ui/message_panel.png")
    Raylib.SetTextureFilter(@message_panel_tex, 0) if @message_panel_tex
  end

  def open(actor_name = nil)
    super
    @drop_state = :select_item
    @selected_drop_item = nil
    @selected_drop_actor = nil
    @message_timer = 0
    @confirm_index = 0
    @confirm_anim_timer = 0
    @focus = :party
  end

  def handle_input
    case @drop_state
    when :select_item
      super
    when :show_message
      if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @confirm_index = 1 - @confirm_index
      elsif Raylib.IsKeyPressed(Raylib::KEY_S)
        @drop_state = :select_item
        @selected_drop_item = nil
        @selected_drop_actor = nil
        @focus = :party
        reopen_for_selection
      elsif Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
        if @confirm_index == 0   # Yes
          perform_drop
          @drop_state = :result_message
          @message_timer = 0
        else                    # No
          @drop_state = :select_item
          @selected_drop_item = nil
          @selected_drop_actor = nil
          @focus = :party
          reopen_for_selection
        end
      end
    when :result_message
      # ничего не делаем
    end
  end

  def update
    super
    case @drop_state
    when :show_message
      @confirm_anim_timer += 1
    when :result_message
      @message_timer += 1
      if @message_timer >= RESULT_DURATION
        @drop_state = :select_item
        @selected_drop_item = nil
        @selected_drop_actor = nil
        @focus = :party
        update_current_actor
        reopen_for_selection
      end
    end
  end

  def draw
    case @drop_state
    when :select_item then super
    when :show_message, :result_message then draw_message
    end
  end

  def confirm_action(item_entry, actor)
    return if item_entry["item"] == "NOTHING"
    @selected_drop_item = item_entry
    @selected_drop_actor = actor
    @drop_state = :show_message
    @message_timer = 0
    @confirm_index = 0
    @confirm_anim_timer = 0
    force_close
  end

  private

  def perform_drop
    return unless @selected_drop_item && @selected_drop_actor

    actor_id = @selected_drop_actor["id"]
    inv_entry = @start_inventory.find { |inv| inv["actor_id"] == actor_id }
    return unless inv_entry

    items = inv_entry["items"]
    idx = items.index { |entry| entry["item"] == @selected_drop_item["item"] }
    if idx
      if items[idx]["equipped"]
        items[idx]["equipped"] = false
      end
      items.delete_at(idx)
      while items.length < 4
        items << { "item" => "NOTHING", "equipped" => false }
      end
    end
    inv_entry["items"] = items
  end

  def draw_message
    template = case @drop_state
               when :show_message
                 @game_text["0003"] || "The {ITEM} {N}will be discarded. OK ?"
               when :result_message
                 @game_text["0004"] || "The {ITEM} {N}is discarded."
               else
                 ""
               end

    item_name = @selected_drop_item ? @selected_drop_item["item"] : "???"
    text = template.gsub("{ITEM}", item_name)
    lines = text.split("{N}")

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
      Raylib.DrawTextEx(@large_font, line_text, Raylib::Vector2.create(panel_x + 40, y_offset), 30, 1, Raylib::WHITE)
      y_offset += 38
    end

    # Иконки Yes/No только в состоянии подтверждения
    if @drop_state == :show_message
      icon_size = 48
      spacing = 20
      total_width = icon_size * 2 + spacing
      start_x = panel_x + (panel_w - total_width) / 2
      icon_y = panel_y - icon_size - 10

      # --- YES ---
      if @yes_tex
        tex_to_draw = @yes_tex
        if @confirm_index == 0 && @yes_anim_tex
          if (@confirm_anim_timer % 24) < 12
            tex_to_draw = @yes_anim_tex
          end
        end
        src = Raylib::Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
        dst = Raylib::Rectangle.create(start_x, icon_y, icon_size, icon_size)
        color = (@confirm_index == 0) ? Raylib::WHITE : Raylib::GRAY
        Raylib.DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0,0), 0, color)
        # Рамка только если нет анимационной текстуры или всегда? Оставим для наглядности, но если есть анимация, рамка может быть лишней.
        # Поэтому рисуем рамку только если нет анимированной текстуры
        if @confirm_index == 0 && !@yes_anim_tex
          Raylib.DrawRectangleLinesEx(dst, 2, Raylib::GREEN)
        end
      end

      # --- NO ---
      if @no_tex
        tex_to_draw = @no_tex
        if @confirm_index == 1 && @no_anim_tex
          if (@confirm_anim_timer % 24) < 12
            tex_to_draw = @no_anim_tex
          end
        end
        src = Raylib::Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
        dst = Raylib::Rectangle.create(start_x + icon_size + spacing, icon_y, icon_size, icon_size)
        color = (@confirm_index == 1) ? Raylib::WHITE : Raylib::GRAY
        Raylib.DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0,0), 0, color)
        if @confirm_index == 1 && !@no_anim_tex
          Raylib.DrawRectangleLinesEx(dst, 2, Raylib::GREEN)
        end
      end
    end
  end

  def reopen_for_selection
    @visible = true
    @anim_phase = 1
    @upper_x = @upper_start_x
    @lower_y = @lower_start_y
    @portrait_x = @portrait_start_x
    @frame_x = @frame_start_x
    @blink_timer = 0
    @blink_duration = 0
    @selection_blink_timer = 0
	@status_view_mode = 0   # сброс режима отображения
    update_current_actor
  end
end