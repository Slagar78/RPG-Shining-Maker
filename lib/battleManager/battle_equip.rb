# lib/battleManager/battle_equip.rb
module BattleEquip
  attr_accessor :equip_state, :equip_items, :equip_weapon_indices, :equip_ring_indices,
                :equip_selected_index, :equip_pending_ring,
                :equip_empty_tex, :equip_unarmed_tex,
                :equip_weapon_slots, :equip_ring_slots

  def init_equip_vars
    @equip_state = nil   # :weapon, :ring или nil
    @equip_items = []    # все предметы, которые можно экипировать
    @equip_weapon_indices = []
    @equip_ring_indices = []
    @equip_selected_index = 0
    @equip_pending_ring = false
    @equip_empty_tex = nil
    @equip_unarmed_tex = nil
    @equip_weapon_slots = []
    @equip_ring_slots = []

    # Загружаем пустую иконку
    path = "assets/items/item_empty.png"
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      @equip_empty_tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(@equip_empty_tex, Raylib::TEXTURE_FILTER_POINT)
    end

    # Иконка "без оружия/кольца"
    path_unarmed = "assets/items/Equippable.png"
    if File.exist?(path_unarmed)
      img = Raylib.LoadImage(path_unarmed)
      @equip_unarmed_tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(@equip_unarmed_tex, Raylib::TEXTURE_FILTER_POINT)
    end
  end

  # Запуск экипировки
  def start_equip_select
    actor = @current_unit[:actor]
    return unless actor

    entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    items = entry ? entry["items"] : []

    # Собираем оружия и кольца отдельно
    weapons = []
    rings = []
    items.each_with_index do |item_entry, idx|
      next if item_entry["item"] == "NOTHING"
      data = @db.find_by_name(item_entry["item"])
      next unless data
      if data["category"] == "Weapon"
        weapons << { item: item_entry["item"], slot_index: idx }
      elsif data["category"] == "Ring"
        rings << { item: item_entry["item"], slot_index: idx }
      end
    end

    @equip_weapon_slots = weapons
    @equip_ring_slots = rings

    has_weapon = !weapons.empty?
    has_ring = !rings.empty?
    
  @battle_menu.close   # скрываем главное меню

  if has_weapon
    start_weapon_select
    @equip_pending_ring = has_ring
  elsif has_ring
    start_ring_select
  else
    # Нет ни оружия, ни колец – всё равно открываем меню (Unarmed/No Ring)
    start_weapon_select   # или start_ring_select – на ваш выбор
    @equip_pending_ring = false
  end
end

  def start_weapon_select
    # Индексы слотов с оружием (0..3) + 4 для Unarmed
    indices = @equip_weapon_slots.map { |w| w[:slot_index] }
    indices << 4
    @equip_weapon_indices = indices
    @equip_selected_index = indices.first
    @equip_state = :weapon
    @battle_state = :equip_weapon
  end

  def start_ring_select
    indices = @equip_ring_slots.map { |r| r[:slot_index] }
    indices << 4
    @equip_ring_indices = indices
    @equip_selected_index = indices.first
    @equip_state = :ring
    @battle_state = :equip_ring
  end

  # Обработка ввода для выбора оружия
  def handle_equip_weapon_input
    return if @equip_weapon_indices.empty?

    if IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)
      pos = @equip_weapon_indices.index(@equip_selected_index) - 1
      pos = @equip_weapon_indices.size - 1 if pos < 0
      @equip_selected_index = @equip_weapon_indices[pos]
      @audio.play_sfx("cursor") if @audio
    elsif IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)
      pos = @equip_weapon_indices.index(@equip_selected_index) + 1
      pos = 0 if pos >= @equip_weapon_indices.size
      @equip_selected_index = @equip_weapon_indices[pos]
      @audio.play_sfx("cursor") if @audio
    end

    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      if @equip_selected_index == 4
        unequip_category(@current_unit, "Weapon")
        @audio.play_sfx("confirm") if @audio
      else
        equip_in_slot(@equip_selected_index, "Weapon")
        @audio.play_sfx("confirm") if @audio
      end
      if @equip_pending_ring
        @equip_pending_ring = false
        start_ring_select
      else
        return_to_item_menu
      end
    elsif IsKeyPressed(KEY_S)
      @equip_pending_ring = false
      return_to_item_menu
      @audio.play_sfx("cancel_menu") if @audio
    end
  end

  # Обработка ввода для выбора кольца
  def handle_equip_ring_input
    return if @equip_ring_indices.empty?

    if IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)
      pos = @equip_ring_indices.index(@equip_selected_index) - 1
      pos = @equip_ring_indices.size - 1 if pos < 0
      @equip_selected_index = @equip_ring_indices[pos]
      @audio.play_sfx("cursor") if @audio
    elsif IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)
      pos = @equip_ring_indices.index(@equip_selected_index) + 1
      pos = 0 if pos >= @equip_ring_indices.size
      @equip_selected_index = @equip_ring_indices[pos]
      @audio.play_sfx("cursor") if @audio
    end

    if IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)
      if @equip_selected_index == 4
        unequip_category(@current_unit, "Ring")
        @audio.play_sfx("confirm") if @audio
      else
        equip_in_slot(@equip_selected_index, "Ring")
        @audio.play_sfx("confirm") if @audio
      end
      return_to_item_menu
    elsif IsKeyPressed(KEY_S)
      return_to_item_menu
      @audio.play_sfx("cancel_menu") if @audio
    end
  end

  def unequip_category(unit, category)
    actor = unit[:actor]
    return unless actor
    entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    return unless entry
    entry["items"].each do |item_entry|
      next if item_entry["item"] == "NOTHING"
      data = @db.find_by_name(item_entry["item"])
      next unless data && data["category"] == category
      item_entry["equipped"] = false
    end
  end

  def equip_in_slot(slot_index, category)
    actor = @current_unit[:actor]
    return unless actor
    entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
    return unless entry
    items = entry["items"]

    # Снимаем все предметы этой категории
    items.each_with_index do |item_entry, idx|
      next if item_entry["item"] == "NOTHING"
      data = @db.find_by_name(item_entry["item"])
      next unless data && data["category"] == category
      item_entry["equipped"] = false
    end
    # Надеваем выбранный
    if items[slot_index] && items[slot_index]["item"] != "NOTHING"
      items[slot_index]["equipped"] = true
    end
  end

  def return_to_item_menu
    @equip_state = nil
    @battle_state = :item_select
    @battle_menu.open_item_menu(@saved_item_menu_action || 0) if @battle_menu
  end

  def update_equip
    # Здесь можно добавить анимацию
  end

  # Отрисовка интерфейса выбора оружия
  def draw_equip_weapon
    draw_equip_interface(:weapon)
  end

  # Отрисовка интерфейса выбора кольца
  def draw_equip_ring
    draw_equip_interface(:ring)
  end

  private

  def draw_equip_interface(mode)
    cx = 576 / 2
    cy = 480 - 80
    positions = [
      { x: cx,        y: cy - 24 },  # верх  (0)
      { x: cx - 32,   y: cy },       # лево  (1)
      { x: cx + 32,   y: cy },       # право (2)
      { x: cx,        y: cy + 24 }   # низ   (3)
    ]
    unarmed_pos = { x: cx + 80, y: cy }

    # Собираем актуальные предметы из инвентаря, чтобы получить свежие equipped
    actor = @current_unit[:actor]
    items = []
    if actor
      entry = @start_inventory.find { |inv| inv["actor_id"] == actor["id"] }
      items = entry ? entry["items"] : []
    end

    # Рисуем 4 слота: показываем только предметы нужной категории
    4.times do |i|
      pos = positions[i]
      item_entry = items[i]
      show_tex = nil

      if item_entry && item_entry["item"] != "NOTHING"
        data = @db.find_by_name(item_entry["item"])
        if data && data["category"] == (mode == :weapon ? "Weapon" : "Ring")
          icon_path = data["icon"]
          show_tex = @battle_menu.load_item_icon({ "icon" => icon_path })
        end
      end
      show_tex ||= @equip_empty_tex

      src = Raylib::Rectangle.create(0, 0, 32, 48)
      dst = Raylib::Rectangle.create(pos[:x] - 16, pos[:y] - 24, 32, 48)
      Raylib.DrawTexturePro(show_tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

      if @equip_selected_index == i
        Raylib.DrawRectangleLinesEx(
          Raylib::Rectangle.create(dst.x - 2, dst.y - 2, dst.width + 4, dst.height + 4),
          2, Raylib::YELLOW
        )
      end
    end

    # Иконка Unarmed / No Ring
    unarmed_tex = @equip_unarmed_tex || @equip_empty_tex
    dst_un = Raylib::Rectangle.create(unarmed_pos[:x] - 16, unarmed_pos[:y] - 24, 32, 48)
    Raylib.DrawTexturePro(unarmed_tex,
      Raylib::Rectangle.create(0, 0, 32, 48),
      dst_un,
      Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

    if @equip_selected_index == 4
      Raylib.DrawRectangleLinesEx(
        Raylib::Rectangle.create(dst_un.x - 2, dst_un.y - 2, dst_un.width + 4, dst_un.height + 4),
        2, Raylib::YELLOW
      )
    end
  end
end