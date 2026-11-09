using System;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private void LoadSpritesetAndTerrain(string folder)
    {
        _allies.Clear();
        _enemies.Clear();

        SpritesetService.Load(folder, out var allies, out var enemies);

        var actorsData = LoadActorsJson();
        var enemiesData = LoadEnemiesJson();

        foreach (var a in allies)
        {
            var actor = actorsData.FirstOrDefault(x => x.Id == a.Id);
            if (actor == null) continue;
            _allies.Add(new BattleUnit { Id = a.Id, Name = actor.Name, Sprite = actor.Sprite, X = a.X, Y = a.Y, Used = true, IsEnemy = false });
        }

        foreach (var e in enemies)
        {
            var enemy = enemiesData.FirstOrDefault(x => x.Id == e.Id);
            if (enemy == null) continue;
            _enemies.Add(new BattleUnit 
            { 
                Id = e.Id, 
                Name = enemy.Name, 
                Sprite = enemy.Sprite, 
                X = e.X, 
                Y = e.Y, 
                Level = e.Level,
                Used = true, 
                IsEnemy = true 
            });
        }

        // Лимиты: 30 союзников, 48 врагов
        while (_allies.Count < 30) _allies.Add(new BattleUnit());
        while (_enemies.Count < 48) _enemies.Add(new BattleUnit());

        foreach (var u in _allies) LoadUnitSprite(u);
        foreach (var u in _enemies) LoadUnitSprite(u);

        if (_currentMap != null)
            _terrain = TerrainService.Load(folder, _currentMap.Width, _currentMap.Height);
            
        if (AlliesList != null) AlliesList.ItemsSource = _allies;
        if (EnemiesList != null) EnemiesList.ItemsSource = _enemies;
    }

    private void LoadUnitSprite(BattleUnit unit)
    {
        if (!unit.Used || string.IsNullOrEmpty(unit.Sprite)) return;
        
        string folder = unit.IsEnemy ? "mapsprites_enemy" : "mapsprites";
        string path = Path.Combine(ProjectPaths.AssetsDir, folder, unit.Sprite + ".png");
        
        if (File.Exists(path))
        {
            try 
            { 
                var fullBmp = new Bitmap(path);
                int frameSize = 48;
                int col = 0;
                int row = 2;

                if (fullBmp.PixelSize.Width >= frameSize && fullBmp.PixelSize.Height >= (row + 1) * frameSize)
                {
                    var cropRect = new Avalonia.PixelRect(col * frameSize, row * frameSize, frameSize, frameSize);
                    unit.Image = new CroppedBitmap(fullBmp, cropRect);
                }
                else
                {
                    unit.Image = fullBmp;
                }
            } 
            catch { } 
        }
    }

    private void OnAllySelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (AlliesList.SelectedItem is BattleUnit unit && unit.Used)
        {
            if (EnemiesList != null) EnemiesList.SelectedItem = null;
            ShowInspector(unit);
            RebuildOverlay();
            CenterOnUnit(unit);
        }
        else if (EnemiesList?.SelectedItem == null)
        {
            HideInspector();
        }
    }

    private void OnEnemySelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (EnemiesList.SelectedItem is BattleUnit unit && unit.Used)
        {
            if (AlliesList != null) AlliesList.SelectedItem = null;
            ShowInspector(unit);
            RebuildOverlay();
            CenterOnUnit(unit);
        }
        else if (AlliesList?.SelectedItem == null)
        {
            HideInspector();
        }
    }

    // ★ Метод автоскролла к юниту ★
    private void CenterOnUnit(BattleUnit unit)
    {
        if (_mapScrollViewer == null || _currentMap == null || _currentBattleIndex < 0) return;
        var entry = _battleEntries[_currentBattleIndex];
        int ts = Ts;
        
        double gx = (unit.X + entry.BattleX) * ts + ts / 2.0;
        double gy = (unit.Y + entry.BattleY) * ts + ts / 2.0;
        
        double targetX = gx - _mapScrollViewer.Viewport.Width / 2;
        double targetY = gy - _mapScrollViewer.Viewport.Height / 2;
        
        // Ограничиваем скролл размерами карты
        if (_mapScrollViewer.Extent.Width > 0)
            targetX = Math.Max(0, Math.Min(targetX, _mapScrollViewer.Extent.Width - _mapScrollViewer.Viewport.Width));
        if (_mapScrollViewer.Extent.Height > 0)
            targetY = Math.Max(0, Math.Min(targetY, _mapScrollViewer.Extent.Height - _mapScrollViewer.Viewport.Height));
        
        _mapScrollViewer.Offset = new Avalonia.Vector(targetX, targetY);
    }

    private void OnAddAllyClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (_allies.All(a => a.Used)) return;
        var actors = LoadActorsJson();
        if (actors.Count == 0) return;

        int newId = -1;
        for (int i = 0; i < actors.Count; i++)
        {
            if (!_allies.Any(a => a.Used && a.Id == i)) { newId = i; break; }
        }
        if (newId == -1) return;

        var actor = actors[newId];
        var freeSlot = _allies.First(a => !a.Used);
        freeSlot.Id = newId;
        freeSlot.Name = actor.Name;
        freeSlot.Sprite = actor.Sprite;
        
        int count = _allies.Count(a => a.Used);
        freeSlot.X = 2 + (count % 4);
        freeSlot.Y = 2 + (count / 4);

        freeSlot.Used = true;
        freeSlot.IsEnemy = false;
        LoadUnitSprite(freeSlot);

        AlliesList.ItemsSource = null;
        AlliesList.ItemsSource = _allies;
        AlliesList.SelectedItem = freeSlot;
        RebuildOverlay();
    }

    private void OnDeleteAllyClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (AlliesList.SelectedItem is BattleUnit unit && unit.Used)
        {
            unit.Used = false;
            unit.Image = null;
            InspectorPanel.IsVisible = false;
            AlliesList.ItemsSource = null;
            AlliesList.ItemsSource = _allies;
            RebuildOverlay();
        }
    }

    private void OnAddEnemyClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (_enemies.All(a => a.Used)) return;
        var enemies = LoadEnemiesJson();
        if (enemies.Count == 0) return;

        int newId = (EnemiesList.SelectedItem is BattleUnit selected && selected.Used) ? selected.Id : 0;
        var enemy = enemies.FirstOrDefault(x => x.Id == newId) ?? enemies[0];
        var freeSlot = _enemies.First(a => !a.Used);
        freeSlot.Id = enemy.Id;
        freeSlot.Name = enemy.Name;
        freeSlot.Sprite = enemy.Sprite;
        
        int count = _enemies.Count(a => a.Used);
        freeSlot.X = 4 + (count % 4);
        freeSlot.Y = 2 + (count / 4);

        freeSlot.Used = true;
        freeSlot.IsEnemy = true;
        LoadUnitSprite(freeSlot);

        EnemiesList.ItemsSource = null;
        EnemiesList.ItemsSource = _enemies;
        EnemiesList.SelectedItem = freeSlot;
        RebuildOverlay();
    }

    private void OnDeleteEnemyClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (EnemiesList.SelectedItem is BattleUnit unit && unit.Used)
        {
            unit.Used = false;
            unit.Image = null;
            InspectorPanel.IsVisible = false;
            EnemiesList.ItemsSource = null;
            EnemiesList.ItemsSource = _enemies;
            RebuildOverlay();
        }
    }

    private class UnitMeta { public int Id; public string Name = ""; public string Sprite = ""; }

    private System.Collections.Generic.List<UnitMeta> LoadActorsJson()
    {
        var res = new System.Collections.Generic.List<UnitMeta>();
        string path = Path.Combine(ProjectPaths.DataDir, "actors", "actors.json");
        if (!File.Exists(path)) return res;
        var root = JsonNode.Parse(File.ReadAllText(path));
        var arr = root?["actors"]?.AsArray();
        if (arr != null)
        {
            for (int i = 0; i < arr.Count; i++)
            {
                var o = arr[i];
                res.Add(new UnitMeta { Id = i, Name = o?["name"]?.GetValue<string>() ?? "", Sprite = o?["mapsprite"]?.GetValue<string>() ?? "" });
            }
        }
        return res;
    }

    private System.Collections.Generic.List<UnitMeta> LoadEnemiesJson()
    {
        var res = new System.Collections.Generic.List<UnitMeta>();
        string path = Path.Combine(ProjectPaths.DataDir, "enemies", "enemies.json");
        if (!File.Exists(path)) return res;
        var root = JsonNode.Parse(File.ReadAllText(path));
        var arr = root?["enemies"]?.AsArray();
        if (arr != null)
        {
            for (int i = 0; i < arr.Count; i++)
            {
                var o = arr[i];
                res.Add(new UnitMeta { Id = i, Name = o?["name"]?.GetValue<string>() ?? "", Sprite = o?["mapsprite"]?.GetValue<string>() ?? "" });
            }
        }
        return res;
    }
}