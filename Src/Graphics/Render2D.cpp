/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2012 Bart Trzynadlowski, Nik Henson 
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
 * Render2D.cpp
 *
 * Implementation of the CRender2D class: OpenGL tile generator graphics. 
 *
 * To-Do List
 * ----------
 * - Is there a better way to handle the overscan regions in wide screen mode? 
 *   Is clearing two thin viewports better than one big clear?
 * - Layer priorities in Spikeout attract mode might not be totally correct.
 * - Are v-scroll values 9 or 10 bits? (Does it matter?) Lost World seems to
 *   have some scrolling issues.
 * - A proper shut-down function is needed! OpenGL might not be available when
 *   the destructor for this class is called.
 *
 * Tile Generator Hardware Overview
 * --------------------------------
 *
 * Model 3's medium resolution tile generator hardware appears to be derived
 * from the Model 2 and System 24 chipset, but is much simpler. It consists of
 * four 64x64 tile layers, comprised of 8x8 pixel tiles, with configurable 
 * priorities. There may be additional features but so far, no known Model 3 
 * games use them.
 *
 * VRAM is comprised of 1 MB for tile data and an additional 128 KB for the 
 * palette (each color occupies 32 bits). The four tilemap layers are referred
 * to as: A (0), A' (1), B (2), and B' (3). Palette RAM may be located on a 
 * separate RAM IC.
 *
 * Registers
 * ---------
 *
 * Registers are listed by their byte offset in the PowerPC address space. Each
 * is 32 bits wide and little endian. Only those registers relevant to 
 * rendering are listed here (see CTileGen for others).
 *
 *		Offset:		Description:
 *
 *		0x20		Layer configuration
 *		0x40		Layer A/A' color offset
 *		0x44		Layer B/B' color offset
 *		0x60		Layer A scroll
 *		0x64		Layer A' scroll
 *		0x68		Layer B scroll
 *		0x6C		Layer B' scroll
 *
 * Layer configuration is formatted as:
 *
 *		31                                     0
 *		 ???? ???? ???? ???? pqrs tuvw ???? ????
 *
 * Bits 'pqrs' control the color depth of layers B', B, A', and A, 
 * respectively. If set, the layer's pattern data is encoded as 4 bits,
 * otherwise the pixels are 8 bits. Bits 'tuvw' form a 4-bit priority code. The
 * other bits are unused or unknown.
 *
 * The remaining registers are described where appropriate further below.
 *
 * VRAM Memory Map
 * ---------------
 *
 * The lower 1 MB of VRAM is used for storing tiles, per-line horizontal scroll
 * values, and the stencil mask, which determines which of each pair of layers
 * is displayed on a given line and column.
 *
 *		00000-F5FFF		Tile pattern data
 * 		F6000-F63FF		Layer A horizontal scroll table (512 lines)
 *		F6400-F67FF		Layer A' horizontal scroll table	
 *		F6800-F6BFF		Layer B horizontal scroll table
 *		F6C00-F6FFF		Layer B' horizontal scroll table
 *		F7000-F77FF		Mask table (assuming 4 bytes per line, 512 lines)
 *		F7800-F7FFF		?
 *		F8000-F9FFF		Layer A name table
 *		FA000-FBFFF		Layer A' name table
 *		FC000-FDFFF		Layer B name table
 *		FE000-FFFFF		Layer B' name table
 *
 * Tiles may actually address the entire 1 MB space, although in practice,
 * that would conflict with the other fixed memory regions.
 * 
 * Palette
 * -------
 *
 * The palette stores 32768 colors. Each entry is a little endian 32-bit word.
 * The upper 16 bits are unused and the lower 16 bits contain the color:
 *
 *		15                 0
 *		 tbbb bbgg gggr rrrr
 *
 * The 't' bit is for transparency. When set, pixels of that color are
 * transparent, unless they are the bottom-most layer.
 *
 * Tile Name Table and Pattern Layout
 * ----------------------------------
 *
 * The name table is a 64x64 array of 16-bit words serving as indices for tile
 * pattern data and the palette. The first 64 words correspond to the first 
 * row of tiles, the next 64 to the second row, etc. Although 64x64 entries
 * describes a 512x512 pixel screen, only the upper-left 62x48 tiles are
 * visible when the vertical and horizontal scroll values are 0. Scrolling 
 * moves the 496x384 pixel 'window' around, with individual wrapping of the
 * two axes.
 *
 * The data is actually arranged in 32-bit chunks in little endian format, so 
 * that tiles 0, 1, 2, and 3 will be stored as 1, 0, 3, 2. Fetching two name 
 * table entries as a single 32-bit word places the left tile in the high 16 
 * bits and the right tile in the low 16 bits.
 *
 * The format of a name table entry in 4-bit color mode is:
 *
 *		15                 0
 *		 jkpp pppp pppp iiii
 *
 * The pattern index is '0ppp pppp pppi iiij'. Multiplying by 32 yields the
 * offset in VRAM at which the tile pattern data is stored. Note that the MSB 
 * of the name table entry becomes the LSB of the pattern index. This allows 
 * for 32768 4-bit tile patterns, each occupying 32 bytes, which means the 
 * whole 1 MB VRAM space can be addressed.
 *
 * The 4-bit pattern data is stored as 8 32-bit words. Each word stores a row
 * of 8 pixels:
 *
 *		31                                     0
 *		 aaaa bbbb cccc dddd eeee ffff gggg hhhh
 *
 * 'a' is the left-most pixel data. These 4-bit values are combined with bits
 * from the name table to form a palette index, which determines the final
 * color. For example, for pixel 'a', the 15-bit color index is:
 *		
 *      14                0
 *		 kpp pppp pppp aaaa
 *
 * Note that index bits are re-used to form the palette index, meaning that
 * the pattern address partly determines the color.
 *
 * In 8-bit color mode, the name table entry looks like:
 *
 *		15                 0
 *		 ?ppp pppp iiii iiii
 *
 * The low 15 'p' and 'i' bits together form the pattern index, which must be 
 * multiplied by 64 to get the offset. The pattern data now consists of 16 32-
 * bit words, each containing four 8-bit pixels:
 *
 *		31                                     0
 *		 aaaa aaaa bbbb bbbb cccc cccc dddd dddd
 *
 * 'a' is the left-most pixel. Each line is therefore comprised of two 32-bit
 * words. The palette index for pixel 'a' is now formed from:
 *
 *		14                0
 *		 ppp pppp aaaa aaaa
 *
 * Stencil Mask 
 * ------------
 *
 * For any pixel position, there are in fact only two visible layers, despite
 * there being four defined layers. The layers are grouped in pairs: A (the
 * 'primary' layer) and A' (the 'alternate') form one pair, and B and B' form
 * the other. Only one of the primary or alternate layers from each group may
 * be visible at a given position. The 'stencil mask' controls this.
 *
 * The mask table is a bit field organized into 512 (or 384?) lines with each
 * bit controlling four columns (32 pixels). The mask does not appear to be
 * affected by scrolling -- that is, it does not scroll with the underlying
 * tiles, which do so independently. The mask remains fixed. Caveat: a bug in
 * Scud Race's 'ROLLING START' animation may indicate this is either not 
 * strictly true or that the upper-left corner of the mask needs to be adjusted
 * slightly. This bug has not been investigated thoroughly yet.
 *
 * Each mask entry is a little endian 32-bit word. The high 16 bits control
 * A/A' and the low 16 bits control B/B'. Each word controls an entire line
 * (32 pixels per bit, 512 pixels per 16-bit line mask). If a bit is set to 1,
 * the pixel from the primary layer is used, otherwise the alternate layer is
 * used when the mask is 0. It is important to remember that the layers may
 * have been scrolled independently. The mask operates on the final resultant
 * two pixels that are determined for each location.
 *
 * Example of a line mask:
 *
 *		31                  15                 0
 *		 0111 0000 0000 1111 0000 0000 1111 1111
 *
 * These settings would display layer A' for the first 32 pixels of the line,
 * followed by layer A for the next 96 pixels, A' for the subsequent 256 
 * pixels, and A for the final 128 pixels. The first 256 pixels of the line
 * would display layer B' and the second 256 pixels would be from layer B.
 *
 * The stencil mask does not affect layer priorities, which are managed 
 * separately regardless of mask settings.
 *
 * Scrolling
 * ---------
 *
 * Each of the four layers can be scrolled independently. Vertical scroll
 * values are stored in the appropriate scroll register and horizontal scroll
 * values can be sourced either from the register (in which case the entire
 * layer will be scrolled uniformly) or from a table in VRAM (which contains
 * independent values for each line). 
 *
 * The scroll registers are laid out as:
 *
 *		31                                     0
 *		 v??? ???y yyyy yyyy h??? ??xx xxxx xxxx
 *
 * The 'y' bits comprise a vertical scroll value in pixels. The 'x' bits form a
 * horizontal scroll value. If 'h' is set, then the VRAM table (line-by-line 
 * scrolling) is used, otherwise the 'x' values are applied to every line. The
 * meaning of 'v' is unknown. It is also possible that the scroll values use
 * more or less bits, but probably no more than 1.
 *
 * Each line must be wrapped back to the beginning of the same line. Likewise,
 * vertical scrolling wraps around back to the top of the tilemap.
 *
 * The horizontal scroll table is a series of 16-bit little endian words, one
 * for each line beginning at 0. It appears all the values can be used for
 * scrolling (no control bits have been observed). The number of bits actually
 * used by the hardware is irrelevant -- wrapping has the effect of making 
 * higher order bits unimportant.
 *
 * Layer Priorities
 * ----------------
 *
 * The layer control register (0x20) contains 4 bits that appear to control
 * layer priorities. It is assumed that the 3D graphics, output by the Real3D
 * pixel processors independently of the tile generator, constitute their own
 * 'layer' and that the 2D tilemaps appear in front or behind. There may be a
 * specific function for each priority bit or the field may be interpreted as a
 * single 4-bit value denoting preset layer orders.
 *
 * Color Offsets
 * -------------
 *
 * Color offsets can be applied to the final RGB color value of every pixel.
 * This is used for effects such as fading to a certain color, lightning (Lost
 * World), etc. The current best guess is that the two registers control each
 * pair (A/A' and B/B') of layers. The format appears to be:
 *
 *		31                                     0
 *		 ???? ???? rrrr rrrr gggg gggg bbbb bbbb
 *
 * Where 'r', 'g', and 'b' appear to be signed 8-bit color offsets. Because
 * they exceed the color resolution of the palette, they must be scaled
 * appropriately.
 *
 * Color offset registers are handled in TileGen.cpp. Two palettes are computed
 * -- one for A/A' and another for B/B'. These are passed to the renderer.
 */

#include <cstring>
#include "Pkgs/glew.h"
#include "Supermodel.h"
#include "Graphics/Shaders2D.h"	// fragment and vertex shaders


/******************************************************************************
 Definitions and Constants
******************************************************************************/

// Shader program files (for use in development builds only)
#define VERTEX_2D_SHADER_FILE	"Src/Graphics/Vertex2D.glsl"
#define FRAGMENT_2D_SHADER_FILE	"Src/Graphics/Fragment2D.glsl"


/******************************************************************************
 Tile Drawing Functions
******************************************************************************/

// Draw 4-bit tile line, no clipping performed
void CRender2D::DrawTileLine4BitNoClip(UINT32 *buf, UINT16 tile, int tileLine, const UINT32 *pal)
{
    unsigned	tileOffset;	// offset of tile pattern within VRAM
    unsigned	palette;   	// color palette bits obtained from tile
    UINT32  	pattern;    // 8 pattern pixels fetched at once

	// Tile pattern offset: each tile occupies 32 bytes when using 4-bit pixels
    tileOffset = ((tile&0x3FFF)<<1) | ((tile>>15)&1);
    tileOffset *= 32;
    tileOffset /= 4;	// VRAM is a UINT32 array

	// Upper color bits; the lower 4 bits come from the tile pattern
	palette = tile&0x7FF0;
    
    // Draw 8 pixels
    pattern = vram[tileOffset+tileLine];
    *buf++ = pal[((pattern>>28)&0xF) | palette];
    *buf++ = pal[((pattern>>24)&0xF) | palette];
    *buf++ = pal[((pattern>>20)&0xF) | palette];
    *buf++ = pal[((pattern>>16)&0xF) | palette];
    *buf++ = pal[((pattern>>12)&0xF) | palette];
    *buf++ = pal[((pattern>>8)&0xF) | palette];
    *buf++ = pal[((pattern>>4)&0xF) | palette];
    *buf++ = pal[((pattern>>0)&0xF) | palette];
}

// Draw 8-bit tile line, no clipping performed
void CRender2D::DrawTileLine8BitNoClip(UINT32 *buf, UINT16 tile, int tileLine, const UINT32 *pal)
{
    unsigned	tileOffset;	// offset of tile pattern within VRAM
    unsigned	palette;   	// color palette bits obtained from tile
    UINT32  	pattern;    // 4 pattern pixels fetched at once

	tileLine *= 2;	// 8-bit pixels, each line is two words
	
	// Tile pattern offset: each tile occupies 64 bytes when using 8-bit pixels
    tileOffset = tile&0x3FFF;
    tileOffset *= 64;
    tileOffset /= 4;

	// Upper color bits
	palette = tile&0x7F00;
    
    // Draw 4 pixels at a time
    pattern = vram[tileOffset+tileLine];
    *buf++ = pal[((pattern>>24)&0xFF) | palette];
    *buf++ = pal[((pattern>>16)&0xFF) | palette];
    *buf++ = pal[((pattern>>8)&0xFF) | palette];
    *buf++ = pal[((pattern>>0)&0xFF) | palette];
    pattern = vram[tileOffset+tileLine+1];
    *buf++ = pal[((pattern>>24)&0xFF) | palette];
    *buf++ = pal[((pattern>>16)&0xFF) | palette];
    *buf++ = pal[((pattern>>8)&0xFF) | palette];
    *buf++ = pal[((pattern>>0)&0xFF) | palette];
}


/******************************************************************************
 Layer Rendering
******************************************************************************/

/*
 * DrawLine():
 *
 * Draws a single scanline of single layer. Vertical (but not horizontal)
 * scrolling is applied here.
 *
 * Parametes:
 *		dest				Destination of 512-pixel output buffer to draw to.
 *		layerNum			Layer number:
 *								0 = Layer A		(@ 0xF8000)
 *								1 = Layer A'	(@ 0xFA000)
 *								2 = Layer B		(@ 0xFC000)
 *								3 = Layer B'	(@ 0xFE000)
 *		y					Line number (0-495).
 *		nameTableBase		Pointer to VRAM name table (see above addresses) 
 *							for this layer.
 *		hScrollTable		Pointer to the line-by-line horizontal scroll value
 *							table for this layer.
 *		pal					Palette to draw with.
 */
void CRender2D::DrawLine(UINT32 *dest, int layerNum, int y, const UINT16 *nameTableBase, const UINT32 *pal)
{
	// Determine the layer color depth (4 or 8-bit pixels)
	bool is4Bit = regs[0x20/4] & (1<<(12+layerNum));
	
	// Compute offsets due to vertical scrolling
	int 		 vScroll    = (regs[0x60/4+layerNum]>>16)&0x1FF;
	const UINT16 *nameTable = &nameTableBase[(64*((y+vScroll)/8)) & 0xFFF];	// clamp to 64x64=0x1000
	int          vOffset    = (y+vScroll)&7;	// vertical pixel offset within 8x8 tile
	
	// Render 512 pixels (64 tiles) w/out any horizontal scrolling or masking
	if (is4Bit)
	{
		for (int tx = 0; tx < 64; tx += 4)
		{
			// Little endian: offsets 0,1,2,3 become 1,0,3,2
			DrawTileLine4BitNoClip(dest, nameTable[1], vOffset, pal);	dest += 8;
			DrawTileLine4BitNoClip(dest, nameTable[0], vOffset, pal);	dest += 8;
			DrawTileLine4BitNoClip(dest, nameTable[3], vOffset, pal);	dest += 8;
			DrawTileLine4BitNoClip(dest, nameTable[2], vOffset, pal);	dest += 8;
			nameTable += 4;	// next set of 4 tiles
		}
	}
	else
	{
		for (int tx = 0; tx < 64; tx += 4)
		{
			DrawTileLine8BitNoClip(dest, nameTable[1], vOffset, pal);	dest += 8;
			DrawTileLine8BitNoClip(dest, nameTable[0], vOffset, pal);	dest += 8;
			DrawTileLine8BitNoClip(dest, nameTable[3], vOffset, pal);	dest += 8;
			DrawTileLine8BitNoClip(dest, nameTable[2], vOffset, pal);	dest += 8;
			nameTable += 4;
		}
	}				
}

// Mix in the appropriate layer (add on top of current contents) with horizontal scrolling under control of the stencil mask
static void MixLine(UINT32 *dest, const UINT32 *src, int layerNum, int y, bool isBottom, const UINT16 *hScrollTable, const UINT16 *maskTableLine, int hFullScroll, bool lineScrollMode)
{
	// Determine horizontal scroll values
	int hScroll;
	if (lineScrollMode)
		hScroll = hScrollTable[y];
	else
		hScroll = hFullScroll;
		
	// Get correct mask table entry
	if (layerNum < 2)	// little endian: layers A and A' use second word in each pair
		++maskTableLine;
		
	// Figure out what mask bit should be to mix in this layer
	UINT16 doCopy;
	if ((layerNum & 1))		// layers 1 and 3 are A' and B': alternates
		doCopy = 0x0000;	// if mask is clear, copy alternate layer
	else
		doCopy = 0x8000;	// copy primary layer when mask is set
			
	// Mix first 60 tiles (4 at a time)
	UINT16 mask = *maskTableLine;	// mask for this line (each bit covers 4 tiles)
	int    i    = hScroll&511;		// line index (where to copy from)
	if (isBottom)
	{
		/*
		 * Bottom layers can be copied in without worrying about transparency 
		 * but we must write blank values when layer is not showing.
		 */
		for (int tx = 0; tx < 60; tx += 4)
		{
			// Only copy pixels if the mask bit is appropriate for this layer type
			if ((mask&0x8000) == doCopy)
			{
				if (i <= (512-32))	// safe to use memcpy for fast blit?
				{
					memcpy(dest, &src[i], 32*sizeof(UINT32));
					i += 32;
					dest += 32;
				}
				else				// slow copy, wrap line boundary
				{
					for (int k = 0; k < 32; k++)
					{
						i &= 511;
						*dest++ = src[i++];
					}
				}
			}
			else
			{
				// Write blank pixels
				memset(dest, 0, 32*sizeof(UINT32));
				i += 32;
				i &= 511;	// wrap line boundaries
				dest += 32;
			}
			
			mask <<= 1;
		}
		
		// Mix last two tiles
		if ((mask&0x8000) == doCopy)
		{
			for (int k = 0; k < 16; k++)
			{
				i &= 511;
				*dest++ = src[i++];
			}
		}
		else	// clear
		{
			for (int k = 0; k < 16; k++)
			{
				i &= 511;
				*dest++ = 0;
			}
		}
	}
	else
	{
		/* 
		 * Subsequent layers must test for transparency while mixing.
		 */
		for (int tx = 0; tx < 60; tx += 4)
		{
			if ((mask&0x8000) == doCopy)
			{
				UINT32	p;
				for (int k = 0; k < 32; k++)
				{
					i &= 511;
					p = src[i++];
					if ((p>>24) != 0)	// opaque pixel, put it down
						*dest = p;
					dest++;
				}
			}
			else
			{
				i += 32;
				i &= 511;
				dest += 32;
			}
				
			mask <<= 1;
		}
		
		if ((mask&0x8000) == doCopy)
		{
			UINT32	p;
			for (int k = 0; k < 16; k++)
			{
				i &= 511;
				p = src[i++];
				if ((p>>24) != 0)
					*dest = p;
				dest++;
			}
		}
	}
}

// Returns true if there is no bottom layer (requiring the color buffer to be cleared)
bool CRender2D::DrawTilemaps(UINT32 *destBottom, UINT32 *destTop)
{
	/*
	 * Precompute data needed for each layer
	 */
	const UINT16 	*nameTableBase[4];
	const UINT16	*hScrollTable[4];
	const UINT16 	*maskTableLine = (UINT16 *) &vram[0xF7000/4];	// start at line 0
	int				hFullScroll[4];
	bool			lineScrollMode[4];
	
	for (int i = 0; i < 4; i++)	// 0=A, 1=A', 2=B, 3=B'
	{
		// Base of name table
		nameTableBase[i] = (UINT16 *) &vram[(0xF8000+i*0x2000)/4];
		
		// Horizontal line scroll tables
		hScrollTable[i] = (UINT16 *) &vram[(0xF6000+i*0x400)/4];
		
		// Load horizontal full-screen scroll values and scroll mode
		hFullScroll[i]    = regs[0x60/4+i]&0x3FF;
		lineScrollMode[i] = regs[0x60/4+i]&0x8000;
	}
	
	/*
	 * Precompute layer mixing order 
	 */
	UINT32 			*dest[4];
	const UINT32	*src[4];
	int				sortedLayerNum[4];
	bool			sortedIsBottom[4];
	const UINT16	*sortedHScrollTable[4];
	int				sortedHFullScroll[4];
	bool			sortedLineScrollMode[4];
	bool			noBottom;	// when true, no layer assigned to bottom surface
	
	switch ((regs[0x20/4]>>8)&0xF)
	{
	case 0x5:	// top: A, B, A'?	bottom: B'
		noBottom = false;
		dest[0]=destBottom; src[0]=lineBuffer[3]; sortedLayerNum[0]=3; sortedIsBottom[0]=true;  sortedHScrollTable[0] = hScrollTable[3]; sortedHFullScroll[0]=hFullScroll[3]; sortedLineScrollMode[0]=lineScrollMode[3];
		dest[1]=destTop;    src[1]=lineBuffer[2]; sortedLayerNum[1]=2; sortedIsBottom[1]=true;  sortedHScrollTable[1] = hScrollTable[2]; sortedHFullScroll[1]=hFullScroll[2]; sortedLineScrollMode[1]=lineScrollMode[2];
		dest[2]=destTop;    src[2]=lineBuffer[0]; sortedLayerNum[2]=0; sortedIsBottom[2]=false; sortedHScrollTable[2] = hScrollTable[0]; sortedHFullScroll[2]=hFullScroll[0]; sortedLineScrollMode[2]=lineScrollMode[0];
		dest[3]=destTop;    src[3]=lineBuffer[1]; sortedLayerNum[3]=1; sortedIsBottom[3]=false; sortedHScrollTable[3] = hScrollTable[1]; sortedHFullScroll[3]=hFullScroll[1]; sortedLineScrollMode[3]=lineScrollMode[1];
		break;
	case 0x9:	// ? all layers on top but relative order unknown (Spikeout Final Edition, after first boss)
		noBottom = true;
		dest[0]=destTop;    src[0]=lineBuffer[2]; sortedLayerNum[0]=2; sortedIsBottom[0]=true;  sortedHScrollTable[0] = hScrollTable[2]; sortedHFullScroll[0]=hFullScroll[2]; sortedLineScrollMode[0]=lineScrollMode[3];
		dest[1]=destTop;    src[1]=lineBuffer[3]; sortedLayerNum[1]=3; sortedIsBottom[1]=false; sortedHScrollTable[1] = hScrollTable[3]; sortedHFullScroll[1]=hFullScroll[3]; sortedLineScrollMode[1]=lineScrollMode[2];
		dest[2]=destTop;    src[2]=lineBuffer[1]; sortedLayerNum[2]=1; sortedIsBottom[2]=false; sortedHScrollTable[2] = hScrollTable[1]; sortedHFullScroll[2]=hFullScroll[1]; sortedLineScrollMode[2]=lineScrollMode[1];
		dest[3]=destTop;    src[3]=lineBuffer[0]; sortedLayerNum[3]=0; sortedIsBottom[3]=false; sortedHScrollTable[3] = hScrollTable[0]; sortedHFullScroll[3]=hFullScroll[0]; sortedLineScrollMode[3]=lineScrollMode[0];
		break;
	case 0xF:	// all on top
		noBottom = true;
		dest[0]=destTop;    src[0]=lineBuffer[2]; sortedLayerNum[0]=2; sortedIsBottom[0]=true;  sortedHScrollTable[0] = hScrollTable[2]; sortedHFullScroll[0]=hFullScroll[2]; sortedLineScrollMode[0]=lineScrollMode[2];
		dest[1]=destTop;    src[1]=lineBuffer[3]; sortedLayerNum[1]=3; sortedIsBottom[1]=false; sortedHScrollTable[1] = hScrollTable[3]; sortedHFullScroll[1]=hFullScroll[3]; sortedLineScrollMode[1]=lineScrollMode[3];
		dest[2]=destTop;    src[2]=lineBuffer[0]; sortedLayerNum[2]=0; sortedIsBottom[2]=false; sortedHScrollTable[2] = hScrollTable[0]; sortedHFullScroll[2]=hFullScroll[0]; sortedLineScrollMode[2]=lineScrollMode[0];
		dest[3]=destTop;    src[3]=lineBuffer[1]; sortedLayerNum[3]=1; sortedIsBottom[3]=false; sortedHScrollTable[3] = hScrollTable[1]; sortedHFullScroll[3]=hFullScroll[1]; sortedLineScrollMode[3]=lineScrollMode[1];
		break;
	case 0x7:	// top: A, B	bottom: A'?, B'
		noBottom = false;
		dest[0]=destBottom; src[0]=lineBuffer[3]; sortedLayerNum[0]=3; sortedIsBottom[0]=true;  sortedHScrollTable[0] = hScrollTable[3]; sortedHFullScroll[0]=hFullScroll[3]; sortedLineScrollMode[0]=lineScrollMode[3];
		dest[1]=destBottom; src[1]=lineBuffer[1]; sortedLayerNum[1]=1; sortedIsBottom[1]=false; sortedHScrollTable[1] = hScrollTable[1]; sortedHFullScroll[1]=hFullScroll[1]; sortedLineScrollMode[1]=lineScrollMode[1];
		dest[2]=destTop;    src[2]=lineBuffer[2]; sortedLayerNum[2]=2; sortedIsBottom[2]=true;  sortedHScrollTable[2] = hScrollTable[2]; sortedHFullScroll[2]=hFullScroll[2]; sortedLineScrollMode[2]=lineScrollMode[2];
		dest[3]=destTop;    src[3]=lineBuffer[0]; sortedLayerNum[3]=0; sortedIsBottom[3]=false; sortedHScrollTable[3] = hScrollTable[0]; sortedHFullScroll[3]=hFullScroll[0]; sortedLineScrollMode[3]=lineScrollMode[0];
		break;
	default:	// unknown, use A and A' on top, B and B' on the bottom
		noBottom = false;
		dest[0]=destBottom; src[0]=lineBuffer[2]; sortedLayerNum[0]=2; sortedIsBottom[0]=true;  sortedHScrollTable[0] = hScrollTable[2]; sortedHFullScroll[0]=hFullScroll[2]; sortedLineScrollMode[0]=lineScrollMode[2];
		dest[1]=destBottom; src[1]=lineBuffer[3]; sortedLayerNum[1]=3; sortedIsBottom[1]=false; sortedHScrollTable[1] = hScrollTable[3]; sortedHFullScroll[1]=hFullScroll[3]; sortedLineScrollMode[1]=lineScrollMode[3];
		dest[2]=destTop;    src[2]=lineBuffer[0]; sortedLayerNum[2]=0; sortedIsBottom[2]=true;  sortedHScrollTable[2] = hScrollTable[0]; sortedHFullScroll[2]=hFullScroll[0]; sortedLineScrollMode[2]=lineScrollMode[0];
		dest[3]=destTop;    src[3]=lineBuffer[1]; sortedLayerNum[3]=1; sortedIsBottom[3]=false; sortedHScrollTable[3] = hScrollTable[1]; sortedHFullScroll[3]=hFullScroll[1]; sortedLineScrollMode[3]=lineScrollMode[1];
		break;
	}
	
	/*
	 * Render and mix each line
	 */
	for (int y = 0; y < 384; y++)
	{
		// Draw one scanline from each layer
		DrawLine(lineBuffer[0], 0, y, nameTableBase[0], pal[0]);
		DrawLine(lineBuffer[1], 1, y, nameTableBase[1], pal[0]);
		DrawLine(lineBuffer[2], 2, y, nameTableBase[2], pal[1]);
		DrawLine(lineBuffer[3], 3, y, nameTableBase[3], pal[1]);
						
		// Mix the layers in the correct order
		for (int i = 0; i < 4; i++)
		{
			MixLine(dest[i], src[i], sortedLayerNum[i], y, sortedIsBottom[i], sortedHScrollTable[i], maskTableLine, sortedHFullScroll[i], sortedLineScrollMode[i]);				
			dest[i] += 496;	// next line
		}
	
		// Next line in mask table
		maskTableLine += 2;
	}
	
	// Indicate whether color buffer must be cleared because no bottom layer
	return noBottom;
}
		

/******************************************************************************
 Frame Display Functions
******************************************************************************/

// Draws a surface to the screen (0 is top and 1 is bottom)
void CRender2D::DisplaySurface(int surface, GLfloat z)
{	
	// Draw the surface
	glActiveTexture(GL_TEXTURE0);	// texture unit 0
	glBindTexture(GL_TEXTURE_2D, texID[surface]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f/512.0f, 0.0f);          	glVertex3f(0.0f, 0.0f, z);
    glTexCoord2f(496.0f/512.0f, 0.0f);         	glVertex3f(1.0f, 0.0f, z);
    glTexCoord2f(496.0f/512.0f, 384.0f/512.0f);	glVertex3f(1.0f, 1.0f, z);
    glTexCoord2f(0.0f/512.0f, 384.0f/512.0f);   glVertex3f(0.0f, 1.0f, z);
    glEnd();
}

// Set up viewport and OpenGL state for 2D rendering (sets up blending function but disables blending)
void CRender2D::Setup2D(bool isBottom, bool clearAll)
{
	// Enable texture mapping and blending
	glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	// alpha of 1.0 is opaque, 0 is transparent
	glDisable(GL_BLEND);

	// Disable Z-buffering
	glDisable(GL_DEPTH_TEST);
	
	// Shader program
	glUseProgram(shaderProgram);
	
	// Clear everything if requested or just overscan areas for wide screen mode
	if (clearAll)
	{
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glViewport(0, 0, totalXPixels, totalYPixels);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else if (isBottom && g_Config.wideScreen)
	{
		// For now, clear w/ black (may want to use color 0 later)
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glViewport(0, 0, xOffs, totalYPixels);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(xOffs+xPixels, 0, totalXPixels, totalYPixels);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// Set up the viewport and orthogonal projection
	glViewport(xOffs, yOffs, xPixels, yPixels);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    gluOrtho2D(0.0, 1.0, 1.0, 0.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

// Bottom layers
void CRender2D::BeginFrame(void)
{
	// Update all layers
	bool clear = DrawTilemaps(surfBottom, surfTop);
	glActiveTexture(GL_TEXTURE0);	// texture unit 0
	glBindTexture(GL_TEXTURE_2D, texID[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 496, 384, GL_RGBA, GL_UNSIGNED_BYTE, surfTop);	
	glBindTexture(GL_TEXTURE_2D, texID[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 496, 384, GL_RGBA, GL_UNSIGNED_BYTE, surfBottom);	
	
	// Display bottom surface
	Setup2D(true, clear);
	if (!clear)
		DisplaySurface(1, 0.0);
}

// Top layers
void CRender2D::EndFrame(void)
{
	// Display top surface
	Setup2D(false, false);
	glEnable(GL_BLEND);
	DisplaySurface(0, -0.5);
}


/******************************************************************************
 Emulation Callbacks
******************************************************************************/

// Deprecated
void CRender2D::WriteVRAM(unsigned addr, UINT32 data)
{
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void CRender2D::AttachRegisters(const UINT32 *regPtr)
{
	regs = regPtr;
	DebugLog("Render2D attached registers\n");
}

void CRender2D::AttachPalette(const UINT32 *palPtr[2])
{
	pal[0] = palPtr[0];
	pal[1] = palPtr[1];
	DebugLog("Render2D attached palette\n");
}

void CRender2D::AttachVRAM(const UINT8 *vramPtr)
{
	vram = (UINT32 *) vramPtr;
	DebugLog("Render2D attached VRAM\n");
}

// Memory pool and offsets within it
#define MEMORY_POOL_SIZE		(2*512*384*4 + 4*512*4)
#define OFFSET_TOP_SURFACE		0				// 512*384*4 bytes
#define OFFSET_BOTTOM_SURFACE	(512*384*4) 	// 512*384*4
#define OFFSET_LINE_BUFFERS		(2*512*384*4)	// 4*512*4 (4 lines)

bool CRender2D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes)
{
	float	memSizeMB = (float)MEMORY_POOL_SIZE/(float)0x100000;
	
	// Load shaders
	if (OKAY != LoadShaderProgram(&shaderProgram,&vertexShader,&fragmentShader,NULL,NULL,vertexShaderSource,fragmentShaderSource))
		return FAIL;
	
	// Get locations of the uniforms
	glUseProgram(shaderProgram);	// bind program
	textureMapLoc = glGetUniformLocation(shaderProgram, "textureMap");
	glUniform1i(textureMapLoc,0);	// attach it to texture unit 0
	
	// Allocate memory for layer surfaces
	memoryPool = new(std::nothrow) UINT8[MEMORY_POOL_SIZE];
	if (NULL == memoryPool)
		return ErrorLog("Insufficient memory for tilemap surfaces (need %1.1f MB).", memSizeMB);	
	memset(memoryPool,0,MEMORY_POOL_SIZE);	// clear textures
	
	// Set up pointers to memory regions
	surfTop    	  = (UINT32 *) &memoryPool[OFFSET_TOP_SURFACE];
	surfBottom 	  = (UINT32 *) &memoryPool[OFFSET_BOTTOM_SURFACE];
	for (int i = 0; i < 4; i++)
		lineBuffer[i] = (UINT32 *) &memoryPool[OFFSET_LINE_BUFFERS + i*512*4];
	
	// Resolution
	xPixels = xRes;
	yPixels = yRes;
	xOffs = xOffset;
	yOffs = yOffset;
	totalXPixels = totalXRes;
	totalYPixels = totalYRes;
			
	// Create textures
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(2, texID);
    for (int i = 0; i < 2; i++)
    {
		glActiveTexture(GL_TEXTURE0);	// texture unit 0
        glBindTexture(GL_TEXTURE_2D, texID[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, surfTop);
        if (glGetError() != GL_NO_ERROR)
        	return ErrorLog("OpenGL was unable to provide 512x512-texel texture maps for tilemap layers.");
    }

	DebugLog("Render2D initialized (allocated %1.1f MB)\n", memSizeMB);
	return OKAY;
}

CRender2D::CRender2D(void)
{
	xPixels = 496;
	yPixels = 384;
	xOffs = 0;
	yOffs = 0;
	
	memoryPool = NULL;
	vram = NULL;
	surfTop = NULL;
	surfBottom = NULL;
	for (int i = 0; i < 4; i++)
		lineBuffer[i] = NULL;
	
	DebugLog("Built Render2D\n");
}

CRender2D::~CRender2D(void)
{
	DestroyShaderProgram(shaderProgram,vertexShader,fragmentShader);
	glDeleteTextures(2, texID);
	
	if (memoryPool != NULL)
	{
		delete [] memoryPool;
		memoryPool = NULL;
	}
	
	vram = NULL;
	surfTop = NULL;
	surfBottom = NULL;
	for (int i = 0; i < 4; i++)
		lineBuffer[i] = NULL;

	DebugLog("Destroyed Render2D\n");
}
