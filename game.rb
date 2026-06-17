# game.rb

$playtest_mode = ARGV.include?("--playtest")

require 'raylib'
require_relative 'lib/camera'
require_relative 'lib/database'
require_relative 'lib/player'
require_relative 'lib/ui'
require_relative 'lib/AudioManager'
require_relative 'lib/item_actions_ui'
require_relative 'lib/GameMap'
require 'json'

shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

class Game
  def initialize
    SetConfigFlags(FLAG_VSYNC_HINT)
    InitWindow(576, 480, "RPG Shinzo")
    SetTargetFPS(60)

    @db = Database.new
    @game_map = GameMap.new(@db.globals["start_map"] || "Granseal")

    @game_text = {}
    if File.exist?("data/text/gamescript.txt")
      File.readlines("data/text/gamescript.txt").each do |line|
        line.chomp!
        next if line.empty?
        id_part, text_part = line.split('=', 2)
        @game_text[id_part] = text_part if id_part && text_part
      end
    end

    @top_layer = @game_map.build_top_layer
    SetTextureFilter(@top_layer.texture, TEXTURE_FILTER_POINT) if @top_layer
    @layer2 = @game_map.build_layer2
    SetTextureFilter(@layer2.texture, TEXTURE_FILTER_POINT) if @layer2

    @zone_hidden = Array.new(@game_map.roof_events.size, false)

    Raylib.InitAudioDevice()
    @audio = AudioManager.new
    @audio.load_sfx(:confirm, "assets/sounds/buttons/button_menu.ogg")
    @audio.load_sfx(:cancel,  "assets/sounds/buttons/Cancel.ogg")
    @audio.play(@game_map.music_file, @game_map.music_volume)
    @audio.set_sfx_volume(:confirm, 1.0)
    @audio.set_sfx_volume(:cancel,  1.0)
    @audio.load_sfx(:block, "assets/sounds/buttons/block_button.ogg")
    @audio.set_sfx_volume(:block, 1.0)
    $audio = @audio

    @player = Player.new(@game_map)
    start_x = @db.globals["start_x"] || @game_map.default_spawn[0]
    start_y = @db.globals["start_y"] || @game_map.default_spawn[1]
    unless @game_map.inside_area?(start_x, start_y)
      start_x, start_y = @game_map.default_spawn
    end
    @player.x = start_x
    @player.y = start_y

    @camera = Camera.new
    @accumulator = 0.0
    @fixed_dt = 1.0 / 60.0

    @menu = BottomMenu.new
    @items_submenu = BottomMenu.new([
      { "id" => 0, "name" => "use",   "icon" => "assets/ui/menu/Use.png",   "icon_anim" => "assets/ui/menu/Use_anim.png" },
      { "id" => 1, "name" => "give",  "icon" => "assets/ui/menu/Give.png",  "icon_anim" => "assets/ui/menu/Give_anim.png" },
      { "id" => 2, "name" => "equip", "icon" => "assets/ui/menu/Equip.png", "icon_anim" => "assets/ui/menu/Equip_anim.png" },
      { "id" => 3, "name" => "drop",  "icon" => "assets/ui/menu/Drop.png",  "icon_anim" => "assets/ui/menu/Drop_anim.png" }
    ])

    codepoints = []
    (32..126).each { |cp| codepoints << cp }
    (0x0400..0x04FF).each { |cp| codepoints << cp }
    cp_ptr = FFI::MemoryPointer.new(:int, codepoints.size)
    cp_ptr.write_array_of_int(codepoints)
    @font = LoadFontEx("assets/ui/fonts/main.ttf", 20, cp_ptr, codepoints.size)
    Raylib.SetTextureFilter(@font.texture, TEXTURE_FILTER_POINT)

    large_codepoints = []
    (32..126).each { |cp| large_codepoints << cp }
    (0x0400..0x04FF).each { |cp| large_codepoints << cp }
    large_cp_ptr = FFI::MemoryPointer.new(:int, large_codepoints.size)
    large_cp_ptr.write_array_of_int(large_codepoints)
    @large_font = LoadFontEx("assets/ui/fonts/main.ttf", 30, large_cp_ptr, large_codepoints.size)
    Raylib.SetTextureFilter(@large_font.texture, TEXTURE_FILTER_POINT)

    @party = @db.actors
    @classes_data = @db.classes
    @class_names = {}
    @classes_data.each { |c| @class_names[c["id"]] = c["name"] }
    @start_inventory = []
    if File.exist?("data/actors/start_inventory.json")
      data = JSON.parse(File.read("data/actors/start_inventory.json"))
      @start_inventory = data["start_inventory"] || []
    end

    @use_menu = UseMenu.new(@font, @db, @party, @classes_data, @class_names, @start_inventory)
    @give_menu = GiveMenu.new(@font, @large_font, @db, @party, @classes_data, @class_names, @start_inventory, @game_text)
    @equip_menu = EquipMenu.new(@font, @db, @party, @classes_data, @class_names, @start_inventory)
    @drop_menu = DropMenu.new(@font, @large_font, @db, @party, @classes_data, @class_names, @start_inventory, @game_text)

    @status_overlay = StatusOverlay.new(@font, @db, @start_inventory, @party, @classes_data, @class_names)
    @magic_overlay = MagicOverlay.new(@font, @db, @start_inventory, @party, @classes_data, @class_names)
    @profile = Profile.new(@font, @db, @start_inventory)
    @search_overlay = SearchOverlay.new(@large_font, @game_text, @party)

    @anim_timer = 0.0
    @anim_delay = 0.33
    @show_anim  = false

    @game_state = :playing
	
	@warp_pending = nil      # данные варпа, ожидающие перехода
    @fade_alpha = 0          # 0..255, 0 = прозрачный, 255 = чёрный
    @fade_state = nil        # :out, :in, nil
    @fade_timer = 0          # для задержки на чёрном экране
	@warp_delay = 0          # задержка перед началом fade (кадры)
	
    @pending_profile_open = false
    @pending_status_open = false
    @pending_menu_open = false
    @active_item_action = nil
    @pending_items_close = false
    @pending_menu_request = false
    @menu_delay = 0
  end

  def run
    previous_time = GetTime()
    until WindowShouldClose()
      current_time = GetTime()
      frame_time = current_time - previous_time
      previous_time = current_time
      frame_time = 0.2 if frame_time > 0.2
      @accumulator += frame_time

      while @accumulator >= @fixed_dt
        handle_input
        update
        @accumulator -= @fixed_dt
      end

      draw
    end
    @audio.stop
    Raylib.CloseAudioDevice()
    Raylib.UnloadFont(@font) if @font
    UnloadRenderTexture(@game_map.static_bg) if @game_map.static_bg
    UnloadRenderTexture(@top_layer) if @top_layer
    UnloadRenderTexture(@layer2) if @layer2
    @game_map.roof_layers.each { |l| UnloadRenderTexture(l) if l }
    CloseWindow()
  end

  def handle_input
    play_ui_sounds
    case @game_state
	
	when :warping
      # во время варпа управление отключено
      return
	
    when :playing
      if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        if @player.moving
          @pending_menu_request = true
        else
          @game_state = :menu
          @audio.play_sfx(:confirm)
          @menu.open
        end
      else
        @player.handle_input
      end
    when :menu
      if IsKeyPressed(KEY_S)
        @game_state = :playing
        @menu.close
      elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        case @menu.selected_index
        when 0
          @game_state = :status
          @status_overlay.open(@player)
        when 1
          @game_state = :magic
          @magic_overlay.open(@player)
        when 2
          @game_state = :items
          @items_submenu.open
        when 3
          @game_state = :search
          @menu.close
          @search_overlay.open
        else
          @game_state = :playing
          @menu.close
        end
      else
        @menu.handle_input
      end
    when :status
      if IsKeyPressed(KEY_S)
        @status_overlay.close
        @pending_menu_open = true
      elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        @status_overlay.close
        @pending_profile_open = true
      else
        @status_overlay.handle_input
      end
    when :magic
      if IsKeyPressed(KEY_S)
        @magic_overlay.close
        @pending_menu_open = true
      else
        @magic_overlay.handle_input
      end
    when :items
      if IsKeyPressed(KEY_S)
        @items_submenu.close
        @game_state = :menu
      elsif IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        unless @pending_items_close
          case @items_submenu.selected_index
          when 0
            @use_menu.open
            @active_item_action = @use_menu
            @game_state = :item_action
          when 1
            @give_menu.open
            @active_item_action = @give_menu
            @game_state = :item_action
          when 2
            @equip_menu.open
            @active_item_action = @equip_menu
            @game_state = :item_action
          when 3
            @drop_menu.open
            @active_item_action = @drop_menu
            @game_state = :item_action
          end
        end
      else
        @items_submenu.handle_input unless @pending_items_close
      end
    when :profile
      if IsKeyPressed(KEY_S) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
        @profile.close
        @pending_status_open = true
      end
    when :search
      @search_overlay.handle_input
      unless @search_overlay.visible
        @game_state = :menu
        @menu.open
      end
    when :item_action
      @active_item_action.handle_input
      @pending_items_close = true if @active_item_action.anim_phase == 3
    end
  end

  def update
    @audio.update
    @player.update_animation
    @player.update_movement if @game_state == :playing
	
    if @game_state == :playing && @game_map
      @game_map.npcs.each { |npc| npc.update(@game_map, @player) }

      # Проверка tile_events для всех персонажей (игрок + NPC)
      if @game_map.tile_events.any?
        characters = [@player] + @game_map.npcs
        characters.each do |char|
          @game_map.tile_events.each do |ev|
            tx = ev['trigger_x']
            ty = ev['trigger_y']
            next unless tx && ty

            cx = ev['close_x']
            cy = ev['close_y']

            # Открытие
            if char.x == tx && char.y == ty
              unless @game_map.open_doors.key?([tx, ty])
                @game_map.open_doors[[tx, ty]] = {
                  original_tile: @game_map.tile_at(tx, ty),
                  event: ev
                }
                @game_map.replace_tile(tx, ty, ev['new_tile_id'])
              end
            end

            # Закрытие
            if cx && cy && char.x == cx && char.y == cy
              door_key = [tx, ty]
              if @game_map.open_doors.key?(door_key) &&
                 char.last_x == tx && char.last_y == ty
                original = @game_map.open_doors.delete(door_key)[:original_tile]
                @game_map.replace_tile(tx, ty, original)
              end
            end
          end
        end
      end
    end
	
	# Fade-логика для варпа (стиль Sega RPG)
    if @game_state == :warping
      if @warp_delay > 0
        @warp_delay -= 1
      elsif @fade_state.nil?
        @fade_state = :out
        @fade_alpha = 0
      end

      if @fade_state == :out
        @fade_alpha += 8
        if @fade_alpha >= 255
          @fade_alpha = 255
          @fade_state = :hold
        end
      elsif @fade_state == :hold
        # Загружаем новую карту (экран уже чёрный)
        pending = @warp_pending
        @audio.stop
        @game_map = GameMap.new(pending[:map_id])
        @top_layer = @game_map.build_top_layer
        SetTextureFilter(@top_layer.texture, TEXTURE_FILTER_POINT) if @top_layer
        @layer2 = @game_map.build_layer2
        SetTextureFilter(@layer2.texture, TEXTURE_FILTER_POINT) if @layer2
        @player.map = @game_map
        @player.x = pending[:target_x]
        @player.y = pending[:target_y]
        @player.last_x = @player.x
        @player.last_y = @player.y		
        @player.direction = pending[:facing] if pending[:facing]
        @player.moving = false
        @player.pattern = 0
        @zone_hidden = Array.new(@game_map.roof_events.size, false)
        @audio.play(@game_map.music_file, @game_map.music_volume)
        @fade_state = :in
      elsif @fade_state == :in
        @fade_alpha -= 8
        if @fade_alpha <= 0
          @fade_alpha = 0
          @fade_state = nil
          @warp_pending = nil
          @game_state = :playing
        end
      end
    end

 if @game_map && @game_map.roof_events.any?
  @game_map.roof_events.each_with_index do |ev, idx|
    # Проверка триггеров (скрыть крышу) – уже работало, но для единообразия тоже обновим
    triggered = false
    if ev['triggers']
      triggered = ev['triggers'].any? { |t| @player.x == t[0] && @player.y == t[1] }
    elsif ev['trigger_x'] && ev['trigger_y']
      triggered = (@player.x == ev['trigger_x'] && @player.y == ev['trigger_y'])
    end

    if triggered
      @zone_hidden[idx] = true
    end

    # Проверка выходов (показать крышу) – раньше было только ev['exit_x'], теперь массив exits
    exited = false
    if ev['exits']
      exited = ev['exits'].any? { |e| @player.x == e[0] && @player.y == e[1] }
    elsif ev['exit_x'] && ev['exit_y']
      exited = (@player.x == ev['exit_x'] && @player.y == ev['exit_y'])
    end

    if exited
      @zone_hidden[idx] = false
    end
  end
end

    if @game_map && @game_map.tile_events.any?
      @game_map.tile_events.each do |ev|
        tx = ev['trigger_x']
        ty = ev['trigger_y']
        next unless tx && ty

        cx = ev['close_x']
        cy = ev['close_y']

        # --- Открытие двери (игрок на триггере) ---
        if @player.x == tx && @player.y == ty
          unless @game_map.open_doors.key?([tx, ty])
            original_tile = @game_map.tile_at(tx, ty)
            new_id = ev['new_tile_id']
            @game_map.open_doors[[tx, ty]] = { original_tile: original_tile, event: ev }
            @game_map.replace_tile(tx, ty, new_id)
          end
        end

        # --- Закрытие двери (игрок на клетке закрытия) ---
        if cx && cy && @player.x == cx && @player.y == cy
          door_key = [tx, ty]
          if @game_map.open_doors.key?(door_key)
            # Проверяем, что предыдущая клетка была клеткой триггера
            if @player.last_x == tx && @player.last_y == ty
              original_tile = @game_map.open_doors[door_key][:original_tile]
              @game_map.replace_tile(tx, ty, original_tile)
              @game_map.open_doors.delete(door_key)
            end
          end
        end
      end
    end

   # Проверка варпов (телепортов)
   if @game_state == :playing && @game_map && @game_map.warp_events.any? && !@player.moving
      @game_map.warp_events.each do |warp|
      if @player.x == warp['trigger_x'] && @player.y == warp['trigger_y']
      target_map = warp['target_map']
      target_x = warp['target_x']
      target_y = warp['target_y']

      # Конвертация из нового формата редактора (0–3) в старые значения игры (2,4,6,8)
      raw_facing = warp['facing'] || 0
      facing = case raw_facing
               when 0 then 2   # Down
               when 1 then 4   # Left
               when 2 then 6   # Right
               when 3 then 8   # Up
               else 2          # по умолчанию Down
               end

      change_map(target_map, target_x, target_y, facing)
      break
    end
  end
end

    if @pending_menu_request
      if !@player.moving
        @menu_delay += 1
        if @menu_delay >= 3
          @pending_menu_request = false
          @menu_delay = 0
          @game_state = :menu
          @menu.open
        end
      else
        @menu_delay = 0
      end
    end

    @menu.update if @game_state == :menu
    @items_submenu.update if @game_state == :items
    @status_overlay.update if @game_state == :status
    @magic_overlay.update if @game_state == :magic
    @active_item_action&.update if @game_state == :item_action

    if @pending_menu_open
      if @game_state == :magic && !@magic_overlay.instance_variable_get(:@visible)
        @game_state = :menu
        @pending_menu_open = false
      elsif @game_state == :status && !@status_overlay.instance_variable_get(:@visible)
        @game_state = :menu
        @pending_menu_open = false
      end
    end

    if @pending_profile_open && !@status_overlay.instance_variable_get(:@visible)
      @audio.play_sfx(:confirm)
      @profile.open(
        @status_overlay.current_actor,
        @status_overlay.instance_variable_get(:@party),
        @status_overlay.instance_variable_get(:@class_names),
        @status_overlay.instance_variable_get(:@classes_data),
        @status_overlay.instance_variable_get(:@portrait_cache),
        @status_overlay.instance_variable_get(:@start_inventory)
      )
      @pending_profile_open = false
      @game_state = :profile
    end
    @profile.update if @game_state == :profile
    @search_overlay.update if @game_state == :search

    if @pending_status_open && !@profile.instance_variable_get(:@visible)
      @status_overlay.open
      @pending_status_open = false
      @game_state = :status
    end

    @camera.update(@player, @game_map) if @game_state == :playing

    if @pending_items_close && @active_item_action && !@active_item_action.visible
      @game_state = :items
      @pending_items_close = false
    end

    @anim_timer += @fixed_dt
    if @anim_timer >= @anim_delay
      @anim_timer -= @anim_delay
      @show_anim = !@show_anim
    end
  end

  def draw
    BeginDrawing()
    ClearBackground(RAYWHITE)
    BeginMode2D(@camera.render_camera)
	  # ЧЁРНАЯ ПОЛОСА НАД КАРТОЙ (16 ПИКСЕЛЕЙ)
      DrawRectangle(0, -16, @game_map.width * @game_map.tile_size, 16, BLACK)  
      # 1. ОСНОВНОЙ СЛОЙ
      DrawTexturePro(
        @game_map.static_bg.texture,
        Rectangle.create(0, 0, @game_map.static_bg.texture.width, -@game_map.static_bg.texture.height),
        Rectangle.create(0, 0, @game_map.static_bg.texture.width, @game_map.static_bg.texture.height),
        Vector2.create(0, 0), 0, WHITE
      )

      # 2. ВТОРОЙ СЛОЙ (без крыш)
      if @layer2
        DrawTexturePro(
          @layer2.texture,
          Rectangle.create(0, 0, @layer2.texture.width, -@layer2.texture.height),
          Rectangle.create(0, 0, @layer2.texture.width, @layer2.texture.height),
          Vector2.create(0, 0), 0, WHITE
        )
      end

      @game_map.draw_animated_tiles(@show_anim) if @game_map
	  
	  @game_map.npcs.each { |npc| npc.draw(@camera.render_camera) }

      @player.draw

      # 3. ВЕРХНИЙ СЛОЙ (кроны деревьев, тип 3) – перекрывает игрока
      DrawTexturePro(
        @top_layer.texture,
        Rectangle.create(0, 0, @top_layer.texture.width, -@top_layer.texture.height),
        Rectangle.create(0, 0, @top_layer.texture.width, @top_layer.texture.height),
        Vector2.create(0, 0), 0, WHITE
      )

      # 4. КРЫШИ – теперь поверх всего, чтобы верхняя кромка не обрезалась
      @game_map.roof_layers.each_with_index do |layer, idx|
      if layer && !@zone_hidden[idx]
        off = @game_map.roof_offsets[idx]
        DrawTexturePro(
        layer.texture,
        Rectangle.create(0, 0, layer.texture.width, -layer.texture.height),
        Rectangle.create(off[:x], off[:y], layer.texture.width, layer.texture.height),
        Vector2.create(0, 0), 0, WHITE
       )
     end
  end
    EndMode2D()

    case @game_state
    when :menu then @menu.draw
    when :status then @status_overlay.draw
    when :magic then @magic_overlay.draw
    when :profile then @profile.draw
    when :items then @items_submenu.draw
    when :item_action then @active_item_action.draw
    when :search then @search_overlay.draw
    end

    if @game_state == :warping
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, @fade_alpha))
    end

    DrawText("FPS: #{GetFPS()}", 576 - 100, 10, 20, DARKGRAY)
    EndDrawing()
  end

def change_map(map_id, target_x, target_y, facing = nil)
  @warp_pending = {
    map_id: map_id,
    target_x: target_x,
    target_y: target_y,
    facing: facing
  }
  @game_state = :warping
  @warp_delay = 10        # ждём 10 кадров (≈ 0.16 сек при 60 FPS)
end

  private

  def play_ui_sounds
    return unless current_menu_active?
    if IsKeyPressed(KEY_S)
      @audio.play_sfx(:cancel)
      return
    end
    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      return if @game_state == :status
      @audio.play_sfx(:confirm)
    end
  end

  def current_menu_active?
    case @game_state
    when :menu, :items
      true
    when :status
      @status_overlay.instance_variable_get(:@anim_phase) == 2
    when :magic
      @magic_overlay.instance_variable_get(:@anim_phase) == 2
    when :profile
      @profile.instance_variable_get(:@anim_phase) == 2
    when :item_action
      @active_item_action&.anim_phase == 2
    else
      false
    end
  end
end

Game.new.run if __FILE__ == $0