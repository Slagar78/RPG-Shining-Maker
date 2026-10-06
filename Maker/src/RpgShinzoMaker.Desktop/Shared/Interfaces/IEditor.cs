// RpgShinzoMaker.Desktop/Shared/Interfaces/IEditor.cs
namespace RpgShinzoMaker.Desktop.Shared.Interfaces;

/// <summary>
/// Общий контракт для всех редакторов (MapEditor, EventEditor, BattleEditor...).
/// MainWindow знает только про этот интерфейс.
/// </summary>
public interface IEditor
{
    /// <summary>Сохранить всё, что редактирует этот редактор.</summary>
    void Save();

    /// <summary>Перечитать данные с диска (например, при переключении вкладки).</summary>
    void Reload();

    /// <summary>Редактор стал активной вкладкой.</summary>
    void OnActivate();

    /// <summary>Редактор перестал быть активной вкладкой.</summary>
    void OnDeactivate();
}