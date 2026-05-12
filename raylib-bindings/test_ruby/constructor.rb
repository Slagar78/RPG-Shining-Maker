# constructor.rb
require 'gosu'
require 'json'

class Constructor < Gosu::Window
  def initialize
    super(1024, 768)
    self.caption = "RPG Shinzo Constructor"
    
    @font = Gosu::Font.new(16)
    @small_font = Gosu::Font.new(12)
    
    @white = Gosu::Color::WHITE
    @black = Gosu::Color::BLACK
    @gray = Gosu::Color::GRAY
    @blue = Gosu::Color::BLUE
    @green = Gosu::Color::GREEN
    @yellow = Gosu::Color::YELLOW
    @red = Gosu::Color::RED
    
    # Состояние
    @show_file_menu = false
    @current_map = "MAP001"
    @zoom = 100
    @current_tab = :map  # :map, :database
    
    # Данные
    @maps = ["MAP001", "MAP002", "MAP003"]
    @selected_map = 0
    
    # Редактор карты
    @map_data = Array.new(20) { Array.new(15, 0) }
    @selected_tile = 1
    @current_tool = :pencil
    
    # Палитра тайлов (без BROWN)
    @tiles = [
      { id: 0, color: @gray, name: "Пусто" },
      { id: 1, color: @green, name: "Трава" },
      { id: 2, color: Gosu::Color::GRAY, name: "Земля" },
      { id: 3, color: @blue, name: "Вода" }
    ]
    
    # База данных
    load_database
    
    # Загрузка иконок
    load_icons
  end
  
  def load_icons
    @icons = {}
    
    icons = {
      map: "assets/icons/map.png",
      database: "assets/icons/database.png",
      playtest: "assets/icons/playtest.png"
    }
    
    icons.each do |key, path|
      @icons[key] = Gosu::Image.new(path) if File.exist?(path)
    end
  end
  
  def load_database
    if File.exist?("data/actors.json")
      @actors = JSON.parse(File.read("data/actors.json"))["actors"]
    else
      @actors = []
    end
    
    if File.exist?("data/items.json")
      @items = JSON.parse(File.read("data/items.json"))["items"]
    else
      @items = []
    end
  end
  
  def draw
    draw_menu_bar
    draw_toolbar
    draw_main_area
    draw_status_bar
    
    draw_file_menu if @show_file_menu
  end
  
  # ===== МЕНЮ =====
  def draw_menu_bar
    Gosu.draw_rect(0, 0, width, 25, @gray, 0)
    file_color = @show_file_menu ? @blue : @black
    @font.draw_text("File", 10, 5, 1, 1, 1, file_color)
  end
  
  def draw_file_menu
    y = 25
    menu_w = 150
    menu_h = 150
    
    Gosu.draw_rect(10, y, menu_w, menu_h, @gray, 10)
    Gosu.draw_rect(10, y, menu_w, menu_h, @white, 11)
    
    items = ["New Project", "Open Project", "Save Project", "---", "Exit"]
    items.each_with_index do |item, i|
      next if item == "---"
      item_y = y + 5 + i * 25
      @font.draw_text(item, 20, item_y, 12, 1, 1, @black)
    end
  end
  
  # ===== ТУЛБАР =====
  def draw_toolbar
    y = 25
    Gosu.draw_rect(0, y, width, 50, @gray, 0)
    Gosu.draw_line(0, y + 50, @black, width, y + 50, @black, 0)
    
    draw_toolbar_icon(10, y + 10, :map, "Map Editor")
    draw_toolbar_icon(60, y + 10, :database, "Database")
    draw_toolbar_icon(110, y + 10, :playtest, "Playtest")
  end
  
  def draw_toolbar_icon(x, y, icon_key, tooltip)
    if @icons[icon_key]
      @icons[icon_key].draw(x, y, 2, 0.8, 0.8)
    else
      Gosu.draw_rect(x, y, 32, 32, @gray, 2)
      Gosu.draw_rect(x + 1, y + 1, 30, 30, @white, 1)
    end
    
    if mouse_x.between?(x, x + 32) && mouse_y.between?(y, y + 32)
      Gosu.draw_rect(x, y, 32, 32, @blue, 2)
      @small_font.draw_text(tooltip, mouse_x + 15, mouse_y - 20, 10, 1, 1, @white)
    end
  end
  
  # ===== ОСНОВНАЯ ОБЛАСТЬ =====
  def draw_main_area
    y = 75
    panel_w = 200
    
    # Левая панель
    Gosu.draw_rect(0, y, panel_w, height - y - 50, @gray, 0)
    
    if @current_tab == :map
      draw_map_tree(panel_w, y)
    else
      draw_database_panel(panel_w, y)
    end
    
    # Правая область
    Gosu.draw_rect(panel_w, y, width - panel_w, height - y - 50, @white, 0)
    Gosu.draw_line(panel_w, y, @black, panel_w, height - 50, @black, 0)
    
    if @current_tab == :map
      draw_map_editor(panel_w, y)
      draw_tile_palette
    else
      draw_database_view(panel_w, y)
    end
  end
  
  def draw_map_tree(panel_w, y)
    @font.draw_text("Map Tree", 10, y + 10, 2, 1, 1, @black)
    
    @maps.each_with_index do |map, i|
      map_y = y + 40 + i * 25
      if i == @selected_map
        Gosu.draw_rect(5, map_y - 2, panel_w - 10, 22, @blue, 2)
        @font.draw_text(map, 15, map_y, 2, 1, 1, @white)
      else
        @font.draw_text(map, 15, map_y, 2, 1, 1, @black)
      end
    end
  end
  
  def draw_database_panel(panel_w, y)
    @font.draw_text("Database", 10, y + 10, 2, 1, 1, @black)
    
    categories = ["Actors", "Items", "Skills", "Classes"]
    categories.each_with_index do |cat, i|
      cat_y = y + 40 + i * 25
      @font.draw_text(cat, 15, cat_y, 2, 1, 1, @black)
    end
  end
  
  def draw_map_editor(panel_w, y)
    @small_font.draw_text("Map: #{@current_map}", panel_w + 10, y + 10, 2, 1, 1, @black)
    draw_map_grid(panel_w + 50, y + 50)
  end
  
  def draw_map_grid(start_x, start_y)
    grid_size = 48
    cols = 20
    rows = 15
    
    (0...cols).each do |x|
      (0...rows).each do |y|
        tile_id = @map_data[x][y]
        tile = @tiles.find { |t| t[:id] == tile_id }
        color = tile ? tile[:color] : @gray
        Gosu.draw_rect(start_x + x * grid_size, start_y + y * grid_size, grid_size, grid_size, color, 2)
        Gosu.draw_rect(start_x + x * grid_size, start_y + y * grid_size, grid_size, grid_size, @black, 3)
      end
    end
  end
  
  def draw_tile_palette
    x = width - 150
    y = 500
    
    @font.draw_text("Tiles", x, y, 2, 1, 1, @black)
    
    @tiles.each_with_index do |tile, i|
      tile_y = y + 30 + i * 45
      Gosu.draw_rect(x, tile_y, 40, 40, tile[:color], 2)
      Gosu.draw_rect(x, tile_y, 40, 40, @black, 3)
      
      if tile[:id] == @selected_tile
        Gosu.draw_rect(x - 2, tile_y - 2, 44, 44, @blue, 1)
      end
      
      @small_font.draw_text(tile[:name], x + 50, tile_y + 12, 2, 1, 1, @black)
    end
  end
  
  def draw_database_view(panel_w, y)
    @font.draw_text("Actors List", panel_w + 10, y + 10, 2, 1, 1, @black)
    
    @actors.each_with_index do |actor, i|
      actor_y = y + 50 + i * 30
      @small_font.draw_text("#{actor['id']}: #{actor['name']}", panel_w + 20, actor_y, 2, 1, 1, @black)
    end
  end
  
  # ===== СТАТУС БАР =====
  def draw_status_bar
    y = height - 50
    Gosu.draw_rect(0, y, width, 50, @gray, 0)
    Gosu.draw_line(0, y, @black, width, y, @black, 0)
    
    @font.draw_text("#{@current_map} (20x15)", 10, y + 15, 2, 1, 1, @black)
    @font.draw_text("Zoom: #{@zoom}%", width / 2 - 40, y + 15, 2, 1, 1, @black)
    @font.draw_text("X: #{mouse_x / 48} Y: #{mouse_y / 48}", width - 150, y + 15, 2, 1, 1, @black)
  end
  
  # ===== ОБРАБОТКА ВВОДА =====
  def button_down(id)
    case id
    when Gosu::MsLeft
      handle_click
    when Gosu::KB_ESCAPE
      close
    end
  end
  
  def handle_click
    # Меню File
    if mouse_y < 25 && mouse_x.between?(10, 60)
      @show_file_menu = !@show_file_menu
      return
    end
    
    if @show_file_menu
      @show_file_menu = false
      return
    end
    
    # Тулбар
    if mouse_y.between?(35, 67)
      if mouse_x.between?(10, 42)
        @current_tab = :map
      elsif mouse_x.between?(60, 92)
        @current_tab = :database
      elsif mouse_x.between?(110, 142)
        show_message("Playtest", "Запуск игры...")
      end
      return
    end
    
    # Рисование на карте
    if @current_tab == :map && mouse_x > 200 && mouse_x < width - 150
      grid_x = (mouse_x - 250) / 48
      grid_y = (mouse_y - 125) / 48
      if grid_x >= 0 && grid_x < 20 && grid_y >= 0 && grid_y < 15
        @map_data[grid_x][grid_y] = @selected_tile
      end
    end
    
    # Выбор тайла
    if mouse_x > width - 150 && mouse_y > 500
      @tiles.each_with_index do |tile, i|
        tile_y = 500 + 30 + i * 45
        if mouse_y.between?(tile_y, tile_y + 40)
          @selected_tile = tile[:id]
        end
      end
    end
  end
  
  def show_message(title, text)
    puts "#{title}: #{text}"
  end
end

Constructor.new.show if __FILE__ == $0