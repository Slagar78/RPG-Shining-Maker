# lib/battleManager/Battle_scenes.rb

class BattleScene
  DELAY_DURATION      = 0.4
  END_DELAY_DURATION  = 0.5
  BAR_HEIGHT          = 144
  TILE_SIZE           = 48
  WORK_WIDTH          = 1152
  WORK_HEIGHT         = 960 - 2 * BAR_HEIGHT
  ENEMY_X             = 124
  ENEMY_Y             = 370
  ALLY_X              = 750
  ALLY_Y              = 480
  GROUND_SCALE        = 2.0
  GROUND_OFFSET_X     = 136
  GROUND_OFFSET_Y     = 422
  STICK_W             = 3
  STICK_H             = 26
  STICK_TEXTURE_PATHS = [
    "assets/ui/Hp_Mp_Points_big.png",
    "assets/ui/Hp_Mp_Points_2_big.png",
    "assets/ui/Hp_Mp_Points_3_big.png",
    "assets/ui/Hp_Mp_Points_4_big.png"
  ]
  LEFT_EDGE_W  = 12
  RIGHT_EDGE_W = 12
  MID_TILE_W   = 8

  IDLE_BEFORE_DURATION = 1.5
  IDLE_AFTER_DURATION  = 1.5
  PRE_ATTACK_DURATION = 0.5   # длительность панели перед атакой

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

    @attacker_anim_timer = 0.0
    @defender_anim_timer = 0.0

    @attacker_current_frame = 0
    @defender_current_frame = 0

    @sub_phase = :idle_before
    @sub_phase_timer = 0.0
    @attack_duration = 0.0
	
	@defender_anim = :defense

    @stick_textures = []
	# Загрузка окантовки для полосок HP/MP
    @edging_tex = nil
    edging_path = "assets/ui/Panel_Edging_Big.png"
    if File.exist?(edging_path)
    img = Raylib.LoadImage(edging_path)
    @edging_tex = Raylib.LoadTextureFromImage(img)
    Raylib.UnloadImage(img)
    Raylib.SetTextureFilter(@edging_tex, Raylib::TEXTURE_FILTER_POINT)
  else
    puts "WARNING: Panel_Edging_Big.png not found"
  end
    load_stick_textures
    load_background

    # Загрузка панели сообщения перед атакой
    @message_panel_tex = nil
    panel_path = "assets/ui/message_battle_panel.png"
    if File.exist?(panel_path)
      img = Raylib.LoadImage(panel_path)
      @message_panel_tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(@message_panel_tex, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: message_battle_panel.png not found"
    end
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

    @attacker_anim_timer = 0.0
    @defender_anim_timer = 0.0
    @attacker_current_frame = 0
    @defender_current_frame = 0

    attacker_anim = @attacker ? @attacker[:battle_anim] : nil
    @attack_duration = attacker_anim ? attacker_anim.total_duration(:attack) : 0.5
    @attack_duration = 0.5 if @attack_duration < 0.1

    @sub_phase = :idle_before
    @sub_phase_timer = 0.0

    load_ground
    puts ">>> Battle scene starting: #{attacker_unit[:enemy] ? 'Enemy' : 'Ally'} vs #{defender_unit[:enemy] ? 'Enemy' : 'Ally'}"
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
	
  # @edging_tex не трогаем – он будет использоваться при следующих атаках

    @battle_manager.end_current_turn
    puts "<<< Battle scene finished"	
   end

 def update
  return unless @active
  dt = Raylib.GetFrameTime()

  # Переход из фазы delay в display
  if @phase == :delay
    @timer -= dt
    if @timer <= 0
      @phase = :display
    end
    return
  end

  # Основная фаза отображения
  if @phase == :display
    @sub_phase_timer += dt

    case @sub_phase
    when :idle_before
      update_animation(dt)
      if @sub_phase_timer >= IDLE_BEFORE_DURATION
        @sub_phase = :pre_attack
        @sub_phase_timer = 0.0
      end

    when :pre_attack
      update_animation(dt)
      if @sub_phase_timer >= PRE_ATTACK_DURATION
        # Определяем анимацию защитника через DamageCalculator
        movetype_key = @defender[:movetype] || "regular"
        dodge_chance = DamageCalculator.physical_dodge_chance(movetype_key)
        @defender_anim = dodge_chance > 30 ? :defense : :idle

        @sub_phase = :attack
        @sub_phase_timer = 0.0
        @attacker_current_frame = 0
        @defender_current_frame = 0
      end

    when :attack
      # Атакующий всегда играет анимацию атаки
      @attacker_current_frame, @attacker_anim_timer = advance_frame(
        @attacker, :attack, @attacker_current_frame, @attacker_anim_timer, dt
      )
      # Защитник играет выбранную анимацию (idle или defense)
      @defender_current_frame, @defender_anim_timer = advance_frame(
        @defender, @defender_anim, @defender_current_frame, @defender_anim_timer, dt
      )

      if @sub_phase_timer >= @attack_duration
        @sub_phase = :idle_after
        @sub_phase_timer = 0.0
        @attacker_current_frame = 0
        @defender_current_frame = 0
      end

    when :idle_after
      update_animation(dt)
      if @sub_phase_timer >= IDLE_AFTER_DURATION
        finish
      end
    end
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

      # ─── GROUND – только под союзником, ×2 ───
      if @attacker && !@attacker[:enemy] && @ground_tex
        gw = @ground_tex.width
        gh = @ground_tex.height
        scaled_w = gw * GROUND_SCALE
        scaled_h = gh * GROUND_SCALE

        gx = ALLY_X - scaled_w / 2 + GROUND_OFFSET_X
        gy = ALLY_Y - scaled_h + GROUND_OFFSET_Y

        src = Raylib::Rectangle.create(0, 0, gw, gh)
        dst = Raylib::Rectangle.create(gx, gy, scaled_w, scaled_h)
        Raylib.DrawTexturePro(@ground_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      # ── Рисуем юнитов ──
      if @sub_phase == :attack
        draw_unit(@attacker, :attack, ALLY_X, ALLY_Y, false, @attacker_current_frame, true)
        draw_unit(@defender, @defender_anim, ENEMY_X, ENEMY_Y, false, @defender_current_frame, true)
      else
        draw_unit(@attacker, :idle,    ALLY_X, ALLY_Y, false, @attacker_current_frame, true)
        draw_unit(@defender, :idle,    ENEMY_X, ENEMY_Y, false, @defender_current_frame, true)
      end

      # === Панели HP/MP ===
      if @attacker && @attacker[:max_hp]
        load_panel_texture
        if @panel_tex
          panel_w = @panel_tex.width
          panel_h = @panel_tex.height
          panel_x = WORK_WIDTH - 16 - panel_w
          panel_y = (BAR_HEIGHT - panel_h) / 2
          draw_panel_with_sprite(@attacker, panel_x + panel_w, panel_y, true)
        end
      end

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

  # Панель сообщения перед атакой (только в idle_before)
if @sub_phase == :pre_attack && @message_panel_tex
  panel_w = @message_panel_tex.width
  panel_h = @message_panel_tex.height
  # Низ панели на 16 px выше нижнего края
  panel_y = 960 - 16 - panel_h
  panel_x = (1152 - panel_w) / 2
  src = Raylib::Rectangle.create(0, 0, panel_w, panel_h)
  dst = Raylib::Rectangle.create(panel_x, panel_y, panel_w, panel_h)
  Raylib.DrawTexturePro(@message_panel_tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
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

    if unit[:actor]
      name = unit[:actor]["name"] || "Ally"
      lvl  = unit[:actor]["level"] || 1
    elsif unit[:enemy]
      e = unit[:enemy]
      name = e["name"] || "Enemy"
      lvl  = e["level"] || 1
    else
      name = "Unit"
      lvl  = 1
    end

    hp     = unit[:hp].to_i
    max_hp = unit[:max_hp].to_i
    mp     = unit[:mp].to_i
    max_mp = unit[:max_mp].to_i

    label_w = [measure_text_ex("HP", font, font_size), measure_text_ex("MP", font, font_size)].max
    max_hp_str = "#{max_hp}/#{max_hp}"
    max_mp_str = "#{max_mp}/#{max_mp}"
    max_number_w = [measure_text_ex(max_hp_str, font, font_size), measure_text_ex(max_mp_str, font, font_size)].max

    max_sticks = [[hp, mp].max, 100].min
    sticks_w = max_sticks * STICK_W

    content_w = text_margin_x + label_w + 8 + sticks_w + 8 + max_number_w + text_margin_x

    name_line = "#{name}  LV #{lvl}"
    name_line_w = measure_text_ex(name_line, font, font_size)
    name_content_w = text_margin_x + name_line_w + text_margin_x

    base_w = 170
    raw_w = [base_w, content_w, name_content_w].max

    mid_area = raw_w - LEFT_EDGE_W - RIGHT_EDGE_W
    tiles = (mid_area.to_f / MID_TILE_W).ceil
    panel_w = LEFT_EDGE_W + RIGHT_EDGE_W + tiles * MID_TILE_W

    if right_aligned
      px = x - panel_w
    else
      px = x
    end
    py = y

    tex = @panel_tex
    if tex
      left_src = Raylib::Rectangle.create(0, 0, LEFT_EDGE_W, tex.height)
      left_dst = Raylib::Rectangle.create(px, py, LEFT_EDGE_W, tex.height)
      Raylib.DrawTexturePro(tex, left_src, left_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

      right_src = Raylib::Rectangle.create(tex.width - RIGHT_EDGE_W, 0, RIGHT_EDGE_W, tex.height)
      right_dst = Raylib::Rectangle.create(px + panel_w - RIGHT_EDGE_W, py, RIGHT_EDGE_W, tex.height)
      Raylib.DrawTexturePro(tex, right_src, right_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

      mid_src = Raylib::Rectangle.create(LEFT_EDGE_W + 2, 0, MID_TILE_W, tex.height)
      tiles.times do |i|
        tile_x = px + LEFT_EDGE_W + i * MID_TILE_W
        tile_dst = Raylib::Rectangle.create(tile_x, py, MID_TILE_W, tex.height)
        Raylib.DrawTexturePro(tex, mid_src, tile_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
      end
    else
      Raylib.DrawRectangle(px, py, panel_w, tex.height, Raylib::Fade(Raylib::BLACK, 0.8))
    end

    tx = px + text_margin_x
    ty_name = py + text_margin_y_name
    ty_hp   = py + 44
    ty_mp   = py + 78

    if font
      Raylib.DrawTextEx(font, name_line, Raylib::Vector2.create(tx, ty_name), font_size, 1, Raylib::WHITE)
    else
      Raylib.DrawText(name_line, tx, ty_name, font_size, Raylib::WHITE)
    end

    bar_area_x = tx
    bar_area_w = panel_w - text_margin_x * 2
    draw_stick_bar(bar_area_x, ty_hp, bar_area_w, hp, max_hp, font, font_size, "HP")
    draw_stick_bar(bar_area_x, ty_mp, bar_area_w, mp, max_mp, font, font_size, "MP")
  end

  # ---------- Рисование шкалы ----------
  def draw_stick_bar(base_x, base_y, area_width, current, max_val, font, font_size, label)
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

    sticks_to_draw = [current, max_val, total_positions].min

    bar_start_x = base_x + label_w + gap
    stick_y = base_y + (font_size - STICK_H) / 2

    # Цветные палочки
    limits = [100, 200, 300, 9999]
    prev_limit = 0

    limits.each_with_index do |limit, idx|
      layer_max = [current, limit].min - prev_limit
      layer_max = 0 if layer_max < 0
      count = [sticks_to_draw, layer_max].min
      next if count <= 0

      tex = @stick_textures[idx]
      (0...count).each do |i|
        stick_x = bar_start_x + i * STICK_W
        if tex
          src = Raylib::Rectangle.create(0, 0, tex.width, tex.height)
          dst = Raylib::Rectangle.create(stick_x, stick_y, STICK_W, STICK_H)
          Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
        end
      end
      prev_limit = limit
    end

    # === Окантовка (только на реально нарисованные палочки) ===
    if @edging_tex && sticks_to_draw >= 2
      left_src  = Raylib::Rectangle.create(0, 0, 3, @edging_tex.height)
      mid_src   = Raylib::Rectangle.create(3, 0, 3, @edging_tex.height)
      right_src = Raylib::Rectangle.create(6, 0, 3, @edging_tex.height)

      # Левый колпачок
      left_dst = Raylib::Rectangle.create(bar_start_x, stick_y, 3, STICK_H)
      Raylib.DrawTexturePro(@edging_tex, left_src, left_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

      # Средняя часть (sticks_to_draw - 2 раз, потому что левый и правый уже заняли 2 палочки)
      (sticks_to_draw - 2).times do |i|
        mid_dst = Raylib::Rectangle.create(bar_start_x + 3 + i * 3, stick_y, 3, STICK_H)
        Raylib.DrawTexturePro(@edging_tex, mid_src, mid_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
      end

      # Правый колпачок
      right_dst = Raylib::Rectangle.create(bar_start_x + (sticks_to_draw - 1) * 3, stick_y, 3, STICK_H)
      Raylib.DrawTexturePro(@edging_tex, right_src, right_dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)
    end

    # Число справа
    number_x = bar_start_x + total_positions * STICK_W + gap
    if font
      Raylib.DrawTextEx(font, number_str, Raylib::Vector2.create(number_x, base_y), font_size, 1, Raylib::WHITE)
    else
      Raylib.DrawText(number_str, number_x, base_y, font_size, Raylib::WHITE)
    end
end

  # ---------- Анимация ----------
  def update_animation(dt)
    case @sub_phase
    when :idle_before, :pre_attack, :idle_after
      @attacker_current_frame, @attacker_anim_timer = advance_frame(@attacker, :idle, @attacker_current_frame, @attacker_anim_timer, dt)
      @defender_current_frame, @defender_anim_timer = advance_frame(@defender, :idle, @defender_current_frame, @defender_anim_timer, dt)
    when :attack
      @attacker_current_frame, @attacker_anim_timer = advance_frame(@attacker, :attack, @attacker_current_frame, @attacker_anim_timer, dt)
      @defender_current_frame, @defender_anim_timer = advance_frame(@defender, :defense, @defender_current_frame, @defender_anim_timer, dt)
    end
  end

  def advance_frame(unit, anim_key, current_frame, timer, dt)
    return [0, timer] unless unit
    anim = unit[:battle_anim]
    return [0, timer] unless anim
    anim_data = anim.send(anim_key)
    return [0, timer] unless anim_data && !anim_data[:frames].empty?
    frames = anim_data[:frames]
    frame_info = frames[current_frame % frames.size]

    timer += dt
    if timer >= frame_info[:duration]
      timer -= frame_info[:duration]
      new_frame = (current_frame + 1) % frames.size
      [new_frame, timer]
    else
      [current_frame, timer]
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
        frame_off_x = frame_info[:offset_x] || 0
        frame_off_y = frame_info[:offset_y] || 0

        x = base_x + anim_data[:offset_x] + frame_off_x
        y = if use_top_left
              base_y + anim_data[:offset_y] + frame_off_y
            else
              base_y + anim_data[:offset_y] + frame_off_y - (tex.height / 2.0)
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