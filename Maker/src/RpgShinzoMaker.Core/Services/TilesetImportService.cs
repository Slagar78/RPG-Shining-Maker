using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace RpgShinzoMaker.Core.Services;

public class ImportedMap
{
    public int Width  { get; set; }
    public int Height { get; set; }
    public int[] TileIndices { get; set; } = Array.Empty<int>();
    public List<byte[]> UniqueTiles { get; set; } = new();
}

public static class TilesetImportService
{
    public const int TileSize      = 48;
    public const int TilesetDim    = 32;
    public const int PaletteCols   = 8;
    public const int MaxTiles      = 1024;
    public const int TilesetPixels = TilesetDim * TileSize;

    public static ImportedMap? Extract(byte[] pixels, int width, int height, int stride)
    {
        if (width % TileSize != 0 || height % TileSize != 0) return null;

        int tilesX = width  / TileSize;
        int tilesY = height / TileSize;
        int total  = tilesX * tilesY;

        var map = new ImportedMap
        {
            Width = tilesX,
            Height = tilesY,
            TileIndices = new int[total],
        };

        const int rowBytes = TileSize * 4;
        var tile = new byte[TileSize * TileSize * 4];

        // ОПТИМИЗАЦИЯ: Словарь для мгновенного поиска (хэш -> список индексов)
        var hashLookup = new Dictionary<int, List<int>>();

        for (int ty = 0; ty < tilesY; ty++)
        {
            for (int tx = 0; tx < tilesX; tx++)
            {
                for (int y = 0; y < TileSize; y++)
                {
                    int srcOff = (ty * TileSize + y) * stride + tx * TileSize * 4;
                    int dstOff = y * rowBytes;
                    Buffer.BlockCopy(pixels, srcOff, tile, dstOff, rowBytes);
                }

                int tileHash = ComputeHash(tile);
                int idx = -1;

                if (hashLookup.TryGetValue(tileHash, out var candidates))
                {
                    foreach (var c in candidates)
                    {
                        // Сравниваем байты только если хэши совпали
                        if (new ReadOnlySpan<byte>(map.UniqueTiles[c]).SequenceEqual(tile))
                        {
                            idx = c;
                            break;
                        }
                    }
                }
                else
                {
                    candidates = new List<int>();
                    hashLookup[tileHash] = candidates;
                }

                if (idx < 0)
                {
                    if (map.UniqueTiles.Count >= MaxTiles) return null;
                    idx = map.UniqueTiles.Count;
                    map.UniqueTiles.Add((byte[])tile.Clone());
                    candidates.Add(idx);
                }

                map.TileIndices[tx * tilesY + ty] = idx;
            }
        }
        return map;
    }

    private static int ComputeHash(ReadOnlySpan<byte> data)
    {
        unchecked
        {
            int hash = 17;
            var ints = MemoryMarshal.Cast<byte, int>(data);
            for (int i = 0; i < ints.Length; i++)
            {
                hash = hash * 31 + ints[i];
            }
            return hash;
        }
    }

    public static byte[] BuildTilesetPixels(List<byte[]> uniqueTiles)
    {
        int dim = TilesetPixels;
        int stride = dim * 4;
        var outPixels = new byte[stride * dim];

        // Белый фон
        for (int i = 0; i < outPixels.Length; i += 4)
        {
            outPixels[i]     = 255;
            outPixels[i + 1] = 255;
            outPixels[i + 2] = 255;
            outPixels[i + 3] = 255;
        }

        int strips = TilesetDim / PaletteCols;
        int tileIdx = 0;
        const int rowBytes = TileSize * 4;

        for (int strip = 0; strip < strips && tileIdx < uniqueTiles.Count; strip++)
        {
            int startCol = strip * PaletteCols;
            for (int row = 0; row < TilesetDim && tileIdx < uniqueTiles.Count; row++)
            {
                for (int c = 0; c < PaletteCols && tileIdx < uniqueTiles.Count; c++)
                {
                    int col = startCol + c;
                    var tile = uniqueTiles[tileIdx];

                    for (int y = 0; y < TileSize; y++)
                    {
                        int dstOff = (row * TileSize + y) * stride + col * TileSize * 4;
                        int srcOff = y * rowBytes;
                        Buffer.BlockCopy(tile, srcOff, outPixels, dstOff, rowBytes);
                    }
                    tileIdx++;
                }
            }
        }
        return outPixels;
    }
}