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
 * TileGen.h
 * 
 * Header file defining the CTileGen (tile generator device) class.
 */

#ifndef INCLUDED_TILEGEN_H
#define INCLUDED_TILEGEN_H


/*
 * CTileGen:
 *
 * Tile generator device. The Model 3's tile generator handles not only the 2D
 * tile map layers but also seems to control video output and VBL IRQs.
 */
class CTileGen
{
public:
	/*
	 * SaveState(SaveState):
	 *
	 * Saves an image of the current device state.
	 *
	 * Parameters:
	 *		SaveState	Block file to save state information to.
	 */
	void SaveState(CBlockFile *SaveState);

	/*
	 * LoadState(SaveState):
	 *
	 * Loads and a state image.
	 *
	 * Parameters:
	 *		SaveState	Block file to load state information from.
	 */
	void LoadState(CBlockFile *SaveState);
	
	/*
	 * BeginVBlank(void):
	 *
	 * Must be called before the VBlank starts.
	 */
	void BeginVBlank(void);
	
	/*
	 * EndVBlank(void)
	 *
	 * Must be called after the VBlank finishes.
	 */
	void EndVBlank(void);

	/*
	 * SyncSnapshots(void):
	 *
	 * Syncs the read-only memory snapshots with the real ones so that rendering
	 * of the current frame can begin in the render thread.  Must be called at the
	 * end of each frame when both the render thread and the PPC thread have finished
	 * their work.  If multi-threaded rendering is not enabled, then this method does
	 * nothing.
	 */
	UINT32 SyncSnapshots(void);

	/*
	 * BeginFrame(void):
	 *
	 * Prepares to render a new frame.  Must be called once per frame prior to
	 * drawing anything and must only access read-only snapshots and variables
     * since it may be running in a separate thread.
	 */
	void BeginFrame(void);

	/*
	 * EndFrame(void):
	 *
	 * Signals the end of rendering for this frame.  Must be called last during
	 * the frame and must only access read-only snapshots and variables since it
	 * may be running in a separate thread.
	 */
	void EndFrame(void);

	/*
	 * ReadRAM(addr):
	 *
	 * Reads the tile generator's little endian RAM (the word is returned with
	 * the MSB read from addr+3). If a big endian device is reading (PowerPC),
	 * the result must be manually flipped.
	 *
	 * Parameters:
	 *		addr	Address in tile generator RAM. Caller must ensure it is 
	 *				clamped to the range 0x000000 to 0x11FFFF because this
	 *				function does not.
	 *
	 * Returns:
	 *		A 32-bit word read as little endian from the RAM address.
	 */
	UINT32 ReadRAM(unsigned addr);
	
	/*
	 * WriteRAM(addr, data):
	 *
	 * Writes to the tile generator's little endian RAM (the word's MSB is
	 * written to addr+3). If a big endian device is writing, the word must be
	 * flipped manually before passing it to this function.
	 *
	 * Parameters:
	 *		addr	Address in tile generator RAM. Caller must ensure it is 
	 *				clamped to the range 0x000000 to 0x11FFFF because this
	 *				function does not.
	 *		data	The data to write.
	 */
	void WriteRAM(unsigned addr, UINT32 data);
	
	/*
	 * WriteRegister(reg, data):
	 *
	 * Writes 32 bits of data to a (little endian) register. If a big endian
	 * device is writing, th word must be flipped.
	 *
	 * Parameters:
	 *		reg		Aligned (32 bits) register offset (0x00-0xFC).
	 *		data	Data to write.
	 */
	void WriteRegister(unsigned reg, UINT32 data);
	
	/*
	 * Reset(void):
	 *
	 * Resets the device. 
	 */
	void Reset(void);
	
	/*
	 * AttachRenderer(render2DPtr):
	 *
	 * Attaches a 2D renderer for the tile generator to use. This function will
	 * immediately pass along the information that a CRender2D object needs to
	 * work with.
	 *
	 * Parameters:
	 *		Render2DPtr		Pointer to a 2D renderer object.
	 */
	void AttachRenderer(CRender2D *Render2DPtr);
	
	/*
	 * Init(IRQObjectPtr):
	 *
	 * One-time initialization of the context. Must be called prior to all
	 * other members. Links the tile generator to an IRQ controller and 
	 * allocates memory for tile generator RAM.
	 *
	 * Parameters:
	 *		IRQObjectPtr	Pointer to the IRQ controller object.
	 *
	 * Returns:
	 *		OKAY is successful, otherwise FAILED if a non-recoverable error
	 *		occurred. Prints own error messages.
	 */
	bool Init(CIRQ *IRQObjectPtr);
	 
	/*
	 * CTileGen(void):
	 * ~CTileGen(void):
	 *
	 * Constructor and destructor.
	 */
	CTileGen(void);
	~CTileGen(void);
	
private:
	// Private member functions
	void		InitPalette(void);
	void		WritePalette(unsigned color, UINT32 data);
	UINT32		UpdateSnapshots(bool copyWhole);
	UINT32		UpdateSnapshot(bool copyWhole, UINT8 *src, UINT8 *dst, unsigned size, UINT8 *dirty);
	
	CIRQ		*IRQ;		// IRQ controller the tile generator is attached to
	CRender2D	*Render2D;	// 2D renderer the tile generator is attached to
	
	// Tile generator VRAM
	UINT8	*memoryPool;	// all memory allocated here
	UINT8   *vram;          // 1.8MB of VRAM
	UINT32	*pal;			// 0x20000 byte (32K colors) palette

	// Read-only snapshots
	UINT8   *vramRO;        // 1.8MB of VRAM                     [read-only snapshot]	
	UINT32  *palRO;	        // 0x20000 byte (32K colors) palette [read-only snapshot]
	
	// Arrays to keep track of dirty pages in memory regions
	UINT8   *vramDirty;
	UINT8   *palDirty;

	// Registers
	UINT32	regs[64];
	UINT32  regsRO[64];     // Read-only copy of registers
	
};


#endif	// INCLUDED_TILEGEN_H
