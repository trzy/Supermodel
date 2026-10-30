// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Ported to Supermodel from MAME (src/mame/sega/segaic24.cpp: segas24_tile_device only).
/*
  Sega system24 hardware

  System24:
    The odd one out.  Medium resolution. Entirely ram-based, no
    graphics roms.  4-layer tilemap hardware in two pairs, selection
    on a 8-pixels basis.  Tile-based sprites(!) organised as a linked
    list.  The tilemap chip has been reused for model1 and model2,
    probably because they had it handy and it handles medium res.

  Port notes (framework-forced changes):
    - MAME renders four 64x64-tile (512x512) tilemap_t objects into cached
      pixmap/flagsmap bitmaps and blits them. Here every pixel is fetched on
      the fly from tile RAM + char RAM (TilemapPixel), which yields the same
      result as MAME's lazily-refreshed caches (tile_w marks the tile dirty,
      char_w marks the gfx dirty which forces a full tilemap refresh before
      the next draw), so no invalidation bookkeeping is needed.
    - tilemap_t::draw() is replaced by TilemapDraw(), the rgb32 draw_rect()
      by an equivalent per-pixel loop. The priority bitmap (lpri) is not
      produced: MAME's rgb32 draw_rect ignores it and Model 1 never reads it.
    - xhout/xvout callbacks are not ported (Model 1 does not use them).
*/

#include "Model1Tilemap.h"
#include "BlockFile.h"
#include "OSD/Logger.h"
#include <cstring>

/*
 * gfx layout (MAME char_layout): 8x8, 4bpp, planes {0,1,2,3}, x step 4 bits,
 * y step 32 bits, 32 bytes per char, xormask = NATIVE_ENDIAN_VALUE_LE_BE(8,0)
 * i.e. the bit stream is the big-endian view of the 16-bit words. Pixel x of
 * row y of char 'code' is therefore nibble (3 - (x & 3)) (counting from the
 * LSB) of word code*16 + y*2 + (x >> 2).
 */

CSys24Tilemap::CSys24Tilemap(UINT16 tileMask)
  : tile_mask(tileMask)
{
  char_ram = new UINT16[CHAR_RAM_WORDS];
  tile_ram = new UINT16[TILE_RAM_WORDS];
  memset(char_ram, 0, CHAR_RAM_WORDS * sizeof(UINT16));
  memset(tile_ram, 0, TILE_RAM_WORDS * sizeof(UINT16));
}

CSys24Tilemap::~CSys24Tilemap()
{
  delete[] char_ram;
  delete[] tile_ram;
}

void CSys24Tilemap::Reset()
{
  // MAME's segas24_tile_device has no device_reset(): nothing to do.
}

void CSys24Tilemap::SaveState(CBlockFile *StateFile)
{
  StateFile->NewBlock("Sys24 Tilemap", __FILE__);
  StateFile->Write(tile_ram, TILE_RAM_WORDS * sizeof(UINT16));
  StateFile->Write(char_ram, CHAR_RAM_WORDS * sizeof(UINT16));
}

void CSys24Tilemap::LoadState(CBlockFile *StateFile)
{
  if (Result::OKAY != StateFile->FindBlock("Sys24 Tilemap"))
  {
    ErrorLog("Unable to load System 24 tilemap state. Save state file is corrupt.");
    return;
  }
  StateFile->Read(tile_ram, TILE_RAM_WORDS * sizeof(UINT16));
  StateFile->Read(char_ram, CHAR_RAM_WORDS * sizeof(UINT16));
}

UINT16 CSys24Tilemap::ReadTile(UINT32 offset) const
{
  return tile_ram[offset & (TILE_RAM_WORDS - 1)];
}

UINT16 CSys24Tilemap::ReadChar(UINT32 offset) const
{
  return char_ram[offset & (CHAR_RAM_WORDS - 1)];
}

void CSys24Tilemap::WriteTile(UINT32 offset, UINT16 data, UINT16 mask)
{
  // COMBINE_DATA; MAME then marks tilemap tile dirty (not needed here)
  UINT16 *p = &tile_ram[offset & (TILE_RAM_WORDS - 1)];
  *p = (*p & ~mask) | (data & mask);
}

void CSys24Tilemap::WriteChar(UINT32 offset, UINT16 data, UINT16 mask)
{
  // COMBINE_DATA; MAME then marks the gfx element dirty (not needed here)
  UINT16 *p = &char_ram[offset & (CHAR_RAM_WORDS - 1)];
  *p = (*p & ~mask) | (data & mask);
}

// Equivalent of tile_info() + gfx decode + tilemap_t::tile_draw() for one pixel.
// MAME tilemaps: 0 = tile_info_0s (0x0000), 1 = 0w (0x1000), 2 = 1s (0x2000), 3 = 1w (0x3000),
// TILEMAP_SCAN_ROWS 64x64 of 8x8.
inline UINT16 CSys24Tilemap::TilemapPixel(int tmap, int tx, int ty, int *pen, int *category) const
{
  UINT16 val = tile_ram[(tmap << 12) | ((ty >> 3) << 6) | (tx >> 3)];
  UINT32 code = val & tile_mask;
  UINT16 color = (val >> 7) & 0xff;
  UINT16 w = char_ram[(code * 16 + (ty & 7) * 2 + ((tx & 7) >> 2)) & (CHAR_RAM_WORDS - 1)];
  int p = (w >> (12 - 4 * (tx & 3))) & 0xf;
  *pen = p;
  *category = (val & 0x8000) != 0;
  // gfx colorbase 0, granularity 16, 512 colours: palette_base = color*16
  return UINT16(color * 16 + p);
}

// tilemap_t::draw(screen, bitmap, cliprect, tpri | flags, lpri) for a non-flipped
// 512x512 tilemap with one scroll value. Blit condition (configure_blit_parameters):
// flagsmap & (CATEGORY_MASK | LAYER0) == (tpri | LAYER0), or with
// TILEMAP_DRAW_OPAQUE: flagsmap & CATEGORY_MASK == tpri.
void CSys24Tilemap::TilemapDraw(const DrawCtx &ctx, int tmap, int scrollx, int scrolly, int tpri,
                                int minX, int minY, int maxX, int maxY) const
{
  if (minX < 0) minX = 0;
  if (minY < 0) minY = 0;
  if (maxX > ctx.width - 1) maxX = ctx.width - 1;
  if (maxY > ctx.height - 1) maxY = ctx.height - 1;

  for (int y = minY; y <= maxY; y++)
  {
    int ty = (y + scrolly) & 511;
    UINT32 *dst = ctx.dest + (size_t)y * ctx.pitch;
    for (int x = minX; x <= maxX; x++)
    {
      int tx = (x + scrollx) & 511;
      int pen, cat;
      UINT16 idx = TilemapPixel(tmap, tx, ty, &pen, &cat);
      if (cat == tpri && (ctx.opaque || pen != 0))
        dst[x] = ctx.pens[idx];
    }
  }
}

// segas24_tile_device::draw_rect (rgb32). MAME walks the 4-word-per-line
// window mask in 128-pixel chunks with fast/clipped paths; all paths reduce to:
// for each pixel, mask bit (0x8000 >> ((x & 127) >> 3)) of word (x >> 7), inverted
// when 'win'; if clear, the pixel is drawn when its flags == tpri|LAYER0
// (non-zero pen and category == tpri) or unconditionally with OPAQUE.
void CSys24Tilemap::draw_rect(const DrawCtx &ctx, int tmap, const UINT16 *mask, UINT16 tpri, int win,
                              int sx, int sy, int xx1, int yy1, int xx2, int yy2) const
{
  // Supermodel addition: clip to the destination size (MAME always uses 496x384)
  int cx2 = xx2 < ctx.width ? xx2 : ctx.width;
  int cy2 = yy2 < ctx.height ? yy2 : ctx.height;

  for (int y = yy1; y < cy2; y++)
  {
    const UINT16 *mrow = mask + y * 4;
    int ty = (sy + (y - yy1)) & 511;
    UINT32 *dst = ctx.dest + (size_t)y * ctx.pitch;
    for (int x = xx1; x < cx2; x++)
    {
      UINT16 m = mrow[x >> 7];
      if (win)
        m = ~m;
      if (m & (0x8000 >> ((x & 127) >> 3)))
        continue;
      int tx = (sx + (x - xx1)) & 511;
      int pen, cat;
      UINT16 idx = TilemapPixel(tmap, tx, ty, &pen, &cat);
      if ((pen != 0 && cat == tpri) || ctx.opaque)
        dst[x] = ctx.pens[idx];
    }
  }
}

// segas24_tile_device::draw_common
void CSys24Tilemap::DrawLayer(int layer, UINT32 *dest, int pitchInPixels, int width, int height, bool opaque, const UINT32 *paletteRGBA) const
{
  DrawCtx ctx = { dest, pitchInPixels, width, height, opaque, paletteRGBA };

  // cliprect = (0,0)-(width-1,height-1)
  const int cl = 0, ct = 0, cr = width - 1, cb = height - 1;

  UINT16 hscr = tile_ram[0x5000+(layer >> 1)];
  UINT16 vscr = tile_ram[0x5004+(layer >> 1)];
  UINT16 ctrl = tile_ram[0x5004+((layer >> 1) & 2)];
  const UINT16 *mask = tile_ram + (layer & 4 ? 0x6800 : 0x6000);
  UINT16 tpri = layer & 1;

  layer >>= 1;

  // Layer disable
  if(vscr & 0x8000)
    return;

  if(ctrl & 0x6000) {
    // Special window/scroll modes
    if(layer & 1)
      return;

    // tile_layer[layer]/[layer|1]->set_scrolly(0, vscr & 0x1ff)
    int scrolly = vscr & 0x1ff;

    if(hscr & 0x8000) {
      const UINT16 *hscrtb = tile_ram + 0x4000 + 0x200*layer;

      switch((ctrl & 0x6000) >> 13) {
      case 1: {
        UINT16 v = (-vscr) & 0x1ff;
        if(!((-vscr) & 0x200))
          layer ^= 1;
        for(int y=ct; y<=cb; y++) {
          int l1 = layer;
          if(y >= v)
            l1 ^= 1;

          hscr = hscrtb[y];

          UINT16 h = hscr & 0x1ff;
          TilemapDraw(ctx, l1, -h, scrolly, tpri, cl, y, cr, y);
        }
        break;
      }
      case 2: case 3: {
        for(int y=ct; y<=cb; y++) {
          int c1max = cr, c2min = cl;
          int l1 = layer;

          hscr = hscrtb[y];

          UINT16 h = hscr & 0x1ff;

          if(c1max >= h)
            c1max = h-1;
          if(c2min < h)
            c2min = h;
          if(!(hscr & 0x200))
            l1 ^= 1;

          TilemapDraw(ctx, l1,   -h, scrolly, tpri, cl,    y, c1max, y);
          TilemapDraw(ctx, l1^1, -h, scrolly, tpri, c2min, y, cr,    y);
        }
        break;
      }
      }

    } else {
      int scrollx = -(hscr & 0x1ff);

      switch((ctrl & 0x6000) >> 13) {
      case 1: {
        int c1max = cb, c2min = ct;
        UINT16 v = (-vscr) & 0x1ff;
        if(c1max >= v)
          c1max = v-1;
        if(c2min < v)
          c2min = v;
        if(!((-vscr) & 0x200))
          layer ^= 1;

        TilemapDraw(ctx, layer,   scrollx, scrolly, tpri, cl, ct,    cr, c1max);
        TilemapDraw(ctx, layer^1, scrollx, scrolly, tpri, cl, c2min, cr, cb);
        break;
      }
      case 2: case 3: {
        int c1max = cr, c2min = cl;
        UINT16 h = (+hscr) & 0x1ff;
        if(c1max >= h)
          c1max = h-1;
        if(c2min < h)
          c2min = h;
        if(!((+hscr) & 0x200))
          layer ^= 1;

        TilemapDraw(ctx, layer,   scrollx, scrolly, tpri, cl,    ct, c1max, cb);
        TilemapDraw(ctx, layer^1, scrollx, scrolly, tpri, c2min, ct, cr,    cb);
        break;
      }
      }
    }

  } else {
    int win = layer & 1;
    int tm = layer;   // MAME: bm/tm = tile_layer[layer]->pixmap()/flagsmap()

    if(hscr & 0x8000) {
      const UINT16 *hscrtb = tile_ram + 0x4000 + 0x200*layer;
      vscr &= 0x1ff;

      for(int y=0; y<384; y++) {
        hscr = (-hscrtb[y]) & 0x1ff;
        if(hscr + 496 <= 512) {
          // Horizontal split unnecessary
          draw_rect(ctx, tm, mask, tpri, win, hscr, vscr,        0,        y,      496,      y+1);
        } else {
          // Horizontal split necessary
          draw_rect(ctx, tm, mask, tpri, win, hscr, vscr,        0,        y, 512-hscr,      y+1);
          draw_rect(ctx, tm, mask, tpri, win,    0, vscr, 512-hscr,        y,      496,      y+1);
        }
        vscr = (vscr + 1) & 0x1ff;
      }
    } else {
      hscr = (-hscr) & 0x1ff;
      vscr = (+vscr) & 0x1ff;

      if(hscr + 496 <= 512) {
        // Horizontal split unnecessary
        if(vscr + 384 <= 512) {
          // Vertical split unnecessary
          draw_rect(ctx, tm, mask, tpri, win, hscr, vscr,        0,        0,      496,      384);
        } else {
          // Vertical split necessary
          draw_rect(ctx, tm, mask, tpri, win, hscr, vscr,        0,        0,      496, 512-vscr);
          draw_rect(ctx, tm, mask, tpri, win, hscr,    0,        0, 512-vscr,      496,      384);
        }
      } else {
        // Horizontal split necessary
        if(vscr + 384 <= 512) {
          // Vertical split unnecessary
          draw_rect(ctx, tm, mask, tpri, win, hscr, vscr,        0,        0, 512-hscr,      384);
          draw_rect(ctx, tm, mask, tpri, win,    0, vscr, 512-hscr,        0,      496,      384);
        } else {
          // Vertical split necessary
          draw_rect(ctx, tm, mask, tpri, win, hscr, vscr,        0,        0, 512-hscr, 512-vscr);
          draw_rect(ctx, tm, mask, tpri, win,    0, vscr, 512-hscr,        0,      496, 512-vscr);
          draw_rect(ctx, tm, mask, tpri, win, hscr,    0,        0, 512-vscr, 512-hscr,      384);
          draw_rect(ctx, tm, mask, tpri, win,    0,    0, 512-hscr, 512-vscr,      496,      384);
        }
      }
    }
  }
}
