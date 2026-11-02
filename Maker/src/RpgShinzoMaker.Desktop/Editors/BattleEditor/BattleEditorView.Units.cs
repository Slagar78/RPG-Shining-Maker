using System;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    // Загрузка слотов из spriteset.json
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
            _enemies.Add(new BattleUnit { Id = e.Id, Name = enemy.Name, Sprite = enemy.Sprite, X = e.X, Y = e.Y, Used = true, IsEnemy = true });
        }

        // Создаём пустые слоты до 16 и 20 (для сетки в панелях)
        while (_allies.Count < 16) _allies.Add(new BattleUnit());
        while (_enemies.Count < 20) _enemies.Add(new BattleUnit());

        // Загружаем картинки
        foreach (var u in _allies) LoadUnitSprite(u);
        foreach (var u in _enemies) LoadUnitSprite(u);

        // Исправлено предупреждение CS8602 (добавлена проверка на null)
        if (_currentMap != null)
            _terrain = TerrainService.Load(folder, _currentMap.Width, _currentMap.Height);
    }

    private void LoadUnitSprite(BattleUnit unit)
    {
        if (!unit.Used || string.IsNullOrEmpty(unit.Sprite)) return;
        
        string folder = unit.IsEnemy ? "mapsprites_enemy" : "mapsprites";
        string path = Path.Combine(ProjectPaths.AssetsDir, folder, unit.Sprite + ".png");
        if (File.Exists(path))
        {
            try { unit.Image = new Bitmap(path); } catch { }
        }
    }

    // ─── Кнопки Add / Del ───
    private void OnAddAllyClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (_allies.All(a => a.Used)) return; // Нет мест
        var actors = LoadActorsJson();
        if (actors.Count == 0) return;

        // Ищем свободный ID (не занятый другими союзниками)
        int newId = -1;
        for (int i = 0; i < actors.Count; i++)
        {
            if (!_allies.Any(a => a.Used && a.Id == i)) { newId = i; break; }
        }
        if (newId == -1) return; // Все заняты

        var actor = actors[newId];
        var freeSlot = _allies.First(a => !a.Used);
        freeSlot.Id = newId;
        freeSlot.Name = actor.Name;
        freeSlot.Sprite = actor.Sprite;
        freeSlot.X = 0; freeSlot.Y = 0;
        freeSlot.Used = true;
        freeSlot.IsEnemy = false;
        LoadUnitSprite(freeSlot);

        _activePanel = 0;
        _selectedSlot = _allies.IndexOf(freeSlot);
        ShowInspector(freeSlot);
        RebuildOverlay();
    }

    private void OnDeleteAllyClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (_activePanel == 0 && _selectedSlot >= 0 && _allies[_selectedSlot].Used)
        {
            _allies[_selectedSlot].Used = false;
            _allies[_selectedSlot].Image = null;
            InspectorPanel.IsVisible = false;
            RebuildOverlay();
        }
    }

    private void OnAddEnemyClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (_enemies.All(a => a.Used)) return;
        var enemies = LoadEnemiesJson();
        if (enemies.Count == 0) return;

        int newId = (_inspectorPanel == 1 && _inspectorSlotIdx >= 0 && _enemies[_inspectorSlotIdx].Used) 
            ? _enemies[_inspectorSlotIdx].Id : 0;

        var enemy = enemies.FirstOrDefault(x => x.Id == newId) ?? enemies[0];
        var freeSlot = _enemies.First(a => !a.Used);
        freeSlot.Id = enemy.Id;
        freeSlot.Name = enemy.Name;
        freeSlot.Sprite = enemy.Sprite;
        freeSlot.X = 0; freeSlot.Y = 0;
        freeSlot.Used = true;
        freeSlot.IsEnemy = true;
        LoadUnitSprite(freeSlot);

        _activePanel = 1;
        _selectedSlot = _enemies.IndexOf(freeSlot);
        ShowInspector(freeSlot);
        RebuildOverlay();
    }

    private void OnDeleteEnemyClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
    {
        if (_activePanel == 1 && _selectedSlot >= 0 && _enemies[_selectedSlot].Used)
        {
            _enemies[_selectedSlot].Used = false;
            _enemies[_selectedSlot].Image = null;
            InspectorPanel.IsVisible = false;
            RebuildOverlay();
        }
    }

    // ─── Парсинг баз ───
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
                res.Add(new UnitMeta {
                    Id = i,
                    Name = o?["name"]?.GetValue<string>() ?? "",
                    Sprite = o?["mapsprite"]?.GetValue<string>() ?? ""
                });
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
                res.Add(new UnitMeta {
                    Id = i,
                    Name = o?["name"]?.GetValue<string>() ?? "",
                    Sprite = o?["mapsprite"]?.GetValue<string>() ?? ""
                });
            }
        }
        return res;
    }
}