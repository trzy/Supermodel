/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * Model1Tilemap.cpp
 *
 * Based on original work by Olivier Galibert in MAME. Portions Copyright (c)
 * Olivier Galibert, used under the BSD 3-Clause License: see Docs/README.txt.
 *
 * Ported to Supermodel from MAME (src/mame/sega/segaic24.cpp:
 * segas24_tile_device only).
 */

/*
  Sega system24 hardware

  System24:
    The odd one out.  Medium resolution. Entirely ram-based, no
    graphics roms.  4-layer tilemap hardware in two pairs, selection
    on a 8-pixels basis.  Tile-based sprites(!) organised as a linked
    list.  The tilemap chip has been reused for model1 and model2,
    probably because they had it handy and it handles medium res.

  Port notes:
    - MAME renders four 64x64-tile (512x512) tilemap_t objects into cached
      pixmap/flagsmap bitmaps and blits them. Here the pixels are fetched on
      the fly from tile RAM + char RAM, which yields the same result as MAME's
      lazily-refreshed caches, so no invalidation bookkeeping is needed.
    - MAME's draw_common() splits the screen into rectangles where the
      512x512 tilemaps wrap, and its split modes into clip rectangles; here
      each line is drawn as spans with wrapping coordinates, which gives the
      same pixels. Spans are drawn a run of up to 8 pixels at a time, within
      one tile and one window mask bit (as the Model 3 tile generator does).
    - The priority bitmap (lpri) is not produced: MAME's rgb32 draw_rect
      ignores it and Model 1 never reads it.
    - xhout/xvout callbacks are not ported (Model 1 does not use them).
*/

#include "Model1Tilemap.h"
#include "BlockFile.h"
#include "OSD/Logger.h"
#include <algorithm>

/*
 * gfx layout (MAME char_layout): 8x8, 4bpp, planes {0,1,2,3}, x step 4 bits,
 * y step 32 bits, 32 bytes per char, xormask = NATIVE_ENDIAN_VALUE_LE_BE(8,0)
 * i.e. the bit stream is the big-endian view of the 16-bit words: a tile row
 * is two words, pixel 0 in the top nibble of the first.
 */

CSys24Tilemap::CSys24Tilemap(UINT16 tileMask)
  : m_charRAM(CHAR_RAM_WORDS, 0),
    m_tileRAM(TILE_RAM_WORDS, 0),
    m_tileMask(tileMask)
{
}

void CSys24Tilemap::Reset()
{
  // MAME's segas24_tile_device has no device_reset(): nothing to do.
}

void CSys24Tilemap::SaveState(CBlockFile *StateFile)
{
  StateFile->NewBlock("Sys24 Tilemap", __FILE__);
  StateFile->Write(m_tileRAM.data(), TILE_RAM_WORDS * sizeof(UINT16));
  StateFile->Write(m_charRAM.data(), CHAR_RAM_WORDS * sizeof(UINT16));
}

void CSys24Tilemap::LoadState(CBlockFile *StateFile)
{
  if (Result::OKAY != StateFile->FindBlock("Sys24 Tilemap"))
  {
    ErrorLog("Unable to load System 24 tilemap state. Save state file is corrupt.");
    return;
  }
  StateFile->Read(m_tileRAM.data(), TILE_RAM_WORDS * sizeof(UINT16));
  StateFile->Read(m_charRAM.data(), CHAR_RAM_WORDS * sizeof(UINT16));
}

UINT16 CSys24Tilemap::ReadTile(UINT32 offset) const
{
  return m_tileRAM[offset & (TILE_RAM_WORDS - 1)];
}

UINT16 CSys24Tilemap::ReadChar(UINT32 offset) const
{
  return m_charRAM[offset & (CHAR_RAM_WORDS - 1)];
}

void CSys24Tilemap::WriteTile(UINT32 offset, UINT16 data, UINT16 mask)
{
  // COMBINE_DATA; MAME then marks the tile dirty (not needed here)
  UINT16 &w = m_tileRAM[offset & (TILE_RAM_WORDS - 1)];
  w = (w & ~mask) | (data & mask);
}

void CSys24Tilemap::WriteChar(UINT32 offset, UINT16 data, UINT16 mask)
{
  // COMBINE_DATA; MAME then marks the gfx element dirty (not needed here)
  UINT16 &w = m_charRAM[offset & (CHAR_RAM_WORDS - 1)];
  w = (w & ~mask) | (data & mask);
}

void CSys24Tilemap::DrawSpan(UINT32 *line, const UINT32 *pens, int tilemap, int ty, int x0, int x1, int scrollx,
                             const UINT16 *maskLine, bool invert, int category, bool anyCategory, bool opaque) const
{
  // MAME tilemaps: TILEMAP_SCAN_ROWS 64x64 of 8x8
  const UINT16 *tileRow = &m_tileRAM[(tilemap << 12) | ((ty >> 3) << 6)];
  const UINT32 charRow = (ty & 7) * 2;

  for (int x = x0; x < x1; )
  {
    // A run stays within one tile and one 8-pixel mask cell
    int tx = (x + scrollx) & 511;
    int n = std::min(std::min(8 - (tx & 7), 8 - (x & 7)), x1 - x);

    bool hidden = false;
    if (maskLine)
      hidden = (((maskLine[x >> 7] << ((x >> 3) & 15)) & 0x8000) != 0) != invert;

    UINT16 tile = tileRow[tx >> 3];
    if (!hidden && (anyCategory || (tile >> 15) == category))
    {
      UINT32 c = ((tile & m_tileMask) * 16 + charRow) & (CHAR_RAM_WORDS - 1);
      UINT32 pixels = (UINT32(m_charRAM[c]) << 16) | m_charRAM[c + 1];   // 8 pens, 4 bits each
      pixels <<= 4 * (tx & 7);
      const UINT32 *colour = pens + ((tile >> 7) & 0xff) * 16;
      for (int k = 0; k < n; k++, pixels <<= 4)
      {
        unsigned pen = pixels >> 28;
        if (pen || opaque)
          line[x + k] = colour[pen];
      }
    }
    x += n;
  }
}

// segas24_tile_device::draw_common
void CSys24Tilemap::DrawLayer(int layer, UINT32 *dest, int pitchInPixels, int width, int height, bool opaque, const UINT32 *paletteRGBA) const
{
  const int tilemap = layer >> 1;
  const int category = layer & 1;
  const UINT16 hscroll = m_tileRAM[0x5000 + tilemap];
  const UINT16 vscroll = m_tileRAM[0x5004 + tilemap];
  const int mode = (m_tileRAM[0x5004 + (tilemap & 2)] >> 13) & 3;  // set by the pair's first tilemap
  const UINT16 *lineScroll = (hscroll & 0x8000) ? &m_tileRAM[0x4000 + 0x200 * tilemap] : nullptr;
  const int w = std::min(width, 496);
  const int h = std::min(height, 384);

  if (vscroll & 0x8000)
    return;   // disabled

  if (mode == 0)
  {
    // Normal: each tilemap has its own scroll; the pair's window mask picks
    // which of the two shows (inverted for the second). Opaque draws every
    // tile, whatever its category.
    const UINT16 *mask = &m_tileRAM[(tilemap & 2) ? 0x6800 : 0x6000];
    for (int y = 0; y < h; y++)
    {
      int scrollx = (-(lineScroll ? lineScroll[y] : hscroll)) & 0x1ff;
      int ty = (vscroll + y) & 0x1ff;
      DrawSpan(dest + (size_t) y * pitchInPixels, paletteRGBA, tilemap, ty, 0, w, scrollx,
               mask + y * 4, tilemap & 1, category, opaque, opaque);
    }
    return;
  }

  // Split modes: the pair's first tilemap's scroll values place a boundary,
  // with one tilemap of the pair on each side. Mode 1 splits the screen
  // vertically at line (-vscroll), modes 2 and 3 horizontally at column
  // hscroll (per line with the line table); bit 9 of the same value says
  // which tilemap comes first.
  if (tilemap & 1)
    return;
  const int scrolly = vscroll & 0x1ff;
  for (int y = 0; y < h; y++)
  {
    UINT32 *line = dest + (size_t) y * pitchInPixels;
    int ty = (y + scrolly) & 511;
    UINT16 hs = lineScroll ? lineScroll[y] : hscroll;
    int split = hs & 0x1ff;
    if (mode == 1)
    {
      int v = (-vscroll) & 0x1ff;
      int first = tilemap ^ (((-vscroll) & 0x200) ? 0 : 1);
      DrawSpan(line, paletteRGBA, first ^ (y >= v ? 1 : 0), ty, 0, w, -split, nullptr, false, category, false, opaque);
    }
    else
    {
      int first = tilemap ^ ((hs & 0x200) ? 0 : 1);
      DrawSpan(line, paletteRGBA, first, ty, 0, std::min(split, w), -split, nullptr, false, category, false, opaque);
      DrawSpan(line, paletteRGBA, first ^ 1, ty, split, w, -split, nullptr, false, category, false, opaque);
    }
  }
}
