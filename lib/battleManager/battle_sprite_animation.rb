# lib/battleManager/battle_sprite_animation.rb
class BattleSpriteAnimation
  attr_reader :idle, :attack, :dodge

  def initialize(base_path)
    @base_path = base_path
    @idle    = load_anim("idle")
    @attack  = load_anim("attack") || @idle
    @dodge = load_anim("dodge") || @idle
  end

  # Возвращает суммарную длительность всех кадров анимации (в секундах)
  def total_duration(anim_key)
    anim = send(anim_key)
    return 0 unless anim && anim[:frames]
    anim[:frames].sum { |f| f[:duration] }
  end

  private

  def load_anim(name)
  return nil unless @base_path && !@base_path.empty?
  json_path = File.join(@base_path, "animation.json")
  return nil unless File.exist?(json_path)

  data = JSON.parse(File.read(json_path))[name]
  return nil unless data

  frames = data["frames"].map do |f|
    tex_path = File.join(@base_path, f["file"])
    next unless File.exist?(tex_path)
    tex = Raylib.LoadTexture(tex_path)
    Raylib.SetTextureFilter(tex, Raylib::TEXTURE_FILTER_POINT)
    {
      tex: tex,
      duration: f["duration"],
      offset_x: f["offset_x"] || 0,
      offset_y: f["offset_y"] || 0
    }
  end.compact

  return nil if frames.empty?

  {
    frames: frames,
    offset_x: data["offset_x"] || 0,
    offset_y: data["offset_y"] || 0
  }
  end
end