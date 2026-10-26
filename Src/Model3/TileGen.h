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
  * TileGen.h
  *
  * Header file defining the CTileGen (tile generator device) class.
  */

#ifndef INCLUDED_TILEGEN_H
#define INCLUDED_TILEGEN_H

#include "BlockFile.h"
#include "CPU/IRQController.h"
#include "Graphics/Render2D.h"
#include "TileGenBuffer.h"

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
	void SaveState(CBlockFile* SaveState);

	/*
	 * LoadState(SaveState):
	 *
	 * Loads and a state image.
	 *
	 * Parameters:
	 *		SaveState	Block file to load state information from.
	 */
	void LoadState(CBlockFile* SaveState);

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
   *
   * Invokes the underlying 2D renderer.
	 */
	void BeginFrame(void);

	/*
	 * PreRenderFrame(void):
	 *
	 * Draws the all top layers (above 3D graphics) and bottom layers (below 3D
	 * graphics) but does not yet display them. May send data to the GPU.
	 *
	 * Invokes the equivalent method in the underlying 2D renderer.
	 */
	void PreRenderFrame(void);

	/*
	 * RenderFrameBottom(void):
	 *
	 * Overwrites the color buffer with bottom surface that was pre-rendered by
	 * the last call to PreRenderFrame().
	 *
	 * Invokes the equivalent method in the underlying 2D renderer.
	 */
	void RenderFrameBottom(void);

	/*
	 * RenderFrameTop(void):
	 *
	 * Draws the top surface (if it exists) that was pre-rendered by the last
	 * call to PreRenderFrame(). Previously drawn graphics layers will be visible
	 * through transparent regions.
	 *
	 * Invokes the equivalent method in the underlying 2D renderer.
	 */
	void RenderFrameTop(void);

	/*
	 * EndFrame(void):
	 *
	 * Signals the end of rendering for this frame.  Must be called last during
	 * the frame and must only access read-only snapshots and variables since it
	 * may be running in a separate thread.
	 *
	 * Invokes the equivalent method in the underlying 2D renderer.
	 */
	void EndFrame(void);

	/*
	 * ReadRAM8(addr):
	 * ReadRAM16(addr):
	 * ReadRAM32(addr):
	 *
	 * Reads the tile generator's little endian RAM (the word is returned with
	 * the MSB read from addr+3). If a big endian device is reading (PowerPC),
	 * the result must be manually adjusted.
	 *
	 * Parameters:
	 *		addr	Address in tile generator RAM. Caller must ensure it is
	 *				clamped to the range 0x000000 to 0x11FFFF because this
	 *				function does not.
	 *
	 * Returns:
	 *		Data from the RAM address.
	 */
	uint8_t  ReadRAM8(unsigned addr) const;
	uint16_t ReadRAM16(unsigned addr) const;
	uint32_t ReadRAM32(unsigned addr) const;

	/*
	 * WriteRAM8(addr, data):
	 * WriteRAM16(addr, data):
	 * WriteRAM32(addr, data):
	 *
	 * Writes to the tile generator's little endian RAM (the word's MSB is
	 * written to addr+3). If a big endian device is writing, the address and
	 * data must be adjusted manually before passing it to this function.
	 *
	 * Parameters:
	 *		addr	Address in tile generator RAM. Caller must ensure it is
	 *				clamped to the range 0x000000 to 0x11FFFF because this
	 *				function does not.
	 *		data	The data to write.
	 */
	void WriteRAM8(unsigned addr, uint8_t data);
	void WriteRAM16(unsigned addr, uint16_t data);
	void WriteRAM32(unsigned addr, uint32_t data);

	/*
	 * ReadRegister(reg):
	 *
	 * Reads 32 bits of data from a (little endian) register. If a big endian
	 * device is reading, the word must be flipped.
	 *
	 * Parameters:
	 *		reg		Aligned (32 bits) register offset (0x00-0xFC).
	 *
	 * Returns:
	 *    Data read as little endian from the register.
	 */
	UINT32 ReadRegister(unsigned reg) const;

	/*
	 * WriteRegister(reg, data):
	 *
	 * Writes 32 bits of data to a (little endian) register. If a big endian
	 * device is writing, the word must be flipped.
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
	void AttachRenderer(CRender2D* Render2DPtr);

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
	Result Init(IIRQController* IRQObjectPtr);


	/*
	 * DrawLine(line):
	 *
	 * Draw a line for the tilegen
	 *
	 * Parameters:
	 *		line	The line number to draw (from 0-383)
	 */
	void DrawLine(int line);	// from 0-383

	/*
	 * Tile and palette RAM bus timing
	 *
	 * The tile generator RAM (Samsung KM4132G271A) runs at 32 MHz, twice the 16 MHz pixel clock. An access takes 4
	 * clocks (row, column, wait, data). Palette RAM (two 8-bit SRAMs, 16 bits wide: the upper 16 bits of a PowerPC
	 * write go nowhere) also runs at 32 MHz, looked up twice per output pixel (two tilemap layers); the chip takes
	 * one write to it every ~6.3 pixels (12.6 clocks), measured on real hardware from the snow of a whole palette
	 * update (Daytona USA 2: dots 6-7 pixels apart). That measurement is of palette RAM: a write to tile RAM takes
	 * one access (4 clocks). Applied to tile RAM too, the palette rate made Scud Race's test screens, which rewrite
	 * the four layers' name tables (8192 words, all to tile RAM) in each vertical blanking, run over into the display
	 * and lose their top lines (GAME ASSIGNMENTS: its title, LINK ID and CAR NUMBER). On each active line the chip
	 * generates 512 pixels (496 visible) as 32 tile pairs of 32 clocks: 28
	 * clocks fetch the next tile pair (16 pixels) of layers A and B, leaving one 4-clock slot for the PowerPC, so
	 * tile RAM is slowed further to one access per 16 pixels. The remaining 144 pixels of the line (horizontal
	 * blanking) are free. Some games rely on this: Scud Race queues its palette uploads at the rate it can write
	 * tile RAM, and the queue overflows if tile RAM writes are too fast.
	 *
	 * SetCPUClock(hz):     PowerPC clock frequency, to convert VRAM clocks to PowerPC cycles.
	 * BeginBusLine(cycle): an active line starts at this PowerPC cycle (ppc_total_cycles()).
	 * BeginBusBlanking():  vertical blanking starts, nothing is fetched.
	 * BusAccessCycles(cycle, write, tileRAM):
	 *    Extra PowerPC cycles for an access to tile RAM (tileRAM = true) or palette RAM at this PowerPC cycle. A
	 *    write is posted, so the PowerPC only waits until the chip takes it; a read waits for its data.
	 */
	void SetCPUClock(unsigned hz);
	void BeginBusLine(UINT64 cpuCycle);
	void BeginBusBlanking(void);
	int  BusAccessCycles(UINT64 cpuCycle, bool write, bool tileRAM);

	/*
	 * CTileGen(config):
	 * ~CTileGen(void):
	 *
	 * Constructor and destructor.
	 *
   * Paramters:
   *    config  Run-time configuration. The reference should be held because
   *            this changes at run-time.
	 */
	CTileGen(const Util::Config::Node& config);
	~CTileGen(void);

private:

	struct ColourOffsetRegister
	{
		void Reset()
		{
			r = 0; g = 0; b = 0;
		}

		void Update(UINT32 reg)
		{
			b = (INT32)(INT8)((reg >> 16) & 0xFF);
			g = (INT32)(INT8)((reg >> 8) & 0xFF);
			r = (INT32)(INT8)((reg >> 0) & 0xFF);
			b *= 2;
			g *= 2;
			r *= 2;
		}

		INT32 r = 0;
		INT32 g = 0;
		INT32 b = 0;
	};

	ColourOffsetRegister m_colourOffsetRegs[2];

	bool	IsEnabled			(int layerNumber) const;
	bool	Above3D				(int layerNumber) const;
	bool	Is4Bit				(int layerNumber) const;
	int		GetYScroll			(int layerNumber) const;
	int		GetXScroll			(int layerNumber) const;
	bool	LineScrollMode		(int layerNumber) const;
	int		GetLineScroll		(int layerNumber, int yCoord) const;
	int		GetTilePairNumber	(int xCoord, int yCoord, int xScroll, int yScroll) const;
	int		GetTileData			(int layerNum, int tilePairNumber) const;
	int		GetVFine			(int yCoord, int yScroll) const;
	int		GetHFine			(int xCoord, int xScroll) const;
	int		GetLineMask			(int layerNumber, int yCoord) const;
	int		GetPixelMask		(int lineMask, int xCoord) const;
	UINT32	GetColour32			(int layer, UINT32 data) const;
	void	Draw4BitTilePair	(int tileData, int hStart, int hEnd, int vFine, UINT32* const lineBuffer, const UINT32* const pal, int& x) const;
	void	Draw8BitTilePair	(int tileData, int hStart, int hEnd, int vFine, UINT32* const lineBuffer, const UINT32* const pal, int& x) const;

	void	WritePalette		(int layer, int address, UINT32 data);
	void	RecomputePalettes	(int layer);	// 0 = bottom, 1 = top

	//const Util::Config::Node& m_config;
	const bool m_gpuMultiThreaded;

	IIRQController*	IRQ;	// IRQ controller the tile generator is attached to
	CRender2D*	Render2D;	// 2D renderer the tile generator is attached to

	UINT8*		memoryPool;		// all memory allocated here
	UINT8*		m_vram;			// 1.125MB of VRAM
	UINT32*		m_vramP;		// vram pointer but integer size
	UINT32*		m_palP;			// just a pointer to the palette ram which comes after the vram

	UINT32*		m_pal[2];		// cached decoded pallettes. 0 = layer 0&1, 1 = layer 2&3

	// Registers
	UINT32	m_regs[64];

	// Tile and palette RAM bus timing, in 1/16 PowerPC cycles (exact for 66, 100 and 166 MHz)
	UINT64	m_busClock = 50;		// one VRAM clock
	UINT64	m_busWriteGap = 630;	// minimum time between palette RAM writes (~6.3 pixels = 12.6 clocks)
	UINT64	m_busFree = 0;			// when the next access can start
	UINT64	m_busLineStart = 0;		// start of the active line
	bool	m_busFetching = false;	// in the active part of the frame

	// buffers we draw to
	std::shared_ptr<TileGenBuffer> m_drawSurface[2];	// drawing surfaces 0 = bottom, 1 = top
	std::shared_ptr<TileGenBuffer> m_drawSurfaceRO[2];	// read only version for threading, we can swap between the 2. Maybe not needed.
};


#endif	// INCLUDED_TILEGEN_H
