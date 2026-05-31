# lib/AudioManager.rb
class AudioManager
  attr_reader :current_file

  def initialize
    @current_bgm = nil
    @current_file = nil
    @sfx = {}
    @saved_music_volume = 0.8
  end

  # ── Фоновая музыка ──
  def play(file, volume = 0.8)
    return if file.nil? || file.empty?
    return if @current_file == file
    stop
    @current_bgm = Raylib.LoadMusicStream(file)
    @current_bgm.looping = true
    @saved_music_volume = volume.clamp(0.0, 1.0)
    Raylib.SetMusicVolume(@current_bgm, @saved_music_volume)
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

  # ── Пауза и продолжение фоновой музыки (работает с OGG) ──
  def pause_music
    return unless @current_bgm
    Raylib.PauseMusicStream(@current_bgm)
  end

  def resume_music
    return unless @current_bgm
    Raylib.ResumeMusicStream(@current_bgm)
  end

  # ── Заглушение и восстановление музыки (резерв) ──
  def mute_music
    return unless @current_bgm
    begin
      vol = Raylib.GetMusicVolume(@current_bgm)
      @saved_music_volume = vol.clamp(0.0, 1.0)
    rescue
      @saved_music_volume = 0.8
    end
    Raylib.SetMusicVolume(@current_bgm, 0.0)
  end

  def unmute_music
    return unless @current_bgm
    Raylib.SetMusicVolume(@current_bgm, @saved_music_volume.clamp(0.0, 1.0))
  end

  # ── Звуковые эффекты ──
  def load_sfx(name, path)
    return unless File.exist?(path)
    @sfx[name] = Raylib.LoadSound(path)
  end

  def play_sfx(name)
    sound = @sfx[name]
    Raylib.PlaySound(sound) if sound
  end

  def stop_sfx(name)
    sound = @sfx[name]
    Raylib.StopSound(sound) if sound
  end

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