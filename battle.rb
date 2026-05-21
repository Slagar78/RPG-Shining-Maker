# battle.rb (корень проекта, автономный)
require 'json'
require 'raylib'
require_relative 'lib/battleManager/hp_mp_panel'
require_relative 'lib/battleManager/battle_sprite_animation'

shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

require_relative 'lib/GameMap'
require_relative 'lib/database'
require_relative 'lib/AudioManager'
require_relative 'lib/battleManager/battle_ui'
require_relative 'lib/battleManager/battle_player'
require_relative 'lib/battleManager/ai'
require_relative 'lib/battleManager/cursor'
require_relative 'lib/battleManager/camera_battle'
require_relative 'lib/battleManager/Battle_scenes'
require_relative 'lib/battleManager/calculate_damage'
require_relative 'lib/battleManager/battle_renderer'

class BattleManager
  attr_reader :game_map, :battle_entry, :battle_state, :battle_menu
  attr_reader :camera, :static_bg, :layer2, :top_layer,
              :highlight_tiles, :highlight_timer, :current_unit,
              :allies, :enemies, :battle_player, :cursor,
              :battle_scene, :target_highlight, :highlight_tex,
              :attack_target, :hp_mp_panel, :target_hp_panel,
              :fade_alpha, :db, :battle_x, :battle_y
  attr_accessor :audio
  attr_accessor :battle_scene_font

  TILE_SIZE = 48
  CURSOR_SPEED = 6.0
  CURSOR_HIDE_DELAY = 12

  def initialize(db = nil, font = nil)
    @db = db
    @font = font
    @audio = nil

    entries_path = "data/battles/entries.json"
    if File.exist?(entries_path)
      entries = JSON.parse(File.read(entries_path))
      if entries.any?
        @battle_entry = entries.first
      else
        raise "No battles found in data/battles/entries.json"
      end
    else
      raise "File data/battles/entries.json not found"
    end

    map_id = @battle_entry["map_id"]
    @game_map = GameMap.new(map_id)

    # === боевая зона ===
    bx = @battle_entry["battle_x"] || 0
    by = @battle_entry["battle_y"] || 0
    bw = @battle_entry["battle_width"]
    bh = @battle_entry["battle_height"]
    if bw.nil? || bw == 0 || bh.nil? || bh == 0
      bw = @game_map.width
      bh = @game_map.height
    end
    @battle_x = bx
    @battle_y = by
    @battle_w = bw
    @battle_h = bh

    @static_bg = @game_map.build_static_background
    SetTextureFilter(@static_bg.texture, TEXTURE_FILTER_POINT) if @static_bg

    @top_layer = @game_map.build_top_layer
    SetTextureFilter(@top_layer.texture, TEXTURE_FILTER_POINT) if @top_layer

    @layer2 = @game_map.build_layer2
    SetTextureFilter(@layer2.texture, TEXTURE_FILTER_POINT) if @layer2

    load_spriteset
    load_terrain

    @battle_menu = BattleMenu.new
	@battle_menu.font = @font
    @hp_mp_panel = HpMpPanel.new(@font)
    @target_hp_panel = HpMpPanel.new(@font)
    @battle_state = :cursor_moving

    @cursor = BattleCursor.new(TILE_SIZE)
    @camera = BattleCamera.new(576, 480, @battle_w * TILE_SIZE, @battle_h * TILE_SIZE)

    @turn_order = []
    @current_unit_index = 0
    @current_unit = nil
    @highlight_tiles = []
    @highlight_timer = 0
	@saved_highlight_tiles = []
    @battle_player = nil
    @enemy_action_timer = 0
    @enemy_move_queue = []
    @enemy_move_index = 0

    @cursor_target_x = 0.0
    @cursor_target_y = 0.0
    @cursor_moving_to_target = false

    @cursor_hide_timer = 0
    @pending_menu = false
    @battle_scene = BattleScene.new(self)
	@renderer = BattleRenderer.new(self)
    @start_x = 0
    @start_y = 0
    @attack_target = nil
    @target_highlight = nil
    @attack_confirm_ready = false
    @attack_targets = []
    @attack_target_index = 0
    @highlight_tex = load_highlight_texture
	
	@fade_alpha = 0
    @pending_attacker = nil
    @pending_defender = nil
    @pending_damage = 0

    prepare_turn_order

    first_unit = @turn_order.first
    if first_unit
      @cursor.move_to(first_unit[:x], first_unit[:y])
      @cursor.visible = true
    end

    @current_unit = first_unit
    start_current_turn
  end

  def load_spriteset
    folder = @battle_entry["folder"]
    path = "data/battles/#{folder}/spriteset.json"
    unless File.exist?(path)
      puts "spriteset.json not found, no units will be placed."
      @allies = []
      @enemies = []
      return
    end

    data = JSON.parse(File.read(path))

    @allies = []
    @enemies = []

    # ========== Союзники ==========
    (data["allies"] || []).each do |info|
      actor = @db.actors.find { |a| a["id"] == info["actor_id"] }
      tex = actor ? load_mapsprite(actor["mapsprite"]) : nil
      pos = [info["x"], info["y"]]

      if unit_at?(pos)
        puts "Предупреждение: союзник с ID #{info['actor_id']} дублирует позицию (#{pos[0]}, #{pos[1]}) и будет пропущен."
        next
      end

      # Расчёт максимальных HP/MP из класса и кривых роста
      max_hp = 0
      max_mp = 0
      ally_battle_anim = nil
      if actor && @db
        klass = @db.classes.find { |c| c["id"] == actor["class_id"] }
        if klass
          lv = actor["level"] || 1
          max_hp = @db.stat_at_level(klass["hp_growth"], lv)
          max_mp = @db.stat_at_level(klass["mp_growth"], lv)

          # Загрузка боевой анимации из класса
          if klass["battle_sprite_path"] && !klass["battle_sprite_path"].empty?
            ally_battle_anim = BattleSpriteAnimation.new(klass["battle_sprite_path"])
          end
        end
      end

      @allies << {
        x: pos[0], y: pos[1],
        actor: actor, tex: tex,
        battle_anim: ally_battle_anim,
        sprite_frame: 0, sprite_timer: 0, sprite_speed: 14,
        hp: max_hp, max_hp: max_hp,
        mp: max_mp, max_mp: max_mp,

        atk: actor ? (actor["atk"] || 10) : 10,
        def: actor ? (actor["def"] || 5) : 5,
        movetype: klass ? (klass["movetype"] || "regular") : "regular"
      }
    end

    # ========== Враги ==========
    (data["enemies"] || []).each do |info|
      enemy = @db.enemies.find { |e| e["id"] == info["enemy_id"] }
      tex = enemy ? load_enemy_mapsprite(enemy["mapsprite"]) : nil
      pos = [info["x"], info["y"]]

      if unit_at?(pos)
        puts "Предупреждение: враг с ID #{info['enemy_id']} дублирует позицию (#{pos[0]}, #{pos[1]}) и будет пропущен."
        next
      end

      if enemy
        stats = enemy.respond_to?(:stats) ? enemy.stats : enemy["stats"] || {}
        base_hp = stats.respond_to?(:max_hp) ? stats.max_hp : stats["max_hp"] || 0
        base_mp = stats.respond_to?(:max_mp) ? stats.max_mp : stats["max_mp"] || 0
      else
        base_hp = 0
        base_mp = 0
      end

      # Загрузка боевого спрайта врага
      battle_sprite_path = enemy ? enemy["battle_sprite_path"] : nil
      battle_anim = (battle_sprite_path && !battle_sprite_path.empty?) ?
              BattleSpriteAnimation.new(battle_sprite_path) : nil

      @enemies << {
        x: pos[0], y: pos[1],
        enemy: enemy, tex: tex,
        sprite_frame: 0, sprite_timer: 0, sprite_speed: 14,
        ai_type: info["ai"] || 0,
        battle_anim: battle_anim,
        hp: base_hp, max_hp: base_hp,
        mp: base_mp, max_mp: base_mp,

        atk: enemy ? (enemy["stats"]["atk"] || 12) : 12,
        def: enemy ? (enemy["stats"]["def"] || 6) : 6,
        movetype: enemy ? (enemy["movetype"] || "regular") : "regular"
      }
    end
  end

  def load_terrain
    folder = @battle_entry["folder"]
    path = "data/battles/#{folder}/terrain.json"
    unless File.exist?(path)
      puts "terrain.json not found, all tiles walkable."
      @terrain = Array.new(@game_map.height) { Array.new(@game_map.width, 1) }
      return
    end
    raw = JSON.parse(File.read(path))
    @terrain = raw
    if @terrain.empty?
      @terrain = Array.new(@game_map.height) { Array.new(@game_map.width, 1) }
    end
    puts "Terrain loaded: #{@terrain.size}x#{@terrain.first.size}"
  end

  def unit_at?(pos)
    (@allies + @enemies).any? { |u| u[:x] == pos[0] && u[:y] == pos[1] }
  end

  def load_highlight_texture
    path = "assets/ui/menu/target_frame.png"
    return nil unless File.exist?(path)
    img = LoadImage(path)
    tex = LoadTextureFromImage(img)
    UnloadImage(img)
    SetTextureFilter(tex, TEXTURE_FILTER_POINT)
    tex
  end


  def load_mapsprite(name)
    return nil if name.nil? || name.empty?
    @mapsprite_cache ||= {}
    return @mapsprite_cache[name] if @mapsprite_cache.key?(name)
    path = "assets/mapsprites/#{name}.png"
    return nil unless File.exist?(path)
    img = LoadImage(path)
    tex = LoadTextureFromImage(img)
    UnloadImage(img)
    SetTextureFilter(tex, TEXTURE_FILTER_POINT)
    @mapsprite_cache[name] = tex
  end

  def load_enemy_mapsprite(name)
    return nil if name.nil? || name.empty?
    @enemy_mapsprite_cache ||= {}
    return @enemy_mapsprite_cache[name] if @enemy_mapsprite_cache.key?(name)
    path = "assets/mapsprites_enemy/#{name}.png"
    return nil unless File.exist?(path)
    img = LoadImage(path)
    tex = LoadTextureFromImage(img)
    UnloadImage(img)
    SetTextureFilter(tex, TEXTURE_FILTER_POINT)
    @enemy_mapsprite_cache[name] = tex
  end

  def music_file
    @battle_entry["music"] || @game_map.music_file
  end

  def music_volume
    @battle_entry["music_volume"] || @game_map.music_volume
  end

  def prepare_turn_order
    @units = @allies + @enemies
    @units.each do |unit|
      if unit[:actor]
        unit[:agi] = unit[:actor]["agi"] || 10
        unit[:mov] = unit[:actor]["mov"] || 3
      elsif unit[:enemy]
        unit[:agi] = unit[:enemy]["stats"]["base_agi"] || 10
        unit[:mov] = unit[:enemy]["stats"]["base_mov"] || 3
      end
    end
    @turn_order = @units.sort_by { |u| -u[:agi] }
    @current_unit_index = 0
  end

  def occupied_tiles(except_unit = nil)
    occupied = []
    (@allies + @enemies).each do |u|
      next if u.equal?(except_unit)
      occupied << [u[:x], u[:y]]
    end
    occupied
  end


def find_adjacent_enemies(unit)
  ux = unit[:x]
  uy = unit[:y]
  @enemies.select do |enemy|
    ex = enemy[:x]
    ey = enemy[:y]
    (ex - ux).abs + (ey - uy).abs == 1
  end
end

def adjacent_enemy?(unit)
  !find_adjacent_enemies(unit).empty?
end

def sort_targets_by_angle(targets, from_x, from_y)
  targets.sort_by do |t|
    dx = t[:x] - from_x
    dy = t[:y] - from_y
    # Вычисляем угол от оси X (вправо) в диапазоне [0, 2*PI)
    rad = Math.atan2(dy, dx)
    angle = rad >= 0 ? rad : rad + 2 * Math::PI
    angle
  end
end

  def start_current_turn
    @current_unit = @turn_order[@current_unit_index]
    return unless @current_unit

    @start_x = @current_unit[:x]
    @start_y = @current_unit[:y]

    @highlight_tiles = calculate_move_range(@current_unit)
	@saved_highlight_tiles = @highlight_tiles.dup
    @highlight_timer = 0
    @enemy_move_queue.clear
    @enemy_move_index = 0
    @cursor_moving_to_target = false

    @cursor_hide_timer = 0
    @cursor.visible = true

    if @current_unit[:actor]
      @battle_player = BattlePlayer.new(@current_unit, @highlight_tiles, @current_unit[:tex])
      @battle_player.map_width = @battle_w
      @battle_player.map_height = @battle_h
      @battle_menu.close
      @battle_state = :player_turn
      sync_cursor_to_unit
    else
      @battle_player = BattlePlayer.new(@current_unit, @highlight_tiles, @current_unit[:tex])
      @battle_player.map_width = @battle_w
      @battle_player.map_height = @battle_h
      @battle_menu.close
      @battle_state = :enemy_turn_wait
      @enemy_action_timer = 40
      sync_cursor_to_unit
    end
    @camera.follow_unit(@current_unit)
  end

  def sync_cursor_to_unit
    return unless @current_unit
    @cursor.move_to(@current_unit[:x], @current_unit[:y])
  end


    def calculate_move_range(unit)
  start_x = unit[:x]
  start_y = unit[:y]
  move   = unit[:mov]
  w      = @battle_w
  h      = @battle_h

  # visited[y][x] – клетка достигнута
  visited = Array.new(h) { Array.new(w, false) }
  queue = []
  visited[start_y][start_x] = true
  queue.push([start_x, start_y, move])

  # Определяем, какие юниты блокируют проход (противоположная команда)
  is_ally = @allies.include?(unit)
  if is_ally
    blocking_positions = @enemies.map { |e| [e[:x], e[:y]] }
    # свои не блокируют
  else
    blocking_positions = @allies.map { |a| [a[:x], a[:y]] }
    # враги не блокируют друг друга
  end

  # Проверка проходимости клетки (без учёта своих)
  passable = lambda do |nx, ny|
    return false if nx < 0 || nx >= w || ny < 0 || ny >= h
    gx = nx + @battle_x
    gy = ny + @battle_y
    return false if gy < 0 || gy >= @terrain.size || gx < 0 || gx >= @terrain[gy].size
    return false if @terrain[gy][gx] == -1             # стена
    return false if blocking_positions.include?([nx, ny]) # чужой юнит блокирует
    true
  end

  while queue.any?
    x, y, steps = queue.shift
    next if steps <= 0

    [[0,1],[0,-1],[1,0],[-1,0]].each do |dx, dy|
      nx = x + dx
      ny = y + dy
      next if visited[ny][nx]
      next unless passable.call(nx, ny)

      visited[ny][nx] = true
      queue.push([nx, ny, steps - 1])
    end
  end

  # Возвращаем все достигнутые клетки (включая занятые своими)
  result = []
  h.times do |y|
    w.times do |x|
      result << [x, y] if visited[y][x]
    end
  end
  result
end


  def cell_free?(x, y, except_unit = nil)
    (@allies + @enemies).none? { |u| u != except_unit && u[:x] == x && u[:y] == y }
  end

  def next_unit
    next_index = (@current_unit_index + 1) % @turn_order.size
    @turn_order[next_index]
  end

  def start_cursor_transition(from_unit, to_unit)
    @cursor.visible = true
    @battle_player = nil
    @battle_menu.close
    @battle_state = :cursor_moving

    start_px = from_unit[:x] * TILE_SIZE + TILE_SIZE / 2
    start_py = from_unit[:y] * TILE_SIZE + TILE_SIZE / 2
    @cursor.move_to_pixel(start_px, start_py)

    @cursor_target_x = to_unit[:x] * TILE_SIZE + TILE_SIZE / 2
    @cursor_target_y = to_unit[:y] * TILE_SIZE + TILE_SIZE / 2

    @cursor_moving_to_target = true
  end

  def end_current_turn
  if @current_unit && @battle_player
    new_x = @battle_player.x
    new_y = @battle_player.y
    if cell_free?(new_x, new_y, @current_unit)
      @current_unit[:x] = new_x
      @current_unit[:y] = new_y
      puts "✅ #{@current_unit[:actor] ? 'Игрок' : 'Враг'} завершил ход на (#{new_x},#{new_y})"
    else
      @current_unit[:x] = @start_x
      @current_unit[:y] = @start_y
      @battle_player.teleport(@start_x, @start_y)
      puts "❌ Клетка занята, возврат на (#{@start_x},#{@start_y})"
      @audio.play_sfx("error") if @audio
    end
  end
  @battle_player = nil
  nxt = next_unit
  start_cursor_transition(@current_unit, nxt)
end

 def handle_input
  case @battle_state
  when :cursor_moving
    # ничего не делаем — движение обрабатывается в update

  when :player_turn
    @battle_player.handle_input(self) if @battle_player
    if @battle_player.moving
      @cursor.visible = false
    end
    @camera.follow_unit(@current_unit)

  when :action_menu
    prev_index = @battle_menu.selected_index
    @battle_menu.handle_input
    if @battle_menu.selected_index != prev_index && @audio
      @audio.play_sfx("cursor")
    end

    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      case @battle_menu.selected_index
      when 0  # Attack
        targets = find_adjacent_enemies(@current_unit)
        if targets.any?
          @attack_targets = sort_targets_by_angle(targets, @current_unit[:x], @current_unit[:y])
          @attack_target_index = 0
          @attack_target = @attack_targets[0]
          @target_highlight = @attack_targets[0]
          @battle_menu.close

          @battle_state = :attack_targeting
          # Радиус атаки 
          neighbors = [
            [@current_unit[:x] + 1, @current_unit[:y]],
            [@current_unit[:x] - 1, @current_unit[:y]],
            [@current_unit[:x],     @current_unit[:y] + 1],
            [@current_unit[:x],     @current_unit[:y] - 1]
          ].select { |nx, ny| nx >= 0 && nx < @battle_w && ny >= 0 && ny < @battle_h }
          @highlight_tiles = neighbors

          if @battle_player
            @battle_player.face_target(@attack_target[:x], @attack_target[:y])
          end
          @audio.play_sfx("cursor") if @audio
        else
          @battle_menu.close
          @battle_state = :player_turn
          @cursor.visible = true
          sync_cursor_to_unit
          @audio.play_sfx("error") if @audio
        end
      when 3  # Stay
        end_current_turn
      else
        @battle_menu.close
        @battle_state = :player_turn
        @cursor.visible = true
        sync_cursor_to_unit
        @audio.play_sfx("cancel_menu") if @audio
      end
    elsif IsKeyPressed(KEY_S)
      @battle_menu.close
      @battle_state = :player_turn
      @cursor.visible = true
      sync_cursor_to_unit
      @audio.play_sfx("cancel_menu") if @audio
    end
  end
end

  def update
    @battle_menu.update
    @cursor.update
    update_units_animation
    @highlight_timer += 1
    @camera.update

    case @battle_state
    when :cursor_moving
      if @cursor_moving_to_target
        cur_px = @cursor.px > 0 ? @cursor.px : @cursor.x * TILE_SIZE
        cur_py = @cursor.py > 0 ? @cursor.py : @cursor.y * TILE_SIZE
        dx = @cursor_target_x - cur_px
        dy = @cursor_target_y - cur_py
        dist = Math.sqrt(dx*dx + dy*dy)
        if dist <= CURSOR_SPEED
          final_tile_x = (@cursor_target_x / TILE_SIZE).round
          final_tile_y = (@cursor_target_y / TILE_SIZE).round
          @cursor.move_to(final_tile_x, final_tile_y)
          @cursor_moving_to_target = false
          @current_unit_index += 1
          @current_unit_index = 0 if @current_unit_index >= @turn_order.size
          @current_unit = @turn_order[@current_unit_index]
          start_current_turn
        else
          step_x = dx / dist * CURSOR_SPEED
          step_y = dy / dist * CURSOR_SPEED
          new_px = cur_px + step_x
          new_py = cur_py + step_y
          @cursor.move_to_pixel(new_px, new_py)
          @camera.follow_point(new_px, new_py)
        end
      end

    when :player_turn
      if @battle_player
        @battle_player.update
        
        unless @battle_player.moving
          if @current_unit[:x] != @battle_player.x || @current_unit[:y] != @battle_player.y
            if cell_free?(@battle_player.x, @battle_player.y, @current_unit)
              @current_unit[:x] = @battle_player.x
              @current_unit[:y] = @battle_player.y
            end
          end
          sync_cursor_to_unit
          if @pending_menu
            can_attack = adjacent_enemy?(@current_unit)
            open_battle_menu(can_attack)
            @pending_menu = false
          end
        end

        if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
          if @battle_player.moving
            @pending_menu = true
          else
            can_attack = adjacent_enemy?(@current_unit)
            open_battle_menu(can_attack)
          end
        end

        @cursor_hide_timer += 1
        if @cursor_hide_timer >= CURSOR_HIDE_DELAY && @cursor.visible
          @cursor.visible = false
        end
      end

    when :enemy_turn_wait
      sync_cursor_to_unit
      @cursor_hide_timer += 1
      if @cursor_hide_timer >= CURSOR_HIDE_DELAY && @cursor.visible
        @cursor.visible = false
      end
      @enemy_action_timer -= 1
      if @enemy_action_timer <= 0
        @cursor.visible = false
        @enemy_move_queue = EnemyAI.decide_moves(@current_unit, @allies, @enemies, @highlight_tiles)
        if @enemy_move_queue.empty?
          end_current_turn
        else
          @enemy_move_index = 0
          @battle_state = :enemy_moving
        end
      end

    when :enemy_moving
      @battle_player.update
      unless @battle_player.moving
        if @enemy_move_index < @enemy_move_queue.size
          target = @enemy_move_queue[@enemy_move_index]
          @battle_player.move_towards(target[0], target[1], self)
          unless @battle_player.moving
            if @battle_player.x == target[0] && @battle_player.y == target[1]
              @enemy_move_index += 1
            end
          end
        else
          end_current_turn
        end
      end

    when :action_menu
      @battle_player&.update_animation

  when :attack_targeting
  @battle_player&.update_animation  # только анимация, без движения
  if @attack_targets.any?
    # Переключение цели стрелками
    if IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)
      @attack_target_index = (@attack_target_index - 1) % @attack_targets.size
      @attack_target = @attack_targets[@attack_target_index]
      @target_highlight = @attack_target
      @battle_player&.face_target(@attack_target[:x], @attack_target[:y])
      @audio.play_sfx("cursor") if @audio
    elsif IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)
      @attack_target_index = (@attack_target_index + 1) % @attack_targets.size
      @attack_target = @attack_targets[@attack_target_index]
      @target_highlight = @attack_target
      @battle_player&.face_target(@attack_target[:x], @attack_target[:y])
      @audio.play_sfx("cursor") if @audio
    end

    # Ждём отпускания A/D, чтобы избежать мгновенного срабатывания
    if IsKeyUp(KEY_A) && IsKeyUp(KEY_D)
      @attack_confirm_ready = true
    end

    if @attack_confirm_ready && (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D))
      dmg = DamageCalculator.physical_for_units(@current_unit, @attack_target)
      # Запоминаем данные для боевой сцены
      @pending_attacker = @current_unit
      @pending_defender = @attack_target
      @pending_damage = dmg
      # Запускаем затемнение
      @fade_alpha = 0
      @battle_state = :fade_to_battle
      # Чистим состояние выбора цели
      @attack_target = nil
      @target_highlight = nil
      @attack_targets.clear
      @cursor.visible = false
      @attack_confirm_ready = false
      @audio.play_sfx("confirm") if @audio

    elsif IsKeyPressed(KEY_S)
      @attack_target = nil
      @target_highlight = nil
      @attack_targets.clear
      @battle_state = :player_turn
      @highlight_tiles = @saved_highlight_tiles.dup  # ← вернули подсветку хода
	  @battle_player&.update_highlight_tiles(@highlight_tiles)
      @cursor.visible = true
      sync_cursor_to_unit
      @attack_confirm_ready = false
      @audio.play_sfx("cancel_menu") if @audio
    end
  else
    @battle_state = :player_turn
    @highlight_tiles = @saved_highlight_tiles.dup  # ← вернули подсветку хода
	@battle_player&.update_highlight_tiles(@highlight_tiles)
    @cursor.visible = true
    sync_cursor_to_unit
  end

    when :battle_scene
    @battle_scene.update

     when :fade_to_battle
      @fade_alpha += 600 * GetFrameTime()
      if @fade_alpha >= 255
        @fade_alpha = 255
        @battle_scene.start(@pending_attacker, @pending_defender, @pending_damage)
        @pending_attacker = nil
        @pending_defender = nil
        @pending_damage = 0
        @battle_state = :battle_scene
      end
	  
    end  # ← конец case
  end    # ← конец метода update

  def update_units_animation
    (@allies + @enemies).each do |unit|
      next if unit == @current_unit && @battle_player
      unit[:sprite_timer] += 1
      if unit[:sprite_timer] >= unit[:sprite_speed]
        unit[:sprite_timer] = 0
        unit[:sprite_frame] = (unit[:sprite_frame] + 1) % 2
      end
    end
  end

  def draw
    @renderer.draw
  end

  def return_from_battle_scene
    @battle_state = :player_turn
    @cursor.visible = true
    sync_cursor_to_unit
  end

  def open_battle_menu(can_attack = false)
    @cursor.visible = false
    @battle_menu.open(can_attack)
    @battle_state = :action_menu
    @audio.play_sfx("confirm") if @audio
  end

  def unload
    UnloadRenderTexture(@static_bg) if @static_bg
    UnloadRenderTexture(@top_layer) if @top_layer
    UnloadRenderTexture(@layer2) if @layer2
    UnloadTexture(@highlight_tex) if @highlight_tex
  end
end

if __FILE__ == $0
  InitWindow(576, 480, "Battle")
  SetTargetFPS(60)
  InitAudioDevice()

  db = Database.new
  DamageCalculator.movetypes = db.movetypes

  # Загрузка шрифтов
  codepoints = []
  (32..126).each { |cp| codepoints << cp }
  (0x0400..0x04FF).each { |cp| codepoints << cp }
  cp_ptr = FFI::MemoryPointer.new(:int, codepoints.size)
  cp_ptr.write_array_of_int(codepoints)

  # Основной шрифт (20px)
  battle_font = LoadFontEx("assets/ui/fonts/main.ttf", 20, cp_ptr, codepoints.size)
  SetTextureFilter(battle_font.texture, TEXTURE_FILTER_POINT)

  # Шрифт для боевых сцен (18px)
  battle_scene_font = LoadFontEx("assets/ui/fonts/Pic.ttf", 18, cp_ptr, codepoints.size)
  SetTextureFilter(battle_scene_font.texture, TEXTURE_FILTER_POINT)

  # Шрифт для меню (30px, чёткий)
  menu_font = LoadFontEx("assets/ui/fonts/main.ttf", 30, cp_ptr, codepoints.size)
  SetTextureFilter(menu_font.texture, TEXTURE_FILTER_POINT)

  battle = BattleManager.new(db, battle_font)
  battle.battle_scene_font = battle_scene_font
  battle.battle_menu.font = menu_font   # заменяем шрифт в меню на 30px

  audio = AudioManager.new
  battle.audio = audio

  audio.load_sfx("confirm",     "assets/sounds/buttons/button_menu.ogg")
  audio.load_sfx("cancel_menu", "assets/sounds/buttons/Cancel.ogg")
  audio.load_sfx("cursor",      "assets/sounds/buttons/Cursor.ogg")
  audio.load_sfx("error",       "assets/sounds/buttons/Error.ogg")

  if battle.music_file && !battle.music_file.empty?
    audio.play(battle.music_file, battle.music_volume)
  end

  until WindowShouldClose()
    battle.handle_input
    battle.update
    audio.update
    BeginDrawing()
      ClearBackground(BLACK)
      battle.draw
    EndDrawing()
  end

  battle.unload
  UnloadFont(battle_font) if battle_font
  UnloadFont(battle_scene_font) if battle_scene_font
  UnloadFont(menu_font) if menu_font    # выгружаем и этот шрифт
  audio.stop
  CloseAudioDevice()
  CloseWindow()
end