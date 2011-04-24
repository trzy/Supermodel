/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski 
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
 * Render.h
 * 
 * Header file defining the CRender class: container for both the 2D and 3D 
 * renderers.
 */

#ifndef INCLUDED_RENDER_H
#define INCLUDED_RENDER_H

/*
 * CRender:
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
	 * BeginFrame(layerFormats):
	 *
	 * Prepare to render a new frame. Must be called once per frame prior to
	 * drawing anything.
	 *
	 * Parameters:
	 *		layerFormats	A bit field indicating how to interpret each of the
	 *						four layers: 0=8-bit pixels, 1=4-bit pixels. Bits
	 *						3-0 correspond to layers 3-0.
	 */
	void BeginFrame(unsigned layerFormats);
	
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
	 *		regPtr		Pointer to the base of the tile generator registers.
	 *					There are assumed to be 64 in all.
	 */
	void AttachRegisters(const UINT32 *regPtr);
	
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
	 * Init(xOffset, yOffset, xRes, yRes):
	 *
	 * One-time initialization of the context. Must be called before any other
	 * members (meaning it should be called even before being attached to any
	 * other objects that want to use it).
	 *
	 * Parameters:
	 *		xOffset	X offset within OpenGL display surface in pixels.
	 *		yOffset	Y offset.
	 *		xRes	Horizontal resolution of display surface in pixels.
	 *		yRes	Vertical resolution.
	 *
	 * Returns:
	 *		OKAY is successful, otherwise FAILED if a non-recoverable error
	 *		occurred. Prints own error messages.
	 */
	BOOL Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes);
	 
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
	void DrawTile4Bit(unsigned tile, UINT32 *buf, unsigned pitch);
	void DrawTile8Bit(unsigned tile, UINT32 *buf, unsigned pitch);
	void DrawRect(int layerNum, UINT32 *layerSurf, const UINT32 *nameTable, unsigned x, unsigned y, unsigned w, unsigned h);
	void UpdateLayer(int layerNum);
	void DisplayLayer(int layerNum, GLfloat z, GLfloat scroll);
	void Setup2D(void);
		
	// Data received from tile generator device object
	const UINT32	*vram;
	const UINT32	*regs;
	
	// OpenGL data
	GLuint   	texID[4];			// IDs for the 4 layer textures
	unsigned	xPixels, yPixels;	// display surface resolution
	unsigned	xOffs, yOffs;		// offset
	
	// Shader programs and input data locations
	GLuint	shaderProgram;	// shader program object
	GLuint	vertexShader;	// vertex shader handle
	GLuint	fragmentShader;	// fragment shader
	GLuint	textureMapLoc;	// location of "textureMap" uniform
	GLuint	bottomLayerLoc;	// uniform

	
	// Dirty rectangles (non-zero indicates region is dirty)
	UINT8	dirty[4][64/DIRTY_RECT_HEIGHT][64/DIRTY_RECT_WIDTH];
	BOOL	allDirty;	// global dirty flag (forces everything to be updated)
	
	// Buffers
	UINT8	*memoryPool;	// all memory is allocated here
	UINT32	*surf;			// 4 512x512x32bpp pixel surfaces
	UINT32	*pal;			// 0x20000 byte (32K colors) palette
};


#endif	// INCLUDED_RENDER2D_H
