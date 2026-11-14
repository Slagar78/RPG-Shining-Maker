using System;
using System.IO;
using NAudio.Wave;
using NAudio.Vorbis; // ★ ДОБАВЛЕНО ДЛЯ OGG ★

namespace RpgShinzoMaker.Desktop.Shared.Audio;

public class MusicPlayer : IDisposable
{
    private WaveOutEvent? _output;
    private WaveStream? _reader; // ★ ИЗМЕНЕНО С AudioFileReader НА WaveStream ★
    private string? _currentFile;
    private float _volume = 0.8f;

    public event Action<string>? TrackStarted;
    public event Action<bool>? PlayStateChanged;

    public bool IsPlaying => _output?.PlaybackState == PlaybackState.Playing;
    public string? CurrentFile => _currentFile;

    public float Volume
    {
        get => _volume;
        set
        {
            _volume = Math.Clamp(value, 0f, 1f);
            if (_output != null) _output.Volume = _volume;
        }
    }

    public void Play(string filePath)
    {
        if (string.IsNullOrWhiteSpace(filePath) || !File.Exists(filePath)) return;

        try
        {
            if (_currentFile != filePath)
            {
                StopInternal();
                
                // ★ ВОТ ЗДЕСЬ ВЫБИРАЕМ ДЕКОДЕР (MP3 или OGG) ★
                if (filePath.EndsWith(".ogg", StringComparison.OrdinalIgnoreCase))
                    _reader = new VorbisWaveReader(filePath);
                else
                    _reader = new AudioFileReader(filePath);

                _output = new WaveOutEvent();
                _output.PlaybackStopped += OnPlaybackStopped;
                _output.Init(_reader);
                _currentFile = filePath;
                TrackStarted?.Invoke(filePath);
            }

            if (_output == null) return;
            _output.Volume = _volume;
            _output.Play();
            PlayStateChanged?.Invoke(true);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[MusicPlayer] Play error: {ex.Message}");
        }
    }

    public void Pause()
    {
        _output?.Pause();
        PlayStateChanged?.Invoke(false);
    }

    public void Stop()
    {
        StopInternal();
        PlayStateChanged?.Invoke(false);
    }

    private void StopInternal()
    {
        if (_output != null)
        {
            _output.PlaybackStopped -= OnPlaybackStopped;
            try { _output.Stop(); } catch { }
            _output.Dispose();
            _output = null;
        }
        _reader?.Dispose();
        _reader = null;
        _currentFile = null;
    }

    private void OnPlaybackStopped(object? s, StoppedEventArgs e) =>
        PlayStateChanged?.Invoke(false);

    public void Dispose() => StopInternal();
}