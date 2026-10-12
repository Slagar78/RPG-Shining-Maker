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
  PANEL_H        = 64
  PANEL_X        = (576 - PANEL_W) / 2   # = 32, автоматическое центрирование
  PANEL_Y        = 16

  # Раскладка иконок предметов на панели
  ICON_W         = 32
  ICON_H         = 48
  ICON_GAP       = 32
  ICON_LEFT_PAD  = 16
  ICON_MAX       = 6

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

    # Иконки предметов текущего магазина (массив текстур, до ICON_MAX штук)
    @shop_icons     = []

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
  end

  def open(shop_data)
    return unless shop_data
    @current_shop   = shop_data
    @visible        = true
    @selected_index = 0
    @anim_timer     = 0.0
    @sub_state      = :main
    @sub_action     = nil

    # Загружаем иконки предметов магазина (до ICON_MAX штук)
    item_names = (shop_data["items"] || []).first(ICON_MAX)
    @shop_icons = item_names.map { |name| load_item_icon(name) }
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
  end

  def handle_input
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
      end
    end
  end

  def update(dt = 1.0 / 60.0)
    return unless @visible
    @anim_timer += dt * 60.0
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

  # Иконки предметов магазина: слева 16px, потом иконки через 32px,
  # справа 16px, максимум 6 штук.
  def draw_panel_items
    return if @shop_icons.empty?

    icon_y = PANEL_Y + (PANEL_H - ICON_H) / 2

    @shop_icons.each_with_index do |tex, i|
      next unless tex
      x = PANEL_X + ICON_LEFT_PAD + i * (ICON_W + ICON_GAP)
      src = Raylib::Rectangle.create(0, 0, ICON_W, ICON_H)
      dst = Raylib::Rectangle.create(x, icon_y, ICON_W, ICON_H)
      Raylib::DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
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
  end
end