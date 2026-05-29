# lib/GameMap.rb
require 'json'
require 'raylib'
include Raylib

class GameMap
  attr_reader :width, :height, :tile_size, :tileset_texture, :music_file, :music_volume, :areas,
              :roof_events
  attr_reader :tileset_path
  # Добавил, чтобы game.rb мог получить перестроенные текстуры после rebuild_layers
  attr_reader :layer2, :top_layer

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

    # Загружаем события крыш (roof events)
    @roof_events = []
    events_path = "data/maps/#{entry['folder']}/events.json"
    if File.exist?(events_path)
      begin
        @roof_events = JSON.parse(File.read(events_path))
      rescue
        @roof_events = []
      end
    end

    # Тайлсет и типы тайлов
    raw_path = data['tileset'] || "assets/tilesets/tileset.png"
    safe_path = raw_path.gsub('\\', '/')
    safe = safe_path.gsub(/[\\\/:]/, '_')
    type_file = "data/tile_types/#{safe}.json"

    if File.exist?(type_file)
      @tile_types = JSON.parse(File.read(type_file))
    else
      @tile_types = []
    end

    tileset_path = safe_path.gsub('//', '/')
    @tileset_texture = LoadTexture(tileset_path)
    @tileset_path = tileset_path
    SetTextureFilter(@tileset_texture, TEXTURE_FILTER_POINT)
    @tile_size = 48

    if @tileset_texture.width == 0 || @tileset_texture.height == 0
      puts "Tileset texture failed to load or has zero dimensions: #{tileset_path}"
      create_fallback_map
      return
    end

    @center_vec = Vector2.create(@tile_size / 2.0, @tile_size / 2.0)

    # ---------- Анимационный тайлсет ----------
    @anim_texture = nil
    @animated_indices = []

    base = File.basename(@tileset_path, ".*")
    anim_png  = "assets/tilesets/Animation_tiles/#{base}_animation.png"
    anim_json = "assets/tilesets/Animation_tiles/#{base}_animation.json"

    if File.exist?(anim_png) && File.exist?(anim_json)
      @anim_texture = LoadTexture(anim_png)
      SetTextureFilter(@anim_texture, TEXTURE_FILTER_POINT)
      @animated_indices = JSON.parse(File.read(anim_json))
      puts "Animation loaded for #{@tileset_path}: #{@animated_indices}"
    end

    tex_w = @tileset_texture.width
    tex_h = @tileset_texture.height
    @full_cols = tex_w / @tile_size
    @full_rows = tex_h / @tile_size
    total_tiles = @full_cols * @full_rows

    if @tile_types.length < total_tiles
      @tile_types += Array.new(total_tiles - @tile_types.length, 0)
    end
  end

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

  def animated_tile?(tile_id)
    @animated_indices.include?(tile_id)
  end

  def passable?(x, y)
    return false if x < 0 || x >= @width || y < 0 || y >= @height

    tile2_id = @tiles2[x][y]
    if tile2_id && tile2_id >= 0
      type2 = @tile_types[tile2_id] || 0
      return false if type2 == 1 || type2 == 2
    end

    tile_id = @tiles[x][y]
    type = @tile_types[tile_id] || 0
    type != 1
  end

  def tile_type_at(x, y)
    return 1 if x < 0 || x >= @width || y < 0 || y >= @height
    tile_id = @tiles[x][y]
    @tile_types[tile_id] || 0
  end

  # Принадлежит ли клетка (x,y) какой-либо зоне крыши из events.json?
  def in_roof_zone?(x, y)
    @roof_events.any? do |ev|
      x1 = [ev['start_x'], ev['end_x']].min
      x2 = [ev['start_x'], ev['end_x']].max
      y1 = [ev['start_y'], ev['end_y']].min
      y2 = [ev['start_y'], ev['end_y']].max
      x.between?(x1, x2) && y.between?(y1, y2)
    end
  end

  # Новая проверка: принадлежит ли клетка (x,y) конкретной зоне по индексу
  def zone_active?(x, y, zone_index)
    ev = @roof_events[zone_index]
    return false unless ev
    x1 = [ev['start_x'], ev['end_x']].min
    x2 = [ev['start_x'], ev['end_x']].max
    y1 = [ev['start_y'], ev['end_y']].min
    y2 = [ev['start_y'], ev['end_y']].max
    x.between?(x1, x2) && y.between?(y1, y2)
  end

  # Перестраивает @layer2 и @top_layer, исключая тайлы из зон с индексами skip_zones
  def rebuild_layers(skip_zones = [])
    if @layer2
      UnloadRenderTexture(@layer2)
      @layer2 = nil
    end
    if @top_layer
      UnloadRenderTexture(@top_layer)
      @top_layer = nil
    end
    @layer2 = build_layer2(skip_zones)
    SetTextureFilter(@layer2.texture, TEXTURE_FILTER_POINT) if @layer2
    @top_layer = build_top_layer(skip_zones)
    SetTextureFilter(@top_layer.texture, TEXTURE_FILTER_POINT) if @top_layer
  end

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
          next if animated_tile?(tile_id)

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

  def build_layer2(skip_zones = [])
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
          next if animated_tile?(tile_id)

          # Пропускаем тайлы, попадающие в активные зоны (если они переданы)
          next if skip_zones.any? { |zi| zone_active?(x, y, zi) }

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

  def build_top_layer(skip_zones = [])
    return nil if @tileset_texture.nil? || @width.nil? || @height.nil?

    rt = LoadRenderTexture(@width * @tile_size, @height * @tile_size)
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT)

    BeginTextureMode(rt)
      ClearBackground(BLANK)

      half = @tile_size / 2.0
      center = @center_vec
      dst = Rectangle.create(0, 0, @tile_size, @tile_size)

      # ========================
      # ПЕРВЫЙ СЛОЙ – тип 3
      # ========================
      (0...@width).each do |x|
        (0...@height).each do |y|
          tile_id = @tiles[x][y]          # ← первый слой!
          next if tile_id.nil? || tile_id < 0
          next if animated_tile?(tile_id)
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

      # ========================
      # ВТОРОЙ СЛОЙ – тип 3
      # ========================
      (0...@width).each do |x|
        (0...@height).each do |y|
          tile_id = @tiles2[x][y]         # ← второй слой!
          next if tile_id.nil? || tile_id < 0
          next if animated_tile?(tile_id)
          type = @tile_types[tile_id] || 0
          next unless type == 3

          # Пропускаем тайлы, попадающие в активные зоны
          next if skip_zones.any? { |zi| zone_active?(x, y, zi) }

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

  def inside_area?(x, y)
    return true if @areas.nil? || @areas.empty?
    area = @areas.first
    start = area['mainLayerStart']
    endp  = area['mainLayerEnd']
    x.between?(start[0], endp[0]) && y.between?(start[1], endp[1])
  end

  def area_bounds
    return nil if @areas.nil? || @areas.empty?
    area = @areas.first
    start = area['mainLayerStart']
    endp  = area['mainLayerEnd']
    {
      left:   start[0] * @tile_size,
      top:    start[1] * @tile_size,
      right:  (endp[0] + 1) * @tile_size,
      bottom: (endp[1] + 1) * @tile_size
    }
  end

  def default_spawn
    if @areas && !@areas.empty?
      area = @areas.first
      start = area['mainLayerStart']
      endp  = area['mainLayerEnd']
      [(start[0] + endp[0]) / 2, (start[1] + endp[1]) / 2]
    else
      [@width / 2, @height / 2]
    end
  end

  # === Отрисовка анимированных тайлов ===
  def draw_animated_tiles(use_animation)
    return if @animated_indices.empty? || @tileset_texture.nil?

    half = @tile_size / 2.0
    center = @center_vec
    dst = Rectangle.create(0, 0, @tile_size, @tile_size)

    # Первый слой
    (0...@width).each do |x|
      (0...@height).each do |y|
        tile_id = @tiles[x][y]
        next if tile_id.nil? || tile_id < 0
        next unless animated_tile?(tile_id)

        texture = use_animation ? @anim_texture : @tileset_texture
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
        DrawTexturePro(texture, src_rect, dst, center, angle, WHITE)
      end
    end

    # Второй слой (теперь без фильтрации – всё уже учтено в перестроенных текстурах)
    (0...@width).each do |x|
      (0...@height).each do |y|
        tile_id = @tiles2[x][y]
        next if tile_id.nil? || tile_id < 0
        next unless animated_tile?(tile_id)

        texture = use_animation ? @anim_texture : @tileset_texture
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
        DrawTexturePro(texture, src_rect, dst, center, angle, WHITE)
      end
    end
  end

  # (build_roof_layer больше не используется, но можно оставить для истории)
end