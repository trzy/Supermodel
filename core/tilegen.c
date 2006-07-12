/*
 * Sega Model 3 Emulator
 * Copyright (C) 2003 Bart Trzynadlowski, Ville Linde, Stefano Teso
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program (license.txt); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * tilegen.c
 *
 * Tilemap generator.
 *
 * Palette Notes:
 *      - Each palette entry occupies 32 bits. Only the second 16 bits are
 *        actually used.
 *      - The format of a 16-bit palette entry is: AGGGGGBBBBBRRRRR
 *
 * Register Notes:
 *
 *      0x20 -- Layer Colors:
 *
 *      31              23    20                                        0
 *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *      |-|-|-|-|-|-|-|-|D|C|B|A|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|
 *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *      D = Layer 0xFE000 color control (0 = 8-bit, 1 = 4-bit)
 *      C = Layer 0xFC000 color control (0 = 8-bit, 1 = 4-bit)
 *      B = Layer 0xFA000 color control (0 = 8-bit, 1 = 4-bit)
 *      A = Layer 0xF8000 color control (0 = 8-bit, 1 = 4-bit)
 */

#include "model3.h"

/******************************************************************/
/* Privates                                                       */
/******************************************************************/

/*
 * NOTE: It is critical that vram be set to NULL if it has not yet been
 * allocated because tilegen_set_layer_format() will try to use it to
 * recache the palette.
 */

static UINT8    *vram = NULL;   // 2D VRAM (passed to tilegen_init())
static UINT8    reg[0x100];     // tilemap generator registers

/*
 * Register Macros
 */

#define REG_LAYER_COLORS    0x20

/*
 * Pre-Decoded Palette Data
 *
 * Depending on the renderer color depth, the data is either 32-bit RGBA8 or
 * 16-bit RGB5A1. The actual order of the fields is obtained from the renderer
 * but it is guaranteed that for 16-bit modes, the color components will each
 * be 5 bits and the alpha will be 1 and for the 32-bit modes, all components
 * will be 8 bits.
 *
 * For alpha channel: All bits set means the color is opaque, all bits clear
 * means the color is transparent. No other bit combinations should be used.
 */

static UINT32 *pal;

/*
 * Layer Format
 *
 * Information that is passed by the renderer on the format of layers. Bit
 * position 0 is the LSB (none of that backwards IBM junk ;)) RGBA bit
 * positions give the position of the LSB of the field.
 */

static UINT     pitch;                  // layer pitch (in pixels)
static UINT     rpos, gpos, bpos, apos; // R, G, B, A bit positions
static INT      bpp;                    // 15 or 32 only

static int tilemap_is_dirty[4];
static int tilemap_dirty[4][64*64];
static int tilemap_depth[4][64*64];
static int tilemap_redraw[4];

/******************************************************************/
/* Rendering                                                      */
/******************************************************************/

/*
 * PUTPIXELx_xx():
 *
 * A handy macro used within the draw_tile_xbit_xx() macros to plot a pixel
 * and advance the buffer pointer.
 */

/*#define PUTPIXEL8_32(bp)                                    \
    do {                                                    \
        pixel = pal[((pattern >> bp) & 0xFF) | pal_bits];   \
        *buf++ = pixel;                                     \
    } while (0)

#define PUTPIXEL4_32(bp)                                    \
    do {                                                    \
        pixel = pal[((pattern >> bp) & 0xF) | pal_bits];    \
        *buf++ = pixel;                                     \
    } while (0)
*/

#define PUTPIXEL8(bp)										\
	do														\
	{														\
		*buf++ = ((pattern >> bp) & 0xff) | pal_bits;		\
	} while(0)

#define PUTPIXEL4(bp)										\
	do														\
	{														\
		*buf++ = ((pattern >> bp) & 0xf) | pal_bits;		\
	} while(0)

/*
 * draw_tile_8bit_32():
 *
 * Draws an 8-bit tile to a 32-bit layer buffer.
 */

static void draw_tile_8bit(UINT tile, UINT16 *buf)
{
    UINT    tile_offs;  // offset of tile within VRAM
    UINT    pal_bits;   // color palette bits obtained from tile
    UINT    y;
    UINT32  pattern;    // 4 pattern pixels fetched at once

    /*
     * Calculate tile offset; each tile occupies 64 bytes when using 8-bit
     * pixels
     */

    //tile_offs = tile & 0x3fff;
    //tile_offs *= 64;
	tile_offs = ((tile & 0x3fff) << 1) | ((tile >> 15) & 1);
    tile_offs *= 32;

    /*
     * Obtain upper color bits; the lower 8 bits come from the tile pattern
     */

    pal_bits = tile & 0x7F00;
    
    /*
     * Draw!
     */

    for (y = 0; y < 8; y++)
    {
        /*
         * Fetch first 4 pixels and draw them
         */

        pattern = *((UINT32 *) &vram[tile_offs]);
        tile_offs += 4;
        PUTPIXEL8(24);
        PUTPIXEL8(16);
        PUTPIXEL8(8);
        PUTPIXEL8(0);

        /*
         * Next 4
         */

        pattern = *((UINT32 *) &vram[tile_offs]);
        tile_offs += 4;
        PUTPIXEL8(24);
        PUTPIXEL8(16);
        PUTPIXEL8(8);
        PUTPIXEL8(0);

        /*
         * Move to the next line
         */

        buf += (pitch - 8); // next line in layer buffer
    }
}

/*
 * draw_tile_4bit_32():
 *
 * Draws a 4-bit tile to a 32-bit layer buffer.
 */

static void draw_tile_4bit(UINT tile, UINT16 *buf)
{
    UINT    tile_offs;  // offset of tile within VRAM
    UINT    pal_bits;   // color palette bits obtained from tile
    UINT    y;
    UINT32  pattern;    // 8 pattern pixels fetched at once

    /*
     * Calculate tile offset; each tile occupies 32 bytes when using 4-bit
     * pixels
     */

    tile_offs = ((tile & 0x3fff) << 1) | ((tile >> 15) & 1);
    tile_offs *= 32;

    /*
     * Obtain upper color bits; the lower 4 bits come from the tile pattern
     */

    pal_bits = tile & 0x7FF0;
    
    /*
     * Draw!
     */

    for (y = 0; y < 8; y++)
    {
        pattern = *((UINT32 *) &vram[tile_offs]);

        /*
         * Draw the 8 pixels we've fetched
         */

        PUTPIXEL4(28);
        PUTPIXEL4(24);
        PUTPIXEL4(20);
        PUTPIXEL4(16);
        PUTPIXEL4(12);
        PUTPIXEL4(8);
        PUTPIXEL4(4);
        PUTPIXEL4(0);

        /*
         * Move to the next line
         */

        tile_offs += 4;     // next tile pattern line
        buf += (pitch - 8); // next line in layer buffer
    }
}

/*
 * draw_layer_8bit_32():
 *
 * Draws an entire layer of 8-bit tiles to a 32-bit layer buffer.
 */

static void draw_layer_8bit(UINT16 *layer, int layer_num)
{
    int ty, tx;
	int tilenum;
    UINT32 tile;
	UINT32 addr = 0xf8000 + (layer_num * 0x2000);

	if (tilemap_is_dirty[layer_num] == 0)
	{
		return;
	}

	tilemap_is_dirty[layer_num] = 0;
    
	tilenum = 0;
    for (ty = 0; ty < 64; ty++)
    {
        for (tx = 0; tx < 64; tx+=2)
        {
			if (tilemap_redraw[layer_num] || tilemap_dirty[layer_num][tilenum+0])
			{
				tilemap_depth[layer_num][tilenum+0] = 0;
				tilemap_dirty[layer_num][tilenum+0] = 0;
				tile = *((UINT32 *) &vram[addr]) >> 16;

				draw_tile_8bit(tile & 0xffff, layer);
			}
			if (tilemap_redraw[layer_num] || tilemap_dirty[layer_num][tilenum+1])
			{
				tilemap_depth[layer_num][tilenum+1] = 0;
				tilemap_dirty[layer_num][tilenum+1] = 0;
				tile = *((UINT32 *) &vram[addr]) >> 0;

				draw_tile_8bit(tile & 0xffff, layer+8);
			}

			addr += 4;
            layer += 16;
			tilenum+=2;
        }

        //addr += (64 - 62) * 2;
        layer += (7 * pitch) + (pitch - 512);   // next tile row
    }

	tilemap_redraw[layer_num] = 0;
}

/*
 * draw_layer_4bit_32():
 *
 * Draws an entire layer of 4-bit tiles to a 32-bit layer buffer.
 */

static void draw_layer_4bit(UINT16 *layer, int layer_num)
{
    int ty, tx;
	int tilenum;
    UINT32 tile;
	UINT32 addr = 0xf8000 + (layer_num * 0x2000);

	if (tilemap_is_dirty[layer_num] == 0)
	{
		return;
	}

	tilemap_is_dirty[layer_num] = 0;
    
	tilenum = 0;
    for (ty = 0; ty < 64; ty++)
    {
        for (tx = 0; tx < 64; tx+=2)
        {
			if (tilemap_redraw[layer_num] || tilemap_dirty[layer_num][tilenum+0])
			{
				tilemap_depth[layer_num][tilenum+0] = 1;
				tilemap_dirty[layer_num][tilenum+0] = 0;
				tile = *((UINT32 *) &vram[addr]) >> 16;

				draw_tile_4bit(tile & 0xffff, layer);
			}
			if (tilemap_redraw[layer_num] || tilemap_dirty[layer_num][tilenum+1])
			{
				tilemap_depth[layer_num][tilenum+0] = 1;
				tilemap_dirty[layer_num][tilenum+1] = 0;
				tile = *((UINT32 *) &vram[addr]) >> 0;

				draw_tile_4bit(tile & 0xffff, layer+8);
			}

			addr += 4;
            layer += 16;
			tilenum+=2;
        }

        //addr += (64 - 62) * 2;
        layer += (7 * pitch) + (pitch - 512);   // next tile row
    }

	tilemap_redraw[layer_num] = 0;
}
    
/*
 * void tilegen_update(void);
 *
 * Renders up to 4 layers for the current frame.
 */

void tilegen_update(void)
{
    UINT8   *layer;
    UINT    layer_colors, layer_color_mask;
    FLAGS   layer_enable_mask;
    int     i, j;

    /*
     * Render layers
     */

	PROFILE_SECT_ENTRY("tilegen");

    layer_colors = BSWAP32(*(UINT32 *) &reg[REG_LAYER_COLORS]);
    layer_color_mask = 0x00100000;  // first layer color bit (moves left)
    layer_enable_mask = 1;

//    layer_colors = 0; // enable this to force 8-bit mode for VF3

	{
		UINT32 *palette;
		int pwidth;
		int ppitch;
		int pheight;
		int p = 0;

		osd_renderer_get_palette_buffer(&palette, &pwidth, &ppitch);

		pheight = 0x10000 / pwidth;
		for (j=0; j < 128; j++)
		{
			int pindex = j * ppitch;
			p = j * 256;
			for (i=0; i < 256; i++)
			{
				int a,r,g,b;
				UINT32 rgba;
				UINT16 pix;

				pix = pal[p];

				a = (pix & 0x8000) ? 0x00 : 0xff;
				b = (pix >> 10) & 0x1f;
				g = (pix >> 5) & 0x1f;
				r = (pix & 0x1f);
				b = (b << 3) | (b >> 2);
				g = (g << 3) | (g >> 2);
				r = (r << 3) | (r >> 2);

				rgba = (a << 24) | (r << 16) | (g << 8) | (b << 0);
				palette[pindex + i] = rgba;
				p++;

				// a very hackish way to counter the effect of using colours as texture coords
				if (i == 239)
					p--;
			}
		}

		osd_renderer_free_palette_buffer();
	}

	for (i = 0; i < 4; i++)
	{
		if ((m3_config.layer_enable & layer_enable_mask))
		{
			osd_renderer_get_layer_buffer(i, &layer, &pitch);

			if ((layer_colors & layer_color_mask))
			{
				draw_layer_4bit((UINT16 *) layer, i);
			}
			else
			{
				draw_layer_8bit((UINT16 *) layer, i);
            }            

			osd_renderer_free_layer_buffer(i);	
		}
		layer_color_mask <<= 1;
		layer_enable_mask <<= 1;
	}

	PROFILE_SECT_EXIT("tilegen");
}

/******************************************************************/
/* VRAM Access                                                    */
/******************************************************************/

static void mark_tilemap_dirty(int layer)
{
	tilemap_is_dirty[layer] = 1;
	tilemap_redraw[layer] = 1;
}

/*
 * UINT16 tilegen_vram_read_16(UINT32 addr);
 *
 * Reads a 16-bit word from VRAM.
 *
 * Parameters:
 *      addr = Address.
 *
 * Returns:
 *      Data read.
 */

UINT16 tilegen_vram_read_16(UINT32 addr)
{
    return(BSWAP16(*(UINT16 *)&vram[addr & 0x1FFFFF]));
}

/*
 * UINT32 tilegen_vram_read_32(UINT32 addr);
 *
 * Reads a 32-bit word from VRAM.
 *
 * Parameters:
 *      addr = Address.
 *
 * Returns:
 *      Data read.
 */

UINT32 tilegen_vram_read_32(UINT32 addr)
{
    return(BSWAP32(*(UINT32 *)&vram[addr & 0x1FFFFF]));
}

/*
 * void tilegen_vram_write_32(UINT32 addr, UINT32 data);
 *
 * Writes a 32-bit word to VRAM. Palette decoding is handled here.
 *
 * Parameters:
 *      addr = Address.
 *      data = Data.
 */

void tilegen_vram_write_32(UINT32 addr, UINT32 data)
{
    UINT    color;

	addr &= 0x1FFFFF;
	data = BSWAP32(data);   // return to its natural order (as on PPC)

	if (addr < 0xf8000)
	{
		*(UINT32 *)&vram[addr] = data;
	}
	else if (addr >= 0x0f8000 && addr < 0x100000)
	{
		UINT32 old_data;
		int ad, layer, layer_depth, old_depth;

		ad = addr - 0xf8000;
		layer = ad / 0x2000;

		old_data = *(UINT32 *)&vram[addr];
		layer_depth = (BSWAP32(*(UINT32 *) &reg[REG_LAYER_COLORS]) >> (20+layer)) & 1;
		old_depth = tilemap_depth[layer][((ad & 0x1fff) / 2)];
		
		if (data != old_data || layer_depth != old_depth)
		{
			*(UINT32 *)&vram[addr] = data;

			//ad = addr - 0xf8000;
			//layer = ad / 0x2000;
			tilemap_dirty[layer][((ad & 0x1fff) / 2) + 0] = 1;
			tilemap_dirty[layer][((ad & 0x1fff) / 2) + 1] = 1;
			tilemap_is_dirty[layer] = 1;
		}
	}
    else if (addr >= 0x100000 && addr < 0x120000)  // palette
    {
		*(UINT32 *)&vram[addr] = data;
        color = (addr - 0x100000) / 4;  // color number

		pal[color] = data;
    }
}

static void recache_palette(void)
{
    UINT32  i;

    for (i = 0x100000; i < 0x11FFFF; i += 4)
       tilegen_vram_write_32(i, BSWAP32(*(UINT32 *) &vram[i]));
}

/******************************************************************/
/* Tilegen I/O Port Access                                        */
/******************************************************************/

/*
 * UINT32 tilegen_read_32(UINT32 a);
 *
 * Reads a 32-bit word from the tilemap generator register area.
 *
 * Parameters:
 *      a = Address.
 *
 * Returns:
 *      Data read.
 */

UINT32 tilegen_read_32(UINT32 a)
{
//    LOG("MODEL3.LOG", "TILEGEN: %08X: read 32 %08X\n", PPC_PC, a);
    //message(0, "tilegen: %08X: read 32 %08X", PPC_PC, a);

    a &= 0xFF;

	switch(a)
	{

	case 0x00: // Status/Control
		// 0x20 (latch) controls layer pairing
		return(0 << 24);
	}

    return(BSWAP32(*(UINT32 *)&reg[a]));
}

/*
 * void tilegen_write_32(UINT32 a, UINT32 d);
 *
 * Writes a 32-bit word to the tilemap generator register area.
 *
 * Parameters:
 *      a = Address.
 *      d = Data.
 */

void tilegen_write_32(UINT32 a, UINT32 d)
{
	/* data is written as 32-bit, but only higher byte is used */

//    LOG("MODEL3.LOG", "TILEGEN: %08X: write 32 %08X = %08X\n", PPC_PC, a, d);

    a &= 0xFF;

	//if (a != 0x60 && a != 0x64 && a != 0x68 && a != 0x6c &&
	//	a != 0x10 && a != 0x0c && a != 0x40 && a != 0x44)
	//printf("tilegen: %08X, %08X\n", a, d);

	switch(a)
	{
	case 0x00:
	case 0x04:
	case 0x08:
	case 0x0C:
		break;
	case 0x10:
        model3_remove_irq((UINT8) (d >> 24));
		break;
	case 0x14:
	case 0x18:
	case 0x1C:
		break;
	case 0x20:
		{
			UINT32 olddata = *(UINT32 *)&reg[a];
			UINT32 newdata = BSWAP32(d);

			if ((olddata & 0x100000) != (newdata & 0x100000))
				mark_tilemap_dirty(0);
			if ((olddata & 0x200000) != (newdata & 0x200000))
				mark_tilemap_dirty(1);
			if ((olddata & 0x400000) != (newdata & 0x400000))
				mark_tilemap_dirty(2);
			if ((olddata & 0x800000) != (newdata & 0x800000))
				mark_tilemap_dirty(3);

			/**(UINT32 *)&reg[0x60] &= ~0x80000000;
			*(UINT32 *)&reg[0x64] &= ~0x80000000;
			*(UINT32 *)&reg[0x68] &= ~0x80000000;
			*(UINT32 *)&reg[0x6c] &= ~0x80000000;*/
			break;
		}
	case 0x24:
	case 0x40:
	case 0x44:
		break;
	case 0x60:
	case 0x64:
	case 0x68:
	case 0x6C:
		break;
	}

    *(UINT32 *)&reg[a] = BSWAP32(d);
}

/******************************************************************/
/* Set Up, Shut Down, and State Management                        */
/******************************************************************/

/*
 * void tilegen_save_state(FILE *fp);
 *
 * Saves the state of the tile generator to a file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void tilegen_save_state(FILE *fp)
{
    fwrite(vram, sizeof(UINT8), 1*1024*1024+2*65536, fp);
    fwrite(reg, sizeof(UINT8), 0x100, fp);
}

/*
 * void tilegen_load_state(FILE *fp);
 *
 * Loads the state of the tile generator from a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void tilegen_load_state(FILE *fp)
{
    fread(vram, sizeof(UINT8), 1*1024*1024+2*65536, fp);
    fread(reg, sizeof(UINT8), 0x100, fp);
    recache_palette();

	mark_tilemap_dirty(0);
	mark_tilemap_dirty(1);
	mark_tilemap_dirty(2);
	mark_tilemap_dirty(3);
}

/*
 * void tilegen_shutdown(void);
 *
 * Shuts down the tilemap generator.
 */

void tilegen_shutdown(void)
{
    vram = NULL;
	SAFE_FREE(pal);
  }

/*
 * void tilegen_init(UINT8 *vramptr);
 *
 * Initializes the tilemap generator.
 *
 * Parameters:
 *      vramptr = Pointer to 2D VRAM.
 */

void tilegen_init(UINT8 *vramptr)
{
    vram = vramptr;

	mark_tilemap_dirty(0);
	mark_tilemap_dirty(1);
	mark_tilemap_dirty(2);
	mark_tilemap_dirty(3);

	pal = malloc(sizeof(UINT32) * 65536);

	atexit(tilegen_shutdown);
}

BOOL tilegen_is_layer_enabled(int layer)
{
	if (layer == 1 && stricmp(m3_config.game_id, "lostwsga") == 0)
		return FALSE;

	switch (layer)
	{
		case 0:
			return *(UINT32 *)&reg[0x60] & 0x80000000 ? 1 : 0;
		case 1:
			return *(UINT32 *)&reg[0x64] & 0x80000000 ? 1 : 0;
		case 2:
			return *(UINT32 *)&reg[0x68] & 0x80000000 ? 1 : 0;
		case 3:
			return *(UINT32 *)&reg[0x6c] & 0x80000000 ? 1 : 0;
	}

	return 0;
}

UINT32 tilegen_get_layer_color_offset(int layer)
{
	switch (layer)
	{
		case 0:
		case 1:
		{
			INT8 r = (*(UINT32 *)&reg[0x40] >> 16) & 0xff;
			INT8 g = (*(UINT32 *)&reg[0x40] >>  8) & 0xff;
			INT8 b = (*(UINT32 *)&reg[0x40] >>  0) & 0xff;

			r += 127;
			g += 127;
			b += 127;

			return (r << 16) | (g << 8) | (b);
		}

		case 2:
		case 3:
		{
			UINT8 r = (*(UINT32 *)&reg[0x44] >> 16) & 0xff;
			UINT8 g = (*(UINT32 *)&reg[0x44] >>  8) & 0xff;
			UINT8 b = (*(UINT32 *)&reg[0x44] >>  0) & 0xff;
			
			r ^= 0x80;
			g ^= 0x80;
			b ^= 0x80;

			return (r << 16) | (g << 8) | (b);
		}
	}

	return 0;
}

UINT32 *tilegen_get_priority_buffer(void)
{
	return (UINT32 *)&vram[0xf7000];
}

BOOL tilegen_is_priority_enabled(void)
{
	UINT32 v = *(UINT32 *)&reg[0x20];
	return (v & 0x80) ? TRUE : FALSE;
}

UINT32 tilegen_get_layer_scroll_pos(int layer)
{
	UINT32 s = *(UINT32 *)&reg[0x60 + (layer*4)];
	return s & 0x7fff7fff;
}

/*
 * void tilegen_reset(void);
 *
 * Resets the tilemap generator. The registers are put into a reset state and
 * the palette is zeroed out.
 */

void tilegen_reset(void)
{
    memset(reg, 0xFF, 0x100);
    memset(pal, 0x00, 65536 * sizeof(UINT32));
}
