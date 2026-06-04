# lib/battleManager/battle_renderer.rb

class BattleRenderer
  def initialize(manager)
    @manager = manager
  end

def draw
  cam_x = -@manager.camera.x
  cam_y = -@manager.camera.y

  bg_offset_x = -@manager.battle_x * BattleManager::TILE_SIZE
  bg_offset_y = -@manager.battle_y * BattleManager::TILE_SIZE

  # --- статичный фон ---
  Raylib.DrawTexturePro(
    @manager.static_bg.texture,
    Raylib::Rectangle.create(0, 0, @manager.static_bg.texture.width, -@manager.static_bg.texture.height),
    Raylib::Rectangle.create(cam_x + bg_offset_x, cam_y + bg_offset_y,
                             @manager.static_bg.texture.width, @manager.static_bg.texture.height),
    Raylib::Vector2.create(0, 0), 0, Raylib::WHITE
  )

  # --- слой 2 (если есть) ---
  if @manager.layer2
    Raylib.DrawTexturePro(
      @manager.layer2.texture,
      Raylib::Rectangle.create(0, 0, @manager.layer2.texture.width, -@manager.layer2.texture.height),
      Raylib::Rectangle.create(cam_x + bg_offset_x, cam_y + bg_offset_y,
                               @manager.layer2.texture.width, @manager.layer2.texture.height),
      Raylib::Vector2.create(0, 0), 0, Raylib::WHITE
    )
  end

  # --- подсветка клеток ---
  if @manager.highlight_tiles.any? && @manager.current_unit
    base_color = Raylib::Color.new
    base_color.r = 220
    base_color.g = 240
    base_color.b = 255
    base_color.a = 50

    @manager.highlight_tiles.each do |tx, ty|
      alpha = (Math.sin(@manager.highlight_timer * 0.2) * 35 + 85).to_i.clamp(0, 255)
      color = Raylib::Color.new
      color.r = base_color.r
      color.g = base_color.g
      color.b = base_color.b
      color.a = alpha

      dst = Raylib::Rectangle.create(
        tx * BattleManager::TILE_SIZE + cam_x,
        ty * BattleManager::TILE_SIZE + cam_y,
        BattleManager::TILE_SIZE,
        BattleManager::TILE_SIZE
      )
      Raylib.DrawRectangleRec(dst, color)
    end
  end

  # --- курсор ---
  @manager.cursor.draw(cam_x, cam_y)

  # --- информационная рамка ---
  draw_info_cursor

  # === Рамка цели ===
  if (@manager.battle_state == :attack_targeting || @manager.battle_state == :give_targeting) && @manager.target_highlight && @manager.highlight_tex
    tx = @manager.target_highlight[:x] * BattleManager::TILE_SIZE + cam_x
    ty = @manager.target_highlight[:y] * BattleManager::TILE_SIZE + cam_y
    src = Raylib::Rectangle.create(0, 0, @manager.highlight_tex.width, @manager.highlight_tex.height)
    dst = Raylib::Rectangle.create(tx, ty, BattleManager::TILE_SIZE, BattleManager::TILE_SIZE)
    Raylib.DrawTexturePro(@manager.highlight_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end

  # --- союзники (кроме активного игрока) ---
  @manager.allies.each do |ally|
    next if ally == @manager.current_unit && @manager.battle_player
    tex = ally[:tex]
    next unless tex
    src = Raylib::Rectangle.create(
      ally[:sprite_frame] * BattleManager::TILE_SIZE,
      2 * BattleManager::TILE_SIZE,
      BattleManager::TILE_SIZE,
      BattleManager::TILE_SIZE
    )
    dst = Raylib::Rectangle.create(
      ally[:x] * BattleManager::TILE_SIZE + cam_x,
      ally[:y] * BattleManager::TILE_SIZE + cam_y - 16,
      BattleManager::TILE_SIZE,
      BattleManager::TILE_SIZE
    )
    Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end

  # --- враги (кроме активного игрока) ---
  @manager.enemies.each do |enemy|
    next if enemy == @manager.current_unit && @manager.battle_player
    tex = enemy[:tex]
    next unless tex
    src = Raylib::Rectangle.create(
      enemy[:sprite_frame] * BattleManager::TILE_SIZE,
      2 * BattleManager::TILE_SIZE,
      BattleManager::TILE_SIZE,
      BattleManager::TILE_SIZE
    )
    dst = Raylib::Rectangle.create(
      enemy[:x] * BattleManager::TILE_SIZE + cam_x,
      enemy[:y] * BattleManager::TILE_SIZE + cam_y - 16,
      BattleManager::TILE_SIZE,
      BattleManager::TILE_SIZE
    )
    Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end

  # --- активный юнит (если есть) ---
  if @manager.battle_player
    draw_active_unit(cam_x, cam_y)
  end

  # --- слой верхнего тайла ---
  Raylib.DrawTexturePro(
    @manager.top_layer.texture,
    Raylib::Rectangle.create(0, 0, @manager.top_layer.texture.width, -@manager.top_layer.texture.height),
    Raylib::Rectangle.create(cam_x + bg_offset_x, cam_y + bg_offset_y,
                             @manager.top_layer.texture.width, @manager.top_layer.texture.height),
    Raylib::Vector2.create(0, 0), 0, Raylib::WHITE
  )

  # --- панель HP/MP (текущего юнита или осматриваемого) ---
  if @manager.battle_state == :info_mode && @manager.info_panel_unit
    @manager.hp_mp_panel.draw(@manager.info_panel_unit, @manager.db)
  elsif ![:info_mode, :info_profile, :enemy_profile].include?(@manager.battle_state)
    @manager.hp_mp_panel.draw(@manager.current_unit, @manager.db) if @manager.current_unit
  end

  # --- панель цели атаки ---
  if @manager.battle_state == :attack_targeting && @manager.attack_target
    @manager.target_hp_panel.draw_bottom_right(@manager.attack_target, @manager.db)
  end

  # --- меню ---
  @manager.battle_menu.draw

  # --- боевая сцена ---
  @manager.battle_scene.draw

  # --- затемнение при переходе в боевую сцену ---
  if @manager.battle_state == :fade_to_battle
    Raylib.DrawRectangle(0, 0, 576, 480, Raylib.Fade(Raylib::BLACK, @manager.fade_alpha / 255.0))
  end
end

  private

def draw_info_cursor
  return unless @manager.instance_variable_get(:@info_cursor_tex)
  return unless [:info_mode, :info_profile].include?(@manager.battle_state)

  tex = @manager.instance_variable_get(:@info_cursor_tex)
  x = @manager.instance_variable_get(:@info_cursor_x) * BattleManager::TILE_SIZE - @manager.camera.x
  y = @manager.instance_variable_get(:@info_cursor_y) * BattleManager::TILE_SIZE - @manager.camera.y

  src = Raylib::Rectangle.create(0, 0, tex.width, tex.height)
  dst = Raylib::Rectangle.create(x, y, BattleManager::TILE_SIZE, BattleManager::TILE_SIZE)
  Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
end

  def draw_active_unit(cam_x, cam_y)
    bp = @manager.battle_player
    tex = bp.instance_variable_get(:@tex)
    return unless tex

    row = case bp.direction
          when 8 then 0
          when 4, 6 then 1
          else 2
          end

    px = bp.visual_x + cam_x
    py = (bp.visual_y - 16) + cam_y
    src = Raylib::Rectangle.create(
      bp.instance_variable_get(:@pattern) * BattleManager::TILE_SIZE,
      row * BattleManager::TILE_SIZE,
      bp.direction == 6 ? -BattleManager::TILE_SIZE : BattleManager::TILE_SIZE,
      BattleManager::TILE_SIZE
    )
    dst = Raylib::Rectangle.create(px, py, BattleManager::TILE_SIZE, BattleManager::TILE_SIZE)
    Raylib.DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
  end
end