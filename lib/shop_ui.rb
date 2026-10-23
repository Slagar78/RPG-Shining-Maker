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
  
  # Обводка выбранного предмета: ценник-бирка 56×72 + по краям.
  # Обводка 68×76 → запас 6 px по горизонтали и 2 px по вертикали.
  SELECTION_W      = 68
  SELECTION_H      = 80
  SELECTION_PAD_X  = 6    # сдвиг обводки от x
  SELECTION_PAD_Y  = 28   # сдвиг обводки от icon_y (учитывает шнурок сверху)
  
  # Панель сообщения снизу
  PANEL_MSG_W        = 480
  PANEL_MSG_H        = 128
  PANEL_MSG_X        = (576 - PANEL_MSG_W) / 2      # = 48
  PANEL_MSG_TARGET_Y = 480 - PANEL_MSG_H - 24       # = 328
  PANEL_MSG_HIDDEN_Y = 480                          # за экраном снизу
  PANEL_MSG_SPEED    = 38                           # px за кадр при 60 FPS
  
  # Позиция "за экраном сверху" для shop_panel
  PANEL_HIDDEN_TOP_Y = -PANEL_H    # = -128

  # ─────────────────────────────────────────────────────────
  # Панель золота (gold_panel.png)
  # ─────────────────────────────────────────────────────────
  GOLD_PANEL_W = 134
  GOLD_PANEL_H = 72
  GOLD_TARGET_X = PANEL_MSG_X + PANEL_MSG_W - GOLD_PANEL_W  # = 394 (правый край message_panel)
  GOLD_TARGET_Y = PANEL_MSG_TARGET_Y + PANEL_MSG_H - GOLD_PANEL_H # = 384 (низ message_panel)
  GOLD_HIDDEN_X = 576 # Прячем за правым краем экрана

  # Скорость печати текста (кадров на символ, как в SearchOverlay)
  TEXT_REVEAL_SPEED  = 2

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

  def initialize(font = nil, db = nil, large_font = nil, game_text = {}, party = [], start_inventory = [], classes_data = [], class_names = {}, font_gold = nil)
    @font           = font
    @large_font     = large_font || font
    @font_gold      = font_gold || font
    @db             = db
    @game_text      = game_text || {}
    @party          = party
    @start_inventory = start_inventory
    @classes_data   = classes_data
    @class_names    = class_names
    
    # Создаем меню выбора персонажа (оно уже есть в item_actions_ui.rb)
    @target_menu = GiveMenu.new(@font, @large_font, @db, @party, @classes_data, @class_names, @start_inventory, @game_text)
    
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
    
    # Цены предметов текущего магазина (параллельно @shop_icons)
    @shop_prices    = []

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

    # Анимация панели сообщения (выезд из низа)
    @panel_anim_y = PANEL_MSG_HIDDEN_Y
    # Анимация shop_panel (выезд из верха)
    @top_panel_anim_y = PANEL_HIDDEN_TOP_Y
    # Анимация gold_panel
    @gold_panel_x = GOLD_HIDDEN_X
    # Печать текста
    @text_reveal_index = 0
    @text_reveal_timer = 0
    @text_full_shown   = false
    
    @buy_item_name = ""
    @buy_item_price = 0
    
    @commands_tex = nil
    @commands_w   = 0
    @commands_h   = 0

    @confirm_index = 0
    @confirm_anim_timer = 0
    @yes_tex = nil
    @no_tex = nil
    @yes_anim_tex = nil
    @no_anim_tex = nil
    @yes_no_panel_tex = nil

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

    @message_panel_tex = nil
    mp_path = "assets/ui/message_panel.png"
    if File.exist?(mp_path)
      @message_panel_tex = Raylib.LoadTexture(mp_path)
      Raylib.SetTextureFilter(@message_panel_tex, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: #{mp_path} not found."
    end

    # Загрузка панели золота
    @gold_panel_tex = nil
    gp_path = "assets/ui/gold_panel.png"
    if File.exist?(gp_path)
      @gold_panel_tex = Raylib.LoadTexture(gp_path)
      Raylib.SetTextureFilter(@gold_panel_tex, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: #{gp_path} not found."
    end

    # ═══ ЗАГРУЗКА YES/NO ТЕКСТУР (теперь внутри метода!) ═══
    if File.exist?("assets/ui/menu/Yes.png")
      @yes_tex = Raylib.LoadTexture("assets/ui/menu/Yes.png")
      Raylib.SetTextureFilter(@yes_tex, 0)
    end
    if File.exist?("assets/ui/menu/No.png")
      @no_tex = Raylib.LoadTexture("assets/ui/menu/No.png")
      Raylib.SetTextureFilter(@no_tex, 0)
    end
    if File.exist?("assets/ui/menu/Yes_Anim.png")
      @yes_anim_tex = Raylib.LoadTexture("assets/ui/menu/Yes_Anim.png")
      Raylib.SetTextureFilter(@yes_anim_tex, 0)
    end
    if File.exist?("assets/ui/menu/No_Anim.png")
      @no_anim_tex = Raylib.LoadTexture("assets/ui/menu/No_Anim.png")
      Raylib.SetTextureFilter(@no_anim_tex, 0)
    end
    if File.exist?("assets/ui/yes-no_panel.png")
      @yes_no_panel_tex = Raylib.LoadTexture("assets/ui/yes-no_panel.png")
      Raylib.SetTextureFilter(@yes_no_panel_tex, 0)
    end
    load_commands_panel
  end

  def load_commands_panel
    path = "assets/ui/Commands.png"
    if File.exist?(path)
      img = Raylib.LoadImage(path)
      @commands_tex = Raylib.LoadTextureFromImage(img)
      Raylib.UnloadImage(img)
      Raylib.SetTextureFilter(@commands_tex, Raylib::TEXTURE_FILTER_POINT)
      @commands_w = @commands_tex.width
      @commands_h = @commands_tex.height
    else
      puts "WARNING: #{path} not found."
      @commands_tex = nil
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
    @shop_prices = item_names.map do |name|
      data = @db ? @db.find_by_name(name) : nil
      data ? (data["price"] || 0).to_i : 0
    end
    @selected_item_index = 0
    @shop_scroll_index   = 0
    @input_timer_left    = 0
    @input_timer_right   = 0
    @panel_anim_y        = PANEL_MSG_HIDDEN_Y
    @top_panel_anim_y    = PANEL_HIDDEN_TOP_Y
    @gold_panel_x        = GOLD_HIDDEN_X
    @text_reveal_index   = 0
    @text_reveal_timer   = 0
    @text_full_shown     = false
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
    @shop_prices  = []
    @selected_item_index = 0
    @shop_scroll_index   = 0
    @input_timer_left    = 0
    @input_timer_right   = 0
    @selection_blink_timer = 0
    @top_panel_anim_y    = PANEL_HIDDEN_TOP_Y
    @gold_panel_x        = GOLD_HIDDEN_X
    @text_reveal_index   = 0
    @text_reveal_timer   = 0
    @text_full_shown     = false
  end

def handle_input(dt = 1.0 / 60.0)
  return unless @visible

  case @sub_state
  when :main
    if Raylib.IsKeyPressed(Raylib::KEY_S)
      close
      return
    end
    old_index = @selected_index
    if Raylib.IsKeyPressed(Raylib::KEY_UP)
      @selected_index = 0
    elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
      @selected_index = 1
    elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
      @selected_index = 2
    elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
      @selected_index = 3
    end
    @anim_timer = 0 if @selected_index != old_index
    
    if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
      @sub_action = @tiles[@selected_index]["name"]
      @sub_state  = :sub
      @panel_anim_y      = PANEL_MSG_HIDDEN_Y
      @top_panel_anim_y  = PANEL_HIDDEN_TOP_Y
      @gold_panel_x      = GOLD_HIDDEN_X
      @text_reveal_index = 0
      @text_reveal_timer = 0
      @text_full_shown   = false
    end

  when :sub
    if Raylib.IsKeyPressed(Raylib::KEY_S)
      @sub_state = :main
      @input_timer_left  = 0
      @input_timer_right = 0
      return
    end
    if !@text_full_shown && (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D))
      @text_reveal_index = message_for_action.length
      @text_full_shown   = true
      return
    end
    return unless @text_full_shown && @top_panel_anim_y >= PANEL_Y

    count = @shop_icons.size
    if count > 0
    old_item_index = @selected_item_index
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
      @anim_timer = 0 if @selected_item_index != old_item_index
    end

    if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
      if @shop_icons.any? && @selected_item_index < @shop_icons.size
        @buy_item_name = @current_shop["items"][@selected_item_index]
        item_data = @db ? @db.find_by_name(@buy_item_name) : nil
        @buy_item_price = item_data ? (item_data["price"] || 0).to_i : 0
        @sub_state = :confirm_buy
        @text_reveal_index = 0
        @text_full_shown = false
        @confirm_index = 0        # <--- СБРАСЫВАЕМ НА YES
        @confirm_anim_timer = 0   # <--- СБРАСЫВАЕМ АНИМАЦИЮ
        return
      end
    end

  when :confirm_buy
    if !@text_full_shown && (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D))
      @text_reveal_index = get_confirm_text.length
      @text_full_shown = true
      return
    end
    return unless @text_full_shown

    # Переключение между YES и NO
    if Raylib.IsKeyPressed(Raylib::KEY_LEFT) || Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
      @confirm_index = 1 - @confirm_index
    elsif Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D)
      if @confirm_index == 0 # YES
        gold_amount = @db&.globals ? (@db.globals["gold"] || 0) : 0
        if gold_amount >= @buy_item_price
          @sub_state = :select_target
          @target_menu.open_for_shop(@buy_item_name)
        else
          @sub_state = :insufficient_funds
        end
        @text_reveal_index = 0
        @text_full_shown = false
      else # NO
        @sub_state = :sub
        @text_reveal_index = 0
        @text_full_shown = false
      end
    elsif Raylib.IsKeyPressed(Raylib::KEY_S)
      # S всегда отменяет
      @sub_state = :sub
      @text_reveal_index = 0
      @text_full_shown = false
    end

  when :insufficient_funds
    # Если текст еще печатается, а игрок нажал кнопку — показываем всё сразу
    if !@text_full_shown && (Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D))
      @text_reveal_index = get_insufficient_funds_text.length
      @text_full_shown = true
      return
    end
    
    # Если текст показан и нажата кнопка — возвращаемся к выбору предметов
    return unless @text_full_shown
    if Raylib.IsKeyPressed(Raylib::KEY_A) || Raylib.IsKeyPressed(Raylib::KEY_D) || Raylib.IsKeyPressed(Raylib::KEY_S)
      @sub_state = :sub
      @text_reveal_index = 0
      @text_full_shown = false
    end

  when :select_target
    @target_menu.handle_input(dt)
  end
end

def update(dt = 1.0 / 60.0)
  return unless @visible
  @anim_timer += dt * 60.0
  @selection_blink_timer += dt * 60.0

  speed = PANEL_MSG_SPEED * dt * 60.0

  # 1. РЕЖИМ ВЫБОРА ПЕРСОНАЖА
  if @sub_state == :select_target
    @target_menu.update(dt)
    unless @target_menu.visible
      # ─── СПИСЫВАЕМ ЗОЛОТО ТОЛЬКО ЕСЛИ ПРЕДМЕТ ОТДАН! ───
      if @target_menu.item_given_successfully
        @db.globals["gold"] -= @buy_item_price if @db && @db.globals
      end

      @sub_state = :sub
      @top_panel_anim_y = PANEL_Y
      @gold_panel_x = GOLD_TARGET_X
      @text_reveal_index = 0
      @text_full_shown = false
    end
    return
  end

  # 2. РЕЖИМ МАГАЗИНА (крест -> подменю)
  if @sub_state == :sub
    if @top_panel_anim_y < PANEL_Y
      @top_panel_anim_y += speed
      @top_panel_anim_y = PANEL_Y if @top_panel_anim_y > PANEL_Y
    end
    if @gold_panel_x > GOLD_TARGET_X
      @gold_panel_x -= speed
      @gold_panel_x = GOLD_TARGET_X if @gold_panel_x < GOLD_TARGET_X
    end

    if @panel_anim_y > PANEL_MSG_TARGET_Y
      @panel_anim_y -= speed
      @panel_anim_y = PANEL_MSG_TARGET_Y if @panel_anim_y < PANEL_MSG_TARGET_Y
    elsif !@text_full_shown
      full_text = message_for_action
      @text_reveal_timer += dt * 60.0
      while @text_reveal_timer >= TEXT_REVEAL_SPEED && @text_reveal_index < full_text.length
        @text_reveal_timer -= TEXT_REVEAL_SPEED
        @text_reveal_index += 1
        while full_text[@text_reveal_index, 3] == "{N}"
          @text_reveal_index += 3
        end
      end
      @text_full_shown = true if @text_reveal_index >= full_text.length
    end

  # 3. РЕЖИМ ПОДТВЕРЖДЕНИЯ ПОКУПКИ И ОШИБКИ
  elsif @sub_state == :confirm_buy || @sub_state == :insufficient_funds
    @confirm_anim_timer += dt * 60.0 if @sub_state == :confirm_buy
    if @top_panel_anim_y > PANEL_HIDDEN_TOP_Y
      @top_panel_anim_y -= speed
      @top_panel_anim_y = PANEL_HIDDEN_TOP_Y if @top_panel_anim_y < PANEL_HIDDEN_TOP_Y
    end
    if @gold_panel_x < GOLD_HIDDEN_X
      @gold_panel_x += speed
      @gold_panel_x = GOLD_HIDDEN_X if @gold_panel_x > GOLD_HIDDEN_X
    end

    unless @text_full_shown
      # Выбираем нужный текст в зависимости от состояния
      full_text = @sub_state == :confirm_buy ? get_confirm_text : get_insufficient_funds_text
      
      @text_reveal_timer += dt * 60.0
      while @text_reveal_timer >= TEXT_REVEAL_SPEED && @text_reveal_index < full_text.length
        @text_reveal_timer -= TEXT_REVEAL_SPEED
        @text_reveal_index += 1
        while full_text[@text_reveal_index, 3] == "{N}"
          @text_reveal_index += 3
        end
      end
      @text_full_shown = true if @text_reveal_index >= full_text.length
    end

  # 4. ЗАКРЫТИЕ
  else
    if @top_panel_anim_y > PANEL_HIDDEN_TOP_Y || @gold_panel_x < GOLD_HIDDEN_X
      if @top_panel_anim_y > PANEL_HIDDEN_TOP_Y
        @top_panel_anim_y -= speed
        @top_panel_anim_y = PANEL_HIDDEN_TOP_Y if @top_panel_anim_y < PANEL_HIDDEN_TOP_Y
      end
      if @gold_panel_x < GOLD_HIDDEN_X
        @gold_panel_x += speed
        @gold_panel_x = GOLD_HIDDEN_X if @gold_panel_x > GOLD_HIDDEN_X
      end
    elsif @panel_anim_y < PANEL_MSG_HIDDEN_Y
      @panel_anim_y += speed
      @panel_anim_y = PANEL_MSG_HIDDEN_Y if @panel_anim_y > PANEL_MSG_HIDDEN_Y
    elsif @sub_action
      @sub_action = nil
      @text_reveal_index = 0
      @text_reveal_timer = 0
      @text_full_shown = false
    end
  end
end

  def selected_action
    @tiles[@selected_index]["name"]
  end

  def draw
    return unless @visible

    # Если мы выбираем персонажа, рисуем ТОЛЬКО меню выбора
    if @sub_state == :select_target
      @target_menu.draw
      return
    end

    # shop_panel — видна, когда выезжает/уже выехала
    draw_panel if @top_panel_anim_y > PANEL_HIDDEN_TOP_Y

    # message_panel — видна, когда выезжает/уже выехала
    draw_message_panel if @panel_anim_y < PANEL_MSG_HIDDEN_Y

    # gold_panel — видна, когда выезжает/уже выехала
    draw_gold_panel if @gold_panel_x < GOLD_HIDDEN_X

    # Крест — только когда все панели полностью спрятаны и мы в :main
    fully_closed = @sub_state == :main &&
                   @top_panel_anim_y <= PANEL_HIDDEN_TOP_Y &&
                   @gold_panel_x >= GOLD_HIDDEN_X &&
                   @panel_anim_y >= PANEL_MSG_HIDDEN_Y
    draw_tiles if fully_closed
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
    y = @top_panel_anim_y

    if @panel_tex
      src = Raylib::Rectangle.create(0, 0, PANEL_W, PANEL_H)
      dst = Raylib::Rectangle.create(PANEL_X, y, PANEL_W, PANEL_H)
      Raylib::DrawTexturePro(@panel_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    else
      Raylib::DrawRectangle(PANEL_X, y, PANEL_W, PANEL_H, Raylib::GRAY)
      Raylib::DrawRectangleLines(PANEL_X, y, PANEL_W, PANEL_H, Raylib::DARKGRAY)
    end

    draw_panel_items
  end

  # ─────────────────────────────────────────────────────────
  # Панель сообщения снизу (выезжает при :sub)
  # ─────────────────────────────────────────────────────────
  def draw_message_panel
    return unless @message_panel_tex
    return if @panel_anim_y >= PANEL_MSG_HIDDEN_Y

    dst = Raylib::Rectangle.create(PANEL_MSG_X, @panel_anim_y, PANEL_MSG_W, PANEL_MSG_H)
    src = Raylib::Rectangle.create(0, 0, @message_panel_tex.width, @message_panel_tex.height)
    Raylib::DrawTexturePro(@message_panel_tex, src, dst,
                           Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

    full_text = case @sub_state
                when :confirm_buy then get_confirm_text
                when :insufficient_funds then get_insufficient_funds_text
                else message_for_action
                end
    display_text = full_text[0, @text_reveal_index] || ""
    return if display_text.empty?

    if @large_font
      # Разбиваем текст по тегу {N}
      lines = display_text.split("{N}", -1)
      
      # Начинаем с panel_y + 10, как в GiveMenu / DropMenu
      y_offset = @panel_anim_y + 10
      
      lines.each do |line|
        Raylib.DrawTextEx(@large_font, line,
                          Raylib::Vector2.create(PANEL_MSG_X + 20, y_offset),
                          30, 1, Raylib::WHITE)
        y_offset += 38 # Межстрочный интервал
      end
    end

    # ═══ ОТРИСОВКА YES / NO ═══
    if @sub_state == :confirm_buy && @text_full_shown
      icon_size = 48
      spacing = 36
      panel_w = 64
      panel_h = 48
      gap_to_panel = 18 # Строго 18 пикселей от иконки NO до панели

      # Считаем ширину ТОЛЬКО двух иконок (без панели)
      icons_total_width = (icon_size * 2) + spacing
      
      # Центрируем ТОЛЬКО иконки
      start_x = PANEL_MSG_X + (PANEL_MSG_W - icons_total_width) / 2
      
      # Опускаем вниз на рамку
      icon_y = @panel_anim_y - icon_size + 10

      # 1. Рисуем иконку YES
      if @yes_tex
        tex_to_draw = @yes_tex
        if @confirm_index == 0 && @yes_anim_tex
          tex_to_draw = @yes_anim_tex if (@confirm_anim_timer % 24) < 12
        end
        src = Raylib::Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
        dst = Raylib::Rectangle.create(start_x, icon_y, icon_size, icon_size)
        color = (@confirm_index == 0) ? Raylib::WHITE : Raylib::GRAY
        Raylib::DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0,0), 0, color)
      end

      # 2. Рисуем иконку NO
      no_icon_x = start_x + icon_size + spacing
      if @no_tex
        tex_to_draw = @no_tex
        if @confirm_index == 1 && @no_anim_tex
          tex_to_draw = @no_anim_tex if (@confirm_anim_timer % 24) < 12
        end
        src = Raylib::Rectangle.create(0, 0, tex_to_draw.width, tex_to_draw.height)
        dst = Raylib::Rectangle.create(no_icon_x, icon_y, icon_size, icon_size)
        color = (@confirm_index == 1) ? Raylib::WHITE : Raylib::GRAY
        Raylib::DrawTexturePro(tex_to_draw, src, dst, Raylib::Vector2.create(0,0), 0, color)
      end

      # 3. Рисуем панель с текстом (строго +18px от иконки NO)
      if @yes_no_panel_tex
        panel_x = no_icon_x + icon_size + gap_to_panel
        
        # Рисуем саму синюю панель
        src = Raylib::Rectangle.create(0, 0, @yes_no_panel_tex.width, @yes_no_panel_tex.height)
        dst = Raylib::Rectangle.create(panel_x, icon_y, panel_w, panel_h)
        Raylib::DrawTexturePro(@yes_no_panel_tex, src, dst, Raylib::Vector2.create(0,0), 0, Raylib::WHITE)

        # Рисуем текст "Yes" или "No" внутри панели
        text = (@confirm_index == 0) ? "Yes" : "No"
        
        # Используем шрифт и центрируем текст внутри панели 64x48
        if @font
          font_size = 20
          text_size = Raylib.MeasureTextEx(@font, text, font_size, 1)
          text_x = panel_x + (panel_w - text_size.x) / 2
          text_y = icon_y + (panel_h - text_size.y) / 2
          Raylib.DrawTextEx(@font, text, Raylib::Vector2.create(text_x, text_y), font_size, 1, Raylib::WHITE)
        end
      end
    end
  end

  def draw_gold_panel
    return unless @gold_panel_tex
    return if @gold_panel_x >= GOLD_HIDDEN_X

    # Двигаем по X, Y фиксирован (справа внизу панели сообщения)
    dst = Raylib::Rectangle.create(@gold_panel_x, GOLD_TARGET_Y, GOLD_PANEL_W, GOLD_PANEL_H)
    src = Raylib::Rectangle.create(0, 0, @gold_panel_tex.width, @gold_panel_tex.height)
    Raylib::DrawTexturePro(@gold_panel_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

    gold_amount = @db&.globals ? (@db.globals["gold"] || 0) : 0

    if @font_gold
      # 1. Рисуем надпись "GOLD"
      label_text = "GOLD"
      label_w = Raylib.MeasureTextEx(@font_gold, label_text, 18, 1).x
      label_x = @gold_panel_x + (GOLD_PANEL_W - label_w) / 2
      label_y = GOLD_TARGET_Y + 16

      Raylib.DrawTextEx(@font_gold, label_text, Raylib::Vector2.create(label_x, label_y), 18, 1, Raylib::WHITE)

      # 2. Рисуем цифры (с пробелами)
      gold_digits = [gold_amount, 9999999999].min.to_s.chars.join(' ')
      
      # Размер шрифта цифр всегда 18
      val_size = 18
      
      gold_val_width = Raylib.MeasureTextEx(@font_gold, gold_digits, val_size, 1).x
      val_x = @gold_panel_x + (GOLD_PANEL_W - gold_val_width) / 2
      val_y = GOLD_TARGET_Y + 38
      
      Raylib.DrawTextEx(@font_gold, gold_digits, Raylib::Vector2.create(val_x, val_y), val_size, 1, Raylib::YELLOW)
    end
  end

  # Текст на панели зависит от действия
  def message_for_action
    case @sub_action
    when "buy"    then @game_text["0009"] || "Что вы хотите купить?"
    when "sell"   then @game_text["0010"] || "What do you want to sell?"
    when "repair" then @game_text["0011"] || "What do you want to repair?"
    when "deals"  then @game_text["0012"] || "What do you want to deal?"
    else "What do you want to do?"
    end
  end

  def get_confirm_text
    # Берем текст из файла. Если там пусто, используем фоллбек (запасной вариант)
    template = @game_text["0013"] || "The {ITEM} costs{N}{PRICE} gold coins.{N}OK?"
    
    # Подставляем название предмета и цену
    text = template.gsub("{ITEM}", @buy_item_name.to_s)
    text = text.gsub("{PRICE}", @buy_item_price.to_s)
    text
  end

  def get_insufficient_funds_text
    @game_text["0018"] || "You don't have enough gold to buy this item."
  end

  # def get_equip_text
    # @game_text["0017"] || "Equip it now?"
  # end

  def draw_panel_items
    return if @shop_icons.empty?

    base_y = @top_panel_anim_y

    # Ценник 24×64, иконка 32×48.
    # Верх строки = верх ценника, центрируем 64px в панели.
    tag_h   = 64
    row_top = base_y + (PANEL_H - tag_h) / 2
    tag_y   = row_top
    icon_y  = row_top + (tag_h - ICON_H)

    cy     = base_y + PANEL_H / 2

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

      # 1. Ценник — САМЫЙ ПЕРВЫЙ, значит сзади всего.
      #    PNG 56×72: слева сверху шнурок, слева снизу прозрачно,
      #    справа бирка. Левый край PNG совпадает с иконкой,
      #    низ PNG = низ иконки → tag_y = icon_y - 24.
      if @price_tag_tex
        tag_x   = x
        tag_y_png = icon_y - 24
        tag_src = Raylib::Rectangle.create(0, 0, 56, 72)
        tag_dst = Raylib::Rectangle.create(tag_x, tag_y_png, 56, 72)
        Raylib::DrawTexturePro(@price_tag_tex, tag_src, tag_dst,
                               Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      # 2. Обводка выбора (поверх ценника)
      if icon_index == @selected_item_index && sel_tex
        sx   = x - SELECTION_PAD_X
        sy   = icon_y - SELECTION_PAD_Y
        ssrc = Raylib::Rectangle.create(0, 0, SELECTION_W, SELECTION_H)
        sdst = Raylib::Rectangle.create(sx, sy, SELECTION_W, SELECTION_H)
        Raylib::DrawTexturePro(sel_tex, ssrc, sdst,
                               Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
      end

      # 3. Иконка предмета — поверх всего
      src = Raylib::Rectangle.create(0, 0, ICON_W, ICON_H)
      dst = Raylib::Rectangle.create(x, icon_y, ICON_W, ICON_H)
      Raylib::DrawTexturePro(tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

      # 4. Цена на бирке — ПОВЁРНУТА на 90° (как в SF2).
      price = @shop_prices[icon_index]
      if price && price > 0
        text = price.to_s
        # Ограничение: максимум 7 цифр (до 9 999 999)
        text = text[0, 7] if text.length > 7

        tag_center_x = x + 46
        tag_center_y = tag_y_png + 43

        size = case text.length
               when 1, 2 then 20
               when 3    then 20
               when 4    then 20
               when 5    then 17
               when 6    then 14
               when 7    then 14
               else 9
               end

        vec = Raylib.MeasureTextEx(@font, text, size, 1)
        tw = vec.x
        th = vec.y

        origin = Raylib::Vector2.create(tw / 2, th / 2)
        pos    = Raylib::Vector2.create(tag_center_x, tag_center_y)

        Raylib.DrawTextPro(@font, text, pos, origin, 90.0, size, 1, DARKGREEN)
      end
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

    # ═══ ПАНЕЛЬ С НАЗВАНИЕМ ВЫБРАННОГО ПУНКТА ═══
    if @commands_tex
      panel_x = center_x + OFFSET + TILE_SIZE / 2 + 16
      panel_y = center_y - @commands_h / 2 + 24

      src = Raylib::Rectangle.create(0, 0, @commands_w, @commands_h)
      dst = Raylib::Rectangle.create(panel_x, panel_y, @commands_w, @commands_h)
      Raylib::DrawTexturePro(@commands_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

      if @large_font
        name = @tiles[@selected_index]["name"].capitalize
        font_size = 30
        text_size = Raylib.MeasureTextEx(@large_font, name, font_size, 1)
        text_x = panel_x + 20
        text_y = panel_y + (@commands_h - text_size.y) / 2
        Raylib.DrawTextEx(@large_font, name, Raylib::Vector2.create(text_x, text_y), font_size, 1, Raylib::WHITE)
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
    Raylib::UnloadTexture(@message_panel_tex) if @message_panel_tex
    @message_panel_tex = nil
    Raylib::UnloadTexture(@gold_panel_tex) if @gold_panel_tex
    @gold_panel_tex = nil    
    Raylib::UnloadTexture(@commands_tex) if @commands_tex
    @commands_tex = nil
    # ═══════════════════════════════════════
    Raylib::UnloadTexture(@yes_tex) if @yes_tex
    Raylib::UnloadTexture(@no_tex) if @no_tex
    Raylib::UnloadTexture(@yes_anim_tex) if @yes_anim_tex
    Raylib::UnloadTexture(@no_anim_tex) if @no_anim_tex
    # ═══════════════════════════════════════
    Raylib::UnloadTexture(@yes_no_panel_tex) if @yes_no_panel_tex
  end
end