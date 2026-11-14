using System;
using System.Collections.Generic;
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Editors.BattleSceneEditor;

public partial class BattleSceneEditorView
{
    private const int ALLY_X = 750;
    private const int ALLY_Y = 480;
    private const float GROUND_SCALE = 2.0f;
    private const int GROUND_OFFSET_X = 136;
    private const int GROUND_OFFSET_Y = 422;

    private const int ENEMY_GAME_X = 124;
    private const int ENEMY_GAME_Y = 370;

    private readonly Dictionary<string, Bitmap?> _imageCache = new();
    private string? _cachedGameRoot;

    private string GetGameRoot()
    {
        if (_cachedGameRoot != null) return _cachedGameRoot;
        var dir = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
        for (int i = 0; i < 10 && dir != null; i++)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "assets")) && Directory.Exists(Path.Combine(dir.FullName, "data")))
            {
                _cachedGameRoot = dir.FullName;
                return _cachedGameRoot;
            }
            dir = dir.Parent;
        }
        _cachedGameRoot = AppDomain.CurrentDomain.BaseDirectory;
        return _cachedGameRoot;
    }

    private string GetAbsolutePath(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return "";
        if (Path.IsPathRooted(path)) return path;
        return Path.Combine(GetGameRoot(), path.Replace('/', Path.DirectorySeparatorChar));
    }

    private Bitmap? GetCachedImage(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return null;
        string fullPath = GetAbsolutePath(path);
        if (!File.Exists(fullPath)) return null;
        if (_imageCache.TryGetValue(fullPath, out var cached)) return cached;

        try
        {
            var bmp = new Bitmap(fullPath);
            _imageCache[fullPath] = bmp;
            return bmp;
        }
        catch
        {
            _imageCache[fullPath] = null;
            return null;
        }
    }

    private void OnCanvasSizeChanged(object? sender, SizeChangedEventArgs e) => UpdatePreview();

    private void UpdatePreview()
    {
        if (PreviewCanvas == null) return;

        if (CurrentBattle == null)
        {
            BgImage.Source = null; GroundImage.Source = null; AllyImage.Source = null; EnemyImage.Source = null;
            TopBar.Width = BottomBar.Width = 0;
            return;
        }

        double previewW = PreviewCanvas.Bounds.Width;
        double previewH = PreviewCanvas.Bounds.Height;
        if (previewW <= 0 || previewH <= 0) return;

        // Масштаб, чтобы вся сцена (1152x960) влезла целиком
        float bgScale = Math.Min((float)previewW / 1152.0f, (float)previewH / 960.0f);
        
        float workW = 1152.0f * bgScale;
        float workH = 960.0f * bgScale;

        float offsetX = (float)((previewW - workW) / 2f);
        float offsetY = (float)((previewH - workH) / 2f);

        float barHeightScaled = 144.0f * bgScale;

        TopBar.Width = workW; TopBar.Height = barHeightScaled;
        Canvas.SetLeft(TopBar, offsetX); Canvas.SetTop(TopBar, offsetY);

        BottomBar.Width = workW; BottomBar.Height = barHeightScaled;
        Canvas.SetLeft(BottomBar, offsetX); Canvas.SetTop(BottomBar, offsetY + workH - barHeightScaled);

        // Фон (672 высота)
        var bgBmp = GetCachedImage(CurrentBattle.Background);
        if (bgBmp != null)
        {
            BgImage.Source = bgBmp;
            BgImage.Width = workW;
            BgImage.Height = 672.0f * bgScale;
            Canvas.SetLeft(BgImage, offsetX);
            Canvas.SetTop(BgImage, offsetY + barHeightScaled);
        }
        else BgImage.Source = null;

        // Земля
        var groundBmp = GetCachedImage(CurrentBattle.Ground);
        if (groundBmp != null)
        {
            GroundImage.Source = groundBmp;
            float scaledW = groundBmp.PixelSize.Width * GROUND_SCALE;
            float scaledH = groundBmp.PixelSize.Height * GROUND_SCALE;
            float gx = ALLY_X - scaledW / 2f + GROUND_OFFSET_X;
            float gy = ALLY_Y - scaledH + GROUND_OFFSET_Y;

            GroundImage.Width = scaledW * bgScale;
            GroundImage.Height = scaledH * bgScale;
            Canvas.SetLeft(GroundImage, offsetX + (gx * bgScale));
            Canvas.SetTop(GroundImage, offsetY + (gy * bgScale));
        }
        else GroundImage.Source = null;

        RenderUnit(AllyImage, AllyAnim, ALLY_X, ALLY_Y, bgScale, offsetX, offsetY);
        RenderUnit(EnemyImage, EnemyAnim, ENEMY_GAME_X, ENEMY_GAME_Y, bgScale, offsetX, offsetY);
    }

    private void RenderUnit(Image imgControl, AnimationSet animSet, int gameX, int gameY, float bgScale, float offsetX, float offsetY)
    {
        if (!animSet.Loaded) { imgControl.Source = null; return; }
        var phase = animSet.GetPhase(animSet.CurrentPhase);
        if (phase.Frames.Count == 0) { imgControl.Source = null; return; }

        int idx = animSet.CurrentFrame[animSet.CurrentPhase] % phase.Frames.Count;
        var frame = phase.Frames[idx];

        string dir = Path.GetDirectoryName(animSet.JsonPath) ?? "";
        string fullPath = Path.Combine(dir, frame.File);

        var bmp = GetCachedImage(fullPath);
        if (bmp == null) { imgControl.Source = null; return; }

        imgControl.Source = bmp;
        double drawW = bmp.PixelSize.Width * bgScale;
        double drawH = bmp.PixelSize.Height * bgScale;

        double finalX = offsetX + ((gameX + frame.OffsetX) * bgScale);
        double finalY = offsetY + ((gameY + frame.OffsetY) * bgScale);

        imgControl.Width = drawW;
        imgControl.Height = drawH;
        Canvas.SetLeft(imgControl, finalX);
        Canvas.SetTop(imgControl, finalY);
    }
}