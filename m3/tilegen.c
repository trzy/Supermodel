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

static UINT32   pal[65536];

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

/******************************************************************/
/* Rendering                                                      */
/******************************************************************/

/*
 * PUTPIXELx_xx():
 *
 * A handy macro used within the draw_tile_xbit_xx() macros to plot a pixel
 * and advance the buffer pointer.
 */

#define PUTPIXEL8_16(bp)                                                    \
    do {                                                                    \
        pixel = *((UINT16 *) &pal[((pattern >> bp) & 0xFF) | pal_bits]);    \
        *buf++ = pixel;                                                     \
    } while (0)

#define PUTPIXEL8_32(bp)                                    \
    do {                                                    \
        pixel = pal[((pattern >> bp) & 0xFF) | pal_bits];   \
        *buf++ = pixel;                                     \
    } while (0)

#define PUTPIXEL4_16(bp)                                                \
    do {                                                                \
        pixel = *((UINT16 *) &pal[((pattern >> bp) & 0xF) | pal_bits]); \
        *buf++ = pixel;                                                 \
    } while (0)

#define PUTPIXEL4_32(bp)                                    \
    do {                                                    \
        pixel = pal[((pattern >> bp) & 0xF) | pal_bits];    \
        *buf++ = pixel;                                     \
    } while (0)


/*
 * draw_tile_8bit_16():
 *
 * Draws an 8-bit tile to a 16-bit layer buffer.
 */

static void draw_tile_8bit_16(UINT tile, UINT16 *buf)
{
    UINT    tile_offs;  // offset of tile within VRAM
    UINT    pal_bits;   // color palette bits obtained from tile
    UINT    y;
    UINT32  pattern;    // 4 pattern pixels fetched at once
    UINT16  pixel;      // a 16-bit RGBA pixel written to the layer buffer

    /*
     * Calculate tile offset; each tile occupies 64 bytes when using 8-bit
     * pixels
     */

    tile_offs = tile & 0x3fff;
    tile_offs *= 64;

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
        PUTPIXEL8_16(24);
        PUTPIXEL8_16(16);
        PUTPIXEL8_16(8);
        PUTPIXEL8_16(0);

        /*
         * Next 4
         */

        pattern = *((UINT32 *) &vram[tile_offs]);
        tile_offs += 4;
        PUTPIXEL8_16(24);
        PUTPIXEL8_16(16);
        PUTPIXEL8_16(8);
        PUTPIXEL8_16(0);

        /*
         * Move to the next line
         */

        buf += (pitch - 8); // next line in layer buffer
    }
}

/*
 * draw_tile_8bit_32():
 *
 * Draws an 8-bit tile to a 32-bit layer buffer.
 */

static void draw_tile_8bit_32(UINT tile, UINT32 *buf)
{
    UINT    tile_offs;  // offset of tile within VRAM
    UINT    pal_bits;   // color palette bits obtained from tile
    UINT    y;
    UINT32  pattern;    // 4 pattern pixels fetched at once
    UINT32  pixel;      // a 32-bit RGBA pixel written to the layer buffer

    /*
     * Calculate tile offset; each tile occupies 64 bytes when using 8-bit
     * pixels
     */

    tile_offs = tile & 0x3fff;
    tile_offs *= 64;

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
        PUTPIXEL8_32(24);
        PUTPIXEL8_32(16);
        PUTPIXEL8_32(8);
        PUTPIXEL8_32(0);

        /*
         * Next 4
         */

        pattern = *((UINT32 *) &vram[tile_offs]);
        tile_offs += 4;
        PUTPIXEL8_32(24);
        PUTPIXEL8_32(16);
        PUTPIXEL8_32(8);
        PUTPIXEL8_32(0);

        /*
         * Move to the next line
         */

        buf += (pitch - 8); // next line in layer buffer
    }
}

/*
 * draw_tile_4bit_16():
 *
 * Draws a 4-bit tile to a 16-bit layer buffer.
 */

static void draw_tile_4bit_16(UINT tile, UINT16 *buf)
{
    UINT    tile_offs;  // offset of tile within VRAM
    UINT    pal_bits;   // color palette bits obtained from tile
    UINT    y;
    UINT32  pattern;    // 8 pattern pixels fetched at once
    UINT16  pixel;      // a 16-bit RGBA pixel written to the layer buffer

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

        PUTPIXEL4_16(28);
        PUTPIXEL4_16(24);
        PUTPIXEL4_16(20);
        PUTPIXEL4_16(16);
        PUTPIXEL4_16(12);
        PUTPIXEL4_16(8);
        PUTPIXEL4_16(4);
        PUTPIXEL4_16(0);

        /*
         * Move to the next line
         */

        tile_offs += 4;     // next tile pattern line
        buf += (pitch - 8); // next line in layer buffer
    }
}

/*
 * draw_tile_4bit_32():
 *
 * Draws a 4-bit tile to a 32-bit layer buffer.
 */

static void draw_tile_4bit_32(UINT tile, UINT32 *buf)
{
    UINT    tile_offs;  // offset of tile within VRAM
    UINT    pal_bits;   // color palette bits obtained from tile
    UINT    y;
    UINT32  pattern;    // 8 pattern pixels fetched at once
    UINT32  pixel;      // a 32-bit RGBA pixel written to the layer buffer

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

        PUTPIXEL4_32(28);
        PUTPIXEL4_32(24);
        PUTPIXEL4_32(20);
        PUTPIXEL4_32(16);
        PUTPIXEL4_32(12);
        PUTPIXEL4_32(8);
        PUTPIXEL4_32(4);
        PUTPIXEL4_32(0);

        /*
         * Move to the next line
         */

        tile_offs += 4;     // next tile pattern line
        buf += (pitch - 8); // next line in layer buffer
    }
}

/*
 * draw_layer_8bit_16():
 *
 * Draws an entire layer of 8-bit tiles to a 16-bit layer buffer.
 */

static void draw_layer_8bit_16(UINT16 *layer, UINT addr)
{
    UINT    ty, tx;
    UINT32  tile;
    
    for (ty = 0; ty < 48; ty++)
    {
        for (tx = 0; tx < 62; tx += 2)          // 2 tiles at a time
        {          
            tile = *((UINT32 *) &vram[addr]);   // fetch 2 tiles (little endian)
            draw_tile_8bit_16(tile >> 16, layer);
            draw_tile_8bit_16(tile & 0xffff, layer + 8);

            addr += 4;
            layer += 16;
        }

        addr += (64 - 62) * 2;
        layer += (7 * pitch) + (pitch - 496);   // next tile row
    }
}

/*
 * draw_layer_8bit_32():
 *
 * Draws an entire layer of 8-bit tiles to a 32-bit layer buffer.
 */

static void draw_layer_8bit_32(UINT32 *layer, UINT addr)
{
    UINT    ty, tx;
    UINT32  tile;
    
    for (ty = 0; ty < 48; ty++)
    {
        for (tx = 0; tx < 62; tx += 2)          // 2 tiles at a time
        {          
            tile = *((UINT32 *) &vram[addr]);   // fetch 2 tiles (little endian)
            draw_tile_8bit_32(tile >> 16, layer);
            draw_tile_8bit_32(tile & 0xffff, layer + 8);

            addr += 4;
            layer += 16;
        }

        addr += (64 - 62) * 2;
        layer += (7 * pitch) + (pitch - 496);   // next tile row
    }
}

/*
 * draw_layer_4bit_16():
 *
 * Draws an entire layer of 4-bit tiles to a 16-bit layer buffer.
 */

static void draw_layer_4bit_16(UINT16 *layer, UINT addr)
{
    UINT    ty, tx;
    UINT32  tile;
    
    for (ty = 0; ty < 48; ty++)
    {
        for (tx = 0; tx < 62; tx += 2)          // 2 tiles at a time
        {          
            tile = *((UINT32 *) &vram[addr]);   // fetch 2 tiles (little endian)
            draw_tile_4bit_16(tile >> 16, layer);
            draw_tile_4bit_16(tile & 0xffff, layer + 8);

            addr += 4;
            layer += 16;
        }

        addr += (64 - 62) * 2;
        layer += (7 * pitch) + (pitch - 496);   // next tile row
    }
}

/*
 * draw_layer_4bit_32():
 *
 * Draws an entire layer of 4-bit tiles to a 32-bit layer buffer.
 */

static void draw_layer_4bit_32(UINT32 *layer, UINT addr)
{
    UINT    ty, tx;
    UINT32  tile;
    
    for (ty = 0; ty < 48; ty++)
    {
        for (tx = 0; tx < 62; tx += 2)          // 2 tiles at a time
        {          
            tile = *((UINT32 *) &vram[addr]);   // fetch 2 tiles (little endian)
            draw_tile_4bit_32(tile >> 16, layer);
            draw_tile_4bit_32(tile & 0xffff, layer + 8);

            addr += 4;
            layer += 16;
        }

        addr += (64 - 62) * 2;
        layer += (7 * pitch) + (pitch - 496);   // next tile row
    }
}
    
/*
 * void tilegen_update(void);
 *
 * Renders up to 4 layers for the current frame.
 */

void tilegen_update(void)
{
    UINT8   *layer;
    UINT    addr, layer_colors, layer_color_mask;
    FLAGS   layer_enable_mask;
    INT     i;

    /*
     * Render layers
     */

    addr = 0xF8000;                 // offset of first layer in VRAM
    layer_colors = BSWAP32(*(UINT32 *) &reg[REG_LAYER_COLORS]);
    layer_color_mask = 0x00100000;  // first layer color bit (moves left)
    layer_enable_mask = 1;

    if (bpp == 16)
    {
        for (i = 0; i < 4; i++)
        {
            osd_renderer_get_layer_buffer(i, &layer, &pitch);

            if ((m3_config.layer_enable & layer_enable_mask))
            {
                if ((layer_colors & layer_color_mask))
                    draw_layer_4bit_16((UINT16 *) layer, addr);
                else
                    draw_layer_8bit_16((UINT16 *) layer, addr);
            }
            else    // layer is disabled, fill with 0 (transparent, no data)
                memset(layer, 0, 384 * pitch * sizeof(UINT16));

			osd_renderer_free_layer_buffer(i);

            addr += 0x2000;
            layer_color_mask <<= 1;
            layer_enable_mask <<= 1;
        }
    }
    else
    {
        for (i = 0; i < 4; i++)
        {
            osd_renderer_get_layer_buffer(i, &layer, &pitch);

            if ((m3_config.layer_enable & layer_enable_mask))
            {
                if ((layer_colors & layer_color_mask))
                    draw_layer_4bit_32((UINT32 *) layer, addr);
                else
                    draw_layer_8bit_32((UINT32 *) layer, addr);
            }            
            else    // layer is disabled, fill with 0 (transparent, no data)
                memset(layer, 0, 384 * pitch * sizeof(UINT32));

			osd_renderer_free_layer_buffer(i);

            addr += 0x2000;
            layer_color_mask <<= 1;
            layer_enable_mask <<= 1;
        }
    }
}

#if 0   -- this version shows colors in palette to make sure its working
void tilegen_update(void)
{
    UINT8   *layer;
    INT     i, j, y, x, color, v;
    UINT32  pixel;

#define YRES 384
#define XRES 496
#define COLORS_PER_ROW  16
#define COLORS_PER_COL  32
#define BOX_SIZE        (YRES / COLORS_PER_COL)
    osd_renderer_get_layer_buffer(0, &layer, &pitch);

    color = 16384;
    for (i = 0; i < COLORS_PER_COL; i++)
    {
        v = i * BOX_SIZE * pitch * 4 + ((XRES - COLORS_PER_ROW * BOX_SIZE) / 2) * 4;

        for (j = 0; j < COLORS_PER_ROW; j++)
        {           
            pixel = pal[color++] | 0xff000000;

            for (y = 0; y < BOX_SIZE; y++)
            {
                for (x = 0; x < BOX_SIZE; x++)
                    *((UINT32 *) &layer[v + y * pitch * 4 + x * 4]) = pixel;
            }

            v += BOX_SIZE * 4;
        }
    }

}
#endif

/******************************************************************/
/* VRAM Access                                                    */
/******************************************************************/

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
    UINT32  rgba;
    UINT8   r, g, b, a;

    addr &= 0x1FFFFF;
    data = BSWAP32(data);   // return to its natural order (as on PPC)
    *(UINT32 *)&vram[addr] = data;

    if (addr >= 0x100000)  // palette
    {
        color = (addr - 0x100000) / 4;  // color number

        a = 0xFF * ((data >> 15) & 1);  // decode the RGBA (make alpha 0xFF or 0x00)
        a ^= 0xFF;                      // invert it (set means clear on Model 3)
        b = (data >> 10) & 0x1f;
        g = (data >> 5) & 0x1f;
        r = (data & 0x1f);

        rgba = (r << rpos) | (g << gpos) | (b << bpos);

        if (bpp == 16)  // 16-bit color
        {
            a &= 1;     // only 1 bit of alpha
            *((UINT16 *) &pal[color]) = (UINT16) (rgba | (a << apos));
        }
        else            // 32-bit color
            pal[color] = (rgba | (a << apos));
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

	switch(a)
	{
	case 0x00:
	case 0x04:
	case 0x08:
	case 0x0C:
		break;
	case 0x10:
        m3_remove_irq((UINT8) (d >> 24));
		break;
	case 0x14:
	case 0x18:
	case 0x1C:
	case 0x20:
	case 0x24:
	case 0x40:
	case 0x44:
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
}

/*
 * void tilegen_shutdown(void);
 *
 * Shuts down the tilemap generator.
 */

void tilegen_shutdown(void)
{
    vram = NULL;
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
}

/*
 * BOOL tilegen_set_layer_format(INT d, UINT r, UINT g, UINT b, UINT a);
 *
 * Describes the layer format. This can be changed at any time and causes the
 * palette to be re-cached.
 *
 * Only 16 and 32-bit color depths are supported. For 16-bit format, R, G, and
 * B must each be 5 bits and a 1-bit alpha component must be present. For
 * 32-bit format, every component must be 8 bits.
 *
 * For the 32-bit, the bit positions will automatically be adjusted by 3 to
 * map the colors from 5 bits to 8 bits. The alpha component is unaffected by
 * this.
 *
 * Parameters:
 *      d = Color depth. Must be 16 or 32.
 *      r = Bit position of the LSB of the red component in the final pixel
 *          word (ie., how far to shift left.)
 *      g = Bit position of the green component.
 *      b = Bit position of the blue component.
 *      a = Bit position of the alpha component.
 *
 * Returns:
 *      MODEL3_OKAY  = Success.
 *      MODEL3_ERROR = Unsupported color depth.
 */

BOOL tilegen_set_layer_format(INT d, UINT r, UINT g, UINT b, UINT a)
{
    if (d != 16 && d != 32)
        return MODEL3_ERROR;    // only 16 and 32-bit formats are supported

    bpp = d;
    rpos = r;
    gpos = g;
    bpos = b;
    apos = a;

    if (bpp == 32)
    {
        rpos += 3;
        gpos += 3;
        bpos += 3;
    }

    if (vram != NULL)
        recache_palette();

    return MODEL3_OKAY;
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
