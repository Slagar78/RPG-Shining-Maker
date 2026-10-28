using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Platform.Storage;
using RpgShinzoMaker.Core.Services;

namespace RpgShinzoMaker.Desktop.Editors.TilesetImporter;

public partial class TilesetImporterView
{
    private async void OnLoadClick(object? sender, RoutedEventArgs e)
    {
        var tl = TopLevel.GetTopLevel(this);
        if (tl == null) return;

        var files = await tl.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Выберите PNG-карту",
            AllowMultiple = false,
            FileTypeFilter = new[] { new FilePickerFileType("PNG") { Patterns = new[] { "*.png" } } }
        });

        if (files.Count == 0) return;
        ProcessImage(files[0].Path.LocalPath);
    }

    private void ProcessImage(string path)
    {
        try
        {
            var bmp = new Bitmap(path);
            var size = bmp.PixelSize;

            if (size.Width % TilesetImportService.TileSize != 0 || size.Height % TilesetImportService.TileSize != 0)
            {
                StatusText.Text = $"Ошибка: {size.Width}x{size.Height} не делится на 48!";
                StatusBarText.Text = "Размер PNG не кратен 48x48";
                SetStatus(StatusBarText.Text);
                return;
            }

            var pixels = ReadPixels(bmp, out int w, out int h, out int stride);
            var imported = TilesetImportService.Extract(pixels, w, h, stride);
            
            if (imported == null)
            {
                StatusText.Text = $"Слишком много уникальных тайлов (> {TilesetImportService.MaxTiles})";
                StatusBarText.Text = StatusText.Text;
                SetStatus(StatusText.Text);
                bmp.Dispose();
                return;
            }

            _sourcePath = path;
            _sourceBitmap?.Dispose();
            _sourceBitmap = bmp;
            _imported = imported;
            _tilesetPixels = TilesetImportService.BuildTilesetPixels(imported.UniqueTiles);

            var tilesetBmp = CreateBitmap(_tilesetPixels, 
                TilesetImportService.TilesetPixels, 
                TilesetImportService.TilesetPixels);

            var displayTiles = new List<CroppedBitmap>();
            for (int y = 0; y < TilesetImportService.TilesetDim; y++)
            {
                for (int x = 0; x < TilesetImportService.TilesetDim; x++)
                {
                    var crop = new CroppedBitmap(tilesetBmp, new PixelRect(
                        x * TilesetImportService.TileSize, 
                        y * TilesetImportService.TileSize, 
                        TilesetImportService.TileSize, 
                        TilesetImportService.TileSize));
                    displayTiles.Add(crop);
                }
            }
            TilesetPreviewList.ItemsSource = displayTiles;

            SourcePreview.Source = _sourceBitmap;
            SourceInfoText.Text = $"{size.Width}×{size.Height} px ({imported.Width}×{imported.Height} tiles)";
            TilesetInfoText.Text = $"Уникальных тайлов: {imported.UniqueTiles.Count} / {TilesetImportService.MaxTiles}";

            SaveTilesetBtn.IsEnabled = true;
            SaveMapBtn.IsEnabled = true;
            StatusText.Text = $"Загружено: {Path.GetFileName(path)} — уникальных тайлов {imported.UniqueTiles.Count}";
            StatusBarText.Text = StatusText.Text;
            SetStatus(StatusText.Text);

            string? baseName = Path.GetFileNameWithoutExtension(path);
            if (!string.IsNullOrEmpty(baseName))
            {
                MapNameBox.Text = Sanitize(baseName);
                TilesetNameBox.Text = Sanitize(baseName) + "_tiles";
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Ошибка: {ex.Message}";
            StatusBarText.Text = StatusText.Text;
            SetStatus(StatusText.Text);
        }
    }

    // УМНАЯ ФУНКЦИЯ: Теперь переводит русские буквы в английские
    private static string Sanitize(string s)
    {
        var sb = new StringBuilder(s.Length);
        foreach (var c in s.ToLower())
        {
            // Транслитерация русских букв
            string eng = c switch
            {
                'а' => "a", 'б' => "b", 'в' => "v", 'г' => "g", 'д' => "d",
                'е' => "e", 'ё' => "e", 'ж' => "zh", 'з' => "z", 'и' => "i",
                'й' => "y", 'к' => "k", 'л' => "l", 'м' => "m", 'н' => "n",
                'о' => "o", 'п' => "p", 'р' => "r", 'с' => "s", 'т' => "t",
                'у' => "u", 'ф' => "f", 'х' => "h", 'ц' => "ts", 'ч' => "ch",
                'ш' => "sh", 'щ' => "sch", 'ъ' => "", 'ы' => "y", 'ь' => "",
                'э' => "e", 'ю' => "yu", 'я' => "ya",
                _ => null
            };

            if (eng != null)
            {
                sb.Append(eng);
            }
            else if (char.IsLetterOrDigit(c) && c < 128)
            {
                sb.Append(c); // Английские буквы и цифры оставляем
            }
            else if (c == '-' || c == '_')
            {
                sb.Append(c); // Дефисы и подчеркивания оставляем
            }
            else
            {
                sb.Append('_'); // Всё остальное (пробелы, спецсимволы) -> подчеркивание
            }
        }
        
        string result = sb.ToString().Trim('_');
        return string.IsNullOrEmpty(result) ? "map" : result;
    }

    private static unsafe byte[] ReadPixels(Bitmap src, out int w, out int h, out int stride)
    {
        var size = src.PixelSize;
        w = size.Width;
        h = size.Height;

        var wb = new WriteableBitmap(size, new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);

        using (var fb = wb.Lock())
        {
            src.CopyPixels(new PixelRect(0, 0, w, h), fb.Address, fb.RowBytes * h, fb.RowBytes);
            stride = fb.RowBytes;

            var pixels = new byte[fb.RowBytes * h];
            Marshal.Copy(fb.Address, pixels, 0, pixels.Length);
            return pixels;
        }
    }

    private static unsafe WriteableBitmap CreateBitmap(byte[] pixels, int w, int h)
    {
        var wb = new WriteableBitmap(new PixelSize(w, h), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);

        using (var fb = wb.Lock())
        {
            for (int y = 0; y < h; y++)
            {
                fixed (byte* src = pixels)
                {
                    Buffer.MemoryCopy(
                        src + y * w * 4,
                        (byte*)fb.Address + y * fb.RowBytes,
                        fb.RowBytes,
                        w * 4);
                }
            }
        }
        return wb;
    }
}