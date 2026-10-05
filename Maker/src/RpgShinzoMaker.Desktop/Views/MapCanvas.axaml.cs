using System;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Shapes;
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
    public event Action<int, int, int, bool>? TileDragged;

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

    private Border? _hoverBorder;
    private readonly List<Ellipse> _gridDots = new();
    private readonly List<Line> _gridLines = new();

    private double _zoom = 1.0;
    public double Zoom => _zoom;

    // Размер тайла на экране = 48 * zoom
    private int TilePx => (int)(GameMap.TileSize * _zoom);

    public bool ShowGridMode { get; set; } = false;
    public int[]? TileTypes { get; set; }

    public MapCanvas()
    {
        InitializeComponent();
        PointerMoved += OnCanvasPointerMoved;
        PointerExited += OnCanvasPointerExited;
    }

    // ══════════════════════════════════════════════
    //   SET MAP — создаёт контролы один раз
    // ══════════════════════════════════════════════
    public void SetMap(GameMap? map, List<CroppedBitmap>? tiles)
    {
        _map = map;
        if (tiles != null) _tiles = tiles;

        if (_map == null) return;

        int ts = TilePx;
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
            var img1 = CreateTileImage(x, y, ts);
            var img2 = CreateTileImage(x, y, ts);

            Layer1Canvas.Children.Add(img1);
            Layer2Canvas.Children.Add(img2);

            var tc = new TileCtrl { L1 = img1, L2 = img2, X = x, Y = y };
            _controls.Add(tc);
            UpdateTileCtrl(tc);
        }

        // ─── Оверлей Grid Mode ───
        GridOverlayCanvas.Children.Clear();
        GridOverlayCanvas.Width  = mapW;
        GridOverlayCanvas.Height = mapH;
        _gridDots.Clear();
        _gridLines.Clear();

        var gridBrush = new SolidColorBrush(Color.Parse("#66FFFFFF"));

        // Вертикальные линии
        for (int x = 0; x <= _map.Width; x++)
        {
            var line = new Line
            {
                StartPoint = new Point(x * ts, 0),
                EndPoint   = new Point(x * ts, mapH),
                Stroke = gridBrush,
                StrokeThickness = 1,
                IsVisible = false,
                IsHitTestVisible = false,
            };
            GridOverlayCanvas.Children.Add(line);
            _gridLines.Add(line);
        }

        // Горизонтальные линии
        for (int y = 0; y <= _map.Height; y++)
        {
            var line = new Line
            {
                StartPoint = new Point(0, y * ts),
                EndPoint   = new Point(mapW, y * ts),
                Stroke = gridBrush,
                StrokeThickness = 1,
                IsVisible = false,
                IsHitTestVisible = false,
            };
            GridOverlayCanvas.Children.Add(line);
            _gridLines.Add(line);
        }

        // Точки типов — поверх линий
        for (int x = 0; x < _map.Width; x++)
        for (int y = 0; y < _map.Height; y++)
        {
            var dot = new Ellipse
            {
                Width  = 14,
                Height = 14,
                Opacity = 0.7,
                Stroke = new SolidColorBrush(Color.Parse("#000000")),
                StrokeThickness = 2,
                IsVisible = false,
                IsHitTestVisible = false,
            };
            Canvas.SetLeft(dot, x * ts + (ts - 14) / 2);
            Canvas.SetTop (dot, y * ts + (ts - 14) / 2);
            GridOverlayCanvas.Children.Add(dot);
            _gridDots.Add(dot);
        }

        // Рамка-курсор — поверх всего
        _hoverBorder = new Border
        {
            BorderBrush = new SolidColorBrush(Color.Parse("#FFD700")),
            BorderThickness = new Thickness(2),
            Width  = ts,
            Height = ts,
            IsVisible = false,
            IsHitTestVisible = false,
        };
        GridOverlayCanvas.Children.Add(_hoverBorder);

        UpdateGridOverlay();
    }

    private Image CreateTileImage(int x, int y, int ts)
    {
        var img = new Image
        {
            Width = ts,
            Height = ts,
            Stretch = Stretch.Fill,
            RenderTransformOrigin = RelativePoint.Center,
        };

        RenderOptions.SetBitmapInterpolationMode(img, BitmapInterpolationMode.None);
        RenderOptions.SetEdgeMode(img, EdgeMode.Aliased);

        Canvas.SetLeft(img, x * ts);
        Canvas.SetTop(img, y * ts);
        return img;
    }

    // ══════════════════════════════════════════════
    //   ZOOM — пересоздаёт карту с новым размером тайлов
    // ══════════════════════════════════════════════
    public void SetZoom(double zoom)
    {
        if (zoom <= 0) return;
        if (Math.Abs(zoom - _zoom) < 0.001) return;
        _zoom = zoom;

        if (_map != null)
        {
            ResizeToZoom();
            ResetScrollOffset();
        }
    }

    // Пересчёт размеров БЕЗ пересоздания контролов.
    // В разы быстрее, чем SetMap — только меняем Width/Height/Canvas.Left/Top.
    private void ResizeToZoom()
    {
        if (_map == null) return;

        int ts = TilePx;
        int mapW = _map.Width  * ts;
        int mapH = _map.Height * ts;

        // RootPanel и фон
        RootPanel.Width  = mapW;
        RootPanel.Height = mapH;

        BgImage.Source = GenerateCheckerboard(mapW, mapH);
        BgImage.Width  = mapW;
        BgImage.Height = mapH;

        // Тайлы — меняем размер и позицию существующих Image
        int i = 0;
        for (int x = 0; x < _map.Width; x++)
        for (int y = 0; y < _map.Height; y++)
        {
            var tc = _controls[i++];
            if (tc.L1 != null)
            {
                tc.L1.Width  = ts;
                tc.L1.Height = ts;
                Canvas.SetLeft(tc.L1, x * ts);
                Canvas.SetTop (tc.L1, y * ts);
            }
            if (tc.L2 != null)
            {
                tc.L2.Width  = ts;
                tc.L2.Height = ts;
                Canvas.SetLeft(tc.L2, x * ts);
                Canvas.SetTop (tc.L2, y * ts);
            }
        }

        // Оверлей Grid Mode
        GridOverlayCanvas.Width  = mapW;
        GridOverlayCanvas.Height = mapH;

        // Линии сетки — идём по порядку: сначала вертикальные (_map.Width + 1),
        // потом горизонтальные (_map.Height + 1)
        int li = 0;
        for (int x = 0; x <= _map.Width; x++)
        {
            var line = _gridLines[li++];
            line.StartPoint = new Point(x * ts, 0);
            line.EndPoint   = new Point(x * ts, mapH);
        }
        for (int y = 0; y <= _map.Height; y++)
        {
            var line = _gridLines[li++];
            line.StartPoint = new Point(0, y * ts);
            line.EndPoint   = new Point(mapW, y * ts);
        }

        // Точки типов
        int di = 0;
        for (int x = 0; x < _map.Width; x++)
        for (int y = 0; y < _map.Height; y++)
        {
            var dot = _gridDots[di++];
            Canvas.SetLeft(dot, x * ts + (ts - 14) / 2);
            Canvas.SetTop (dot, y * ts + (ts - 14) / 2);
        }

        // Рамка-курсор
        if (_hoverBorder != null)
        {
            _hoverBorder.Width  = ts;
            _hoverBorder.Height = ts;
        }

        UpdateGridOverlay();
    }

    private void ResetScrollOffset()
    {
        var parent = this.Parent;
        while (parent != null)
        {
            if (parent is ScrollViewer sv)
            {
                sv.Offset = new Vector(0, 0);
                return;
            }
            parent = parent.Parent;
        }
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

    // ══════════════════════════════════════════════
    //   GRID MODE — обновить иконки типов
    // ══════════════════════════════════════════════
    public void UpdateGridOverlay()
    {
        if (_map == null || _gridDots.Count == 0) return;

        foreach (var line in _gridLines)
            line.IsVisible = ShowGridMode;

        for (int x = 0; x < _map.Width; x++)
        for (int y = 0; y < _map.Height; y++)
        {
            int idx = x * _map.Height + y;
            if (idx < 0 || idx >= _gridDots.Count) continue;

            var dot = _gridDots[idx];
            int tileId = (CurrentLayer == 0) ? _map.Tiles[idx] : _map.Tiles2[idx];

            if (!ShowGridMode || TileTypes == null || tileId < 0 || tileId >= TileTypes.Length)
            {
                dot.IsVisible = false;
                continue;
            }

            int type = TileTypes[tileId];
            dot.Fill = type switch
            {
                0 => new SolidColorBrush(Color.Parse("#4CAF50")),
                1 => new SolidColorBrush(Color.Parse("#E74C3C")),
                2 => new SolidColorBrush(Color.Parse("#3498DB")),
                3 => new SolidColorBrush(Color.Parse("#E67E22")),
                _ => Brushes.Transparent
            };
            dot.IsVisible = type >= 0 && type <= 3;
        }
    }

    private void UpdateTileCtrl(TileCtrl tc)
    {
        if (_map == null || tc.L1 == null || tc.L2 == null) return;

        int idx = tc.X * _map.Height + tc.Y;

        ApplyToImage(tc.L1,
            _map.Tiles[idx], _map.Rot[idx], _map.MirrorX[idx], _map.MirrorY[idx],
            opacity: 1.0,
            visible: ShowLayer1);

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

    private void OnCanvasPointerMoved(object? sender, PointerEventArgs e)
    {
        if (_map == null) return;

        int ts = TilePx;
        var p = e.GetPosition(RootPanel);
        int tx = (int)(p.X / ts);
        int ty = (int)(p.Y / ts);

        if (tx < 0 || tx >= _map.Width || ty < 0 || ty >= _map.Height)
        {
            if (_hoverBorder != null) _hoverBorder.IsVisible = false;
            return;
        }

        var props = e.GetCurrentPoint(RootPanel).Properties;
        bool isLeft  = props.IsLeftButtonPressed;
        bool isRight = props.IsRightButtonPressed;

        if (_hoverBorder != null)
        {
            _hoverBorder.IsVisible = true;
            Canvas.SetLeft(_hoverBorder, tx * ts);
            Canvas.SetTop (_hoverBorder, ty * ts);

            string color = isRight ? "#FF2222" : "#FFD700";
            _hoverBorder.BorderBrush = new SolidColorBrush(Color.Parse(color));
        }

        if (!isLeft && !isRight) return;

        int idx = tx * _map.Height + ty;
        int tileId = (CurrentLayer == 0) ? _map.Tiles[idx] : _map.Tiles2[idx];
        TileDragged?.Invoke(tx, ty, tileId, isLeft);
    }

    private void OnCanvasPointerExited(object? sender, PointerEventArgs e)
    {
        if (_hoverBorder != null) _hoverBorder.IsVisible = false;
    }

    // ══════════════════════════════════════════════
    //   КЛИК ПО КАРТЕ
    // ══════════════════════════════════════════════
    private void OnCanvasPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (_map == null) return;

        int ts = TilePx;
        var p = e.GetPosition(RootPanel);
        int tx = (int)(p.X / ts);
        int ty = (int)(p.Y / ts);

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
