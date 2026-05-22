# lib/AudioManager.rb
class AudioManager
  def initialize
    @current_bgm = nil
    @current_file = nil
    @sfx = {} # Хранилище для звуков
  end

  # ── Фоновая музыка ──
  def play(file, volume = 0.8)
    return if file.nil? || file.empty?
    return if @current_file == file

    stop
    @current_bgm = Raylib.LoadMusicStream(file)
    @current_bgm.looping = true
    Raylib.SetMusicVolume(@current_bgm, volume.clamp(0.0, 1.0))
    Raylib.PlayMusicStream(@current_bgm)
    @current_file = file
  end

  def stop
    if @current_bgm
      Raylib.UnloadMusicStream(@current_bgm)
      @current_bgm = nil
      @current_file = nil
    end
  end

  def update
    if @current_bgm
      Raylib.UpdateMusicStream(@current_bgm)
    end
  end

  # ── Звуковые эффекты ──
  # Загружает звук и кладет в хранилище под именем (ключом)
  def load_sfx(name, path)
    return unless File.exist?(path)
    @sfx[name] = Raylib.LoadSound(path)
  end

  # Проигрывает звук
  def play_sfx(name)
    sound = @sfx[name]
    Raylib.PlaySound(sound) if sound
  end

  # Установить громкость загруженного звука (0.0..1.0)
  def set_sfx_volume(name, volume)
    sound = @sfx[name]
    return unless sound
    Raylib.SetSoundVolume(sound, volume.clamp(0.0, 1.0))
  end

  # ── Очистка ──
  def unload_all_sfx
    @sfx.each_value { |s| Raylib.UnloadSound(s) }
    @sfx.clear
  end

  def cleanup
    stop
    unload_all_sfx
  end
end