# lib/battleManager/Battle_scenes.rb

class BattleScene
  SCENE_DURATION      = 3.0
  DELAY_DURATION      = 0.4
  END_DELAY_DURATION  = 0.5
  BAR_HEIGHT          = 144
  TILE_SIZE           = 48

  WORK_WIDTH  = 1152
  WORK_HEIGHT = 960 - 2 * BAR_HEIGHT   # 672

  ENEMY_X = 70
  ENEMY_Y = 410

  ALLY_X = 750
  ALLY_Y = 450

  def initialize(battle_manager)
    @battle_manager = battle_manager
    @timer = 0.0
    @active = false
    @finished = false
    @phase = nil
    @background_tex = nil
    @render_texture = nil
    @panel_tex = nil   # текстура HpMpPanel_x2.png (336x124)

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

    load_background
  end

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

    @sub_phase = :idle_before
    @sub_phase_timer = 0.0

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

      if @background_tex
        src = Raylib::Rectangle.create(0, 0, @background_tex.width, @background_tex.height)
        dst = Raylib::Rectangle.create(0, BAR_HEIGHT, WORK_WIDTH, WORK_HEIGHT)
        Raylib.DrawTexturePro(@background_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      Raylib.DrawRectangle(0, 0, WORK_WIDTH, BAR_HEIGHT, Raylib::BLACK)
      Raylib.DrawRectangle(0, 960 - BAR_HEIGHT, WORK_WIDTH, BAR_HEIGHT, Raylib::BLACK)

      if @sub_phase == :attack
        draw_unit(@attacker, :attack, ALLY_X, ALLY_Y, false, @attacker_current_frame, true)
        draw_unit(@defender, :defense, ENEMY_X, ENEMY_Y, false, @defender_current_frame, true)
      else
        draw_unit(@attacker, :idle,    ALLY_X, ALLY_Y, false, @attacker_current_frame, true)
        draw_unit(@defender, :idle,    ENEMY_X, ENEMY_Y, false, @defender_current_frame, true)
      end

      # === Панели HP/MP (новая текстура HpMpPanel_x2.png, 336x124, scale = 1.0) ===
      if @attacker && @attacker[:max_hp]
        load_panel_texture
        if @panel_tex
          panel_w = @panel_tex.width   # 336
          panel_h = @panel_tex.height  # 124
          # Верхняя панель: справа, вертикально по центру верхней полосы (144)
          panel_x = WORK_WIDTH - 16 - panel_w
          panel_y = (BAR_HEIGHT - panel_h) / 2   # (144 - 124) / 2 = 10
          draw_panel_with_sprite(@attacker, panel_x + panel_w, panel_y, true)
        end
      end

      if @defender && @defender[:max_hp]
        load_panel_texture
        if @panel_tex
          panel_w = @panel_tex.width
          panel_h = @panel_tex.height
          # Нижняя панель: слева, вертикально по центру нижней полосы
          panel_x = 16
          panel_y = 960 - BAR_HEIGHT + (BAR_HEIGHT - panel_h) / 2
          draw_panel_with_sprite(@defender, panel_x, panel_y, false)
        end
      end
    end

    Raylib.EndTextureMode()

    src = Raylib::Rectangle.create(0, 0, 1152, -960)
    dst = Raylib::Rectangle.create(0, 0, 576, 480)
    Raylib.DrawTexturePro(@render_texture.texture, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end

  private

  # ---------- Загрузка текстуры панели (новая) ----------
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

  # ---------- Отрисовка панели с текстом (scale = 1.0) ----------
  def draw_panel_with_sprite(unit, x, y, right_aligned)
    return unless @panel_tex

    # ═══════════════════════════════════════════
    # НАСТРОЙКИ под текстуру 336×124
    # ═══════════════════════════════════════════
    scale               = 1.0   # текстура уже нужного размера
    font_size           = 36    # шрифт поменьше, чтобы влезло в 124 пикселя
    text_margin_x       = 20
    text_margin_y_name  = 10
    text_offset_y_hp    = 44
    text_offset_y_mp    = 76
    # ═══════════════════════════════════════════

    w = @panel_tex.width
    h = @panel_tex.height
    dw = (w * scale).to_i
    dh = (h * scale).to_i

    x -= dw if right_aligned

    # Рисуем панель
    src = Raylib::Rectangle.create(0, 0, w, h)
    dst = Raylib::Rectangle.create(x, y, dw, dh)
    Raylib.DrawTexturePro(@panel_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

    # Достаём шрифт из BattleManager
    font = @battle_manager.battle_scene_font || @battle_manager.instance_variable_get(:@font)

    # Имя юнита
    name = if unit[:actor]
             unit[:actor]["name"] || "Ally"
           elsif unit[:enemy]
             unit[:enemy]["name"] || "Enemy"
           else
             "Unit"
           end

    # Координаты текста (scale = 1.0, поэтому просто прибавляем отступы)
    tx = x + text_margin_x
    ty_name = y + text_margin_y_name
    ty_hp   = y + text_offset_y_hp
    ty_mp   = y + text_offset_y_mp

    # Вывод текста
    if font
      Raylib.DrawTextEx(font, name, Raylib::Vector2.create(tx, ty_name), font_size, 1, Raylib::WHITE)
      Raylib.DrawTextEx(font, "HP #{unit[:hp].to_i}/#{unit[:max_hp].to_i}",
                        Raylib::Vector2.create(tx, ty_hp), font_size, 1, Raylib::WHITE)
      Raylib.DrawTextEx(font, "MP #{unit[:mp].to_i}/#{unit[:max_mp].to_i}",
                        Raylib::Vector2.create(tx, ty_mp), font_size, 1, Raylib::WHITE)
    else
      Raylib.DrawText(name, tx, ty_name, font_size, Raylib::WHITE)
      Raylib.DrawText("HP #{unit[:hp].to_i}/#{unit[:max_hp].to_i}", tx, ty_hp, font_size, Raylib::WHITE)
      Raylib.DrawText("MP #{unit[:mp].to_i}/#{unit[:max_mp].to_i}", tx, ty_mp, font_size, Raylib::WHITE)
    end
  end

  # ─── Старые методы анимации (без изменений) ───
  def update_sub_phase(dt)
    @sub_phase_timer += dt
    case @sub_phase
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
    when :idle_before, :idle_after
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
    # Fallback: mapsprite
    tex = unit[:tex]
    return unless tex
    x = base_x
    y = base_y - (TILE_SIZE / 2)
    src = Raylib::Rectangle.create(0, 2 * TILE_SIZE, TILE_SIZE, TILE_SIZE)
    dst = Raylib::Rectangle.create(x, y, TILE_SIZE, TILE_SIZE)
    Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end
end