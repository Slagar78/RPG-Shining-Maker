# test_night_compare.rb
require 'raylib'

shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
Raylib.load_lib(shared_lib_path + 'libraylib.dll')
include Raylib

TILE_SIZE = 48
MAP_W = 10
MAP_H = 10
WIN_W = MAP_W * TILE_SIZE * 2 + 40   # две карты + отступ
WIN_H = MAP_H * TILE_SIZE + 100      # + место для надписей

tileset_path = "assets/tilesets/tileset.png"
unless File.exist?(tileset_path)
  puts "Файл #{tileset_path} не найден, положите любой тайлсет в assets/tilesets/"
  exit
end

InitWindow(WIN_W, WIN_H, "Day vs Evening")
SetTargetFPS(60)

tileset = LoadTexture(tileset_path)
SetTextureFilter(tileset, TEXTURE_FILTER_POINT)

# Простая карта (все тайлы 0)
map = Array.new(MAP_W) { Array.new(MAP_H, 0) }

# Параметры вечернего затемнения
evening_alpha = 180   # 0-255, 180 = ощутимый полумрак, можно менять

until WindowShouldClose()
  BeginDrawing()
  ClearBackground(RAYWHITE)

  # Левая карта (без затемнения) – смещение по X = 20
  left_offset_x = 20
  (0...MAP_W).each do |x|
    (0...MAP_H).each do |y|
      tile_id = map[x][y]
      src = Rectangle.create((tile_id % 16) * TILE_SIZE, (tile_id / 16) * TILE_SIZE, TILE_SIZE, TILE_SIZE)
      dst = Rectangle.create(left_offset_x + x * TILE_SIZE, 60 + y * TILE_SIZE, TILE_SIZE, TILE_SIZE)
      DrawTexturePro(tileset, src, dst, Vector2.create(0, 0), 0, WHITE)
    end
  end
  DrawText("DAY", left_offset_x + (MAP_W * TILE_SIZE) / 2 - 30, 30, 20, DARKGREEN)

  # Правая карта (с вечерним затемнением)
  right_offset_x = left_offset_x + MAP_W * TILE_SIZE + 40
  (0...MAP_W).each do |x|
    (0...MAP_H).each do |y|
      tile_id = map[x][y]
      src = Rectangle.create((tile_id % 16) * TILE_SIZE, (tile_id / 16) * TILE_SIZE, TILE_SIZE, TILE_SIZE)
      dst = Rectangle.create(right_offset_x + x * TILE_SIZE, 60 + y * TILE_SIZE, TILE_SIZE, TILE_SIZE)
      DrawTexturePro(tileset, src, dst, Vector2.create(0, 0), 0, WHITE)
    end
  end
  # Затемняющий прямоугольник поверх правой карты
  DrawRectangle(right_offset_x, 60, MAP_W * TILE_SIZE, MAP_H * TILE_SIZE, Fade(BLACK, evening_alpha / 255.0))
  DrawText("EVENING", right_offset_x + (MAP_W * TILE_SIZE) / 2 - 40, 30, 20, ORANGE)

  # Подсказка
  DrawText("Press ESC to close", 10, WIN_H - 30, 20, DARKGRAY)

  EndDrawing()
end

UnloadTexture(tileset)
CloseWindow()