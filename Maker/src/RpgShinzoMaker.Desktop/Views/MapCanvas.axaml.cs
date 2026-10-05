using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Views;

public partial class MapCanvas : UserControl
{
    private WriteableBitmap? _surface;
    private WriteableBitmap? _sourceTileset;
    private GameMap? _map;

    private int _tsCols, _tsRows, _tsStrips;

    public bool ShowLayer1 { get; set; } = true;
    public bool ShowLayer2 { get; set; } = true;
    public int CurrentLayer { get; set; } = 0;

    // События наружу: tileX, tileY, tileId, isLeftButton
    public event Action<int, int, int, bool>? TileClicked;

    public MapCanvas()
    {
        InitializeComponent();
        PointerPressed += OnCanvasPointerPressed;
    }

    private void OnCanvasPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (_map == null) return;

        var p = e.GetPosition(Surface);
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

    public void SetMap(GameMap? map, Bitmap? sourceTileset)
    {
        _map = map;

        if (sourceTileset != null)
        {
            _sourceTileset = ConvertToWriteableBitmap(sourceTileset);
            _tsCols = _sourceTileset.PixelSize.Width  / GameMap.TileSize;
            _tsRows = _sourceTileset.PixelSize.Height / GameMap.TileSize;
            _tsStrips = _tsCols / 8;
        }

        Redraw();
    }

    private static WriteableBitmap ConvertToWriteableBitmap(Bitmap src)
    {
        int w = src.PixelSize.Width;
        int h = src.PixelSize.Height;

        var wb = new WriteableBitmap(
            src.PixelSize, src.Dpi,
            PixelFormat.Bgra8888, AlphaFormat.Premul);

        using var fb = wb.Lock();
        src.CopyPixels(new PixelRect(0, 0, w, h),
            fb.Address, fb.RowBytes * fb.Size.Height, fb.RowBytes);
        return wb;
    }

    public void Redraw()
    {
        if (_map == null || _sourceTileset == null)
        {
            Surface.Source = null;
            return;
        }

        int mapW = _map.Width  * GameMap.TileSize;
        int mapH = _map.Height * GameMap.TileSize;

        if (_surface == null ||
            _surface.PixelSize.Width  != mapW ||
            _surface.PixelSize.Height != mapH)
        {
            _surface = new WriteableBitmap(
                new PixelSize(mapW, mapH),
                new Vector(96, 96),
                PixelFormat.Bgra8888, AlphaFormat.Premul);
        }

        using (var fb = _surface.Lock())
        {
            unsafe
            {
                byte* basePtr = (byte*)fb.Address;
                int stride = fb.RowBytes;

                FillCheckerboard(basePtr, stride, mapW, mapH);

                if (ShowLayer1)
                    DrawLayer(basePtr, stride, _map.Tiles, _map.Rot, _map.MirrorX, _map.MirrorY, 255);

                if (ShowLayer2)
                {
                    byte a2 = ShowLayer1 ? (byte)96 : (byte)255;
                    DrawLayer(basePtr, stride, _map.Tiles2, _map.Rot2, _map.MirrorX2, _map.MirrorY2, a2);
                }
            }
        }

        Surface.Source = _surface;
        Surface.Width  = mapW;
        Surface.Height = mapH;
    }

    private unsafe void FillCheckerboard(byte* basePtr, int stride, int w, int h)
    {
        const int cell = 24;
        for (int y = 0; y < h; y++)
        {
            uint* row = (uint*)(basePtr + y * stride);
            for (int x = 0; x < w; x++)
            {
                bool light = ((x / cell) + (y / cell)) % 2 == 0;
                row[x] = light ? 0xFF3A3A3A : 0xFF2A2A2A;
            }
        }
    }

    private unsafe void DrawLayer(byte* basePtr, int stride,
        int[] tiles, int[] rot, bool[] mx, bool[] my, byte layerAlpha)
    {
        if (_map == null || _sourceTileset == null) return;

        int h = _map.Height;
        int ts = GameMap.TileSize;
        int mapW = _map.Width * ts;
        int mapH = _map.Height * ts;

        using var srcFb = _sourceTileset.Lock();
        byte* srcPtr = (byte*)srcFb.Address;
        int srcStride = srcFb.RowBytes;
        int srcW = _sourceTileset.PixelSize.Width;
        int srcH = _sourceTileset.PixelSize.Height;

        bool isOpaque = (layerAlpha == 255);

        for (int x = 0; x < _map.Width; x++)
        for (int y = 0; y < h; y++)
        {
            int idx = x * h + y;
            int tileId = tiles[idx];
            if (tileId < 0) continue;
            if (!GetTileSourcePos(tileId, out int sc, out int sr)) continue;

            int dstX = x * ts;
            int dstY = y * ts;

            for (int oy = 0; oy < ts; oy++)
            {
                int sy = sr * ts + oy;
                if (sy < 0 || sy >= srcH) continue;
                int dy = dstY + oy;
                if (dy < 0 || dy >= mapH) continue;

                uint* dstRow = (uint*)(basePtr + dy * stride);
                uint* srcRow = (uint*)(srcPtr + sy * srcStride);

                for (int ox = 0; ox < ts; ox++)
                {
                    int sx = sc * ts + ox;
                    if (sx < 0 || sx >= srcW) continue;
                    int dx = dstX + ox;
                    if (dx < 0 || dx >= mapW) continue;

                    uint src = srcRow[sx];
                    byte srcA = (byte)((src >> 24) & 0xFF);
                    if (srcA == 0) continue;

                    if (isOpaque)
                    {
                        dstRow[dx] = src;
                    }
                    else
                    {
                        int a = (srcA * layerAlpha) / 255;
                        if (a == 0) continue;

                        uint dst = dstRow[dx];
                        byte sr2 = (byte)((src >> 16) & 0xFF);
                        byte sg  = (byte)((src >> 8)  & 0xFF);
                        byte sb  = (byte)( src        & 0xFF);
                        byte dr  = (byte)((dst >> 16) & 0xFF);
                        byte dg  = (byte)((dst >> 8)  & 0xFF);
                        byte db  = (byte)( dst        & 0xFF);

                        byte r = (byte)((sr2 * a + dr * (255 - a)) / 255);
                        byte g = (byte)((sg  * a + dg * (255 - a)) / 255);
                        byte b = (byte)((sb  * a + db * (255 - a)) / 255);

                        dstRow[dx] = 0xFF000000u | ((uint)r << 16) | ((uint)g << 8) | (uint)b;
                    }
                }
            }
        }
    }

    private bool GetTileSourcePos(int tileId, out int col, out int row)
    {
        col = 0; row = 0;
        if (_tsStrips <= 0 || _tsRows <= 0) return false;

        int perStrip = _tsRows * 8;
        if (tileId < 0 || tileId >= _tsStrips * perStrip) return false;

        int strip = tileId / perStrip;
        int rest  = tileId % perStrip;
        row = rest / 8;
        col = strip * 8 + (rest % 8);
        return true;
    }
}