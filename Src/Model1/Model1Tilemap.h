// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Ported to Supermodel from MAME (src/mame/sega/segaic24.cpp, segaic24.h: segas24_tile_device only).
/*
  Sega System 24 tilemap chip (315-5292), as reused by Sega Model 1 for its
  2D layers.

  Software renderer replacing MAME's tilemap_t / bitmap path. It produces the
  same pixels MAME's segas24_tile_device::draw() (rgb32 variant) produces.

  Memory (word offsets, as MAME's tile_r/tile_w/char_r/char_w):
    Tile RAM: 0x8000 words  (Model 1: 0x700000-0x70FFFF, offset = (addr & 0xFFFF) >> 1)
    Char RAM: 0x40000 words (Model 1: 0x780000-0x7FFFFF, offset = (addr & 0x7FFFF) >> 1)

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

class CBlockFile;

class CSys24Tilemap
{
public:
  // MAME: S24TILE(config, m_tiles, 0, 0x3fff) on Model 1
  explicit CSys24Tilemap(UINT16 tileMask = 0x3FFF);
  ~CSys24Tilemap();

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
   * opaque ? TILEMAP_DRAW_OPAQUE : 0) with cliprect = (0,0)-(width-1,height-1).
   * Model 1 screen is 496x384; pass width=496, height=384 for exact MAME output
   * (MAME's normal-mode path hard-codes 496x384; here it is additionally clipped
   * to width/height so a smaller buffer is safe).
   *
   * Pixels the layer does not draw are left untouched in dest. Drawn pixels
   * are dest = paletteRGBA[index] (copied verbatim, including alpha).
   *
   * Note (MAME behaviour, reproduced): in the normal path, opaque writes every
   * pixel not hidden by the window mask, regardless of tile category; in the
   * special window/scroll modes (ctrl & 0x6000) opaque still only draws tiles
   * whose category matches layer & 1. A disabled layer (vscroll bit 15) draws
   * nothing even when opaque.
   */
  void DrawLayer(int layer, UINT32 *dest, int pitchInPixels, int width, int height, bool opaque, const UINT32 *paletteRGBA) const;

  // Direct access (debugging / integrator)
  const UINT16 *GetTileRAM() const { return tile_ram; }
  const UINT16 *GetCharRAM() const { return char_ram; }

  static constexpr UINT32 TILE_RAM_WORDS = 0x10000 / 2;
  static constexpr UINT32 CHAR_RAM_WORDS = 0x80000 / 2;

private:
  struct DrawCtx
  {
    UINT32 *dest;
    int pitch;
    int width;
    int height;
    bool opaque;
    const UINT32 *pens;
  };

  // Fetch one pixel of 512x512 tilemap 'tmap' (0..3) at (tx,ty):
  // returns palette index; sets *pen (0..15) and *category (0/1)
  inline UINT16 TilemapPixel(int tmap, int tx, int ty, int *pen, int *category) const;

  // Replacement for tilemap_t::draw() on a single-scroll 512x512 tilemap:
  // tilemap x = (x + scrollx) & 511, y = (y + scrolly) & 511 (MAME semantics of
  // set_scrollx/set_scrolly with dx=dy=0). Clip rect is inclusive.
  void TilemapDraw(const DrawCtx &ctx, int tmap, int scrollx, int scrolly, int tpri,
                   int minX, int minY, int maxX, int maxY) const;

  // Replacement for segas24_tile_device::draw_rect() (rgb32 variant)
  void draw_rect(const DrawCtx &ctx, int tmap, const UINT16 *mask, UINT16 tpri, int win,
                 int sx, int sy, int xx1, int yy1, int xx2, int yy2) const;

  UINT16 *char_ram;
  UINT16 *tile_ram;
  UINT16 tile_mask;
};

#endif // INCLUDED_MODEL1TILEMAP_H
