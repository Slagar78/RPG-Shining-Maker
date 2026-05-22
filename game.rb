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

# ========== ОСНОВНАЯ ИГРА ==========
class Game
  def initialize
    SetConfigFlags(FLAG_VSYNC_HINT)
    InitWindow(576, 480, "RPG Shinzo")
    SetTargetFPS(60)

    @db = Database.new
    @game_map = GameMap.new(@db.globals["start_map"] || "Granseal")

    # --- Загрузка текстов из gamescript.txt ---
    @game_text = {}
    if File.exist?("data/text/gamescript.txt")
      File.readlines("data/text/gamescript.txt").each do |line|
        line.chomp!
        next if line.empty?
        id_part, text_part = line.split('=', 2)
        @game_text[id_part] = text_part if id_part && text_part
      end
    end

    @static_bg = @game_map.build_static_background
    SetTextureFilter(@static_bg.texture, TEXTURE_FILTER_POINT) if @static_bg

    @top_layer = @game_map.build_top_layer
    SetTextureFilter(@top_layer.texture, TEXTURE_FILTER_POINT) if @top_layer
	@layer2 = @game_map.build_layer2
    SetTextureFilter(@layer2.texture, TEXTURE_FILTER_POINT) if @layer2

    Raylib.InitAudioDevice()
    @audio = AudioManager.new
    # Глобальная загрузка звуков интерфейса
    @audio.load_sfx(:confirm, "assets/sounds/buttons/button_menu.ogg")
    @audio.load_sfx(:cancel,  "assets/sounds/buttons/Cancel.ogg")
    @audio.play(@game_map.music_file, @game_map.music_volume)
	@audio.set_sfx_volume(:confirm, 1.0)   # можно 0.8, 0.9 – на ваш слух
    @audio.set_sfx_volume(:cancel,  1.0)
	@audio.load_sfx(:block, "assets/sounds/buttons/block_button.ogg")
    @audio.set_sfx_volume(:block, 1.0)
	$audio = @audio

    @player = Player.new(@game_map)
	# Устанавливаем стартовые координаты из глобальных настроек
    start_x = @db.globals["start_x"] || 5
    start_y = @db.globals["start_y"] || 5
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

    # Крупный шрифт для окон сообщений (30 px)
    large_codepoints = []
    (32..126).each { |cp| large_codepoints << cp }
    (0x0400..0x04FF).each { |cp| large_codepoints << cp }
    large_cp_ptr = FFI::MemoryPointer.new(:int, large_codepoints.size)
    large_cp_ptr.write_array_of_int(large_codepoints)
    @large_font = LoadFontEx("assets/ui/fonts/main.ttf", 30, large_cp_ptr, large_codepoints.size)
    Raylib.SetTextureFilter(@large_font.texture, TEXTURE_FILTER_POINT)

    # ---------- Данные из базы ----------
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
	
    @game_state = :playing
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
    UnloadRenderTexture(@static_bg) if @static_bg
    UnloadRenderTexture(@top_layer) if @top_layer
	UnloadRenderTexture(@layer2) if @layer2
    CloseWindow()
  end

  def handle_input
    play_ui_sounds
    case @game_state
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
		when 3                         # <-- Четвёртая плитка "Поиск"
          @game_state = :search
          @menu.close                  # скрываем плитки
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
      if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_S)
        @search_overlay.close
        @game_state = :menu
        @menu.open                    # снова показываем плитки
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
  end

  def draw
    BeginDrawing()
    ClearBackground(RAYWHITE)
	BeginMode2D(@camera.render_camera)
      # 1. ОСНОВНОЙ СЛОЙ (земля, стены без верхушек)
      DrawTexturePro(
        @static_bg.texture,
        Rectangle.create(0, 0, @static_bg.texture.width, -@static_bg.texture.height),
        Rectangle.create(0, 0, @static_bg.texture.width, @static_bg.texture.height),
        Vector2.create(0, 0), 0, WHITE
      )

      # 2. ВТОРОЙ СЛОЙ (декор, tiles2) – рисуется поверх основы
      if @layer2
        DrawTexturePro(
          @layer2.texture,
          Rectangle.create(0, 0, @layer2.texture.width, -@layer2.texture.height),
          Rectangle.create(0, 0, @layer2.texture.width, @layer2.texture.height),
          Vector2.create(0, 0), 0, WHITE
        )
      end

      @player.draw

      # 3. ВЕРХНИЙ СЛОЙ (кроны деревьев, тип 3) – перекрывает игрока
      DrawTexturePro(
        @top_layer.texture,
        Rectangle.create(0, 0, @top_layer.texture.width, -@top_layer.texture.height),
        Rectangle.create(0, 0, @top_layer.texture.width, @top_layer.texture.height),
        Vector2.create(0, 0), 0, WHITE
      )
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

    DrawText("FPS: #{GetFPS()}", 576 - 100, 10, 20, DARKGRAY)
    EndDrawing()
  end
  
 private

 def play_ui_sounds
  return unless current_menu_active?

  if IsKeyPressed(KEY_S)
    @audio.play_sfx(:cancel)
    return
  end

  if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
    # В статусе звук confirm проигрывается позже (при открытии профиля),
    # чтобы избежать дубля, здесь его не вызываем.
    return if @game_state == :status

    @audio.play_sfx(:confirm)
  end
end
  
def current_menu_active?
  case @game_state
  when :menu, :items
    true # BottomMenu и ItemsSubmenu всегда активны, когда открыты
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