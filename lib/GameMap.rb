# lib/GameMap.rb
require 'json'
require 'raylib'
include Raylib

class GameMap
  attr_reader :width, :height, :tile_size, :tileset_texture, :music_file, :music_volume, :areas

  def initialize(map_id = "Granseal")
    @src_rect_cache = {}

    # --- Загружаем индекс карт ---
    entries_path = "data/maps/entries.json"
    unless File.exist?(entries_path)
      puts "entries.json not found, creating empty map"
      create_fallback_map
      return
    end

    entries = JSON.parse(File.read(entries_path))
    entry = entries.find { |e| e["folder"] == map_id }
    unless entry
      puts "Map with folder '#{map_id}' not found in entries.json"
      create_fallback_map
      return
    end

    # --- Загружаем layout.json ---
    layout_path = "data/maps/#{entry['folder']}/layout.json"
    unless File.exist?(layout_path)
      puts "layout.json missing for #{entry['folder']}"
      create_fallback_map
      return
    end

    data = JSON.parse(File.read(layout_path))

    @width  = data['width']
    @height = data['height']

    # Первый слой
    @tiles    = data['tiles']
    @rot      = data['rot']      || Array.new(@width) { Array.new(@height, 0) }
    @mirror_x = data['mirror_x'] || Array.new(@width) { Array.new(@height, false) }
    @mirror_y = data['mirror_y'] || Array.new(@width) { Array.new(@height, false) }

    # Второй слой
    if data['tiles2']
      @tiles2    = data['tiles2']
      @rot2      = data['rot2']      || Array.new(@width) { Array.new(@height, 0) }
      @mirror_x2 = data['mirror_x2'] || Array.new(@width) { Array.new(@height, false) }
      @mirror_y2 = data['mirror_y2'] || Array.new(@width) { Array.new(@height, false) }
    else
      @tiles2    = Array.new(@width) { Array.new(@height, -1) }
      @rot2      = Array.new(@width) { Array.new(@height, 0) }
      @mirror_x2 = Array.new(@width) { Array.new(@height, false) }
      @mirror_y2 = Array.new(@width) { Array.new(@height, false) }
    end

    # Сетка коллизий (0 – проходимо, 1 – стена)
    @collision = data['collision'] || Array.new(@width) { Array.new(@height, 0) }

    # Музыка (теперь из entries.json)
    @music_file   = entry['music']        || ""
    @music_volume = entry['music_volume'] || 0.8

    # Загружаем зоны (areas.json), если имеются
    areas_path = "data/maps/#{entry['folder']}/areas.json"
    if File.exist?(areas_path)
      @areas = JSON.parse(File.read(areas_path))
    else
      @areas = []
    end

    # Тайлсет и типы тайлов
    raw_path = data['tileset'] || "assets/tilesets/tileset.png"
    # Для safe-имени оставляем двойной слеш (он был в исходном layout3.json)
    safe_path = raw_path.gsub('\\', '/')          # assets/tilesets//tileset009.png
    safe = safe_path.gsub(/[\\\/:]/, '_')
    type_file = "data/tile_types/#{safe}.json"

    if File.exist?(type_file)
      @tile_types = JSON.parse(File.read(type_file))
    else
      @tile_types = []
    end

    # Для загрузки текстуры убираем двойной слеш
    tileset_path = safe_path.gsub('//', '/')
    @tileset_texture = LoadTexture(tileset_path)
    SetTextureFilter(@tileset_texture, TEXTURE_FILTER_POINT)
    @tile_size = 48

    # Защита от "пустой" текстуры
    if @tileset_texture.width == 0 || @tileset_texture.height == 0
      puts "Tileset texture failed to load or has zero dimensions: #{tileset_path}"
      create_fallback_map
      return
    end

    @center_vec = Vector2.create(@tile_size / 2.0, @tile_size / 2.0)

    tex_w = @tileset_texture.width
    tex_h = @tileset_texture.height
    @full_cols = tex_w / @tile_size
    @full_rows = tex_h / @tile_size
    total_tiles = @full_cols * @full_rows

    if @tile_types.length < total_tiles
      @tile_types += Array.new(total_tiles - @tile_types.length, 0)
    end
  end

  # Запасной вариант – пустая карта
  def create_fallback_map
    @width  = 20
    @height = 15
    @tiles    = Array.new(@width) { Array.new(@height, 0) }
    @tiles2   = Array.new(@width) { Array.new(@height, -1) }
    @rot      = Array.new(@width) { Array.new(@height, 0) }
    @rot2     = Array.new(@width) { Array.new(@height, 0) }
    @mirror_x  = Array.new(@width) { Array.new(@height, false) }
    @mirror_x2 = Array.new(@width) { Array.new(@height, false) }
    @mirror_y  = Array.new(@width) { Array.new(@height, false) }
    @mirror_y2 = Array.new(@width) { Array.new(@height, false) }
    @collision = Array.new(@width) { Array.new(@height, 0) }
    @tile_types = []
    @tileset_texture = nil
    @music_file   = ""
    @music_volume = 0.8
    @areas = []
  end

  # Возвращает прямоугольник в текстуре тайлсета для заданного ID тайла
  def tile_src_rect(tile_id)
    return nil if tile_id.nil? || tile_id < 0
    return @src_rect_cache[tile_id] if @src_rect_cache.key?(tile_id)

    strips = @full_cols / 8
    rows_per_strip = @full_rows
    strip = tile_id / (8 * rows_per_strip)
    local = tile_id % (8 * rows_per_strip)

    col = strip * 8 + (local % 8)
    row = local / 8

    rect = Rectangle.create(col * @tile_size, row * @tile_size,
                            @tile_size, @tile_size)
    @src_rect_cache[tile_id] = rect
    rect
  end

  # Проходимость: тип 1 (стена) блокирует всегда, тип 2 (лёд) на втором слое тоже блокирует
  def passable?(x, y)
    return false if x < 0 || x >= @width || y < 0 || y >= @height

    # Проверка второго слоя
    tile2_id = @tiles2[x][y]
    if tile2_id && tile2_id >= 0
      type2 = @tile_types[tile2_id] || 0
      return false if type2 == 1 || type2 == 2
    end

    # Проверка первого слоя
    tile_id = @tiles[x][y]
    type = @tile_types[tile_id] || 0
    type != 1
  end

  # Возвращает тип тайла на клетке (0, 1, 2, 3…). За границами – тип 1.
  def tile_type_at(x, y)
    return 1 if x < 0 || x >= @width || y < 0 || y >= @height
    tile_id = @tiles[x][y]
    @tile_types[tile_id] || 0
  end

  # Основной фон – все тайлы первого слоя
  def build_static_background
    return nil if @tileset_texture.nil? || @width.nil? || @height.nil?

    rt = LoadRenderTexture(@width * @tile_size, @height * @tile_size)
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT)

    BeginTextureMode(rt)
      ClearBackground(BLANK)

      half = @tile_size / 2.0
      center = @center_vec
      dst = Rectangle.create(0, 0, @tile_size, @tile_size)

      (0...@width).each do |x|
        (0...@height).each do |y|
          tile_id = @tiles[x][y]
          next if tile_id.nil? || tile_id < 0

          src_rect = tile_src_rect(tile_id)
          next unless src_rect

          rot    = @rot[x][y] || 0
          flip_x = @mirror_x[x][y] || false
          flip_y = @mirror_y[x][y] || false

          world_cx = x * @tile_size + half
          world_cy = y * @tile_size + half

          dst.x = world_cx
          dst.y = world_cy
          dst.width  = flip_x ? -@tile_size : @tile_size
          dst.height = flip_y ? -@tile_size : @tile_size

          angle = rot * 90.0
          DrawTexturePro(@tileset_texture, src_rect, dst, center, angle, WHITE)
        end
      end
    EndTextureMode()
    rt
  end

  # Второй слой (tiles2) – декоративный, на проходимость влияет через passable?
  def build_layer2
    return nil if @tileset_texture.nil? || @width.nil? || @height.nil?

    rt = LoadRenderTexture(@width * @tile_size, @height * @tile_size)
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT)

    BeginTextureMode(rt)
      ClearBackground(BLANK)

      half = @tile_size / 2.0
      center = @center_vec
      dst = Rectangle.create(0, 0, @tile_size, @tile_size)

      (0...@width).each do |x|
        (0...@height).each do |y|
          tile_id = @tiles2[x][y]
          next if tile_id.nil? || tile_id < 0

          src_rect = tile_src_rect(tile_id)
          next unless src_rect

          rot    = @rot2[x][y] || 0
          flip_x = @mirror_x2[x][y] || false
          flip_y = @mirror_y2[x][y] || false

          world_cx = x * @tile_size + half
          world_cy = y * @tile_size + half

          dst.x = world_cx
          dst.y = world_cy
          dst.width  = flip_x ? -@tile_size : @tile_size
          dst.height = flip_y ? -@tile_size : @tile_size

          angle = rot * 90.0
          DrawTexturePro(@tileset_texture, src_rect, dst, center, angle, WHITE)
        end
      end
    EndTextureMode()
    rt
  end

  # Верхний слой – только тип 3 (кроны деревьев, крыши), перекрывает игрока
  def build_top_layer
    return nil if @tileset_texture.nil? || @width.nil? || @height.nil?

    rt = LoadRenderTexture(@width * @tile_size, @height * @tile_size)
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT)

    BeginTextureMode(rt)
      ClearBackground(BLANK)

      half = @tile_size / 2.0
      center = @center_vec
      dst = Rectangle.create(0, 0, @tile_size, @tile_size)

      # Первый слой – тип 3
      (0...@width).each do |x|
        (0...@height).each do |y|
          tile_id = @tiles[x][y]
          next if tile_id.nil? || tile_id < 0
          type = @tile_types[tile_id] || 0
          next unless type == 3

          src_rect = tile_src_rect(tile_id)
          next unless src_rect

          rot    = @rot[x][y] || 0
          flip_x = @mirror_x[x][y] || false
          flip_y = @mirror_y[x][y] || false

          world_cx = x * @tile_size + half
          world_cy = y * @tile_size + half

          dst.x = world_cx
          dst.y = world_cy
          dst.width  = flip_x ? -@tile_size : @tile_size
          dst.height = flip_y ? -@tile_size : @tile_size

          angle = rot * 90.0
          DrawTexturePro(@tileset_texture, src_rect, dst, center, angle, WHITE)
        end
      end

      # Второй слой – тип 3
      (0...@width).each do |x|
        (0...@height).each do |y|
          tile_id = @tiles2[x][y]
          next if tile_id.nil? || tile_id < 0
          type = @tile_types[tile_id] || 0
          next unless type == 3

          src_rect = tile_src_rect(tile_id)
          next unless src_rect

          rot    = @rot2[x][y] || 0
          flip_x = @mirror_x2[x][y] || false
          flip_y = @mirror_y2[x][y] || false

          world_cx = x * @tile_size + half
          world_cy = y * @tile_size + half

          dst.x = world_cx
          dst.y = world_cy
          dst.width  = flip_x ? -@tile_size : @tile_size
          dst.height = flip_y ? -@tile_size : @tile_size

          angle = rot * 90.0
          DrawTexturePro(@tileset_texture, src_rect, dst, center, angle, WHITE)
        end
      end
    EndTextureMode()
    rt
  end
end