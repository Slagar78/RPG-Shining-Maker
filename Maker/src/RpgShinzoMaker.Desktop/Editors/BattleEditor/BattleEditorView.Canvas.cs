using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Media;
using Avalonia.Media.Imaging; // <--- ДОБАВЛЕНО (для BitmapInterpolationMode)
using Avalonia.VisualTree;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private void InitCanvas()
    {
        if (MapCanvasControl == null) return;

        MapCanvasControl.TileClicked  += OnTileClicked;
        MapCanvasControl.TileDragged  += OnTileDragged;
        MapCanvasControl.TileReleased += OnTileReleased;
        MapCanvasControl.TileHover    += OnTileHover;

        _mapScrollViewer = MapCanvasControl
            .GetVisualDescendants()
            .OfType<ScrollViewer>()
            .FirstOrDefault();

        if (_mapScrollViewer != null)
            _mapScrollViewer.ScrollChanged += (_, _) => UpdateOverlayPositions();
    }

    private void RebuildOverlay()
    {
        if (OverlayCanvas == null) return;

        OverlayCanvas.Children.Clear();
        _overlayElements.Clear();

        if (_currentMap == null || _currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];

        // 1. Боевая зона (красная рамка) — если не тянем новую
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

        // 2. Юниты (Союзники и Враги)
        foreach (var u in _allies.Where(x => x.Used)) DrawUnitOnOverlay(u);
        foreach (var u in _enemies.Where(x => x.Used)) DrawUnitOnOverlay(u);

        // 3. Terrain-маркеры (если включён режим Terrain)
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
                        -1 => Color.Parse("#E74C3C"), // Block
                        0  => Color.Parse("#F1C40F"), // Fly
                        _  => Color.Parse("#4CAF50")  // Walk
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
                    _overlayElements.Add(new OverlayElement { 
                        Control = marker, 
                        TileX = mapX, 
                        TileY = mapY, 
                        IsTileSized = false, 
                        Width = 0, Height = 0
                    });
                }
            }
        }

        // 4. Активная рамка выделения зоны (пока тянем мышкой)
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
        var img = new Image
        {
            Source = u.Image,
            Stretch = Stretch.Uniform,
            IsHitTestVisible = false
        };
        RenderOptions.SetBitmapInterpolationMode(img, BitmapInterpolationMode.None);
        
        OverlayCanvas.Children.Add(img);
        _overlayElements.Add(new OverlayElement { Control = img, TileX = u.X, TileY = u.Y, IsTileSized = true, Width = 1, Height = 1 });

        // Жёлтая рамка выделения
        if ((u.IsEnemy && _activePanel == 1 && _selectedSlot == _enemies.IndexOf(u)) ||
            (!u.IsEnemy && _activePanel == 0 && _selectedSlot == _allies.IndexOf(u)))
        {
            var frame = new Border
            {
                BorderBrush = new SolidColorBrush(Color.Parse("#FFD700")),
                BorderThickness = new Thickness(2),
                IsHitTestVisible = false
            };
            OverlayCanvas.Children.Add(frame);
            _overlayElements.Add(new OverlayElement { Control = frame, TileX = u.X, TileY = u.Y, IsTileSized = true, Width = 1, Height = 1 });
        }
    }

    private void UpdateOverlayPositions()
    {
        if (_currentMap == null || OverlayCanvas == null) return;

        int ts = Ts;
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
                // Terrain-маркер
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

        // Клик по юниту (выделение)
        var clickedAlly = _allies.FirstOrDefault(a => a.Used && a.X == tx && a.Y == ty);
        if (clickedAlly != null)
        {
            _activePanel = 0;
            _selectedSlot = _allies.IndexOf(clickedAlly);
            ShowInspector(clickedAlly);
            RebuildOverlay();
            return;
        }

        var clickedEnemy = _enemies.FirstOrDefault(e => e.Used && e.X == tx && e.Y == ty);
        if (clickedEnemy != null)
        {
            _activePanel = 1;
            _selectedSlot = _enemies.IndexOf(clickedEnemy);
            ShowInspector(clickedEnemy);
            RebuildOverlay();
            return;
        }

        // ЛКМ по пустому месту — поставить выбранного юнита из панели
        if (isLeft)
        {
            BattleUnit? unitToMove = null;
            if (_activePanel == 0 && _selectedSlot >= 0 && _selectedSlot < _allies.Count) unitToMove = _allies[_selectedSlot];
            else if (_activePanel == 1 && _selectedSlot >= 0 && _selectedSlot < _enemies.Count) unitToMove = _enemies[_selectedSlot];

            if (unitToMove != null && unitToMove.Used)
            {
                unitToMove.X = tx;
                unitToMove.Y = ty;
                RebuildOverlay();
            }
        }
    }

    private void OnTileDragged(int tx, int ty, int tileId, bool isLeft)
    {
        if (_viewMode == 2) { PaintTerrain(tx, ty); return; }
        
        if (_zoneDragActive)
        {
            _selCurrentX = tx;
            _selCurrentY = ty;
            RebuildOverlay();
            return;
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
            entry.BattleX = minX;
            entry.BattleY = minY;
            entry.BattleWidth = w;
            entry.BattleHeight = h;

            UpdateToolbarInfo();
            RebuildOverlay();
        }
    }

    private void OnTileHover(int tx, int ty) { }
}