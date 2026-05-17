# lib/battleManager/Battle_scenes.rb

class BattleScene
  SCENE_DURATION      = 3.0
  DELAY_DURATION      = 0.4
  END_DELAY_DURATION  = 0.5
  BAR_HEIGHT          = 144
  TILE_SIZE           = 48

  WORK_WIDTH  = 1152
  WORK_HEIGHT = 960 - 2 * BAR_HEIGHT   # 672

  ENEMY_X = 124
  ENEMY_Y = 370

  ALLY_X = 750
  ALLY_Y = 480

  # ─── Настройки отрисовки земли (ground) ───
  GROUND_SCALE   = 2.0    # ×2 для рендера 1152×960 → на экране 576×480
  GROUND_OFFSET_X = 136
  GROUND_OFFSET_Y = 422

  # ─── Анимация выезда игрока и панели ───
  SLIDE_IN_DURATION  = 0.5   # секунд
  SLIDE_IN_OFFSET_X  = 500   # на сколько пикселей правее стартуют

  # ─── Текстуры и параметры палочек HP/MP (sticks) ───
  STICK_W = 3
  STICK_H = 26
  STICK_TEXTURE_PATHS = [
    "assets/ui/Hp_Mp_Points_big.png",      # жёлтый (0-100)
    "assets/ui/Hp_Mp_Points_2_big.png",    # зелёный (100-200)
    "assets/ui/Hp_Mp_Points_3_big.png",    # фиолетовый (200-300)
    "assets/ui/Hp_Mp_Points_4_big.png"     # чёрный (300+)
  ]
  LEFT_EDGE_W  = 12   # левый край панели (как в  hp_mp_panel)
  RIGHT_EDGE_W = 12   # правый край панели
  MID_TILE_W   = 8    # ширина тайла для растягивания середины

  def initialize(battle_manager)
    @battle_manager = battle_manager
    @timer = 0.0
    @active = false
    @finished = false
    @phase = nil
    @background_tex = nil
    @render_texture = nil
    @panel_tex = nil
    @ground_tex = nil

    @attacker = nil
    @defender = nil
    @anim_timer = 0.0

    @attacker_current_frame = 0
    @defender_current_frame = 0

    @sub_phase = :idle_before
    @sub_phase_timer = 0.0
    @attack_duration = 0.0
    @defense_duration = 0.0
    @idle_duration = 0.0

    @slide_in = false
    @slide_in_timer = 0.0

    # Палочки HP/MP
    @stick_textures = []
    load_stick_textures

    load_background
  end

  # ---------- Загрузка фона битвы ----------
  def load_background
    bg_path = @battle_manager.battle_entry["background"]
    return unless bg_path && !bg_path.empty? && File.exist?(bg_path)
    img = Raylib.LoadImage(bg_path)
    @background_tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(@background_tex, Raylib::TEXTURE_FILTER_POINT)
  rescue => e
    puts "Failed to load battle background: #{e.message}"
  end

  # ---------- Загрузка земли (ground) ----------
  def load_ground
    ground_path = @battle_manager.battle_entry["ground"]
    return unless ground_path && !ground_path.empty? && File.exist?(ground_path)

    img = Raylib.LoadImage(ground_path)
    @ground_tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(@ground_tex, Raylib::TEXTURE_FILTER_POINT)
  rescue => e
    puts "Failed to load ground texture: #{e.message}"
  end

  # ---------- Загрузка палочек HP/MP ----------
  def load_stick_textures
    STICK_TEXTURE_PATHS.each do |path|
      if File.exist?(path)
        img = Raylib.LoadImage(path)
        tex = Raylib.LoadTextureFromImage(img)
        Raylib.UnloadImage(img)
        Raylib.SetTextureFilter(tex, Raylib::TEXTURE_FILTER_POINT)
        @stick_textures << tex
      else
        @stick_textures << nil
        puts "WARNING: stick texture not found: #{path}"
      end
    end
  end

  def start(attacker_unit, defender_unit)
    @attacker = attacker_unit
    @defender = defender_unit
    @timer = DELAY_DURATION
    @active = true
    @finished = false
    @phase = :delay
    @render_texture = Raylib.LoadRenderTexture(1152, 960)

    @anim_timer = 0.0
    @attacker_current_frame = 0
    @defender_current_frame = 0

    attacker_anim = @attacker ? @attacker[:battle_anim] : nil
    defender_anim = @defender ? @defender[:battle_anim] : nil

    @attack_duration = attacker_anim ? attacker_anim.total_duration(:attack) : 0.5
    @defense_duration = defender_anim ? defender_anim.total_duration(:defense) : 0.5
    @idle_duration = attacker_anim ? attacker_anim.total_duration(:idle) : 0.8

    @attack_duration = 0.5 if @attack_duration < 0.1
    @defense_duration = 0.5 if @defense_duration < 0.1
    @idle_duration = 0.8 if @idle_duration < 0.1

    @sub_phase = :slide_in           # начинаем с выезда
    @sub_phase_timer = 0.0
    @slide_in = true
    @slide_in_timer = 0.0

    load_ground

    puts ">>> Battle scene starting: #{attacker_unit[:enemy] ? 'Enemy' : 'Ally'} vs #{defender_unit[:enemy] ? 'Enemy' : 'Ally'}"
    puts "    attack: #{@attack_duration.round(2)}s, defense: #{@defense_duration.round(2)}s, idle: #{@idle_duration.round(2)}s"
  end

  def finish
    return if @finished
    @finished = true
    @active = false
    @phase = nil
    Raylib.UnloadRenderTexture(@render_texture) if @render_texture
    @render_texture = nil

    if @ground_tex
      Raylib.UnloadTexture(@ground_tex)
      @ground_tex = nil
    end

    if @panel_tex
      Raylib.UnloadTexture(@panel_tex)
      @panel_tex = nil
    end

    @battle_manager.return_from_battle_scene
    puts "<<< Battle scene finished"
  end

  def update
    return unless @active
    dt = Raylib.GetFrameTime()
    @timer -= dt

    if @phase == :display
      update_sub_phase(dt)
      update_animation(dt)
    end

    case @phase
    when :delay
      if @timer <= 0
        @timer = SCENE_DURATION
        @phase = :display
      end
    when :display
      if @timer <= 0
        @timer = END_DELAY_DURATION
        @phase = :end_delay
      end
    when :end_delay
      finish if @timer <= 0
    end
  end

  def draw
    return unless @active && @render_texture

    Raylib.BeginTextureMode(@render_texture)

    case @phase
    when :delay, :end_delay
      Raylib.ClearBackground(Raylib::BLACK)
    when :display
      Raylib.ClearBackground(Raylib::BLACK)

      # Фон битвы
      if @background_tex
        src = Raylib::Rectangle.create(0, 0, @background_tex.width, @background_tex.height)
        dst = Raylib::Rectangle.create(0, BAR_HEIGHT, WORK_WIDTH, WORK_HEIGHT)
        Raylib.DrawTexturePro(@background_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      # Чёрные полосы сверху и снизу (под панели)
      Raylib.DrawRectangle(0, 0, WORK_WIDTH, BAR_HEIGHT, Raylib::BLACK)
      Raylib.DrawRectangle(0, 960 - BAR_HEIGHT, WORK_WIDTH, BAR_HEIGHT, Raylib::BLACK)

      # ──────────────────────────────────────
      #  Вычисляем смещение для анимации выезда (только для союзника и его земли)
      # ──────────────────────────────────────
      slide_offset = 0
      if @sub_phase == :slide_in
        t = (@sub_phase_timer / SLIDE_IN_DURATION).clamp(0.0, 1.0)
        ease = 1.0 - (1.0 - t) ** 2   # ease out quad
        slide_offset = SLIDE_IN_OFFSET_X * (1.0 - ease)
      end

      # ──────────────────────────────────────
      #  GROUND – только под союзником, ×2
      # ──────────────────────────────────────
      if @attacker && !@attacker[:enemy] && @ground_tex
        gw = @ground_tex.width
        gh = @ground_tex.height
        scaled_w = gw * GROUND_SCALE
        scaled_h = gh * GROUND_SCALE

        gx = ALLY_X - scaled_w / 2 + GROUND_OFFSET_X + slide_offset   # земля тоже сдвигается
        gy = ALLY_Y - scaled_h + GROUND_OFFSET_Y

        src = Raylib::Rectangle.create(0, 0, gw, gh)
        dst = Raylib::Rectangle.create(gx, gy, scaled_w, scaled_h)
        Raylib.DrawTexturePro(@ground_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      # ── Рисуем юнитов (враг без смещения, союзник с учётом slide_offset) ──
      ally_draw_x = ALLY_X + slide_offset
      if @sub_phase == :attack
        draw_unit(@attacker, :attack, ally_draw_x, ALLY_Y, false, @attacker_current_frame, true)
        draw_unit(@defender, :defense, ENEMY_X, ENEMY_Y, false, @defender_current_frame, true)
      else
        draw_unit(@attacker, :idle,    ally_draw_x, ALLY_Y, false, @attacker_current_frame, true)
        draw_unit(@defender, :idle,    ENEMY_X, ENEMY_Y, false, @defender_current_frame, true)
      end

      # === Панели HP/MP ===
      # Панель союзника — без выезда (фиксированная позиция)
      if @attacker && @attacker[:max_hp]
        load_panel_texture
        if @panel_tex
          panel_w = @panel_tex.width
          panel_h = @panel_tex.height
          panel_x = WORK_WIDTH - 16 - panel_w               # базовая позиция без slide_offset
          panel_y = (BAR_HEIGHT - panel_h) / 2
          draw_panel_with_sprite(@attacker, panel_x + panel_w, panel_y, true)
        end
      end

      # Панель врага — как и раньше, без смещения
      if @defender && @defender[:max_hp]
        load_panel_texture
        if @panel_tex
          panel_w = @panel_tex.width
          panel_h = @panel_tex.height
          panel_x = 16
          panel_y = 960 - BAR_HEIGHT + (BAR_HEIGHT - panel_h) / 2
          draw_panel_with_sprite(@defender, panel_x, panel_y, false)
        end
      end
    end

    Raylib.EndTextureMode()

    # Масштабирование в окно 576x480
    src = Raylib::Rectangle.create(0, 0, 1152, -960)
    dst = Raylib::Rectangle.create(0, 0, 576, 480)
    Raylib.DrawTexturePro(@render_texture.texture, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end

  private

  # ---------- Загрузка текстуры панели ----------
  def load_panel_texture
    return if @panel_tex
    path = "assets/ui/HpMpPanel_x2.png"
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      @panel_tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(@panel_tex, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: #{path} not found, HP/MP panel disabled."
    end
  end

  def measure_text_ex(text, font, font_size)
    if font
      Raylib.MeasureTextEx(font, text, font_size, 1).x
    else
      Raylib.MeasureText(text, font_size)
    end
  end

  # ---------- Отрисовка панели с текстом и полосками ----------
    def draw_panel_with_sprite(unit, x, y, right_aligned)
    return unless @panel_tex

    font_size = 36
    text_margin_x = 20
    text_margin_y_name = 10

    font = @battle_manager.battle_scene_font || @battle_manager.instance_variable_get(:@font)

    name = if unit[:actor]
             unit[:actor]["name"] || "Ally"
           elsif unit[:enemy]
             unit[:enemy]["name"] || "Enemy"
           else
             "Unit"
           end

    hp = unit[:hp].to_i
    max_hp = unit[:max_hp].to_i
    mp = unit[:mp].to_i
    max_mp = unit[:max_mp].to_i

    # ─── Расчёт ширины панели под содержимое ───
    label_w = [measure_text_ex("HP", font, font_size), measure_text_ex("MP", font, font_size)].max
    max_hp_str = "#{max_hp}/#{max_hp}"
    max_mp_str = "#{max_mp}/#{max_mp}"
    max_number_w = [measure_text_ex(max_hp_str, font, font_size), measure_text_ex(max_mp_str, font, font_size)].max

    max_sticks = [[hp, mp].max, 100].min
    sticks_w = max_sticks * STICK_W

    # Ширина содержимого: метка + палочки + число + отступы
    content_w = text_margin_x + label_w + 8 + sticks_w + 8 + max_number_w + text_margin_x

    # Ширина панели с учётом тайлов (как в старом hp_mp_panel.rb)
    base_w = 170
    raw_w = [base_w, content_w].max
    mid_area = raw_w - LEFT_EDGE_W - RIGHT_EDGE_W
    tiles = (mid_area.to_f / MID_TILE_W).ceil
    panel_w = LEFT_EDGE_W + RIGHT_EDGE_W + tiles * MID_TILE_W

    # Позиция панели 
    if right_aligned
      px = x - panel_w
    else
      px = x
    end
    py = y

    # ─── Рисуем фон панели (тайлинг) ───
    tex = @panel_tex
    if tex
      # Левый край
      left_src = Raylib::Rectangle.create(0, 0, LEFT_EDGE_W, tex.height)
      left_dst = Raylib::Rectangle.create(px, py, LEFT_EDGE_W, tex.height)
      Raylib.DrawTexturePro(tex, left_src, left_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

      # Правый край (сдвинут на panel_w - RIGHT_EDGE_W)
      right_src = Raylib::Rectangle.create(tex.width - RIGHT_EDGE_W, 0, RIGHT_EDGE_W, tex.height)
      right_dst = Raylib::Rectangle.create(px + panel_w - RIGHT_EDGE_W, py, RIGHT_EDGE_W, tex.height)
      Raylib.DrawTexturePro(tex, right_src, right_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

      # Середина (повторяем тайл)
      mid_src = Raylib::Rectangle.create(LEFT_EDGE_W + 2, 0, MID_TILE_W, tex.height)  # +2 чтобы избежать швов
      tiles.times do |i|
        tile_x = px + LEFT_EDGE_W + i * MID_TILE_W
        tile_dst = Raylib::Rectangle.create(tile_x, py, MID_TILE_W, tex.height)
        Raylib.DrawTexturePro(tex, mid_src, tile_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
      end
    else
      # Если текстура не загружена – фон
      Raylib.DrawRectangle(px, py, panel_w, tex.height, Raylib::Fade(Raylib::BLACK, 0.8))
    end

    # ─── Рисуем текст и палочки ───
    tx = px + text_margin_x
    ty_name = py + text_margin_y_name
    ty_hp   = py + 44   # те же значения, что и раньше
    ty_mp   = py + 78   # MP на 78 (без изменений)

    # Имя
    if font
      Raylib.DrawTextEx(font, name, Raylib::Vector2.create(tx, ty_name), font_size, 1, Raylib::WHITE)
    else
      Raylib.DrawText(name, tx, ty_name, font_size, Raylib::WHITE)
    end

    # HP и MP
    bar_area_x = tx
    bar_area_w = panel_w - text_margin_x * 2
    draw_stick_bar(bar_area_x, ty_hp, bar_area_w, hp, max_hp, font, font_size, "HP")
    draw_stick_bar(bar_area_x, ty_mp, bar_area_w, mp, max_mp, font, font_size, "MP")
  end

  # ---------- Рисование шкалы (HP или MP) ----------
 def draw_stick_bar(base_x, base_y, area_width, current, max_val, font, font_size, label)
  # Метка "HP"/"MP"
  if font
    Raylib.DrawTextEx(font, label, Raylib::Vector2.create(base_x, base_y), font_size, 1, Raylib::WHITE)
  else
    Raylib.DrawText(label, base_x, base_y, font_size, Raylib::WHITE)
  end

  label_w = measure_text_ex(label, font, font_size)
  number_str = "#{current}/#{max_val}"
  number_w = measure_text_ex(number_str, font, font_size)

  gap = 8
  available_sticks_w = area_width - label_w - number_w - gap * 2
  total_positions = [(available_sticks_w / STICK_W).floor, 100].min
  total_positions = 0 if total_positions < 0

  # Сколько всего палочек можем нарисовать (но не больше current/max)
  sticks_to_draw = [current, max_val, total_positions].min

  bar_start_x = base_x + label_w + gap
  stick_y = base_y + (font_size - STICK_H) / 2

  # Слои: жёлтый (0-100), зелёный (100-200), фиолетовый (200-300), чёрный (>300)
  limits = [100, 200, 300, 9999]
  prev_limit = 0

  limits.each_with_index do |limit, idx|
    # Сколько палочек этого слоя: от 0 до min(current, limit) минус уже нарисованные
    layer_max = [current, limit].min - prev_limit
    layer_max = 0 if layer_max < 0
    count = [sticks_to_draw, layer_max].min
    next if count <= 0

    tex = @stick_textures[idx]
    (0...count).each do |i|
      stick_x = bar_start_x + i * STICK_W   # рисуем всегда с начала, перекрывая предыдущий слой
      if tex
        src = Raylib::Rectangle.create(0, 0, tex.width, tex.height)
        dst = Raylib::Rectangle.create(stick_x, stick_y, STICK_W, STICK_H)
        Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end
    end
    prev_limit = limit
  end

  # Число справа от палочек
  number_x = bar_start_x + total_positions * STICK_W + gap
  if font
    Raylib.DrawTextEx(font, number_str, Raylib::Vector2.create(number_x, base_y), font_size, 1, Raylib::WHITE)
  else
    Raylib.DrawText(number_str, number_x, base_y, font_size, Raylib::WHITE)
  end
end

  # ─── Методы анимации (обновлённые с поддержкой slide_in) ───
  def update_sub_phase(dt)
    @sub_phase_timer += dt
    case @sub_phase
    when :slide_in
      if @sub_phase_timer >= SLIDE_IN_DURATION
        @sub_phase = :idle_before
        @sub_phase_timer = 0.0
        @slide_in = false
      end
    when :idle_before
      if @sub_phase_timer >= @idle_duration
        @sub_phase = :attack
        @sub_phase_timer = 0.0
        @attacker_current_frame = 0
        @defender_current_frame = 0
      end
    when :attack
      if @sub_phase_timer >= @attack_duration
        @sub_phase = :idle_after
        @sub_phase_timer = 0.0
        @attacker_current_frame = 0
        @defender_current_frame = 0
      end
    when :idle_after
      # idle до конца сцены
    end
  end

  def update_animation(dt)
    @anim_timer += dt
    case @sub_phase
    when :slide_in, :idle_before, :idle_after
      @attacker_current_frame = advance_frame(@attacker, :idle, @attacker_current_frame)
      @defender_current_frame = advance_frame(@defender, :idle, @defender_current_frame)
    when :attack
      @attacker_current_frame = advance_frame(@attacker, :attack, @attacker_current_frame)
      @defender_current_frame = advance_frame(@defender, :defense, @defender_current_frame)
    end
  end

  def advance_frame(unit, anim_key, current_frame)
    return 0 unless unit
    anim = unit[:battle_anim]
    return 0 unless anim
    anim_data = anim.send(anim_key)
    return 0 unless anim_data && !anim_data[:frames].empty?
    frames = anim_data[:frames]
    frame_info = frames[current_frame % frames.size]
    if @anim_timer >= frame_info[:duration]
      @anim_timer -= frame_info[:duration]
      (current_frame + 1) % frames.size
    else
      current_frame
    end
  end

  def draw_unit(unit, anim_key, base_x, base_y, flip_h, frame_idx = 0, use_top_left = false)
    return unless unit
    anim = unit[:battle_anim]
    if anim
      anim_data = anim.send(anim_key)
      if anim_data && !anim_data[:frames].empty?
        idx = frame_idx % anim_data[:frames].size
        frame_info = anim_data[:frames][idx]
        tex = frame_info[:tex]
        x = base_x + anim_data[:offset_x]
        y = if use_top_left
              base_y + anim_data[:offset_y]
            else
              base_y + anim_data[:offset_y] - (tex.height / 2.0)
            end
        src = Raylib::Rectangle.create(0, 0, flip_h ? -tex.width : tex.width, tex.height)
        dst = Raylib::Rectangle.create(x, y, tex.width, tex.height)
        Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
        return
      end
    end
    # Fallback: карта/спрайт
    tex = unit[:tex]
    return unless tex
    x = base_x
    y = base_y - (TILE_SIZE / 2)
    src = Raylib::Rectangle.create(0, 2 * TILE_SIZE, TILE_SIZE, TILE_SIZE)
    dst = Raylib::Rectangle.create(x, y, TILE_SIZE, TILE_SIZE)
    Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end
end