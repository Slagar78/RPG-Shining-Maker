# battle.rb (корень проекта, автономный)
require 'json'
require 'raylib'
require_relative 'lib/battleManager/hp_mp_panel'
require_relative 'lib/battleManager/battle_sprite_animation'
require_relative 'lib/battleManager/unit_death'
require_relative 'lib/ui'
require_relative 'lib/enemy_profile'

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
require_relative 'lib/battleManager/exp_calculator'
require_relative 'lib/battleManager/battle_renderer'
require_relative 'lib/battleManager/battle_give'
require_relative 'lib/battleManager/battle_drop'
require_relative 'lib/battleManager/battle_equip'
require_relative 'lib/battleManager/battle_utils'

class BattleManager
  include BattleGive
  include BattleDrop
  include BattleEquip
  include BattleUtils
  attr_reader :game_map, :battle_entry, :battle_state, :battle_menu
  attr_reader :camera, :static_bg, :layer2, :top_layer,
              :highlight_tiles, :highlight_timer, :current_unit,
              :allies, :enemies, :battle_player, :cursor,
              :battle_scene, :target_highlight, :highlight_tex,
              :attack_target, :hp_mp_panel, :target_hp_panel,
              :fade_alpha, :db, :battle_x, :battle_y, :info_panel_unit			  
  attr_accessor :audio
  attr_accessor :battle_scene_font

  TILE_SIZE = 48
  CURSOR_SPEED = 8.0
  CURSOR_HIDE_DELAY = 12

  def initialize(db = nil, font = nil, game_text = {})
    @db = db
    @font = font
    @game_text = game_text
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
      # Если боевая зона = вся карта, то смещение не имеет смысла
      bx = 0
      by = 0
    end
    @battle_x = bx
    @battle_y = by
    @battle_w = bw
    @battle_h = bh

    @cursor = BattleCursor.new(TILE_SIZE)
    @camera = BattleCamera.new(576, 480, @battle_w * TILE_SIZE, @battle_h * TILE_SIZE)
    # @camera.set_zone_offset(@battle_x * TILE_SIZE, @battle_y * TILE_SIZE)

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
    @pending_info = false
    @battle_scene = BattleScene.new(self, @game_text)

    # Загружаем start_inventory (общий, как в game.rb)
    @start_inventory = []
    if File.exist?("data/actors/start_inventory.json")
      data = JSON.parse(File.read("data/actors/start_inventory.json"))
      @start_inventory = data["start_inventory"] || []
    end
    @profile = Profile.new(@font, @db, @start_inventory)
    @enemy_profile = EnemyProfile.new(@font, @db)
    @portrait_cache = {}

    @renderer = BattleRenderer.new(self)
    @start_x = 0
    @start_y = 0
    @attack_target = nil
    @target_highlight = nil
    @attack_confirm_ready = false
    @attack_targets = []
    @attack_target_index = 0
    @highlight_tex = load_highlight_texture

    @message_panel_tex = Raylib.LoadTexture("assets/ui/message_panel.png")
    Raylib.SetTextureFilter(@message_panel_tex, Raylib::TEXTURE_FILTER_POINT)

    @give_message_timer = 0
    @give_msg_full_lines = []
    @give_msg_char_index = 0
    @give_msg_char_timer = 0
    @give_msg_char_speed = 3
    @give_msg_finished = false

    @give_message_id = nil
    @give_message_params = {}

    @info_cursor_tex = load_highlight_texture   # текстура рамки
    @info_cursor_x = 0
    @info_cursor_y = 0
    @death_anim = nil
    @info_panel_unit = nil

    @info_cursor_vx = 0
    @info_cursor_vy = 0
    @info_cursor_px = 0
    @info_cursor_py = 0

    @last_attacker = nil
    @last_defender = nil       # запоминаем цель атаки для проверки HP после сцены
    @last_defender_hp_before = nil

    @fade_alpha = 0
    @pending_attacker = nil
    @pending_defender = nil
    @pending_damage = 0
    @pending_exp_amount = 0
    @give_swap_target_unit = nil
    @give_targets = []
    @give_target_index = 0
    @pending_give_item = nil
    @saved_item_menu_action = nil
    @panel_slide_state = :visible
    init_give_animation_vars
    init_drop_vars
    init_equip_vars

    prepare_turn_order

    first_unit = @turn_order.first
    if first_unit
      @cursor.move_to(first_unit[:x], first_unit[:y])
      @cursor.visible = true
    end

    @current_unit = first_unit
    @camera.snap_to(first_unit[:x], first_unit[:y]) if first_unit
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

        atk: (klass ? @db.stat_at_level(klass["attack_growth"], lv) : 4),
        def: (klass ? @db.stat_at_level(klass["defense_growth"], lv) : 5),
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

        atk: enemy ? enemy["stats"]["base_att"] : 0,
        def: enemy ? enemy["stats"]["base_def"] : 0,
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

  def start_current_turn
    @camera.smooth_factor = 0.2
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
	@battle_player.blinking = false if @battle_player
	
	
	# Панель сразу помещаем за правый край и ставим состояние :hidden
    w = @hp_mp_panel.panel_width(@current_unit, @db)
    @hp_mp_panel.x_offset = w + 8
    @panel_slide_state = :hidden	
  end

  def sync_cursor_to_unit
    return unless @current_unit
    @cursor.move_to(@current_unit[:x], @current_unit[:y])	
  end
  
def start_cursor_return_from_info
  # Перемещаем курсор в пиксельные координаты центра клетки, где он сейчас (info_cursor)
  start_px = @info_cursor_x * TILE_SIZE + TILE_SIZE / 2
  start_py = @info_cursor_y * TILE_SIZE + TILE_SIZE / 2
  @cursor.move_to_pixel(start_px, start_py)

  # Цель – центр клетки текущего юнита
  @cursor_target_x = @current_unit[:x] * TILE_SIZE + TILE_SIZE / 2
  @cursor_target_y = @current_unit[:y] * TILE_SIZE + TILE_SIZE / 2

  @cursor.visible = true
  @battle_state = :cursor_returning
  @camera.smooth_factor = 0.3
  @camera.follow_point(start_px, start_py)
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

  # Переключаем камеру на быстрый, но плавный скролл перед началом движения
  @camera.smooth_factor = 0.3
  @camera.follow_point(start_px, start_py)
end
  
  def start_panel_slide_out
    return unless @current_unit
    w = @hp_mp_panel.panel_width(@current_unit, @db)
    @hp_mp_panel.x_offset = w + 8   # сразу за экран
    @panel_slide_state = :hidden
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
  start_panel_slide_out
  @battle_player = nil
  nxt = next_unit
  start_cursor_transition(@current_unit, nxt)
end

def current_actor_spells
  return [] unless @current_unit && @current_unit[:actor]
  actor = @current_unit[:actor]
  klass = @db.classes.find { |c| c["id"] == actor["class_id"] }
  return [] unless klass && klass["spell_list"]
  # Возвращаем сырой список заклинаний (без иконок)
  klass["spell_list"].select { |s| s["level"] <= actor["level"] }
end

def current_actor_items
  return [] unless @current_unit && @current_unit[:actor]
  actor = @current_unit[:actor]
  entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
  return [] unless entry

  entry["items"].map do |item_entry|
    item_name = item_entry["item"]
    if item_name == "NOTHING"
      { "item" => "NOTHING", "icon" => nil }
    else
      item_data = @db ? @db.find_by_name(item_name) : nil
      icon = item_data ? item_data["icon"] : nil
      { "item" => item_name, "icon" => icon }
    end
  end
end

def apply_exp_to_actor(actor, amount, unit)
  return unless actor && amount > 0 && unit
  actor["exp"] ||= 0
  actor["exp"] += amount
  puts "#{actor['name']} получил #{amount} опыта. Всего: #{actor['exp']}"

  while actor["exp"] >= 100
    actor["exp"] -= 100
    level_up(actor, unit)
  end
end

def level_up(actor, unit)
  old_level = actor["level"]
  actor["level"] = (old_level || 1) + 1
  new_level = actor["level"]
  puts "#{actor['name']} достиг уровня #{new_level}!"

  klass = @db.classes.find { |c| c["id"] == actor["class_id"] }
  return unless klass

  old_max_hp = unit[:max_hp]
  old_max_mp = unit[:max_mp]

  new_max_hp = @db.stat_at_level(klass["hp_growth"], new_level)
  new_max_mp = @db.stat_at_level(klass["mp_growth"], new_level)
  new_atk    = @db.stat_at_level(klass["attack_growth"], new_level)
  new_def    = @db.stat_at_level(klass["defense_growth"], new_level)
  new_agi    = @db.stat_at_level(klass["agility_growth"], new_level)
  new_mov    = klass["move"] || unit[:mov]

  unit[:max_hp] = new_max_hp
  unit[:max_mp] = new_max_mp
  unit[:atk]    = new_atk
  unit[:def]    = new_def
  unit[:agi]    = new_agi
  unit[:mov]    = new_mov

  hp_diff = new_max_hp - old_max_hp
  mp_diff = new_max_mp - old_max_mp
  unit[:hp] = [unit[:hp] + hp_diff, new_max_hp].min
  unit[:mp] = [unit[:mp] + mp_diff, new_max_mp].min

  # Обновляем данные актора для сохранения (опционально)
  actor["hp"] = new_max_hp
  actor["mp"] = new_max_mp
end

 def handle_input
   case @battle_state
   when :cursor_moving
     # ничего не делаем — движение обрабатывается в update

   when :player_turn
     return if @cursor.visible   # ← ждём полного исчезновения курсора
     @battle_player.handle_input(self) if @battle_player

     if IsKeyPressed(KEY_S)
       if @battle_player.moving
         @pending_info = true
        else
         @info_cursor_x = @current_unit[:x]
         @info_cursor_y = @current_unit[:y]
         @info_cursor_px = @info_cursor_x * TILE_SIZE + TILE_SIZE / 2
         @info_cursor_py = @info_cursor_y * TILE_SIZE + TILE_SIZE / 2
         @info_cursor_vx = 0
         @info_cursor_vy = 0
		 @battle_player.face_target(@current_unit[:x], @current_unit[:y] + 1)  # поворот вниз
         @battle_state = :info_mode
         @battle_player.blinking = true if @battle_player
       end
       return
     end

   when :action_menu
     prev_index = @battle_menu.selected_index
     @battle_menu.handle_input
     if @battle_menu.selected_index != prev_index && @audio
       @audio.play_sfx("cursor")
     end

     if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
       case @battle_menu.selected_index
       when 0  # Attack
         # Блокируем атаку, если стоим на союзнике
         unless cell_free?(@battle_player.x, @battle_player.y, @current_unit)
           @battle_menu.close
           @battle_state = :player_turn
           @cursor.visible = true
           sync_cursor_to_unit
           return
         end

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

       when 1  # Magic
         spells = current_actor_spells
         if spells.any?
           @battle_menu.open_magic(spells)
           @battle_state = :magic_select
           @audio.play_sfx("confirm") if @audio
         else
           @battle_menu.close
           @battle_state = :player_turn
           @cursor.visible = true
           sync_cursor_to_unit
           @audio.play_sfx("error") if @audio
         end

       when 2  # Item
         @saved_item_menu_action = nil
         @battle_menu.open_item_menu
         @battle_state = :item_select
         @audio.play_sfx("confirm") if @audio

       when 3  # Stay
         unless cell_free?(@battle_player.x, @battle_player.y, @current_unit)
           # Стоим на союзнике – Stay не завершает ход
           @battle_menu.close
           @battle_state = :player_turn
           @cursor.visible = true
           sync_cursor_to_unit
           @audio.play_sfx("cancel_menu") if @audio
         else
           end_current_turn
         end
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

    when :info_mode
     unless @info_panel_unit
       @info_cursor_vx = 0
       @info_cursor_vy = 0
       if IsKeyDown(KEY_LEFT)
         @info_cursor_vx = -8
       elsif IsKeyDown(KEY_RIGHT)
         @info_cursor_vx = 8
       elsif IsKeyDown(KEY_UP)
         @info_cursor_vy = -8
       elsif IsKeyDown(KEY_DOWN)
         @info_cursor_vy = 8
       end
     end

     if IsKeyPressed(KEY_A)
       ally = @allies.find { |a| a[:x] == @info_cursor_x && a[:y] == @info_cursor_y }
       if ally
         open_profile_for_ally(ally)
         @battle_state = :info_profile
         @audio.play_sfx("confirm") if @audio
       else
         enemy = @enemies.find { |e| e[:x] == @info_cursor_x && e[:y] == @info_cursor_y }
         if enemy
           open_profile_for_enemy(enemy)
           @battle_state = :enemy_profile
           @audio.play_sfx("confirm") if @audio
         end
       end
     end

     if IsKeyPressed(KEY_D)
       unit = (@allies + @enemies).find { |u| u[:x] == @info_cursor_x && u[:y] == @info_cursor_y }
       if unit
         @info_panel_unit = unit
         @audio.play_sfx("confirm") if @audio
       end
     end

     if IsKeyPressed(KEY_S)
       if @info_panel_unit
         @info_panel_unit = nil
       else
         # Запускаем плавное возвращение, а потом уже player_turn
         @battle_player.blinking = false if @battle_player
         start_cursor_return_from_info
       end
     end

   when :info_profile
     # Только запускаем анимацию закрытия по S, состояние сменится в update
     if IsKeyPressed(KEY_S)
       @audio.play_sfx("cancel_menu") if @audio
       @profile.close
     end

   when :enemy_profile
     if IsKeyPressed(KEY_S)
       @audio.play_sfx("cancel_menu") if @audio
       @enemy_profile.close
     end

   when :magic_select
     @battle_menu.handle_input
     if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
       @battle_menu.close_magic
       @battle_state = :player_turn
       @cursor.visible = true
       sync_cursor_to_unit
       @audio.play_sfx("confirm") if @audio
     elsif IsKeyPressed(KEY_S)
       @battle_menu.close_magic
       @battle_state = :action_menu          # возврат в главное меню
       @audio.play_sfx("cancel_menu") if @audio
     end

   when :item_select
     @battle_menu.handle_input
     if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
       action = @battle_menu.selected_item_action   # 0=Use, 1=Give, 2=Equip, 3=Drop
       @saved_item_menu_action = action
       case action
       when 0  # Use
         items = current_actor_items
         if items.any?
           @battle_menu.close_item_menu
           @battle_menu.open_item_grid(:use, items)
           @battle_state = :item_grid_select
           @audio.play_sfx("confirm") if @audio
         else
           @battle_menu.close_item_menu
           @battle_state = :player_turn
           @cursor.visible = true
           sync_cursor_to_unit
           @audio.play_sfx("error") if @audio
         end
       when 1  # Give
         items = current_actor_items
         if items.any?
           @battle_menu.close_item_menu
           @battle_menu.open_item_grid(:give, items)
           @battle_state = :item_grid_select
           @audio.play_sfx("confirm") if @audio
         else
           @battle_menu.close_item_menu
           @battle_state = :player_turn
           @cursor.visible = true
           sync_cursor_to_unit
           @audio.play_sfx("error") if @audio
         end		 
	    when 2  # Equip
           @battle_menu.close_item_menu
           start_equip_select
           @audio.play_sfx("confirm") if @audio		 
       when 3  # Drop
         items = current_actor_items
         if items.any?
           @battle_menu.close_item_menu
           @battle_menu.open_item_grid(:drop, items)
           @battle_state = :item_grid_select
           @audio.play_sfx("confirm") if @audio
         else
           @battle_menu.close_item_menu
           @battle_state = :player_turn
           @cursor.visible = true
           sync_cursor_to_unit
           @audio.play_sfx("error") if @audio
         end
	   end
     elsif IsKeyPressed(KEY_S)
       @saved_item_menu_action = nil
       @battle_menu.close_item_menu
       @battle_state = :action_menu          # возврат в главное меню
       @audio.play_sfx("cancel_menu") if @audio
     end

   when :item_grid_select
     @battle_menu.handle_input
     @battle_player&.update_animation
     if (result = @battle_menu.fetch_pending_grid_item)
       item, mode = result
       case mode
       when :use
         # (пока заглушка, вернёмся позже)
         @battle_menu.close_item_grid
         @battle_menu.open_item_menu(@saved_item_menu_action || 0)
         @battle_state = :item_select
         @audio.play_sfx("confirm") if @audio

       when :give
         @battle_menu.close_item_grid
         
         if @battle_player
           @current_unit[:x] = @battle_player.x
           @current_unit[:y] = @battle_player.y
         end
         
         @pending_give_item = item
         @give_targets = adjacent_allies(@current_unit)
         
         if @give_targets.empty?
           @audio.play_sfx("error") if @audio
           @pending_give_item = nil
           @battle_menu.open_item_menu(@saved_item_menu_action || 0)
           @battle_state = :item_select
         else
           # ВСЕГДА переходим в выбор цели (рамка + подсветка)
           @give_target_index = 0
           @target_highlight = @give_targets[0]
           @battle_player&.face_target(@target_highlight[:x], @target_highlight[:y])
           @battle_state = :give_targeting
           neighbors = [
             [@current_unit[:x] + 1, @current_unit[:y]],
             [@current_unit[:x] - 1, @current_unit[:y]],
             [@current_unit[:x],     @current_unit[:y] + 1],
             [@current_unit[:x],     @current_unit[:y] - 1]
           ].select { |nx, ny| nx >= 0 && nx < @battle_w && ny >= 0 && ny < @battle_h }
           @highlight_tiles = neighbors
           @audio.play_sfx("cursor") if @audio
         end

       when :give_swap
         @battle_menu.close_item_grid
         perform_give_swap(item)   # он сам вызовет start_give_message
         @audio.play_sfx("confirm") if @audio
         
       when :drop
         @battle_menu.close_item_grid
         start_drop_confirm(item)
         @audio.play_sfx("confirm") if @audio
       end

     elsif IsKeyPressed(KEY_S)
       @battle_menu.close_item_grid
       @battle_menu.open_item_menu(@saved_item_menu_action || 0)
       @battle_state = :item_select
       @audio.play_sfx("cancel_menu") if @audio
     end

   when :give_targeting
     handle_give_targeting_input
   when :give_animation
     # ничего не делаем   
   when :give_message
     handle_give_message_input
   when :drop_confirm
     handle_drop_confirm_input
   when :drop_message
     handle_drop_message_input
   when :equip_weapon
     handle_equip_weapon_input
   when :equip_ring
     handle_equip_ring_input

   end
 end

  def update
    @battle_menu.update
    @cursor.update		
    update_units_animation
    @highlight_timer += 1

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
          @camera.follow_point(new_px.round, new_py.round)
        end
      end
	  
	when :cursor_returning
      cur_px = @cursor.px
      cur_py = @cursor.py
      dx = @cursor_target_x - cur_px
      dy = @cursor_target_y - cur_py
      dist = Math.sqrt(dx*dx + dy*dy)

    if dist <= CURSOR_SPEED
      # Прибыли – синхронизируемся и переходим в player_turn
      final_tile_x = (@cursor_target_x / TILE_SIZE).round
      final_tile_y = (@cursor_target_y / TILE_SIZE).round
      @cursor.move_to(final_tile_x, final_tile_y)
	  
      @battle_state = :player_turn
      sync_cursor_to_unit   # на всякий случай подстрахует
    else
      step_x = dx / dist * CURSOR_SPEED
      step_y = dy / dist * CURSOR_SPEED
      new_px = cur_px + step_x
      new_py = cur_py + step_y
      @cursor.move_to_pixel(new_px, new_py)
	  @camera.follow_point(new_px.round, new_py.round)   # <-- камера следует за курсором
    end

    when :player_turn
      if @battle_player
        @battle_player.update

        # Сначала синхронизируем координаты, если остановились
        unless @battle_player.moving
          if @current_unit[:x] != @battle_player.x || @current_unit[:y] != @battle_player.y
            if cell_free?(@battle_player.x, @battle_player.y, @current_unit)
              @current_unit[:x] = @battle_player.x
              @current_unit[:y] = @battle_player.y
            end
          end
        end

        # Камера всегда привязана к визуальному центру юнита (и при движении, и после остановки)
        @camera.follow_point(
          @battle_player.visual_x + TILE_SIZE / 2,
          @battle_player.visual_y + TILE_SIZE / 2
        )

        # Меню открывается всегда, независимо от клетки
	    unless @cursor.visible
          if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
            if @battle_player.moving
              @pending_menu = true
            else
              can_attack = adjacent_enemy?(@current_unit)
              open_battle_menu
            end
          end
	    end

        # Обновляем позицию и курсор
        unless @battle_player.moving
          sync_cursor_to_unit
          if @pending_info
             @info_cursor_x = @current_unit[:x]
             @info_cursor_y = @current_unit[:y]
             @info_cursor_px = @info_cursor_x * TILE_SIZE + TILE_SIZE / 2
             @info_cursor_py = @info_cursor_y * TILE_SIZE + TILE_SIZE / 2
             @info_cursor_vx = 0
             @info_cursor_vy = 0
             @battle_state = :info_mode
             @battle_player.blinking = true if @battle_player
             @pending_info = false
          end

          if @pending_menu
            can_attack = adjacent_enemy?(@current_unit)
            open_battle_menu
            @pending_menu = false
          end
        end

        @cursor_hide_timer += 1
        if @cursor_hide_timer >= CURSOR_HIDE_DELAY && @cursor.visible
           @cursor.visible = false
        if @panel_slide_state == :hidden
          @hp_mp_panel.x_offset = 0
          @panel_slide_state = :visible
        end
      end
	end

    when :enemy_turn_wait
      sync_cursor_to_unit
      @cursor_hide_timer += 1
      if @cursor_hide_timer >= CURSOR_HIDE_DELAY && @cursor.visible
        @cursor.visible = false
		if @panel_slide_state == :hidden
          @hp_mp_panel.x_offset = 0
          @panel_slide_state = :visible
        end
	  end
	  
	  @battle_player&.update_animation
	  return if @cursor.visible   # ← враг ничего не делает, пока курсор на экране
	  
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

      # Камера всегда привязана к визуальному центру (и при движении, и после остановки)
      @camera.follow_point(
        @battle_player.visual_x + TILE_SIZE / 2,
        @battle_player.visual_y + TILE_SIZE / 2
      )

      unless @battle_player.moving
        # Синхронизируем координаты текущего юнита с реальной позицией
        @current_unit[:x] = @battle_player.x
        @current_unit[:y] = @battle_player.y

        if @enemy_move_index < @enemy_move_queue.size
          target = @enemy_move_queue[@enemy_move_index]
          # Если клетка занята другим врагом – пропускаем её, не пытаемся встать
          if @enemies.any? { |e| e != @current_unit && e[:x] == target[0] && e[:y] == target[1] }
            @enemy_move_index += 1
          else
            @battle_player.move_towards(target[0], target[1], self)
            unless @battle_player.moving
              if @battle_player.x == target[0] && @battle_player.y == target[1]
                @enemy_move_index += 1
              end
            end
          end
        else
          end_current_turn
        end
      end

    when :action_menu
      @battle_player&.update_animation
	  
	when :item_grid_select
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
      @last_defender_hp_before = @attack_target[:hp]
    # === СЧИТАЕМ ОПЫТ ===
      exp_amount = 0
    if @current_unit[:actor]   # опыт даётся только союзникам
      if @attack_target[:hp] - dmg <= 0
    # Удар смертельный → опыт за уничтожение
      exp_amount = ExpCalculator.calculate_destroy_exp(@current_unit, @attack_target)
    else
    # Враг выживет → опыт за урон
      exp_amount = ExpCalculator.calculate_damage_exp(@current_unit, @attack_target, dmg)
      end
    end
    @pending_exp_amount = exp_amount
	  
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

    when :magic_select
      @battle_player&.update_animation 
	
	when :item_select
      @battle_player&.update_animation
	  
	when :equip_weapon
      update_equip
      @battle_player&.update_animation
    when :equip_ring
      update_equip
      @battle_player&.update_animation
	  
	when :give_targeting
      @battle_player&.update_animation  
	  
	when :give_animation
      @battle_player&.update_animation
      update_give_animation  

    when :battle_scene
      @battle_scene.update

     when :fade_to_battle
      @fade_alpha += 600 * GetFrameTime()
      
	if @fade_alpha >= 255
      @fade_alpha = 255
      @battle_scene.start(@pending_attacker, @pending_defender, @pending_damage, @pending_exp_amount)
	  @last_attacker = @pending_attacker
      @last_defender = @pending_defender
      @pending_attacker = nil
      @pending_defender = nil
      @pending_damage = 0
      @battle_state = :battle_scene
    end

    
    when :info_mode
      if @info_cursor_vx != 0 || @info_cursor_vy != 0
        @info_cursor_px += @info_cursor_vx
        @info_cursor_py += @info_cursor_vy
        max_px = (@battle_w * TILE_SIZE) - 1
        max_py = (@battle_h * TILE_SIZE) - 1
        @info_cursor_px = @info_cursor_px.clamp(0, max_px)
        @info_cursor_py = @info_cursor_py.clamp(0, max_py)
        @info_cursor_x = (@info_cursor_px / TILE_SIZE).floor
        @info_cursor_y = (@info_cursor_py / TILE_SIZE).floor
      end
      @camera.follow_point(@info_cursor_px, @info_cursor_py)
      @battle_player&.update
	  @battle_player&.update_animation
	  
    when :info_profile
      @battle_player&.update
      @profile.update
      if @profile.instance_variable_get(:@ready_to_close)
        @profile.force_close
        @battle_state = :info_mode
      end

    when :enemy_profile
      @battle_player&.update
      @enemy_profile.update
    if @enemy_profile.instance_variable_get(:@ready_to_close)
      @enemy_profile.force_close
      @battle_state = :info_mode
    end
	
	when :give_message
      update_give_message
	when :drop_confirm
      update_drop_confirm
    when :drop_message
      @battle_player&.update_animation
      update_drop_message
	  
    when :death_animation
      @death_anim&.update
      @battle_player&.update_animation
  
  if @death_anim&.finished
    @death_anim = nil
    # Юнит уже удалён из массивов, завершаем ход
    if @current_unit && @current_unit[:hp] > 0
      end_current_turn
    else
      @current_unit_index += 1
      @current_unit_index = 0 if @current_unit_index >= @turn_order.size
      @current_unit = @turn_order[@current_unit_index]
      start_current_turn if @current_unit
      end
    end
  end  # ← конец case
  @camera.update
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
  @battle_menu.draw if @battle_menu.visible
  @death_anim.draw(@camera) if @death_anim
  if @battle_state == :info_profile
     @profile.draw
  elsif @battle_state == :enemy_profile
     @enemy_profile.draw
  end
  if @battle_state == :give_animation
     draw_give_animation
  end
  if @battle_state == :give_message
     draw_give_message
  end
  if @battle_state == :drop_confirm
    draw_drop_confirm
  elsif @battle_state == :drop_message
    draw_drop_message
  end
  if @battle_state == :equip_weapon
    draw_equip_weapon
  elsif @battle_state == :equip_ring
    draw_equip_ring
  end
end

def return_from_battle_scene
  if @last_defender && @last_defender[:hp] <= 0
    if @last_attacker && @last_attacker[:actor]
      @last_attacker[:actor]["kills"] ||= 0
      @last_attacker[:actor]["kills"] += 1
    end
    start_unit_death(@last_defender)
  else
    end_current_turn
  end
  @last_defender = nil
  @last_attacker = nil
  @last_defender_hp_before = nil
end

def start_unit_death(unit)
  # Удаляем из боевых списков
  if @enemies.include?(unit)
    @enemies.delete(unit)
  elsif @allies.include?(unit)
    @allies.delete(unit)
  end

  # Удаляем из очереди и корректируем индекс
  removed_index = @turn_order.index(unit)
  @turn_order.delete(unit)
  if removed_index && removed_index < @current_unit_index
    @current_unit_index -= 1
  end

  @death_anim = UnitDeath.new(unit, TILE_SIZE)
  @battle_state = :death_animation
  @cursor.visible = false
  @battle_menu.close if @battle_menu
end

def open_battle_menu
  # Принудительно синхронизируем позицию юнита с визуальным положением (если игрок не двигается)
  if @battle_player && !@battle_player.moving
    @current_unit[:x] = @battle_player.x
    @current_unit[:y] = @battle_player.y
  end

  can_attack = adjacent_enemy?(@current_unit)
  @cursor.visible = false
  @battle_menu.open(can_attack)
  @battle_state = :action_menu
  @audio.play_sfx("confirm") if @audio
end


def open_profile_for_ally(ally_unit)
  party = @allies.map { |a| a[:actor] }.compact
  classes_data = @db.classes
  class_names = {}
  classes_data.each { |c| class_names[c["id"]] = c["name"] }

  # Используем уже созданный @profile и общий кэш портретов
  @profile.open(ally_unit[:actor]["name"], party, class_names, classes_data, @portrait_cache, @start_inventory)
end

def open_profile_for_enemy(enemy_unit)
  @enemy_profile.open(enemy_unit[:enemy], @portrait_cache)
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

game_text = {}
begin
  File.readlines("data/text/gamescript.txt", encoding: 'UTF-8').each do |line|
    line.strip!
    next if line.empty? || line.start_with?('#')
    if line =~ /^(\d+)=(.+)$/
      game_text[$1] = $2.strip
    end
  end
rescue
  puts "WARNING: gamescript.txt not loaded"
end
  
  $spells = db.spells if db && db.spells
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

  battle = BattleManager.new(db, battle_font, game_text)
  battle.battle_scene_font = battle_scene_font
  battle.battle_menu.font = menu_font   # заменяем шрифт в меню на 30px
  battle.battle_scene.message_font = menu_font

  audio = AudioManager.new
  battle.audio = audio

  audio.load_sfx("confirm",     "assets/sounds/buttons/button_menu.ogg")
  audio.load_sfx("cancel_menu", "assets/sounds/buttons/Cancel.ogg")
  audio.load_sfx("cursor",      "assets/sounds/buttons/Cursor.ogg")
  audio.load_sfx("error",       "assets/sounds/buttons/Error.ogg")
  audio.load_sfx("attack", "assets/sounds/sounds_BattleScenes/Attack.mp3")
  
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