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
 * TileGen.cpp
 * 
 * Implementation of the CTileGen class: 2D tile generator. Palette decoding 
 * and synchronization with the renderer (which may run in a separate thread)
 * are performed here as well. For a description of the tile generator
 * hardware, please refer to the 2D rendering engine source code.
 *
 * Palettes
 * --------
 *
 * Multiple copies of the 32K-color palette data are maintained. The first is
 * the raw data as written to the VRAM. Two copies are computed, one for layers
 * A/A' and the other for layers B/B'. These pairs of layers have independent 
 * color offset registers associated with them. The renderer uses these 
 * "computed" palettes.
 *
 * The computed palettes are updated whenever the real palette is modified, a
 * single color entry at a time. If the color register is modified, the entire
 * palette has to be recomputed accordingly.
 *
 * The read-only copy of the palette, which is generated for the renderer, only
 * stores the two computed palettes.
 *
 * TO-DO List:
 * -----------
 * - For consistency, the registers should probably be byte reversed (this is a
 *   little endian device), forcing the Model3 Read32/Write32 handlers to
 *   manually reverse the data. This keeps with the convention for VRAM.
 *   Need to finish ripping out code that no longer does anything. Removed a lot but there's still more.
 */

#include "TileGen.h"

#include <cstring>
#include <algorithm>
#include "Supermodel.h"

// Offsets of memory regions within TileGen memory pool
#define OFFSET_VRAM         0x000000	// VRAM and palette data
#define MEM_POOL_SIZE_RW    (0x120000)

#define MEMORY_POOL_SIZE	(MEM_POOL_SIZE_RW)


/******************************************************************************
 Save States
******************************************************************************/

void CTileGen::SaveState(CBlockFile *SaveState)
{
	SaveState->NewBlock("Tile Generator", __FILE__);
	SaveState->Write(m_vram, 0x120000); // Don't write out palette, read-only snapshots or dirty page arrays, just VRAM
	SaveState->Write(m_regs, sizeof(m_regs));
}

void CTileGen::LoadState(CBlockFile *SaveState)
{
	if (Result::OKAY != SaveState->FindBlock("Tile Generator"))
	{
		ErrorLog("Unable to load tile generator state. Save state file is corrupt.");
		return;
	}
	
	// Load memory one word at a time
	for (int i = 0; i < 0x120000; i += 4)
	{
		UINT32 data;
		SaveState->Read(&data, sizeof(data));
		WriteRAM32(i, data);
	}	
	SaveState->Read(m_regs, sizeof(m_regs));

	m_colourOffsetRegs[0].Update(m_regs[0x40 / 4]);
	RecomputePalettes(0);	// layer 0 & 1
	m_colourOffsetRegs[1].Update(m_regs[0x44 / 4]);
	RecomputePalettes(1);	// layer 2 & 3

	SyncSnapshots();
}


/******************************************************************************
 Rendering
******************************************************************************/

void CTileGen::BeginVBlank(void)
{
/*
	printf("08: %X\n", regs[0x08/4]);
	printf("0C: %X\n", regs[0x0C/4]);
	printf("20: %X\n", regs[0x20/4]);
	printf("40: %X\n", regs[0x40/4]);
	printf("44: %X\n", regs[0x44/4]);
	printf("60: %08X\n", regs[0x60/4]);
	printf("64: %08X\n", regs[0x64/4]);
	printf("68: %08X\n", regs[0x68/4]);
	printf("6C: %08X\n", regs[0x6C/4]);
	printf("\n");
*/
}

void CTileGen::EndVBlank(void)
{
	//
}

UINT32 CTileGen::SyncSnapshots(void)
{
	// clear surfaces
	for (auto& s : m_drawSurface) {
		s->Clear();
	}

	// draw buffers (this should be called elsewhere later)
	for (int i = 0; i < 384; i++) {
		DrawLine(i);
	}

	// swap buffers
	for (int i = 0; i < 2; i++) {
		std::swap(m_drawSurface[i], m_drawSurfaceRO[i]);
	}

	Render2D->AttachDrawBuffers(m_drawSurfaceRO[0], m_drawSurfaceRO[1]);
	
	return UINT32(0);
}

void CTileGen::BeginFrame(void)
{
	Render2D->BeginFrame();
}

void CTileGen::PreRenderFrame(void)
{
  Render2D->PreRenderFrame();
}

void CTileGen::RenderFrameBottom(void)
{
  Render2D->RenderFrameBottom();
}

void CTileGen::RenderFrameTop(void)
{
  Render2D->RenderFrameTop();
}

void CTileGen::EndFrame(void)
{
	Render2D->EndFrame();
}

/******************************************************************************
 Emulation Functions
******************************************************************************/

UINT32 CTileGen::ReadRAM32(unsigned addr) const
{
	return *(UINT32 *) &m_vram[addr];
}

void CTileGen::WriteRAM32(unsigned addr, UINT32 data)
{
	*(UINT32 *) &m_vram[addr] = data;

	if (addr >= 0x100000) {

		addr -= 0x100000;
		unsigned color = addr / 4;	// color index

		// Both palettes will be modified simultaneously
		WritePalette(0, color, data);
		WritePalette(1, color, data);
	}
}

//TODO: 8- and 16-bit handlers have not been thoroughly tested
uint8_t CTileGen::ReadRAM8(unsigned addr) const
{
  return m_vram[addr];
}

void CTileGen::WriteRAM8(unsigned addr, uint8_t data)
{
  uint32_t tmp = ReadRAM32(addr & ~3);
  uint32_t shift = (addr & 3) * 8;
  uint32_t mask = 0xff << shift;
  tmp &= ~mask;
  tmp |= uint32_t(data) << shift;
  WriteRAM32(addr & ~3, tmp);
}

// Star Wars Trilogy uses this
uint16_t CTileGen::ReadRAM16(unsigned addr) const
{
  return *((uint16_t *) &m_vram[addr]);
}

void CTileGen::WriteRAM16(unsigned addr, uint16_t data)
{
  uint32_t tmp = ReadRAM32(addr & ~1);
  uint32_t shift = (addr & 1) * 16;
  uint32_t mask = 0xffff << shift;
  tmp &= ~mask;
  tmp |= uint32_t(data) << shift;
  WriteRAM32(addr & ~1, tmp);
}

UINT32 CTileGen::ReadRegister(unsigned reg) const
{
  reg &= 0xFF;
  return m_regs[reg/4];
}

void CTileGen::WriteRegister(unsigned reg, UINT32 data)
{
	reg &= 0xFF;

	switch (reg)
	{
	case 0x00:
	case 0x08:
	case 0x0C:
		//end of frame
	case 0x20:
	case 0x60:
	case 0x64:
	case 0x68:
	case 0x6C:
		break;
	case 0x40:	// layer A/A' color offset
		if (m_regs[reg / 4] != data) {
			m_colourOffsetRegs[0].Update(data);
			RecomputePalettes(0);
		}
		break;
	case 0x44:	// layer B/B' color offset
		if (m_regs[reg / 4] != data) {
			m_colourOffsetRegs[1].Update(data);
			RecomputePalettes(1);
		}
		break;
	case 0x10:	// IRQ acknowledge
		IRQ->Deassert(data&0xFF);
		// MAME believes only lower 4 bits should be cleared
		//IRQ->Deassert(data & 0x0F);
		break;
	default:
		DebugLog("Tile Generator reg %02X = %08X\n", reg, data);
		//printf("%02X = %08X\n", reg, data);
		break;
	}

	// Modify register
	m_regs[reg/4] = data;
}

void CTileGen::Reset(void)
{
	unsigned memSize = (m_gpuMultiThreaded ? MEMORY_POOL_SIZE : MEM_POOL_SIZE_RW);
	memset(memoryPool, 0, memSize);
	memset(m_regs, 0, sizeof(m_regs));

	DebugLog("Tile Generator reset\n");
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void CTileGen::AttachRenderer(CRender2D *Render2DPtr)
{
	Render2D = Render2DPtr;

	Render2D->AttachVRAM(m_vram);
	Render2D->AttachRegisters(m_regs);

	DebugLog("Tile Generator attached a Render2D object\n");
}

Result CTileGen::Init(CIRQ *IRQObjectPtr)
{
	unsigned memSize   = (m_gpuMultiThreaded ? MEMORY_POOL_SIZE : MEM_POOL_SIZE_RW);
	float	 memSizeMB = (float)memSize/(float)0x100000;
	
	// Allocate all memory for all TileGen RAM regions
	memoryPool = new(std::nothrow) UINT8[memSize];
	if (NULL == memoryPool)
		return ErrorLog("Insufficient memory for tile generator object (needs %1.1f MB).", memSizeMB);
	
	// Set up main pointers
	m_vram	= (UINT8 *) &memoryPool[OFFSET_VRAM];
	m_vramP	= (UINT32*)m_vram;
	m_palP	= m_vramP + 0x40000;

	// Hook up the IRQ controller
	IRQ = IRQObjectPtr;
	
	DebugLog("Initialized Tile Generator (allocated %1.1f MB and connected to IRQ controller)\n", memSizeMB);
	return Result::OKAY;
}

CTileGen::CTileGen(const Util::Config::Node& config)
	: //m_config(config),
	m_gpuMultiThreaded(config["GPUMultiThreaded"].ValueAs<bool>()),
	IRQ(nullptr),
	Render2D(nullptr),
	memoryPool(nullptr),
	m_vram(nullptr),
	m_vramP(nullptr),
	m_palP(nullptr),
	m_pal{nullptr},
	m_regs{}
{
	for (auto& s : m_drawSurface) {
		s = std::make_shared<TileGenBuffer>();
	}

	for (auto& s : m_drawSurfaceRO) {
		s = std::make_shared<TileGenBuffer>();
	}

	for (auto& p : m_pal) {
		p = new UINT32[0x8000];
		memset(p, 0, 0x8000 * sizeof(UINT32));
	}

	DebugLog("Built Tile Generator\n");
}

CTileGen::~CTileGen(void)
{
	// Dump tile generator RAM
#if 0
	FILE *fp;
	fp = fopen("tileram", "wb");
	if (NULL != fp)
	{
		fwrite(memoryPool, sizeof(UINT8), 0x120000, fp);
		fclose(fp);
		printf("dumped %s\n", "tileram");
	}
	else
		printf("unable to dump %s\n", "tileram");
#endif
		
	IRQ = nullptr;
	delete [] memoryPool;
	memoryPool = nullptr;

	m_vram	= nullptr;	// all allocated in mem pool
	m_vramP	= nullptr;
	m_palP	= nullptr;

	for (auto& p : m_pal) {
		delete[] p;
		p = nullptr;
	}
	
	DebugLog("Destroyed Tile Generator\n");
}

bool CTileGen::IsEnabled(int layerNumber) const
{
	return (m_regs[0x60 / 4 + layerNumber] & 0x80000000) > 0;
}

bool CTileGen::Above3D(int layerNumber) const
{
	return (m_regs[0x20 / 4] >> (8 + layerNumber)) & 0x1;
}

bool CTileGen::Is4Bit(int layerNumber) const
{
	return (m_regs[0x20 / 4] & (1 << (12 + layerNumber))) != 0;
}

int CTileGen::GetYScroll(int layerNumber) const
{
	return (m_regs[0x60 / 4 + layerNumber] >> 16) & 0x1FF;
}

int CTileGen::GetXScroll(int layerNumber) const
{
	return m_regs[0x60 / 4 + layerNumber] & 0x3FF;
}

bool CTileGen::LineScrollMode(int layerNumber) const
{
	return (m_regs[0x60 / 4 + layerNumber] & 0x8000) != 0;
}

int CTileGen::GetLineScroll(int layerNumber, int yCoord) const
{
	int index = ((0xF6000 + (layerNumber * 0x400)) / 4) + (yCoord / 2);
	int shift = (1 - (yCoord % 2)) * 16;

	return (m_vramP[index] >> shift) & 0xFFFFu;
}

int CTileGen::GetTileNumber(int xCoord, int yCoord, int xScroll, int yScroll) const
{
	int xIndex = ((xCoord + xScroll) / 8) & 0x3F;
	int yIndex = ((yCoord + yScroll) / 8) & 0x3F;

	return (yIndex * 64) + xIndex;
}

int CTileGen::GetTileData(int layerNum, int tileNumber) const
{
	int addressBase = (0xF8000 + (layerNum * 0x2000)) / 4;
	int offset = tileNumber / 2;							// two tiles per 32bit word
	int shift = (1 - (tileNumber % 2)) * 16;				// triple check this

	return (m_vramP[addressBase + offset] >> shift) & 0xFFFFu;
}

int CTileGen::GetVFine(int yCoord, int yScroll) const
{
	return (yCoord + yScroll) & 7;
}

int CTileGen::GetHFine(int xCoord, int xScroll) const
{
	return (xCoord + xScroll) & 7;
}

int CTileGen::GetLineMask(int layerNumber, int yCoord) const
{
	auto shift = (layerNumber < 2) ? 16u : 0u;
	int index = (0xF7000 / 4) + yCoord;

	return ((m_vramP[index] >> shift) & 0xFFFFu);
}

int CTileGen::GetPixelMask(int lineMask, int xCoord) const
{
	int maskTest = 1 << (15 - (xCoord / 32));

	return (lineMask & maskTest) != 0;		// zero means alt layer
}

UINT32 CTileGen::GetColour32(int layer, UINT32 data) const
{
	int a = (((data >> 15) +1) & 1) * 255;
	int b = ((((data >> 10) & 0x1F) * 255) / 31) & a;
	int g = ((((data >> 5 ) & 0x1F) * 255) / 31) & a;
	int r = (((data & 0x1F)         * 255) / 31) & a;

	auto rr = m_colourOffsetRegs[layer].r + r;
	auto gg = m_colourOffsetRegs[layer].g + g;
	auto bb = m_colourOffsetRegs[layer].b + b;

	//std::clamp is embarassingly slow in debug mode .. 

	if (rr > 255)		rr = 255;
	else if (rr < 0)	rr = 0;

	if (gg > 255)		gg = 255;
	else if (gg < 0)	gg = 0;

	if (bb > 255)		bb = 255;
	else if (bb < 0)	bb = 0;

	return (a << 24) | (bb << 16) | (gg << 8) | rr;
}

void CTileGen::Draw4Bit(int tileData, int hFine, int vFine, UINT32* const lineBuffer, const UINT32* const pal, int& x) const
{
	// Tile pattern offset: each tile occupies 32 bytes when using 4-bit pixels (offset of tile pattern within VRAM)
	int patternOffset = ((tileData & 0x3FFF) << 1) | ((tileData >> 15) & 1);
	patternOffset *= 32;
	patternOffset /= 4;

	// Upper color bits; the lower 4 bits come from the tile pattern
	int paletteIndex = tileData & 0x7FF0;

	auto pattern = m_vramP[patternOffset + vFine];

	for (int i = 0; i < 8 - hFine; i++, x++) {
		auto p = (pattern >> ((7 - (hFine + i)) * 4)) & 0xFu;
		auto colour32 = pal[paletteIndex | p];
		if (colour32 < 0x1000000) {
			continue;
		}
		lineBuffer[x] = colour32;
	}
}

/*
    the code above looks hard to understand but unrolled looks something like this
	each line decodes 1 pixel (8 pixels make 1 tile)

	pattern = vram[tileOffset + tileLine];
	*colour16++ = pal[ paletteIndex | ((pattern >> 28) & 0xF) ];
	*colour16++ = pal[ paletteIndex | ((pattern >> 24) & 0xF) ];
	*colour16++ = pal[ paletteIndex | ((pattern >> 20) & 0xF) ];
	*colour16++ = pal[ paletteIndex | ((pattern >> 16) & 0xF) ];
	*colour16++ = pal[ paletteIndex | ((pattern >> 12) & 0xF) ];
	*colour16++ = pal[ paletteIndex | ((pattern >> 8)  & 0xF) ];
	*colour16++ = pal[ paletteIndex | ((pattern >> 4)  & 0xF) ];
	*colour16++ = pal[ paletteIndex | ((pattern >> 0)  & 0xF) ];
*/

void CTileGen::Draw8Bit(int tileData, int hFine, int vFine, UINT32* const lineBuffer, const UINT32* const pal, int& x) const
{
	// Tile pattern offset: each tile occupies 64 bytes when using 8-bit pixels
	int patternOffset = tileData & 0x3FFF;
	patternOffset *= 64;
	patternOffset /= 4;

	// Upper color bits
	int paletteIndex = tileData & 0x7F00;

	auto pattern1 = m_vramP[patternOffset + (vFine * 2)];			// first 4 pixels
	auto pattern2 = m_vramP[patternOffset + (vFine * 2) + 1];		// next 4 pixels

	// might not hit this loop
	for (int i = 0; i < 4 - hFine; i++, x++) {
		auto p = (pattern1 >> ((3 - ((hFine + i) % 4)) * 8)) & 0xFFu;
		auto colour32 = pal[paletteIndex | p];
		if (colour32 < 0x1000000) {
			continue;
		}
		lineBuffer[x] = colour32;
	}

	for (int i = 0; i < 4 - (hFine % 4); i++, x++) {
		auto p = (pattern2 >> ((3 - ((hFine + i) % 4)) * 8)) & 0xFFu;
		auto colour32 = pal[paletteIndex | p];
		if (colour32 < 0x1000000) {
			continue;
		}
		lineBuffer[x] = colour32;
	}
}

void CTileGen::WritePalette(int layer, int address, UINT32 data)
{
	m_pal[layer][address] = GetColour32(layer, data);
}

void CTileGen::RecomputePalettes(int layer)
{
	for (int i = 0; i < 32768; i++) {
		WritePalette(layer, i, m_palP[i]);
	}
}

void CTileGen::DrawLine(int line)
{
	for (int i = 2; i-- > 0;) {

		const int primaryIndex	= i * 2;
		const int altIndex		= (i * 2) + 1;

		bool hasLayer[2] = { IsEnabled(primaryIndex), IsEnabled(altIndex) };
		if (!hasLayer[0] && !hasLayer[1]) {
			continue;	// both disabled let's try next pair
		}

		UINT32* drawLayers[2] = { m_drawSurface[Above3D(primaryIndex)]->GetLine(line), m_drawSurface[Above3D(altIndex)]->GetLine(line) };

		int lineMask	= GetLineMask(primaryIndex, line);
		int scrollX[2]	= { LineScrollMode(primaryIndex) ? GetLineScroll(primaryIndex,line) : GetXScroll(primaryIndex),  LineScrollMode(altIndex) ? GetLineScroll(altIndex,line) : GetXScroll(altIndex) };
		int scrollY[2]	= { GetYScroll(primaryIndex),GetYScroll(altIndex) };
		
		for (int x = 0; x < 496; ) {

			int layer = (GetPixelMask(lineMask, x) + 1) & 1;	// 1 means primary layer, so we flip so it's zero

			if (hasLayer[layer]) {

				const auto index = primaryIndex + layer;

				int tileNumber	= GetTileNumber(x, line, scrollX[layer], scrollY[layer]);
				int	hFine		= GetHFine(x, scrollX[layer]);
				int vFine		= GetVFine(line, scrollY[layer]);
				int tileData	= GetTileData(index, tileNumber);

				if (Is4Bit(index)) {
					Draw4Bit(tileData, hFine, vFine, drawLayers[layer], m_pal[index / 2], x);
				}
				else {
					Draw8Bit(tileData, hFine, vFine, drawLayers[layer], m_pal[index / 2], x);
				}
			}
			else {
				int	hFine = GetHFine(x, 0);
				x += 32 - hFine;
			}
		}

	}
}
