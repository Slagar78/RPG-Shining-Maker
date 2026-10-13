# lib/shop_ui.rb
#
# Меню магазина — крест из 4 плиток (Buy / Sell / Repair / Deals) снизу,
# а при выборе действия сверху появляется панель shop_panel.png,
# на которой рисуются иконки предметов магазина.
#
# Открывается НЕ по клавише на карте, а из скрипта (DialogManager),
# когда игрок взаимодействует с NPC-торговцем.
#
# shops.json — массив магазинов, у каждого числовой "id".

require 'raylib'
require 'json'

class ShopMenu
  TILE_SIZE      = 48
  OFFSET         = 48
  ANIM_INTERVAL  = 24

  # Панель подменю (Buy / Sell / Repair / Deals)
  PANEL_W        = 512
  PANEL_H        = 128
  PANEL_X        = (576 - PANEL_W) / 2   # = 32, автоматическое центрирование
  PANEL_Y        = 16

  # ─────────────────────────────────────────────────────────
  # Раскладка иконок предметов на панели магазина
  # ─────────────────────────────────────────────────────────
  #
  # Панель: 512 px шириной.
  # Формула раскладки:
  #   pad + N×ICON_W + (N-1)×ICON_GAP + pad = PANEL_W
  #   36  + 7×32     + 6×36           + 36  = 512  ✓
  #
  # Внутри каждого ICON_GAP = 36:
  #   24 px — место под ценник
  #   12 px — разделитель до следующей иконки

  ICON_W         = 32   # ширина одной иконки предмета
  ICON_H         = 48   # высота одной иконки предмета (равна высоте тайла)
  ICON_GAP       = 36   # промежуток между иконками (24 цена + 12 разделитель)
  ICON_LEFT_PAD  = 24   # отступ
  ICON_MAX       = 7    # максимум иконок на панели (влезает ровно 7)
  
  # Обводка выбранного предмета: иконка+ценник (56×48) + по 4 px с каждой стороны
  SELECTION_W    = 64
  SELECTION_H    = 56
  SELECTION_PAD  = 4

  attr_reader :visible, :selected_index, :current_shop, :sub_state

  # ─────────────────────────────────────────────────────────
  # Статические загрузчики
  # ─────────────────────────────────────────────────────────

  def self.load_all(path = "data/shop/shops.json")
    unless File.exist?(path)
      puts "WARNING: #{path} not found, no shops loaded."
      return []
    end
    data = JSON.parse(File.read(path))
    data["shops"] || []
  end

  def self.find_by_id(shops, shop_id)
    return nil unless shops && shop_id
    shops.find { |s| s["id"] == shop_id }
  end

  # ─────────────────────────────────────────────────────────
  # Инстанс
  # ─────────────────────────────────────────────────────────

  def initialize(font = nil, db = nil)
    @font           = font
    @db             = db
    @visible        = false
    @selected_index = 0
    @anim_timer     = 0.0
    @current_shop   = nil

    # :main — только крест. :sub — сверху панель подменю.
    @sub_state      = :main
    @sub_action     = nil

    # Кеш иконок предметов (общий для всех магазинов)
    @icon_cache     = {}

    # Иконки предметов текущего магазина
    @shop_icons     = []

    # Индекс выбранной иконки товара в :sub
    @selected_item_index = 0

    # Индекс первой ВИДИМОЙ иконки в окне (для скролла)
    @shop_scroll_index   = 0

    # Автоповтор стрелок ←/→
    @input_timer_left    = 0
    @input_timer_right   = 0
    
    # Таймер пульсации стрелок прокрутки
    @selection_blink_timer = 0

    @tiles = [
      { "id" => 0, "name" => "buy",    "icon" => "assets/ui/menu/Buy.png",    "icon_anim" => "assets/ui/menu/Buy_Anim.png" },
      { "id" => 1, "name" => "sell",   "icon" => "assets/ui/menu/Sell.png",   "icon_anim" => "assets/ui/menu/Sell_Anim.png" },
      { "id" => 2, "name" => "repair", "icon" => "assets/ui/menu/Repair.png", "icon_anim" => "assets/ui/menu/Repair_Anim.png" },
      { "id" => 3, "name" => "deals",  "icon" => "assets/ui/menu/Deals.png",  "icon_anim" => "assets/ui/menu/Deals_Anim.png" }
    ]

    load_textures
  end

  def load_textures
    @textures = []
    @tiles.each do |tile|
      normal = File.exist?(tile["icon"])      ? Raylib.LoadTexture(tile["icon"])      : nil
      anim   = File.exist?(tile["icon_anim"]) ? Raylib.LoadTexture(tile["icon_anim"]) : nil
      Raylib.SetTextureFilter(normal, Raylib::TEXTURE_FILTER_POINT) if normal
      Raylib.SetTextureFilter(anim,   Raylib::TEXTURE_FILTER_POINT) if anim
      @textures << { normal: normal, anim: anim }
    end

    @panel_tex = nil
    if File.exist?("assets/ui/shop_panel.png")
      @panel_tex = Raylib.LoadTexture("assets/ui/shop_panel.png")
      Raylib.SetTextureFilter(@panel_tex, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: assets/ui/shop_panel.png not found."
    end

    @selection_tex_red  = nil
    @selection_tex_dark = nil

    red_path  = "assets/ui/menu/shop_selection_red.png"
    dark_path = "assets/ui/menu/shop_selection_dark.png"

    if File.exist?(red_path)
      @selection_tex_red = Raylib.LoadTexture(red_path)
      Raylib.SetTextureFilter(@selection_tex_red, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: #{red_path} not found."
    end

    if File.exist?(dark_path)
      @selection_tex_dark = Raylib.LoadTexture(dark_path)
      Raylib.SetTextureFilter(@selection_tex_dark, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: #{dark_path} not found."
    end
    @price_tag_tex = nil
    price_path = "assets/ui/menu/price_tag.png"
    if File.exist?(price_path)
      @price_tag_tex = Raylib.LoadTexture(price_path)
      Raylib.SetTextureFilter(@price_tag_tex, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: #{price_path} not found."
    end
  end

  def open(shop_data)
    return unless shop_data
    @current_shop   = shop_data
    @visible        = true
    @selected_index = 0
    @anim_timer     = 0.0
    @selection_blink_timer = 0
    @sub_state      = :main
    @sub_action     = nil

    item_names = shop_data["items"] || []
    @shop_icons = item_names.map { |name| load_item_icon(name) }
    @selected_item_index = 0
    @shop_scroll_index   = 0
    @input_timer_left    = 0
    @input_timer_right   = 0
  end

  def open_by_id(shops, shop_id)
    shop = self.class.find_by_id(shops, shop_id)
    if shop
      open(shop)
      true
    else
      puts "WARNING: shop with id=#{shop_id.inspect} not found."
      false
    end
  end

  def close
    @visible      = false
    @current_shop = nil
    @sub_state    = :main
    @sub_action   = nil
    @shop_icons   = []
    @selected_item_index = 0
    @shop_scroll_index   = 0
    @input_timer_left    = 0
    @input_timer_right   = 0
    @selection_blink_timer = 0
  end

  def handle_input(dt = 1.0 / 60.0)
    return unless @visible

    case @sub_state
    when :main
      # ─── ВЫХОД ИЗ МАГАЗИНА ───
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        close
        return
      end

      # Навигация по кресту
      if Raylib.IsKeyPressed(Raylib::KEY_UP)
        @selected_index = 0
      elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
        @selected_index = 1
      elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
        @selected_index = 2
      elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
        @selected_index = 3
      end

      # Подтверждение — открываем панель подменю
      if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
        @sub_action = @tiles[@selected_index]["name"]
        @sub_state  = :sub
      end

    when :sub
      # Возврат из подменю в крест
      if Raylib.IsKeyPressed(Raylib::KEY_S)
        @sub_state  = :main
        @sub_action = nil
        return
      end

      # Навигация по иконкам товаров + сдвиг окна (с автоповтором)
      # БЕЗ wrap-around — на первом и последнем останавливаемся.
      count = @shop_icons.size
      if count > 0
        # ── ВЛЕВО ──
        if Raylib.IsKeyPressed(Raylib::KEY_LEFT)
          if @selected_item_index > 0
            @selected_item_index -= 1
            adjust_shop_scroll
          end
          @input_timer_left = 0
        elsif Raylib.IsKeyDown(Raylib::KEY_LEFT)
          @input_timer_left += dt * 60.0
          if @input_timer_left > 20 && ((@input_timer_left - 20) % 5) < 1
            if @selected_item_index > 0
              @selected_item_index -= 1
              adjust_shop_scroll
            end
          end
        else
          @input_timer_left = 0
        end

        # ── ВПРАВО ──
        if Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
          if @selected_item_index < count - 1
            @selected_item_index += 1
            adjust_shop_scroll
          end
          @input_timer_right = 0
        elsif Raylib.IsKeyDown(Raylib::KEY_RIGHT)
          @input_timer_right += dt * 60.0
          if @input_timer_right > 20 && ((@input_timer_right - 20) % 5) < 1
            if @selected_item_index < count - 1
              @selected_item_index += 1
              adjust_shop_scroll
            end
          end
        else
          @input_timer_right = 0
        end
      end
    end
  end

  def update(dt = 1.0 / 60.0)
    return unless @visible
    @anim_timer += dt * 60.0
    @selection_blink_timer += dt * 60.0
  end

  def selected_action
    @tiles[@selected_index]["name"]
  end

  def draw
    return unless @visible

    draw_panel if @sub_state == :sub
    draw_tiles
  end

  private

  # ─────────────────────────────────────────────────────────
  # Иконки предметов
  # ─────────────────────────────────────────────────────────

  def load_item_icon(item_name)
    return @icon_cache[item_name] if @icon_cache.key?(item_name)

    item_data = @db ? @db.find_by_name(item_name) : nil
    path = item_data ? item_data["icon"] : nil

    tex = nil
    if path && File.exist?(path)
      img = Raylib.LoadImage(path)
      tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(tex, Raylib::TEXTURE_FILTER_POINT)
    end

    @icon_cache[item_name] = tex
    tex
  end

  # ─────────────────────────────────────────────────────────
  # Рисование
  # ─────────────────────────────────────────────────────────

  def draw_panel
    if @panel_tex
      src = Raylib::Rectangle.create(0, 0, PANEL_W, PANEL_H)
      dst = Raylib::Rectangle.create(PANEL_X, PANEL_Y, PANEL_W, PANEL_H)
      Raylib::DrawTexturePro(@panel_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    else
      Raylib::DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, Raylib::GRAY)
      Raylib::DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, Raylib::DARKGRAY)
    end

    draw_panel_items
  end

  def draw_panel_items
    return if @shop_icons.empty?

    # Ценник 24×64, иконка 32×48.
    # Верх строки = верх ценника, центрируем 64px в панели.
    tag_h   = 64
    row_top = PANEL_Y + (PANEL_H - tag_h) / 2   # = 16 + (128−64)/2 = 48
    tag_y   = row_top
    icon_y  = row_top + (tag_h - ICON_H)        # = 48 + 16 = 64 (низ совпадает)

    cy     = PANEL_Y + PANEL_H / 2

    toggle = ((@anim_timer / 20).to_i % 2) == 1
    sel_tex = toggle ? @selection_tex_red : @selection_tex_dark

    # ═══════════════════════════════════════════════════════════
    # ИКОНКИ ТОВАРОВ (ценник → обводка → иконка)
    # ═══════════════════════════════════════════════════════════
    ICON_MAX.times do |slot|
      icon_index = @shop_scroll_index + slot
      break if icon_index >= @shop_icons.size

      tex = @shop_icons[icon_index]
      next unless tex

      x = PANEL_X + ICON_LEFT_PAD + slot * (ICON_W + ICON_GAP)

      # 1. Ценник — САМЫЙ ПЕРВЫЙ, значит сзади всего
      if @price_tag_tex
        tag_x   = x + ICON_W
        tag_src = Raylib::Rectangle.create(0, 0, 24, 64)
        tag_dst = Raylib::Rectangle.create(tag_x, tag_y, 24, 64)
        Raylib::DrawTexturePro(@price_tag_tex, tag_src, tag_dst,
                               Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      # 2. Обводка выбора (поверх ценника)
      if icon_index == @selected_item_index && sel_tex
        sx   = x - SELECTION_PAD
        sy   = icon_y - SELECTION_PAD
        ssrc = Raylib::Rectangle.create(0, 0, SELECTION_W, SELECTION_H)
        sdst = Raylib::Rectangle.create(sx, sy, SELECTION_W, SELECTION_H)
        Raylib::DrawTexturePro(sel_tex, ssrc, sdst,
                               Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      # 3. Иконка предмета — САМАЯ ПОСЛЕДНЯЯ, поверх всего
      src = Raylib::Rectangle.create(0, 0, ICON_W, ICON_H)
      dst = Raylib::Rectangle.create(x, icon_y, ICON_W, ICON_H)
      Raylib::DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    end

    # ═══════════════════════════════════════════════════════════
    # СТРЕЛКИ ПРОКРУТКИ — ПОСЛЕ иконок, значит поверх всего.
    #
    # ВАЖНО: Raylib отсекает треугольники, обход вершин которых
    # идёт ПО часовой стрелке. Поэтому у ЛЕВОЙ стрелки вершины
    # идут против часовой: (острие) → (низ) → (верх).
    # ═══════════════════════════════════════════════════════════
    if @shop_icons.size > ICON_MAX

      pulse = Math.sin(@selection_blink_timer * 0.2) * 0.4 + 0.6
      arrow_color = Raylib.Fade(Raylib::WHITE, pulse)

      if @shop_scroll_index > 0
       cx = PANEL_X + 8
        Raylib.DrawTriangle(
          Raylib::Vector2.create(cx - 6, cy),
          Raylib::Vector2.create(cx + 6, cy + 5),
          Raylib::Vector2.create(cx + 6, cy - 5),
          arrow_color
        )
      end

      if @shop_scroll_index + ICON_MAX < @shop_icons.size
        cx = PANEL_X + PANEL_W - 8
        Raylib.DrawTriangle(
          Raylib::Vector2.create(cx + 6, cy),
          Raylib::Vector2.create(cx - 6, cy - 5),
          Raylib::Vector2.create(cx - 6, cy + 5),
          arrow_color
        )
      end
    end
  end

  # Держим выбранную иконку внутри видимого окна ICON_MAX.
  def adjust_shop_scroll
    count = @shop_icons.size
    return if count == 0

    if @selected_item_index < @shop_scroll_index
      @shop_scroll_index = @selected_item_index
    elsif @selected_item_index >= @shop_scroll_index + ICON_MAX
      @shop_scroll_index = @selected_item_index - ICON_MAX + 1
    end
  end

  def draw_tiles
    center_x = 576 / 2
    center_y = 480 - 80

    positions = [
      { x: center_x,          y: center_y - OFFSET + 24 },
      { x: center_x - OFFSET, y: center_y },
      { x: center_x + OFFSET, y: center_y },
      { x: center_x,          y: center_y + OFFSET - 24 }
    ]

    (0..3).each do |i|
      tex = @textures[i]
      pos = positions[i]
      texture = nil

      if tex
        if i == @selected_index
          use_anim = (@anim_timer % ANIM_INTERVAL) < (ANIM_INTERVAL / 2) && tex[:anim]
          texture  = use_anim ? tex[:anim] : tex[:normal]
        else
          texture = tex[:normal]
        end
      end

      dst = Raylib::Rectangle.create(
        pos[:x] - TILE_SIZE / 2,
        pos[:y] - TILE_SIZE / 2,
        TILE_SIZE, TILE_SIZE
      )
      src = Raylib::Rectangle.create(0, 0, TILE_SIZE, TILE_SIZE)

      if texture
        Raylib::DrawTexturePro(texture, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      else
        Raylib::DrawRectangle(pos[:x] - TILE_SIZE / 2, pos[:y] - TILE_SIZE / 2, TILE_SIZE, TILE_SIZE, Raylib::GRAY)
        Raylib::DrawRectangleLines(pos[:x] - TILE_SIZE / 2, pos[:y] - TILE_SIZE / 2, TILE_SIZE, TILE_SIZE, Raylib::DARKGRAY)
      end
    end
  end

  public

  def unload
    @textures.each do |t|
      Raylib::UnloadTexture(t[:normal]) if t[:normal]
      Raylib::UnloadTexture(t[:anim])   if t[:anim]
    end
    @textures = []

    @icon_cache.each_value { |t| Raylib::UnloadTexture(t) if t }
    @icon_cache = {}
    @shop_icons = []

    Raylib::UnloadTexture(@panel_tex) if @panel_tex
    @panel_tex = nil
    
    Raylib::UnloadTexture(@selection_tex_red)  if @selection_tex_red
    Raylib::UnloadTexture(@selection_tex_dark) if @selection_tex_dark
    @selection_tex_red  = nil
    @selection_tex_dark = nil
    Raylib::UnloadTexture(@price_tag_tex) if @price_tag_tex
    @price_tag_tex = nil
  end
end