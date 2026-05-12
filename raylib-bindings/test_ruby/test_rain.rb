# test_rain_rpg.rb — Top‑down rain for RPG (Shining Force style)
require 'raylib'

shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

TILE_SIZE = 48
MAP_W = 10
MAP_H = 10
WIN_W = MAP_W * TILE_SIZE
WIN_H = MAP_H * TILE_SIZE + 60   # extra space for text

tileset_path = "assets/tilesets/tileset.png"
unless File.exist?(tileset_path)
  puts "Файл #{tileset_path} не найден, положите любой тайлсет в assets/tilesets/"
  exit
end

InitWindow(WIN_W, WIN_H, "Top‑down Rain RPG")
SetTargetFPS(60)

tileset = LoadTexture(tileset_path)
SetTextureFilter(tileset, TEXTURE_FILTER_POINT)

# Simple map (all tiles = 0)
map = Array.new(MAP_W) { Array.new(MAP_H, 0) }

# ─── Rain particles ────────────────────────────
RAIN_DROPS = 250          # increased amount for dense rain
DROPLET_LEN = 9..13       # length of each streak
WIND = 0.8                # horizontal drift (rightward)

rain_drops = []
RAIN_DROPS.times do
  rain_drops << {
    x:      rand(WIN_W + 40),          # a bit off‑screen for smooth enter
    y:      rand(-20..0),              # start above the map
    speed:  rand(5..9),               # falling speed
    length: rand(DROPLET_LEN)         # streak length
  }
end

# ─── Ripples (circles on the ground) ────────────
RIPPLE_MAX = 120
ripples = []
RIPPLE_MAX.times do
  ripples << { x: 0, y: 0, radius: 0, life: 0, max_radius: 0 }
end

def spawn_ripple(ripples, x, ground_y)
  free = ripples.find { |r| r[:life] <= 0 }
  return unless free

  free[:x]          = x
  free[:y]          = ground_y - rand(0..2)   # on the ground
  free[:radius]     = 1
  free[:max_radius] = rand(10..20)
  free[:life]       = 25                     # frames
end

def update_rain(drops, win_w, map_h, wind, ripples)
  ground = map_h * TILE_SIZE                # bottom of the map

  drops.each do |d|
    d[:y] += d[:speed]
    d[:x] += wind                           # slant by wind

    # When it hits the ground, create a ripple and reset to top
    if d[:y] > ground
      spawn_ripple(ripples, d[:x], ground)
      d[:y] = rand(-30..-5)
      d[:x] = rand(win_w + 40)
      d[:speed] = rand(5..9)
      d[:length] = rand(DROPLET_LEN)
    end

    # If it drifts too far right, bring it back from the left
    if d[:x] > win_w + 40
      d[:x] = -40
    end
  end

  # Update ripples (expand and fade)
  ripples.each do |r|
    next if r[:life] <= 0
    r[:radius] += 0.4
    r[:life]   -= 1
  end
end

def draw_rain_streaks(drops)
  drops.each do |d|
    # A short slanted line: from (x, y) downwards‑right
    end_x = d[:x] + d[:length] * WIND      # horizontal shift because of slant
    end_y = d[:y] + d[:length]

    # Draw a thin white line with low opacity
    DrawLineEx(
      Vector2.create(d[:x], d[:y]),
      Vector2.create(end_x, end_y),
      1.5,
      Fade(RAYWHITE, 0.25)   # very transparent – feels like a sheet of rain
    )
  end
end

def draw_ripples(ripples)
  ripples.each do |r|
    next if r[:life] <= 0
    alpha = r[:life] / 25.0
    DrawCircleLines(
      r[:x].to_i,
      r[:y].to_i,
      r[:radius].to_i,
      Fade(RAYWHITE, alpha * 0.5)   # subtle white rings
    )
  end
end

# ─── Main loop ──────────────────────────────────
rain_on = true

until WindowShouldClose()
  if IsKeyPressed(KEY_SPACE)
    rain_on = !rain_on
  end

  if rain_on
    update_rain(rain_drops, WIN_W, MAP_H, WIND, ripples)
  end

  BeginDrawing()
  ClearBackground(RAYWHITE)

  # Draw tiles
  (0...MAP_W).each do |x|
    (0...MAP_H).each do |y|
      tile_id = map[x][y]
      src = Rectangle.create(
        (tile_id % 16) * TILE_SIZE,
        (tile_id / 16) * TILE_SIZE,
        TILE_SIZE, TILE_SIZE
      )
      dst = Rectangle.create(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE)
      DrawTexturePro(tileset, src, dst, Vector2.create(0, 0), 0, WHITE)
    end
  end

  # Rain overlays
  if rain_on
    # Faint blueish shade over the whole map (cloudy atmosphere)
    DrawRectangle(
      0, 0, MAP_W * TILE_SIZE, MAP_H * TILE_SIZE,
      Fade(Raylib::BLUE, 0.15)   # лёгкий голубоватый фильтр  # dark blue, very low opacity
    )

    draw_rain_streaks(rain_drops)
    draw_ripples(ripples)
  end

  # Status text
  status = rain_on ? "RAIN (SPACE to stop)" : "CLEAR (SPACE to start rain)"
  DrawText(status, 10, MAP_H * TILE_SIZE + 10, 20, DARKGRAY)
  DrawText("ESC to close", WIN_W - 150, MAP_H * TILE_SIZE + 10, 20, DARKGRAY)

  EndDrawing()
end

UnloadTexture(tileset)
CloseWindow()