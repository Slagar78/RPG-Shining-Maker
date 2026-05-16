# lib/battleManager/battle_ui.rb

class BattleMenu
  attr_reader :selected_index

  def initialize
    @tiles = [
      { "id" => 0, "name" => "attack", "icon" => "assets/ui/menu/Attack.png", "icon_anim" => "assets/ui/menu/Attack_Anim.png" },
      { "id" => 1, "name" => "magic",  "icon" => "assets/ui/menu/Menu2.png",  "icon_anim" => "assets/ui/menu/Menu2_Anim.png" },
      { "id" => 2, "name" => "item",   "icon" => "assets/ui/menu/Menu3.png",  "icon_anim" => "assets/ui/menu/Menu3_Anim.png" },
      { "id" => 3, "name" => "stay",   "icon" => "assets/ui/menu/Stay.png",   "icon_anim" => "assets/ui/menu/Stay_Anim.png" }
    ]

    @visible = false
    @selected_index = 0
    @anim_timer = 0
    @tile_size = 48
    @offset = 48
    @font = nil   # будет установлен из BattleManager

    load_textures
    load_commands_panel
  end

  # Сеттер для шрифта
  def font=(font)
    @font = font
  end

  def load_textures
    @textures = []
    @tiles.each do |tile|
      normal = Raylib.LoadTexture(tile["icon"])
      anim   = Raylib.LoadTexture(tile["icon_anim"])
      Raylib.SetTextureFilter(normal, 0)
      Raylib.SetTextureFilter(anim, 0)
      @textures << { normal: normal, anim: anim }
    end
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

  def open
    @visible = true
    @selected_index = 0
    @anim_timer = 0
  end

  def close
    @visible = false
  end

  def handle_input
    return unless @visible

    if Raylib.IsKeyPressed(Raylib::KEY_UP)
      @selected_index = 0
    elsif Raylib.IsKeyPressed(Raylib::KEY_LEFT)
      @selected_index = 1
    elsif Raylib.IsKeyPressed(Raylib::KEY_RIGHT)
      @selected_index = 2
    elsif Raylib.IsKeyPressed(Raylib::KEY_DOWN)
      @selected_index = 3
    end
  end

  def update
    return unless @visible
    @anim_timer += 1
  end

  def draw
    return unless @visible

    center_x = 576 / 2
    center_y = 480 - 80

    positions = [
      { x: center_x,           y: center_y - @offset + 24 },   # верхняя (Attack)
      { x: center_x - @offset, y: center_y },                  # левая (Magic)
      { x: center_x + @offset, y: center_y },                  # правая (Item)
      { x: center_x,           y: center_y + @offset - 24 }    # нижняя (Stay)
    ]

    # Рисуем иконки
    (0..3).each do |i|
      tex = @textures[i]
      pos = positions[i]

      if i == @selected_index
        use_anim = (@anim_timer % 24) < 12 && tex[:anim]
        texture = use_anim ? tex[:anim] : tex[:normal]
      else
        texture = tex[:normal]
      end

      dst = Raylib::Rectangle.create(pos[:x] - @tile_size/2, pos[:y] - @tile_size/2, @tile_size, @tile_size)
      src = Raylib::Rectangle.create(0, 0, @tile_size, @tile_size)
      Raylib.DrawTexturePro(texture, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)
    end

    # Рисуем панель Commands.png и текст на ней
    if @commands_tex
      panel_x = center_x + @offset + @tile_size/2 + 12   # справа от кнопок с отступом
      panel_y = center_y - @commands_h/2                 # вертикально по центру

      src = Raylib::Rectangle.create(0, 0, @commands_w, @commands_h)
      dst = Raylib::Rectangle.create(panel_x, panel_y, @commands_w, @commands_h)
      Raylib.DrawTexturePro(@commands_tex, src, dst, Raylib::Vector2.create(0, 0), 0, Raylib::WHITE)

      # Текст названия команды
      if @font
  name = @tiles[@selected_index]["name"].capitalize
  font_size = 30
  text_size = Raylib.MeasureTextEx(@font, name, font_size, 1)
  text_x = panel_x + 20
  text_y = panel_y + (@commands_h - text_size.y) / 2
  Raylib.DrawTextEx(@font, name, Raylib::Vector2.create(text_x, text_y), font_size, 1, Raylib::WHITE)
end
    end
  end
end