using System;
using System.Collections.Generic;
using System.Diagnostics;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.BattleEditor;

public partial class BattleEditorView
{
    private void SaveAll()
    {
        if (_currentBattleIndex < 0) return;

        try
        {
            var entry = _battleEntries[_currentBattleIndex];
            var folder = entry.Folder;

            // 1. entries.json
            BattleEntriesService.Save(BattleEntriesService.GetEntriesPath(), _battleEntries);

            // 2. spriteset.json
            var alliesList = new List<UnitPlacement>();
            foreach (var a in _allies) if (a.Used) alliesList.Add(new UnitPlacement { Id = a.Id, X = a.X, Y = a.Y });

            var enemiesList = new List<UnitPlacement>();
            foreach (var e in _enemies) 
                if (e.Used) 
                    enemiesList.Add(new UnitPlacement 
                    { 
                        Id = e.Id, 
                        X = e.X, 
                        Y = e.Y,
                        Level = e.Level
                    });

            SpritesetService.Save(folder, alliesList, enemiesList);

            // 3. terrain.json
            if (_terrain != null)
                TerrainService.Save(folder, _terrain);

            Debug.WriteLine("[BATTLE] Всё сохранено.");
            StatusChanged?.Invoke("Сохранено успешно!");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[BATTLE] Ошибка сохранения: {ex}");
            StatusChanged?.Invoke($"Ошибка сохранения: {ex.Message}");
        }
    }
}