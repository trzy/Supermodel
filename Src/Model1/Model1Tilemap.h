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
 * Model1Tilemap.h
 *
 * Based on original work by Olivier Galibert in MAME. Portions Copyright (c)
 * Olivier Galibert, used under the BSD 3-Clause License: see Docs/README.txt.
 *
 * Ported to Supermodel from MAME (src/mame/sega/segaic24.cpp, segaic24.h:
 * segas24_tile_device only).
 */

/*
  Sega System 24 tilemap chip (315-5292), as reused by Sega Model 1 for its
  2D layers.

  Software renderer replacing MAME's tilemap_t / bitmap path. It produces the
  same pixels MAME's segas24_tile_device::draw() (rgb32 variant) produces.

  Memory (word offsets, as MAME's tile_r/tile_w/char_r/char_w):
    Tile RAM: 0x8000 words  (Model 1: 0x700000-0x70FFFF, offset = (addr & 0xFFFF) >> 1)
    Char RAM: 0x40000 words (Model 1: 0x780000-0x7FFFFF, offset = (addr & 0x7FFFF) >> 1)

  Tile RAM layout:
    0x0000-0x3FFF  four 64x64 tilemaps of 8x8 tiles (0x1000 words each), in
                   two pairs: 0/1 and 2/3
    0x4000-0x47FF  line scroll tables, 0x200 words per tilemap
    0x5000-0x5003  horizontal scroll per tilemap (bit 15: use the line table)
    0x5004-0x5007  vertical scroll per tilemap (bit 15: disabled); words 0x5004
                   and 0x5006 also hold the pair's split mode in bits 13-14
    0x6000, 0x6800 window masks for pairs 0/1 and 2/3: 4 words (64 bits, one per
                   8 pixels) per line; a set bit shows the pair's second tilemap

  Palette index formation (same as MAME's gfx_element / tilemap):
    tile word val: code = val & tileMask (0x3FFF on Model 1),
                   color = (val >> 7) & 0xFF, category = val >> 15
    index = color * 16 + pen (pen = 4bpp pixel 0..15), i.e. 0..4095.
    MAME's tile gfx element has colour base 0 and 512 colours (8192 pens / 16),
    so no offset is applied: paletteRGBA[index] is Model 1 palette RAM entry
    'index' (0x900000 + index*2). Pen 0 is transparent (unless opaque).
*/

#ifndef INCLUDED_MODEL1TILEMAP_H
#define INCLUDED_MODEL1TILEMAP_H

#include "Types.h"
#include <vector>

class CBlockFile;

class CSys24Tilemap
{
public:
  // MAME: S24TILE(config, m_tiles, 0, 0x3fff) on Model 1
  explicit CSys24Tilemap(UINT16 tileMask = 0x3FFF);

  // MAME has no device_reset for this chip; RAM is preserved across Reset()
  // (it is zeroed only at construction, like MAME's device_start).
  void Reset();

  void SaveState(CBlockFile *StateFile);
  void LoadState(CBlockFile *StateFile);

  // 16-bit handlers. Offsets are WORD offsets; mask = MAME mem_mask.
  UINT16 ReadTile(UINT32 offset) const;
  void   WriteTile(UINT32 offset, UINT16 data, UINT16 mask = 0xFFFF);
  UINT16 ReadChar(UINT32 offset) const;
  void   WriteChar(UINT32 offset, UINT16 data, UINT16 mask = 0xFFFF);

  /*
   * DrawLayer(layer, dest, pitch, width, height, opaque, paletteRGBA):
   *
   * Equivalent of MAME m_tiles->draw(screen, bitmap, cliprect, layer, 0,
   * opaque ? TILEMAP_DRAW_OPAQUE : 0) on the 496x384 screen. Layer = tilemap
   * * 2 + category: the tiles of that category are drawn. dest is clipped to
   * width x height as well.
   *
   * Pixels the layer does not draw are left untouched in dest. Drawn pixels
   * are dest = paletteRGBA[index] (copied verbatim, including alpha).
   *
   * Note (MAME behaviour, reproduced): in the normal mode, opaque writes every
   * pixel not hidden by the window mask, regardless of tile category; in the
   * split modes opaque still only draws tiles of the layer's category. A
   * disabled layer (vscroll bit 15) draws nothing even when opaque.
   */
  void DrawLayer(int layer, UINT32 *dest, int pitchInPixels, int width, int height, bool opaque, const UINT32 *paletteRGBA) const;

  // Direct access (debugging / integrator)
  const UINT16 *GetTileRAM() const { return m_tileRAM.data(); }
  const UINT16 *GetCharRAM() const { return m_charRAM.data(); }

  static constexpr UINT32 TILE_RAM_WORDS = 0x10000 / 2;
  static constexpr UINT32 CHAR_RAM_WORDS = 0x80000 / 2;

private:
  // Draws screen pixels x0..x1-1 of one line from 'tilemap', tilemap row ty,
  // tilemap column (x + scrollx) & 511. With a window mask line, pixels whose
  // mask bit (inverted if 'invert') is set are skipped. Tiles of another
  // category than 'category' are skipped unless 'anyCategory'.
  void DrawSpan(UINT32 *line, const UINT32 *pens, int tilemap, int ty, int x0, int x1, int scrollx,
                const UINT16 *maskLine, bool invert, int category, bool anyCategory, bool opaque) const;

  std::vector<UINT16> m_charRAM;
  std::vector<UINT16> m_tileRAM;
  UINT16 m_tileMask;
};

#endif // INCLUDED_MODEL1TILEMAP_H
