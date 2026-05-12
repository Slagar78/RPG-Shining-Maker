#!/usr/bin/env ruby
require 'raylib'

include Raylib
R = Raylib

# Загрузка DLL (Windows)
if Gem.win_platform?
  shared_lib_path = Gem::Specification.find_by_name('raylib-bindings').full_gem_path + '/lib/'
  Raylib.load_lib(shared_lib_path + 'libraylib.dll')
end

WIDTH = 500
HEIGHT = 400
SIZE = 48

R.InitWindow(WIDTH, HEIGHT, "Test Right‑Click Rotate/Flip")
R.SetTargetFPS(60)

# Создаём разноцветную текстуру 48x48 (4 квадрата 24x24)
image = R.GenImageColor(SIZE, SIZE, R::BLANK)
(0...SIZE).each do |y|
  (0...SIZE).each do |x|
    color = if x < SIZE / 2 && y < SIZE / 2
              R::RED
            elsif x >= SIZE / 2 && y < SIZE / 2
              R::GREEN
            elsif x < SIZE / 2 && y >= SIZE / 2
              R::BLUE
            else
              R::YELLOW
            end
    R.ImageDrawPixel(image, x, y, color)
  end
end
texture = R.LoadTextureFromImage(image)
R.UnloadImage(image)

# Состояние квадрата
rot = 0.0
flip_x = false
flip_y = false

# Режим правой кнопки
mode = :rotate   # :rotate, :flip_h, :flip_v

# Кнопки выбора режима
btn_rotate = R::Rectangle.create(10, 10, 100, 30)
btn_flip_h = R::Rectangle.create(120, 10, 100, 30)
btn_flip_v = R::Rectangle.create(230, 10, 100, 30)

# Прямоугольник квадрата (для проверки попадания правого клика)
square_rect = R::Rectangle.create(
  WIDTH / 2.0 - SIZE / 2.0,
  HEIGHT / 2.0 + 30 - SIZE / 2.0,
  SIZE, SIZE
)

until R.WindowShouldClose
  mouse_pos = R.GetMousePosition

  # Выбор режима левой кнопкой
  if R.IsMouseButtonPressed(R::MOUSE_BUTTON_LEFT)
    if R.CheckCollisionPointRec(mouse_pos, btn_rotate)
      mode = :rotate
    elsif R.CheckCollisionPointRec(mouse_pos, btn_flip_h)
      mode = :flip_h
    elsif R.CheckCollisionPointRec(mouse_pos, btn_flip_v)
      mode = :flip_v
    end
  end

  # Действие правой кнопкой по квадрату
  if R.IsMouseButtonPressed(R::MOUSE_BUTTON_RIGHT)
    if R.CheckCollisionPointRec(mouse_pos, square_rect)
      case mode
      when :rotate
        rot = (rot + 90.0) % 360.0
      when :flip_h
        flip_x = !flip_x
      when :flip_v
        flip_y = !flip_y
      end
    end
  end

  R.BeginDrawing
  R.ClearBackground(R::RAYWHITE)

  # Кнопки режимов
  R.DrawRectangleRec(btn_rotate, R::LIGHTGRAY)
  R.DrawText("Rotate", btn_rotate.x + 5, btn_rotate.y + 5, 20, R::BLACK)
  R.DrawRectangleRec(btn_flip_h, R::LIGHTGRAY)
  R.DrawText("Flip H", btn_flip_h.x + 5, btn_flip_h.y + 5, 20, R::BLACK)
  R.DrawRectangleRec(btn_flip_v, R::LIGHTGRAY)
  R.DrawText("Flip V", btn_flip_v.x + 5, btn_flip_v.y + 5, 20, R::BLACK)

  # Подсветка активного режима
  highlight_x = case mode
                when :rotate then btn_rotate.x
                when :flip_h then btn_flip_h.x
                when :flip_v then btn_flip_v.x
                end
  R.DrawRectangleLines(highlight_x, 10, 100, 30, R::RED)

  # Рисуем квадрат
  src = R::Rectangle.create(
    flip_x ? SIZE.to_f : 0,
    flip_y ? SIZE.to_f : 0,
    flip_x ? -SIZE.to_f : SIZE.to_f,
    flip_y ? -SIZE.to_f : SIZE.to_f
  )

  center_x = WIDTH / 2.0
  center_y = HEIGHT / 2.0 + 30
  dst = R::Rectangle.create(center_x, center_y, SIZE, SIZE)
  origin = R::Vector2.create(SIZE / 2.0, SIZE / 2.0)

  R.DrawTexturePro(texture, src, dst, origin, rot, R::WHITE)

  # Информация
  R.DrawText("Right‑click on square to apply", 10, HEIGHT - 60, 20, R::DARKGRAY)
  R.DrawText("Mode: #{mode}", 10, HEIGHT - 35, 20, R::RED)
  R.DrawText("Angle: #{rot.to_i}°", 120, 50, 20, R::DARKGRAY)
  R.DrawText("FlipH: #{flip_x}", 120, 75, 20, R::DARKGRAY)
  R.DrawText("FlipV: #{flip_y}", 120, 100, 20, R::DARKGRAY)

  R.EndDrawing
end

R.UnloadTexture(texture)
R.CloseWindow