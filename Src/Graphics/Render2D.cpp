//TODO: organize memory pool more tightly: 2 512x384 layers plus 4 extra lines
/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson 
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
 * - Add dirty rectangles? 
 * - Are v-scroll values 9 or 10 bits? 
 * - Add fast paths for no scrolling (including unclipped tile rendering).
 * - Inline the loops in the tile renderers.
 * - Update description of tile generator before you forget :)
 * - A proper shut-down function is needed! OpenGL might not be available when
 *   the destructor for this class is called.
 */

#include <string.h>
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
void CRender2D::DrawTileLine4BitNoClip(UINT32 *buf, UINT16 tile, int tileLine)
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

// Draw 8-bit tile line, clipped at left edge
void CRender2D::DrawTileLine8BitNoClip(UINT32 *buf, UINT16 tile, int tileLine)
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


// Draw 4-bit tile line, clipped at left edge
void CRender2D::DrawTileLine4Bit(UINT32 *buf, int offset, UINT16 tile, int tileLine)
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
    for (int bitPos = 28; bitPos >= 0; bitPos -= 4)
   	{
    	if (offset >= 0)
    		buf[offset] = pal[((pattern>>bitPos)&0xF) | palette];
    	++offset;
    }
}

// Draw 4-bit tile line, clipped at right edge
void CRender2D::DrawTileLine4BitRightClip(UINT32 *buf, int offset, UINT16 tile, int tileLine, int numPixels)
{
    unsigned	tileOffset;	// offset of tile pattern within VRAM
    unsigned	palette;   	// color palette bits obtained from tile
    UINT32  	pattern;    // 8 pattern pixels fetched at once
    int			bitPos;

	// Tile pattern offset: each tile occupies 32 bytes when using 4-bit pixels
    tileOffset = ((tile&0x3FFF)<<1) | ((tile>>15)&1);
    tileOffset *= 32;
    tileOffset /= 4;	// VRAM is a UINT32 array

	// Upper color bits; the lower 4 bits come from the tile pattern
	palette = tile&0x7FF0;
    
    // Draw 8 pixels
    pattern = vram[tileOffset+tileLine];
    bitPos = 28;
    for (int i = 0; i < numPixels; i++)
   	{
    	buf[offset] = pal[((pattern>>bitPos)&0xF) | palette];
    	++offset;
    	bitPos -= 4;
    }
}

// Draw 8-bit tile line, clipped at left edge
void CRender2D::DrawTileLine8Bit(UINT32 *buf, int offset, UINT16 tile, int tileLine)
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
    for (int bitPos = 24; bitPos >= 0; bitPos -= 8)
   	{
    	if (offset >= 0)
    		buf[offset] = pal[((pattern>>bitPos)&0xFF) | palette];
    	++offset;
    }
    
    pattern = vram[tileOffset+tileLine+1];
    for (int bitPos = 24; bitPos >= 0; bitPos -= 8)
   	{
    	if (offset >= 0)
    		buf[offset] = pal[((pattern>>bitPos)&0xFF) | palette];
    	++offset;
    }
}

// Draw 8-bit tile line, clipped at right edge
void CRender2D::DrawTileLine8BitRightClip(UINT32 *buf, int offset, UINT16 tile, int tileLine, int numPixels)
{
    unsigned	tileOffset;	// offset of tile pattern within VRAM
    unsigned	palette;   	// color palette bits obtained from tile
    UINT32  	pattern;    // 4 pattern pixels fetched at once
    int			bitPos;

	tileLine *= 2;	// 8-bit pixels, each line is two words
	
	// Tile pattern offset: each tile occupies 64 bytes when using 8-bit pixels
    tileOffset = tile&0x3FFF;
    tileOffset *= 64;
    tileOffset /= 4;
    
	// Upper color bits
	palette = tile&0x7F00;
    
    // Draw 4 pixels at a time
    pattern = vram[tileOffset+tileLine];
    bitPos = 24;
    for (int i = 0; (i < 4) && (i < numPixels); i++)
   	{
    	buf[offset] = pal[((pattern>>bitPos)&0xFF) | palette];
    	++offset;
    	bitPos -= 8;
    }
    
    pattern = vram[tileOffset+tileLine+1];
    bitPos = 24;
    for (int i = 0; (i < 4) && (i < numPixels); i++)
   	{
    	buf[offset] = pal[((pattern>>bitPos)&0xFF) | palette];
    	++offset;
    	bitPos -= 8;
    }
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
 *		dest				Destination of 512-pixel wide output buffer to draw
 *							to.
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
 */
void CRender2D::DrawLine(UINT32 *dest, int layerNum, int y, const UINT16 *nameTableBase)
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
			DrawTileLine4BitNoClip(dest, nameTable[1], vOffset);	dest += 8;
			DrawTileLine4BitNoClip(dest, nameTable[0], vOffset);	dest += 8;
			DrawTileLine4BitNoClip(dest, nameTable[3], vOffset);	dest += 8;
			DrawTileLine4BitNoClip(dest, nameTable[2], vOffset);	dest += 8;
			nameTable += 4;	// next set of 4 tiles
		}
	}
	else
	{
		for (int tx = 0; tx < 64; tx += 4)
		{
			DrawTileLine8BitNoClip(dest, nameTable[1], vOffset);	dest += 8;
			DrawTileLine8BitNoClip(dest, nameTable[0], vOffset);	dest += 8;
			DrawTileLine8BitNoClip(dest, nameTable[3], vOffset);	dest += 8;
			DrawTileLine8BitNoClip(dest, nameTable[2], vOffset);	dest += 8;
			nameTable += 4;
		}
	}				
}

void CRender2D::MixLine(UINT32 *dest, const UINT32 *src, int layerNum, int y, bool isBottom)
{
	/*
	 * Mix in the appropriate layer under control of the stencil mask, applying
	 * horizontal scrolling in theprocess
	 */
		 
	// Line scroll table
	const UINT16 *hScrollTable = (UINT16 *) &vram[(0xF6000+layerNum*0x400)/4];
	
	// Load horizontal full-screen scroll values and scroll mode
	int  hFullScroll    = regs[0x60/4+layerNum]&0x3FF;
	bool lineScrollMode = regs[0x60/4+layerNum]&0x8000;
	
	// Load horizontal scroll values
	int hScroll;
	if (lineScrollMode)
		hScroll = hScrollTable[y];
	else
		hScroll = hFullScroll;
		
	// Get correct offset into mask table
	const UINT16 *maskTable = (UINT16 *) &vram[0xF7000/4];
	maskTable += 2*y;
	if (layerNum < 2)	// little endian: layers A and A' use second word in each pair
		++maskTable;
		
	// Figure out what mask bit should be to mix in this layer
	UINT16 doCopy;
	if ((layerNum & 1))		// layers 1 and 3 are A' and B': alternates
		doCopy = 0x0000;	// if mask is clear, copy alternate layer
	else
		doCopy = 0x8000;	// copy primary layer when mask is set
			
	// Mix first 60 tiles (4 at a time)
	UINT16 mask = *maskTable;	// mask for this line (each bit covers 4 tiles)
	int    i    = hScroll&511;	// line index (where to copy from)
	for (int tx = 0; tx < 60; tx += 4)
	{
		// If bottom layer, we can copy without worrying about transparency, and must also write blank values when this layer is not showing
		//TODO: move this test outside of loop
		if (isBottom)
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
		}
		else
		{
			// Copy while testing for transparencies
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
		}
		
		mask <<= 1;
	}
	
	// Mix last two tiles
	if (isBottom)
	{
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


void CRender2D::DrawTilemaps(UINT32 *destBottom, UINT32 *destTop)
{
	// Base address of all 4 name tables
	const UINT16 *nameTableBase[4];
	nameTableBase[0] = (UINT16 *) &vram[(0xF8000+0*0x2000)/4];	// A
	nameTableBase[1] = (UINT16 *) &vram[(0xF8000+1*0x2000)/4];	// A'
	nameTableBase[2] = (UINT16 *) &vram[(0xF8000+2*0x2000)/4];	// B
	nameTableBase[3] = (UINT16 *) &vram[(0xF8000+3*0x2000)/4];	// B'
	
	// Render and mix each line
	for (int y = 0; y < 384; y++)
	{
		// Draw each layer
		DrawLine(lineBuffer[0], 0, y, nameTableBase[0]);
		DrawLine(lineBuffer[1], 1, y, nameTableBase[1]);
		DrawLine(lineBuffer[2], 2, y, nameTableBase[2]);
		DrawLine(lineBuffer[3], 3, y, nameTableBase[3]);

		//TODO: could probably further optimize: only have a single layer clear masked-out areas, then if alt. layer is being written to same place, don't bother worrying about transparencies if directly on top
		// Combine according to priority settings		
		// NOTE: question mark indicates unobserved and therefore unknown
		switch ((regs[0x20/4]>>8)&0xF)
		{
		case 0x5:	// top: A, B, A'?	bottom: B'
			MixLine(destBottom, lineBuffer[3], 3, y, true);
			MixLine(destTop, lineBuffer[2], 2, y, true);
			MixLine(destTop, lineBuffer[0], 0, y, false);
			MixLine(destTop, lineBuffer[1], 1, y, false);
			break;
		case 0xF:	// all on top
			memset(destBottom, 0, 496*sizeof(UINT32));	//TODO: use glClear(GL_COLOR_BUFFER_BIT) if there is no bottom layer
			MixLine(destTop, lineBuffer[2], 2, y, true);
			MixLine(destTop, lineBuffer[3], 3, y, false);
			MixLine(destTop, lineBuffer[0], 0, y, false);
			MixLine(destTop, lineBuffer[1], 1, y, false);
			break;
		case 0x7:	// top: A, B	bottom: A'?, B'
			MixLine(destBottom, lineBuffer[3], 3, y, true);
			MixLine(destBottom, lineBuffer[1], 1, y, false);
			MixLine(destTop, lineBuffer[2], 2, y, true);
			MixLine(destTop, lineBuffer[0], 0, y, false);
			break;
		default:	// unknown, use A and A' on top, B and B' on the bottom
			MixLine(destBottom, lineBuffer[2], 2, y, true);
			MixLine(destBottom, lineBuffer[3], 3, y, false);
			MixLine(destTop, lineBuffer[0], 0, y, true);
			MixLine(destTop, lineBuffer[1], 1, y, false);
			break;
		}
		
		// Advance to next line in output surfaces
		destBottom += 496;
		destTop += 496;
	}
}
		

/******************************************************************************
 Frame Display Functions
******************************************************************************/

// Draws a surface to the screen (0 is top and 1 is bottom)
void CRender2D::DisplaySurface(int surface, GLfloat z)
{	
	glBindTexture(GL_TEXTURE_2D, texID[surface]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f/512.0f, 0.0f);          	glVertex3f(0.0f, 0.0f, z);
    glTexCoord2f(496.0f/512.0f, 0.0f);         	glVertex3f(1.0f, 0.0f, z);
    glTexCoord2f(496.0f/512.0f, 384.0f/512.0f);	glVertex3f(1.0f, 1.0f, z);
    glTexCoord2f(0.0f/512.0f, 384.0f/512.0f);   glVertex3f(0.0f, 1.0f, z);
    glEnd();
}

// Set up viewport and OpenGL state for 2D rendering (sets up blending function but disables blending)
void CRender2D::Setup2D(void)
{
	// Set up the viewport and orthogonal projection
	glViewport(xOffs, yOffs, xPixels, yPixels);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    gluOrtho2D(0.0, 1.0, 1.0, 0.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Enable texture mapping and blending
	glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	// alpha of 1.0 is opaque, 0 is transparent
	glDisable(GL_BLEND);

	// Disable Z-buffering
	glDisable(GL_DEPTH_TEST);
	
	// Shader program
	glUseProgram(shaderProgram);
}

// Convert color offset register data to RGB
void CRender2D::ColorOffset(GLfloat colorOffset[3], UINT32 reg)
{
	INT8	ir, ig, ib;
	
	ib = (reg>>16)&0xFF;
	ig = (reg>>8)&0xFF;
	ir = (reg>>0)&0xFF;
	
	/*
	 * Uncertain how these should be interpreted. It appears to be signed, 
	 * which means the values range from -128 to +127. The division by 128
	 * normalizes this to roughly -1,+1.
	 */
	colorOffset[0] = (GLfloat) ir * (1.0f/128.0f);
	colorOffset[1] = (GLfloat) ig * (1.0f/128.0f);
	colorOffset[2] = (GLfloat) ib * (1.0f/128.0f);		
	//printf("%08X -> %g,%g,%g\n", reg, colorOffset[2], colorOffset[1], colorOffset[0]);
}

// Bottom layers
void CRender2D::BeginFrame(void)
{
	GLfloat	colorOffset[3];	
	
	// Update all layers
	DrawTilemaps(surfBottom, surfTop);
	glBindTexture(GL_TEXTURE_2D, texID[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 496, 384, GL_RGBA, GL_UNSIGNED_BYTE, surfTop);	
	glBindTexture(GL_TEXTURE_2D, texID[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 496, 384, GL_RGBA, GL_UNSIGNED_BYTE, surfBottom);	
	
	// Display bottom surface
	Setup2D();
	ColorOffset(colorOffset, regs[0x44/4]);
	glUniform3fv(colorOffsetLoc, 1, colorOffset);
	DisplaySurface(1, 0.0);
}

// Top layers
void CRender2D::EndFrame(void)
{	
	GLfloat	colorOffset[3];
	
	// Display top surface
	Setup2D();
	glEnable(GL_BLEND);
	ColorOffset(colorOffset, regs[0x40/4]);
	glUniform3fv(colorOffsetLoc, 1, colorOffset);
	DisplaySurface(0, -0.5);
}


/******************************************************************************
 Emulation Callbacks
******************************************************************************/

void CRender2D::WriteVRAM(unsigned addr, UINT32 data)
{
	if (vram[addr/4] == data)	// do nothing if no changes
		return;
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void CRender2D::AttachRegisters(const UINT32 *regPtr)
{
	regs = regPtr;
	DebugLog("Render2D attached registers\n");
}

void CRender2D::AttachPalette(const UINT32 *palPtr)
{
	pal = palPtr;
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

bool CRender2D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes)
{
	float	memSizeMB = (float)MEMORY_POOL_SIZE/(float)0x100000;
	
	// Load shaders
	if (OKAY != LoadShaderProgram(&shaderProgram,&vertexShader,&fragmentShader,NULL,NULL,vertexShaderSource,fragmentShaderSource))
		return FAIL;
	
	// Get locations of the uniforms
	glUseProgram(shaderProgram);	// bind program
	textureMapLoc = glGetUniformLocation(shaderProgram, "textureMap");
	glUniform1i(textureMapLoc,0);	// attach it to texture unit 0
	colorOffsetLoc = glGetUniformLocation(shaderProgram, "colorOffset");
	
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
			
	// Create textures
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(2, texID);
    for (int i = 0; i < 2; i++)
    {
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
