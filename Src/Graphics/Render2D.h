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
 * Render2D.h
 * 
 * Header file defining the CRender2D class: OpenGL tile generator graphics.
 */

#ifndef INCLUDED_RENDER2D_H
#define INCLUDED_RENDER2D_H

#include "Pkgs/glew.h"


/*
 * CRender2D:
 *
 * Tile generator graphics engine. This must be constructed and initialized 
 * before being attached to any objects that want to make use of it. Apart from
 * the constructor, all members assume that a global GL device
 * context is available and that GL functions may be called.
 */
class CRender2D
{
public:
	/*
	 * BeginFrame(void):
	 *
	 * Prepare to render a new frame. Must be called once per frame prior to
	 * drawing anything.
	 */
	void BeginFrame(void);
	
	/*
	 * EndFrame(void):
	 *
	 * Signals the end of rendering for this frame. Must be called last during
	 * the frame.
	 */
	void EndFrame(void);
	 
	/*
	 * WriteVRAM(addr, data):
	 *
	 * Indicates what will be written next to the tile generator's RAM. The
	 * VRAM address must not have yet been updated, to allow the renderer to
	 * check for changes. Data is accepted in the same form as the tile 
	 * generator: the MSB is what was written to addr+3. This function is 
	 * intended to facilitate on-the-fly decoding of tiles and palette data.
	 *
	 * Parameters:
	 *		addr	Address in tile generator RAM. Caller must ensure it is 
	 *				clamped to the range 0x000000 to 0x11FFFF because this
	 *				function does not.
	 *		data	The data to write.
	 */
	void WriteVRAM(unsigned addr, UINT32 data);
	
	/*
	 * AttachRegisters(regPtr):
	 *
	 * Attaches tile generator registers. This must be done prior to any 
	 * rendering otherwise the program may crash with an access violation.
	 *
	 * Parameters:
	 *		regPtr	Pointer to the base of the tile generator registers. There
	 *				are assumed to be 64 in all.
	 */
	void AttachRegisters(const UINT32 *regPtr);
	
	/*
	 * AttachPalette(palPtr):
	 *
	 * Attaches tile generator palettes. This must be done prior to any
	 * rendering.
	 *
	 * Parameters:
	 *		palPtr	Pointer to two palettes. The first is for layers A/A' and
	 *				the second is for B/B'.
	 */
	void AttachPalette(const UINT32 *palPtr[2]);

	/*
	 * AttachVRAM(vramPtr):
	 *
	 * Attaches tile generator RAM. This must be done prior to any rendering
	 * otherwise the program may crash with an access violation.
	 *
	 * Parameters:
	 *		vramPtr		Pointer to the base of the tile generator RAM (0x120000
	 *					bytes). VRAM is assumed to be in little endian format.
	 */
	void AttachVRAM(const UINT8 *vramPtr);

	/*
	 * Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes);
	 *
	 * One-time initialization of the context. Must be called before any other
	 * members (meaning it should be called even before being attached to any
	 * other objects that want to use it).
	 *
	 * Parameters:
	 *		xOffset		X offset of the viewable area within OpenGL display 
	 *                  surface, in pixels.
	 *		yOffset		Y offset.
	 *		xRes		Horizontal resolution of the viewable area.
	 *		yRes		Vertical resolution.
	 *		totalXRes	Horizontal resolution of the complete display area.
	 *		totalYRes	Vertical resolution.
	 *
	 * Returns:
	 *		OKAY is successful, otherwise FAILED if a non-recoverable error
	 *		occurred. Prints own error messages.
	 */
	bool Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes);
	 
	/*
	 * CRender2D(void):
	 * ~CRender2D(void):
	 *
	 * Constructor and destructor.
	 */
	CRender2D(void);
	~CRender2D(void);
	
private:
	// Private member functions
	void DrawTileLine8BitNoClip(UINT32 *buf, UINT16 tile, int tileLine, const UINT32 *pal);
	void DrawTileLine4BitNoClip(UINT32 *buf, UINT16 tile, int tileLine, const UINT32 *pal);
	void DrawLine(UINT32 *dest, int layerNum, int y, const UINT16 *nameTableBase, const UINT32 *pal);
	bool DrawTilemaps(UINT32 *destBottom, UINT32 *destTop);
	void DisplaySurface(int surface, GLfloat z);
	void Setup2D(bool isBottom, bool clearAll);
			
	// Data received from tile generator device object
	const UINT32	*vram;
	const UINT32    *pal[2];	// palettes for A/A' and B/B'
	const UINT32	*regs;
	
	// OpenGL data
	GLuint   	texID[2];					// IDs for the 2 layer textures (top and bottom)
	unsigned	xPixels, yPixels;			// display surface resolution
	unsigned	xOffs, yOffs;				// offset
	unsigned	totalXPixels, totalYPixels;	// total display surface resolution
	
	// Shader programs and input data locations
	GLuint	shaderProgram;	// shader program object
	GLuint	vertexShader;	// vertex shader handle
	GLuint	fragmentShader;	// fragment shader
	GLuint	textureMapLoc;	// location of "textureMap" uniform

	// Buffers
	UINT8	*memoryPool;	// all memory is allocated here
	UINT32	*surfTop;		// 512x384x32bpp pixel surface for top layers
	UINT32	*surfBottom;	// bottom layers
	UINT32	*lineBuffer[4];	// 512 32bpp pixel line buffers for layer composition
};


#endif	// INCLUDED_RENDER2D_H
