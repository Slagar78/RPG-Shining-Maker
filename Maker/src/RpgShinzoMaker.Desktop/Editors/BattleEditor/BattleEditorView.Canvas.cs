using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Avalonia.VisualTree;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private Border? _mapCanvasHoverBorder;
    private BattleUnit? _draggingUnit;
    private DispatcherTimer? _dragScrollTimer;
    private Point _lastMousePosition; // ★ Храним позицию мыши здесь ★

    private void InitCanvas()
    {
        if (MapCanvasControl == null) return;

        MapCanvasControl.TileClicked  += OnTileClicked;
        MapCanvasControl.TileDragged  += OnTileDragged;
        MapCanvasControl.TileReleased += OnTileReleased;
        MapCanvasControl.TileHover    += OnTileHover;

        // ★ Запоминаем позицию мыши при каждом движении ★
        MapCanvasControl.PointerMoved += (s, e) =>
        {
            _lastMousePosition = e.GetPosition(MapCanvasControl);
        };

        // Глобальный обработчик отпускания мыши
        this.AddHandler(PointerReleasedEvent, OnGlobalPointerReleased, RoutingStrategies.Bubble);

        AttachScrollViewer();
    }

    private void AttachScrollViewer()
    {
        if (_mapScrollViewer != null) return;
        if (MapCanvasControl == null) return;

        _mapScrollViewer = MapCanvasControl.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
        
        if (_mapScrollViewer != null)
        {
            _mapScrollViewer.ScrollChanged += (_, _) => UpdateOverlayPositions();
        }
    }

    private void RebuildOverlay()
    {
        AttachScrollViewer();
        
        if (OverlayCanvas == null) return;

        OverlayCanvas.Children.Clear();
        _overlayElements.Clear();

        if (_currentMap == null || _currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];

        // Находим и прячем желтую рамку MapCanvas
        if (_mapCanvasHoverBorder == null)
        {
            _mapCanvasHoverBorder = MapCanvasControl.GetVisualDescendants().OfType<Border>()
                .FirstOrDefault(b => b.BorderBrush is SolidColorBrush brush && brush.Color == Color.Parse("#FFD700"));
            if (_mapCanvasHoverBorder != null) _mapCanvasHoverBorder.IsVisible = false;
        }

        // Боевая зона
        if (!_zoneDragActive && entry.BattleWidth > 0 && entry.BattleHeight > 0)
        {
            var border = new Border
            {
                BorderBrush = new SolidColorBrush(Color.Parse("#FF3333")),
                BorderThickness = new Thickness(2),
                IsHitTestVisible = false,
            };
            OverlayCanvas.Children.Add(border);
            _overlayElements.Add(new OverlayElement { Control = border, TileX = entry.BattleX, TileY = entry.BattleY, IsTileSized = false, Width = entry.BattleWidth, Height = entry.BattleHeight });
        }

        foreach (var u in _allies.Where(x => x.Used)) DrawUnitOnOverlay(u);
        foreach (var u in _enemies.Where(x => x.Used)) DrawUnitOnOverlay(u);

        // Terrain-маркеры
        if (_viewMode == 2 && _terrain != null)
        {
            int bw = (entry.BattleWidth > 0) ? entry.BattleWidth : _currentMap.Width;
            int bh = (entry.BattleHeight > 0) ? entry.BattleHeight : _currentMap.Height;
            int bx = entry.BattleX;
            int by = entry.BattleY;

            for (int y = 0; y < bh; y++)
            {
                for (int x = 0; x < bw; x++)
                {
                    int mapX = x + bx;
                    int mapY = y + by;
                    if (mapX >= _currentMap.Width || mapY >= _currentMap.Height) continue;

                    int t = _terrain[mapY][mapX];
                    var col = t switch
                    {
                        -1 => Color.Parse("#E74C3C"),
                        0  => Color.Parse("#F1C40F"),
                        _  => Color.Parse("#4CAF50")
                    };

                    var marker = new Border
                    {
                        Background = new SolidColorBrush(col),
                        Width = 10, Height = 10,
                        CornerRadius = new CornerRadius(2),
                        IsHitTestVisible = false,
                        Opacity = 0.8
                    };
                    
                    OverlayCanvas.Children.Add(marker);
                    _overlayElements.Add(new OverlayElement { Control = marker, TileX = mapX, TileY = mapY, IsTileSized = false, Width = 0, Height = 0 });
                }
            }
        }

        // Активная рамка выделения зоны
        if (_zoneDragActive)
        {
            int minX = Math.Min(_selStartX, _selCurrentX);
            int minY = Math.Min(_selStartY, _selCurrentY);
            int w = Math.Abs(_selStartX - _selCurrentX) + 1;
            int h = Math.Abs(_selStartY - _selCurrentY) + 1;

            var border = new Border
            {
                BorderBrush = new SolidColorBrush(Color.Parse("#FFD700")),
                BorderThickness = new Thickness(2),
                Background = new SolidColorBrush(Color.Parse("#33FFD700")),
                IsHitTestVisible = false,
            };
            OverlayCanvas.Children.Add(border);
            _overlayElements.Add(new OverlayElement { Control = border, TileX = minX, TileY = minY, IsTileSized = false, Width = w, Height = h });
        }

        UpdateOverlayPositions();
    }

    private void DrawUnitOnOverlay(BattleUnit u)
    {
        if (_currentMap == null || _currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];

        int globalX = u.X + entry.BattleX;
        int globalY = u.Y + entry.BattleY;

        var container = new Panel { IsHitTestVisible = false };
        
        var img = new Image { Source = u.Image, Stretch = Stretch.Uniform };
        RenderOptions.SetBitmapInterpolationMode(img, BitmapInterpolationMode.None);
        container.Children.Add(img);

        bool isSelected = (u.IsEnemy && EnemiesList.SelectedItem == u) || (!u.IsEnemy && AlliesList.SelectedItem == u);

        if (u == _hoveredUnit || isSelected || u == _draggingUnit)
        {
            var frame = new Border { Classes = { "hover-pulse" } };
            container.Children.Add(frame);
        }

        OverlayCanvas.Children.Add(container);
        _overlayElements.Add(new OverlayElement { Control = container, TileX = globalX, TileY = globalY, IsTileSized = true, Width = 1, Height = 1 });
    }

    private void UpdateOverlayPositions()
    {
        if (_currentMap == null || OverlayCanvas == null) return;

        if (MapCanvasControl != null)
        {
            OverlayCanvas.Width = MapCanvasControl.Bounds.Width;
            OverlayCanvas.Height = MapCanvasControl.Bounds.Height;
        }

        int ts = Ts;
        
        if (_mapScrollViewer == null) AttachScrollViewer();
        
        double ox = _mapScrollViewer?.Offset.X ?? 0;
        double oy = _mapScrollViewer?.Offset.Y ?? 0;

        foreach (var el in _overlayElements)
        {
            Canvas.SetLeft(el.Control, el.TileX * ts - ox);
            Canvas.SetTop (el.Control, el.TileY * ts - oy);

            if (el.IsTileSized)
            {
                el.Control.Width  = ts;
                el.Control.Height = ts;
            }
            else if (el.Width == 0 && el.Height == 0)
            {
                el.Control.Width = ts / 3.0;
                el.Control.Height = ts / 3.0;
                Canvas.SetLeft(el.Control, el.TileX * ts - ox + ts - (ts / 3.0));
                Canvas.SetTop (el.Control, el.TileY * ts - oy + ts - (ts / 3.0));
            }
            else
            {
                el.Control.Width  = el.Width  * ts;
                el.Control.Height = el.Height * ts;
            }
        }
    }

    private void OnTileClicked(int tx, int ty, int tileId, bool isLeft)
    {
        if (_mapCanvasHoverBorder != null) _mapCanvasHoverBorder.IsVisible = false;

        if (_viewMode == 2) { PaintTerrain(tx, ty); return; }

        if (_selectingZone)
        {
            if (isLeft)
            {
                _zoneDragActive = true;
                _selStartX = _selCurrentX = tx;
                _selStartY = _selCurrentY = ty;
                RebuildOverlay();
            }
            return;
        }

        if (_currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];
        int localX = tx - entry.BattleX;
        int localY = ty - entry.BattleY;

        var clickedAlly = _allies.FirstOrDefault(a => a.Used && a.X == localX && a.Y == localY);
        if (clickedAlly != null) 
        { 
            AlliesList.SelectedItem = clickedAlly;
            if (isLeft) StartDragging(clickedAlly);
            return; 
        }

        var clickedEnemy = _enemies.FirstOrDefault(e => e.Used && e.X == localX && e.Y == localY);
        if (clickedEnemy != null) 
        { 
            EnemiesList.SelectedItem = clickedEnemy;
            if (isLeft) StartDragging(clickedEnemy);
            return; 
        }

        if (isLeft)
        {
            BattleUnit? unitToMove = null;
            if (AlliesList.SelectedItem is BattleUnit selectedAlly && selectedAlly.Used)
            {
                unitToMove = selectedAlly;
            }
            else if (EnemiesList.SelectedItem is BattleUnit selectedEnemy && selectedEnemy.Used)
            {
                unitToMove = selectedEnemy;
            }

            if (unitToMove != null)
            {
                bool isOccupied = _allies.Any(a => a.Used && a != unitToMove && a.X == localX && a.Y == localY) ||
                                  _enemies.Any(e => e.Used && e.X == localX && e.Y == localY);
                                  
                if (!isOccupied)
                {
                    unitToMove.X = localX;
                    unitToMove.Y = localY;
                    RebuildOverlay();
                }
            }
        }
    }

    private void OnTileDragged(int tx, int ty, int tileId, bool isLeft)
    {
        if (_mapCanvasHoverBorder != null) _mapCanvasHoverBorder.IsVisible = false;

        if (_viewMode == 2) { PaintTerrain(tx, ty); return; }
        if (_zoneDragActive)
        {
            _selCurrentX = tx;
            _selCurrentY = ty;
            RebuildOverlay();
        }
    }

    private void OnTileReleased(int tx, int ty, bool isLeft)
    {
        if (_zoneDragActive)
        {
            _zoneDragActive = false;
            _selectingZone = false;
            ZoneBtn.IsChecked = false;

            int minX = Math.Min(_selStartX, _selCurrentX);
            int minY = Math.Min(_selStartY, _selCurrentY);
            int w = Math.Abs(_selStartX - _selCurrentX) + 1;
            int h = Math.Abs(_selStartY - _selCurrentY) + 1;

            var entry = _battleEntries[_currentBattleIndex];
            entry.BattleX = minX; entry.BattleY = minY;
            entry.BattleWidth = w; entry.BattleHeight = h;

            UpdateToolbarInfo();
            RebuildOverlay();
        }
    }

    private void OnTileHover(int tx, int ty) 
    {
        if (_mapCanvasHoverBorder != null) _mapCanvasHoverBorder.IsVisible = false;

        if (_currentMap == null || _currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];
        int localX = tx - entry.BattleX;
        int localY = ty - entry.BattleY;
        
        var hovered = _allies.FirstOrDefault(a => a.Used && a.X == localX && a.Y == localY) ??
                      _enemies.FirstOrDefault(e => e.Used && e.X == localX && e.Y == localY);
                      
        if (hovered != _hoveredUnit)
        {
            _hoveredUnit = hovered;
            RebuildOverlay();
        }
    }

    private void StartDragging(BattleUnit unit)
    {
        _draggingUnit = unit;
        
        if (_dragScrollTimer != null) return;
        _dragScrollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
        _dragScrollTimer.Tick += OnDragScrollTick;
        _dragScrollTimer.Start();
    }

    private void StopDragging()
    {
        if (_dragScrollTimer != null)
        {
            _dragScrollTimer.Stop();
            _dragScrollTimer.Tick -= OnDragScrollTick;
            _dragScrollTimer = null;
        }
        _draggingUnit = null;
    }

    private void OnGlobalPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        if (_draggingUnit != null) StopDragging();
    }

    private void OnDragScrollTick(object? sender, EventArgs e)
    {
        if (_draggingUnit == null || _currentMap == null || _currentBattleIndex < 0) return;
        if (_mapScrollViewer == null || MapCanvasControl == null) return;

        int ts = Ts;
        var entry = _battleEntries[_currentBattleIndex];

        // ★ Используем сохранённую позицию мыши ★
        var pos = _lastMousePosition;
        
        double scrollX = _mapScrollViewer.Offset.X;
        double scrollY = _mapScrollViewer.Offset.Y;
        double vw = _mapScrollViewer.Viewport.Width;
        double vh = _mapScrollViewer.Viewport.Height;

        double margin = 60;
        double step = 12;
        double newScrollX = scrollX;
        double newScrollY = scrollY;

        if (pos.X < margin) newScrollX -= step;
        else if (pos.X > vw - margin) newScrollX += step;
        if (pos.Y < margin) newScrollY -= step;
        else if (pos.Y > vh - margin) newScrollY += step;

        newScrollX = Math.Max(0, Math.Min(newScrollX, _mapScrollViewer.Extent.Width - vw));
        newScrollY = Math.Max(0, Math.Min(newScrollY, _mapScrollViewer.Extent.Height - vh));

        if (Math.Abs(newScrollX - scrollX) > 0.1 || Math.Abs(newScrollY - scrollY) > 0.1)
        {
            _mapScrollViewer.Offset = new Vector(newScrollX, newScrollY);
        }

        int localX = (int)((pos.X + newScrollX) / ts) - entry.BattleX;
        int localY = (int)((pos.Y + newScrollY) / ts) - entry.BattleY;

        if (localX == _draggingUnit.X && localY == _draggingUnit.Y) return;
        if (localX < 0 || localY < 0) return;
        if (localX >= entry.BattleWidth || localY >= entry.BattleHeight) return;
        if (localX + entry.BattleX >= _currentMap.Width || localY + entry.BattleY >= _currentMap.Height) return;

        bool isOccupied = _allies.Any(a => a.Used && a != _draggingUnit && a.X == localX && a.Y == localY) ||
                          _enemies.Any(e => e.Used && e != _draggingUnit && e.X == localX && e.Y == localY);
        if (isOccupied) return;

        _draggingUnit.X = localX;
        _draggingUnit.Y = localY;
        RebuildOverlay();
    }
}