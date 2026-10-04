using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Views;

public partial class MapCanvas : UserControl
{
    private WriteableBitmap? _surface;
    private WriteableBitmap? _sourceTileset;  // ← WriteableBitmap, не Bitmap
    private GameMap? _map;

    private int _tsCols, _tsRows, _tsStrips;

    public MapCanvas()
    {
        InitializeComponent();
    }

    public void SetMap(GameMap? map, Bitmap? sourceTileset)
    {
        _map = map;

        if (sourceTileset != null)
        {
            // Конвертируем Bitmap → WriteableBitmap один раз
            _sourceTileset = ConvertToWriteableBitmap(sourceTileset);
            _tsCols = _sourceTileset.PixelSize.Width  / GameMap.TileSize;
            _tsRows = _sourceTileset.PixelSize.Height / GameMap.TileSize;
            _tsStrips = _tsCols / 8;
        }

        Redraw();
    }

    /// <summary>
    /// Копирует Bitmap в WriteableBitmap для прямого доступа к пикселям.
    /// </summary>
    private static WriteableBitmap ConvertToWriteableBitmap(Bitmap src)
    {
        int w = src.PixelSize.Width;
        int h = src.PixelSize.Height;

        var wb = new WriteableBitmap(
            src.PixelSize,
            src.Dpi,
            PixelFormat.Bgra8888,
            AlphaFormat.Premul);

        using var fb = wb.Lock();
        src.CopyPixels(
            new PixelRect(0, 0, w, h),
            fb.Address,
            fb.RowBytes * fb.Size.Height,
            fb.RowBytes);

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
                PixelFormat.Bgra8888,
                AlphaFormat.Premul);
        }

        using (var fb = _surface.Lock())
        {
            unsafe
            {
                byte* basePtr = (byte*)fb.Address;
                int stride = fb.RowBytes;

                FillCheckerboard(basePtr, stride, mapW, mapH);

                DrawLayer(basePtr, stride,
                    _map.Tiles, _map.Rot, _map.MirrorX, _map.MirrorY);

                DrawLayer(basePtr, stride,
                    _map.Tiles2, _map.Rot2, _map.MirrorX2, _map.MirrorY2);
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
        int[] tiles, int[] rot, bool[] mx, bool[] my)
    {
        if (_map == null || _sourceTileset == null) return;

        int h = _map.Height;
        int ts = GameMap.TileSize;
        int mapW = _map.Width  * ts;
        int mapH = _map.Height * ts;

        using var srcFb = _sourceTileset.Lock();
        byte* srcPtr = (byte*)srcFb.Address;
        int srcStride = srcFb.RowBytes;
        int srcW = _sourceTileset.PixelSize.Width;
        int srcH = _sourceTileset.PixelSize.Height;

        for (int x = 0; x < _map.Width; x++)
        {
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

                        uint c = srcRow[sx];
                        if ((c & 0xFF000000) == 0) continue;  // прозрачный

                        dstRow[dx] = c;
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
        int cInStrip = rest % 8;
        col = strip * 8 + cInStrip;
        return true;
    }
}