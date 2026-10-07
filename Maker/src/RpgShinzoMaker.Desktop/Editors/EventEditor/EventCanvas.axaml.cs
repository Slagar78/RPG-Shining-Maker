// RpgShinzoMaker.Desktop/Editors/EventEditor/EventCanvas.axaml.cs
using System;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Collections;        // ← для AvaloniaList<>
using Avalonia.Controls;
using Avalonia.Controls.Shapes;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;           // ← для PixelFormat / AlphaFormat
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Editors.EventEditor;

/// <summary>
/// Канвас редактора событий.
/// Рисует карту (L1 + L2) + подсветки событий поверх.
/// Умеет сообщать наружу клик по клетке — чтобы форма могла заполнить координаты.
/// </summary>
public partial class EventCanvas : UserControl
{
    // ─── Настройки ───
    public bool ShowLayer1 { get; set; } = true;
    public bool ShowLayer2 { get; set; } = true;

    /// <summary>Событие: клик по клетке карты. (tx, ty).</summary>
    public event Action<int, int>? TileClicked;

    // ─── Состояние ───
    private GameMap? _map;
    private List<CroppedBitmap> _tiles = new();
    private MapEvents? _events;
    private string _currentSection = "roof";
    private int _selectedIndex = -1;

    private double _zoom = 1.0;
    private int TilePx => (int)(GameMap.TileSize * _zoom);

    // ─── Кэш шахматного фона ───
    private static readonly ImageBrush CheckerBrush = CreateCheckerBrush();

    // ─── Контролы карты ───
    private class TileCtrl
    {
        public Image? L1;
        public Image? L2;
        public int X;
        public int Y;
    }
    private readonly List<TileCtrl> _controls = new();

    public EventCanvas()
    {
        InitializeComponent();
    }

    // ══════════════════════════════════════════════════════════════
    //   НАСТРОЙКА КАНВАСА
    // ══════════════════════════════════════════════════════════════
    public void SetMap(GameMap? map, List<CroppedBitmap>? tiles)
    {
        _map = map;
        if (tiles != null) _tiles = tiles;

        if (_map == null) return;

        int ts = TilePx;
        int mapW = _map.Width  * ts;
        int mapH = _map.Height * ts;

        RootPanel.Width  = mapW;
        RootPanel.Height = mapH;

        BgBorder.Width  = mapW;
        BgBorder.Height = mapH;
        BgBorder.Background = CheckerBrush;

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

            _controls.Add(new TileCtrl { L1 = img1, L2 = img2, X = x, Y = y });
        }

        Redraw();
    }

    public void SetEvents(MapEvents events, string section, int selectedIndex)
    {
        _events = events;
        _currentSection = section;
        _selectedIndex = selectedIndex;
        RedrawOverlay();
    }

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

    // ══════════════════════════════════════════════════════════════
    //   СОЗДАНИЕ IMAGE ДЛЯ КЛЕТКИ
    // ══════════════════════════════════════════════════════════════
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

    // ══════════════════════════════════════════════════════════════
    //   ПЕРЕРИСОВКА ПРИ СМЕНЕ ЗУМА
    // ══════════════════════════════════════════════════════════════
    private void ResizeToZoom()
    {
        if (_map == null) return;

        int ts = TilePx;
        int mapW = _map.Width  * ts;
        int mapH = _map.Height * ts;

        RootPanel.Width  = mapW;
        RootPanel.Height = mapH;
        BgBorder.Width   = mapW;
        BgBorder.Height  = mapH;

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

        Redraw();
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

    // ══════════════════════════════════════════════════════════════
    //   REDRAW — вся карта + оверлей
    // ══════════════════════════════════════════════════════════════
    public void Redraw()
    {
        if (_map == null) return;

        foreach (var tc in _controls)
            UpdateTileCtrl(tc);

        RedrawOverlay();
    }

    private void UpdateTileCtrl(TileCtrl tc)
    {
        if (_map == null || tc.L1 == null || tc.L2 == null) return;

        int idx = tc.X * _map.Height + tc.Y;

        ApplyToImage(tc.L1,
            _map.Tiles[idx], _map.Rot[idx], _map.MirrorX[idx], _map.MirrorY[idx],
            opacity: 1.0,
            visible: ShowLayer1);

        double layer2Opacity = ShowLayer1 ? 0.5 : 1.0;
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

    // ══════════════════════════════════════════════════════════════
    //   ОВЕРЛЕЙ — ПОДСВЕТКИ СОБЫТИЙ
    // ══════════════════════════════════════════════════════════════
    private void RedrawOverlay()
    {
        OverlayCanvas.Children.Clear();
        if (_map == null || _events == null) return;

        switch (_currentSection)
        {
            case "roof":        DrawRoofs();       break;
            case "tile_change": DrawTileChanges(); break;
            case "stair":       DrawStairs();      break;
            case "warp":        DrawWarps();       break;
            case "npc":         DrawNpcs();        break;
        }
    }

    // ─── Roof ───
    private void DrawRoofs()
    {
        if (_events == null) return;

        for (int i = 0; i < _events.Roofs.Count; i++)
        {
            var r = _events.Roofs[i];
            bool selected = i == _selectedIndex;
            byte alpha = (byte)(selected ? 255 : 140);

            // Прямоугольник крыши — зелёный
            int x1 = Math.Min(r.StartX, r.EndX);
            int y1 = Math.Min(r.StartY, r.EndY);
            int x2 = Math.Max(r.StartX, r.EndX);
            int y2 = Math.Max(r.StartY, r.EndY);

            AddRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1,
                Color.FromArgb(alpha, 0, 255, 0),
                selected ? Color.FromArgb(60, 0, 255, 0) : null,
                thickness: 4);

            // Триггеры — красные
            if (r.TriggerX >= 0 && r.TriggerY >= 0)
                AddRect(r.TriggerX, r.TriggerY, 1, 1,
                    Color.FromArgb(alpha, 255, 0, 0), null, thickness: 4);

            if (r.Trigger2X >= 0 && r.Trigger2Y >= 0)
                AddRect(r.Trigger2X, r.Trigger2Y, 1, 1,
                    Color.FromArgb(alpha, 255, 0, 0), null, thickness: 4);

            // Выходы — голубые
            if (r.ExitX >= 0 && r.ExitY >= 0)
                AddRect(r.ExitX, r.ExitY, 1, 1,
                    Color.FromArgb(alpha, 0, 150, 255), null, thickness: 4);

            if (r.Exit2X >= 0 && r.Exit2Y >= 0)
                AddRect(r.Exit2X, r.Exit2Y, 1, 1,
                    Color.FromArgb(alpha, 0, 150, 255), null, thickness: 4);
        }
    }

    // ─── Tile Change ───
    private void DrawTileChanges()
    {
        if (_events == null) return;

        for (int i = 0; i < _events.TileChanges.Count; i++)
        {
            var tc = _events.TileChanges[i];
            bool selected = i == _selectedIndex;
            byte alpha = (byte)(selected ? 255 : 140);

            if (tc.TriggerX >= 0 && tc.TriggerY >= 0)
                AddRect(tc.TriggerX, tc.TriggerY, 1, 1,
                    Color.FromArgb(alpha, 255, 255, 0), null, thickness: 4);

            if (tc.CloseX >= 0 && tc.CloseY >= 0)
                AddRect(tc.CloseX, tc.CloseY, 1, 1,
                    Color.FromArgb(alpha, 0, 200, 255), null, thickness: 4);
        }
    }

    // ─── Stairs ───
    private void DrawStairs()
    {
        if (_events == null) return;
        int ts = TilePx;

        for (int i = 0; i < _events.Stairs.Count; i++)
        {
            var st = _events.Stairs[i];
            bool selected = i == _selectedIndex;
            byte alpha = (byte)(selected ? 255 : 140);

            int dx = Math.Sign(st.EndX - st.StartX);
            int dy = Math.Sign(st.EndY - st.StartY);
            int steps = Math.Max(Math.Abs(st.EndX - st.StartX),
                                 Math.Abs(st.EndY - st.StartY));

            for (int k = 0; k <= steps; k++)
            {
                int cx = st.StartX + k * dx;
                int cy = st.StartY + k * dy;
                double centerX = cx * ts + ts / 2.0;
                double centerY = cy * ts + ts / 2.0;
                double half = ts / 2.0;

                var line = new Line
                {
                    Stroke = new SolidColorBrush(Color.FromArgb(alpha, 0, 120, 255)),
                    StrokeThickness = 5,
                    StartPoint = st.Direction == 1
                        ? new Point(centerX - half, centerY + half)
                        : new Point(centerX - half, centerY - half),
                    EndPoint = st.Direction == 1
                        ? new Point(centerX + half, centerY - half)
                        : new Point(centerX + half, centerY + half),
                    IsHitTestVisible = false,
                };
                OverlayCanvas.Children.Add(line);
            }
        }
    }

    // ─── Warps ───
    private void DrawWarps()
    {
        if (_events == null) return;
        int ts = TilePx;

        for (int i = 0; i < _events.Warps.Count; i++)
        {
            var w = _events.Warps[i];
            bool selected = i == _selectedIndex;
            byte alpha = (byte)(selected ? 255 : 140);

            if (w.TriggerX < 0 || w.TriggerY < 0) continue;

            AddRect(w.TriggerX, w.TriggerY, 1, 1,
                Color.FromArgb(alpha, 200, 0, 200), null, thickness: 4);

            // Стрелка направления
            double cx = w.TriggerX * ts + ts / 2.0;
            double cy = w.TriggerY * ts + ts / 2.0;
            double sz = ts * 0.3;

            Point[] pts = w.Facing switch
            {
                0 => new[] { new Point(cx, cy + sz), new Point(cx - sz, cy - sz/2), new Point(cx + sz, cy - sz/2) },
                1 => new[] { new Point(cx - sz, cy), new Point(cx + sz/2, cy - sz), new Point(cx + sz/2, cy + sz) },
                2 => new[] { new Point(cx + sz, cy), new Point(cx - sz/2, cy - sz), new Point(cx - sz/2, cy + sz) },
                _ => new[] { new Point(cx, cy - sz), new Point(cx - sz, cy + sz/2), new Point(cx + sz, cy + sz/2) }
            };

            var poly = new Polygon
            {
                Points = new List<Point>(pts),
                Stroke = new SolidColorBrush(Color.FromArgb(alpha, 255, 255, 0)),
                StrokeThickness = 3,
                Fill = new SolidColorBrush(Color.FromArgb(100, 255, 255, 0)),
                IsHitTestVisible = false,
            };
            OverlayCanvas.Children.Add(poly);
        }
    }

    // ─── NPC ───
    private void DrawNpcs()
    {
        if (_events == null) return;
        int ts = TilePx;

        for (int i = 0; i < _events.Npcs.Count; i++)
        {
            var n = _events.Npcs[i];
            bool selected = i == _selectedIndex;
            byte alpha = (byte)(selected ? 255 : 140);

            // Пока спрайта нет — рисуем рамку
            AddRect(n.X, n.Y, 1, 1,
                Color.FromArgb(alpha, 255, 165, 0), null, thickness: 3, dashed: true);
        }
    }

    // ─── Утилита: добавить прямоугольник в клетках ───
    private void AddRect(int x, int y, int wCells, int hCells,
                         Color stroke, Color? fill, double thickness,
                         bool dashed = false)
    {
        int ts = TilePx;

        var rect = new Rectangle
        {
            Width = wCells * ts,
            Height = hCells * ts,
            Stroke = new SolidColorBrush(stroke),
            StrokeThickness = thickness,
            Fill = fill.HasValue ? new SolidColorBrush(fill.Value) : null,
            IsHitTestVisible = false,
        };

        if (dashed)
        {
            rect.StrokeDashArray = new AvaloniaList<double> { 4, 3 };
        }

        Canvas.SetLeft(rect, x * ts);
        Canvas.SetTop(rect, y * ts);
        OverlayCanvas.Children.Add(rect);
    }

    // ══════════════════════════════════════════════════════════════
    //   КЛИК ПО КАНВАСУ
    // ══════════════════════════════════════════════════════════════
    private void OnCanvasPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (_map == null) return;

        int ts = TilePx;
        var p = e.GetPosition(RootPanel);
        int tx = (int)(p.X / ts);
        int ty = (int)(p.Y / ts);

        if (tx < 0 || tx >= _map.Width)  return;
        if (ty < 0 || ty >= _map.Height) return;

        TileClicked?.Invoke(tx, ty);
    }

    // ══════════════════════════════════════════════════════════════
    //   ШАХМАТНЫЙ ФОН
    // ══════════════════════════════════════════════════════════════
    private static ImageBrush CreateCheckerBrush()
    {
        const int size = 48;
        var wb = new WriteableBitmap(
            new PixelSize(size, size),
            new Vector(96, 96),
            PixelFormat.Bgra8888,
            AlphaFormat.Premul);

        using (var fb = wb.Lock())
        {
            unsafe
            {
                byte* basePtr = (byte*)fb.Address;
                int stride = fb.RowBytes;

                for (int y = 0; y < size; y++)
                {
                    uint* row = (uint*)(basePtr + y * stride);
                    for (int x = 0; x < size; x++)
                    {
                        bool light = ((x / 24) + (y / 24)) % 2 == 0;
                        row[x] = light ? 0xFF3A3A3A : 0xFF2A2A2A;
                    }
                }
            }
        }

        return new ImageBrush(wb)
        {
            TileMode = TileMode.Tile,
            Stretch = Stretch.Fill,
            DestinationRect = new RelativeRect(0, 0, size, size, RelativeUnit.Absolute),
        };
    }
}