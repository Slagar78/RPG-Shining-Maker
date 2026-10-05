using System;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Views;

public partial class MapCanvas : UserControl
{
    public bool ShowLayer1 { get; set; } = true;
    public bool ShowLayer2 { get; set; } = true;
    public int CurrentLayer { get; set; } = 0;

    public event Action<int, int, int, bool>? TileClicked;

    private GameMap? _map;
    private List<CroppedBitmap> _tiles = new();

    private class TileCtrl
    {
        public Image? L1;
        public Image? L2;
        public int X;
        public int Y;
    }

    private readonly List<TileCtrl> _controls = new();

    public MapCanvas()
    {
        InitializeComponent();
    }

    // ══════════════════════════════════════════════
    //   SET MAP — создаёт контролы один раз
    // ══════════════════════════════════════════════
    public void SetMap(GameMap? map, List<CroppedBitmap>? tiles)
    {
        _map = map;
        if (tiles != null) _tiles = tiles;

        if (_map == null) return;

        int ts = GameMap.TileSize;
        int mapW = _map.Width * ts;
        int mapH = _map.Height * ts;

        RootPanel.Width  = mapW;
        RootPanel.Height = mapH;

        // Фон
        BgImage.Source = GenerateCheckerboard(mapW, mapH);
        BgImage.Width  = mapW;
        BgImage.Height = mapH;

        Layer1Canvas.Children.Clear();
        Layer2Canvas.Children.Clear();
        _controls.Clear();

        for (int x = 0; x < _map.Width; x++)
        for (int y = 0; y < _map.Height; y++)
        {
            var img1 = CreateTileImage(x, y);
            var img2 = CreateTileImage(x, y);

            Layer1Canvas.Children.Add(img1);
            Layer2Canvas.Children.Add(img2);

            var tc = new TileCtrl { L1 = img1, L2 = img2, X = x, Y = y };
            _controls.Add(tc);
            UpdateTileCtrl(tc);
        }
    }

    private Image CreateTileImage(int x, int y)
    {
        var img = new Image
        {
            Width = GameMap.TileSize,
            Height = GameMap.TileSize,
            Stretch = Stretch.Fill,
            RenderTransformOrigin = RelativePoint.Center,
        };

        RenderOptions.SetBitmapInterpolationMode(img, BitmapInterpolationMode.None);
        RenderOptions.SetEdgeMode(img, EdgeMode.Aliased);

        Canvas.SetLeft(img, x * GameMap.TileSize);
        Canvas.SetTop(img, y * GameMap.TileSize);
        return img;
    }

    // ══════════════════════════════════════════════
    //   REDRAW TILE — обновляет ОДИН тайл (мгновенно)
    // ══════════════════════════════════════════════
    public void RedrawTile(int tx, int ty)
    {
        if (_map == null) return;
        if (tx < 0 || tx >= _map.Width) return;
        if (ty < 0 || ty >= _map.Height) return;

        int idx = tx * _map.Height + ty;
        if (idx < 0 || idx >= _controls.Count) return;

        UpdateTileCtrl(_controls[idx]);
    }

    // ══════════════════════════════════════════════
    //   REDRAW — обновляет всю карту
    // ══════════════════════════════════════════════
    public void Redraw()
    {
        if (_map == null) return;

        foreach (var tc in _controls)
            UpdateTileCtrl(tc);
    }

    private void UpdateTileCtrl(TileCtrl tc)
    {
        if (_map == null || tc.L1 == null || tc.L2 == null) return;

        int idx = tc.X * _map.Height + tc.Y;

        // Слой 1
        ApplyToImage(tc.L1,
            _map.Tiles[idx], _map.Rot[idx], _map.MirrorX[idx], _map.MirrorY[idx],
            opacity: 1.0,
            visible: ShowLayer1);

        // Слой 2 — полупрозрачный если виден слой 1
        double layer2Opacity = ShowLayer1 ? 0.376 : 1.0;
        ApplyToImage(tc.L2,
            _map.Tiles2[idx], _map.Rot2[idx], _map.MirrorX2[idx], _map.MirrorY2[idx],
            opacity: layer2Opacity,
            visible: ShowLayer2);
    }

    private void ApplyToImage(Image img,
        int tileId, int rot, bool mx, bool my,
        double opacity, bool visible)
    {
        if (!visible || tileId < 0 || tileId >= _tiles.Count)
        {
            img.IsVisible = false;
            return;
        }

        img.IsVisible = true;
        img.Source = _tiles[tileId];
        img.Opacity = opacity;

        if (mx || my || rot != 0)
        {
            var tg = new TransformGroup();
            if (mx || my)
                tg.Children.Add(new ScaleTransform(mx ? -1 : 1, my ? -1 : 1));
            if (rot != 0)
                tg.Children.Add(new RotateTransform(rot * 90));
            img.RenderTransform = tg;
        }
        else
        {
            img.RenderTransform = null;
        }
    }

    // ══════════════════════════════════════════════
    //   ШАХМАТНЫЙ ФОН
    // ══════════════════════════════════════════════
    private static WriteableBitmap GenerateCheckerboard(int w, int h)
    {
        var wb = new WriteableBitmap(
            new PixelSize(w, h),
            new Vector(96, 96),
            PixelFormat.Bgra8888,
            AlphaFormat.Premul);

        using (var fb = wb.Lock())
        {
            unsafe
            {
                byte* basePtr = (byte*)fb.Address;
                int stride = fb.RowBytes;

                for (int y = 0; y < h; y++)
                {
                    uint* row = (uint*)(basePtr + y * stride);
                    for (int x = 0; x < w; x++)
                    {
                        bool light = ((x / 24) + (y / 24)) % 2 == 0;
                        row[x] = light ? 0xFF3A3A3A : 0xFF2A2A2A;
                    }
                }
            }
        }

        return wb;
    }

    // ══════════════════════════════════════════════
    //   КЛИК ПО КАРТЕ
    // ══════════════════════════════════════════════
    private void OnCanvasPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (_map == null) return;

        var p = e.GetPosition(RootPanel);
        int tx = (int)(p.X / GameMap.TileSize);
        int ty = (int)(p.Y / GameMap.TileSize);

        if (tx < 0 || tx >= _map.Width) return;
        if (ty < 0 || ty >= _map.Height) return;

        int idx = tx * _map.Height + ty;
        int tileId = (CurrentLayer == 0) ? _map.Tiles[idx] : _map.Tiles2[idx];

        var props = e.GetCurrentPoint(this).Properties;
        bool isLeft = props.IsLeftButtonPressed;
        bool isRight = props.IsRightButtonPressed;

        if (isLeft || isRight)
            TileClicked?.Invoke(tx, ty, tileId, isLeft);
    }
}