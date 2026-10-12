# lib/shop_ui.rb
#
# Меню магазина — крест из 4 плиток, как BottomMenu в ui.rb.
# Открывается НЕ по клавише на карте, а из скрипта (DialogManager),
# когда игрок взаимодействует с NPC-торговцем или другим объектом.
#
# Состояния:
#   :main   — только крест из 4 плиток (Buy / Sell / Repair / Deals)
#   :sub    — сверху панель shop_panel.png (480×64), снизу крест
#
# На данном этапе — только сам факт открытия подменю (панель).
# Списки товаров и покупка/продажа будут отдельными подменю позже.

require 'raylib'
require 'json'

class ShopMenu
  TILE_SIZE      = 48
  OFFSET         = 48
  ANIM_INTERVAL  = 24   # кадров на половину цикла анимации

  # Панель подменю (Buy / Sell / Repair / Deals)
  PANEL_W        = 480
  PANEL_H        = 64
  PANEL_X        = 24   # отступ слева
  PANEL_Y        = 16   # отступ сверху

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

  def initialize(font = nil)
    @font           = font
    @visible        = false
    @selected_index = 0
    @anim_timer     = 0.0
    @current_shop   = nil

    # :main — только крест. :sub — сверху панель подменю.
    @sub_state      = :main
    @sub_action     = nil   # "buy" / "sell" / "repair" / "deals"

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

    # Панель подменю (Buy / Sell / Repair / Deals)
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

    # 1. Панель подменю (только в состоянии :sub)
    draw_panel if @sub_state == :sub

    # 2. Крест из плиток (всегда, пока магазин открыт)
    draw_tiles
  end

  private

  def draw_panel
    if @panel_tex
      src = Raylib::Rectangle.create(0, 0, PANEL_W, PANEL_H)
      dst = Raylib::Rectangle.create(PANEL_X, PANEL_Y, PANEL_W, PANEL_H)
      Raylib::DrawTexturePro(@panel_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    else
      # Fallback, если PNG ещё нет
      Raylib::DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, Raylib::GRAY)
      Raylib::DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, Raylib::DARKGRAY)
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
    Raylib::UnloadTexture(@panel_tex) if @panel_tex
    @panel_tex = nil
  end
end