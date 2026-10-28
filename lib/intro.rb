# lib/intro.rb

class Intro
  def initialize
    @texture = nil
    path = "assets/intro/Shinzo.png"
    
    if File.exist?(path)
      @texture = Raylib.LoadTexture(path)
      Raylib.SetTextureFilter(@texture, Raylib::TEXTURE_FILTER_POINT)
    else
      puts "WARNING: #{path} not found!"
    end

    @state = :wait
    @timer = 0.0
    @alpha = 0.0
    @done = false

    # Тайминги (в секундах)
    @wait_time = 0.5
    @fade_in_time = 1.0
    @hold_time = 2.0
    @fade_out_time = 1.0
  end

  def skip
    @done = true
  end

  def update(dt)
    return if @done

    @timer += dt

    case @state
    when :wait
      if @timer >= @wait_time
        @state = :fade_in
        @timer = 0.0
      end
    when :fade_in
      @alpha = @timer / @fade_in_time
      if @timer >= @fade_in_time
        @alpha = 1.0
        @state = :hold
        @timer = 0.0
      end
    when :hold
      if @timer >= @hold_time
        @state = :fade_out
        @timer = 0.0
      end
    when :fade_out
      @alpha = 1.0 - (@timer / @fade_out_time)
      if @timer >= @fade_out_time
        @alpha = 0.0
        @state = :done
        @done = true
      end
    end
  end

  def draw
    # Если картинка загружена и мы не в состоянии ожидания/завершения
    if @texture && @state != :wait && @state != :done
      # Raylib.Fade позволяет плавно менять прозрачность (0.0 до 1.0)
      color = Raylib.Fade(Raylib::WHITE, @alpha)
      Raylib.DrawTexture(@texture, 0, 0, color)
    end
  end

  def done?
    @done
  end

  def unload
    Raylib.UnloadTexture(@texture) if @texture
    @texture = nil
  end
end