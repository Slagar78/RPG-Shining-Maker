using System;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using RpgShinzoMaker.Core.Services;
using RpgShinzoMaker.Desktop.Shared.Interfaces;

namespace RpgShinzoMaker.Desktop.Editors.TilesetImporter;

public partial class TilesetImporterView : UserControl, IEditor
{
    public event Action<string>? StatusChanged;
    private string? _sourcePath;
    private Bitmap? _sourceBitmap;
    private ImportedMap? _imported;
    private byte[]? _tilesetPixels;

    public TilesetImporterView()
    {
        InitializeComponent();
    }

    // Вызывается при нажатии SAVE в главном тулбаре
    public void Save()
    {
        if (_imported == null) return;
        
        // Вызываем диалог сохранения карты
        OnSaveMapClick(this, new Avalonia.Interactivity.RoutedEventArgs());
    }

    public void Reload() { }
    public void OnActivate() { }
    public void OnDeactivate() { }
    private void SetStatus(string text) => StatusChanged?.Invoke(text);
}