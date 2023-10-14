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
#include "Supermodel.h"

// Macros that divide memory regions into pages and mark them as dirty when they are written to
#define PAGE_WIDTH 10
#define PAGE_SIZE (1<<PAGE_WIDTH)
#define DIRTY_SIZE(arraySize) (1+(arraySize-1)/(8*PAGE_SIZE))
#define MARK_DIRTY(dirtyArray, addr) dirtyArray[addr>>(PAGE_WIDTH+3)] |= 1<<((addr>>PAGE_WIDTH)&7)

// Offsets of memory regions within TileGen memory pool
#define OFFSET_VRAM         0x000000	// VRAM and palette data
#define OFFSET_PAL_A        0x120000	// computed A/A' palette
#define OFFSET_PAL_B		0x140000	// computed B/B' palette 
#define MEM_POOL_SIZE_RW    (0x120000+0x040000)

#define OFFSET_VRAM_RO      0x160000   // [read-only snapshot]
#define OFFSET_PAL_RO_A     0x280000   // [read-only snapshot]
#define OFFSET_PAL_RO_B		0x2A0000
#define MEM_POOL_SIZE_RO    (0x120000+0x040000)

#define OFFSET_VRAM_DIRTY   0x2C0000
#define OFFSET_PAL_A_DIRTY  (OFFSET_VRAM_DIRTY+DIRTY_SIZE(0x120000))
#define OFFSET_PAL_B_DIRTY	(OFFSET_PAL_A_DIRTY+DIRTY_SIZE(0x20000))
#define MEM_POOL_SIZE_DIRTY (DIRTY_SIZE(0x120000)+2*DIRTY_SIZE(0x20000))	// VRAM + 2 palette dirty buffers

#define MEMORY_POOL_SIZE	(MEM_POOL_SIZE_RW+MEM_POOL_SIZE_RO+MEM_POOL_SIZE_DIRTY)


/******************************************************************************
 Save States
******************************************************************************/

void CTileGen::SaveState(CBlockFile *SaveState)
{
	SaveState->NewBlock("Tile Generator", __FILE__);
	SaveState->Write(vram, 0x120000); // Don't write out palette, read-only snapshots or dirty page arrays, just VRAM
	SaveState->Write(regs, sizeof(regs));
}

void CTileGen::LoadState(CBlockFile *SaveState)
{
	if (OKAY != SaveState->FindBlock("Tile Generator"))
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
	SaveState->Read(regs, sizeof(regs));
	
	// If multi-threaded, update read-only snapshots too
	if (m_gpuMultiThreaded)
		UpdateSnapshots(true);
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
	if (!m_gpuMultiThreaded)
		return 0;
	
	// Update read-only snapshots
	return UpdateSnapshots(false);
}

UINT32 CTileGen::UpdateSnapshot(bool copyWhole, UINT8 *src, UINT8 *dst, unsigned size, UINT8 *dirty)
{
	unsigned dirtySize = DIRTY_SIZE(size);
	if (copyWhole)
	{
		// If updating whole region, then just copy all data in one go
		memcpy(dst, src, size);
		memset(dirty, 0, dirtySize);
		return size;
	}
	else
	{
		// Otherwise, loop through dirty pages array to find out what needs to be updated and copy only those parts
		UINT32 copied = 0;
		UINT8 *pSrc = src;
		UINT8 *pDst = dst;
		for (unsigned i = 0; i < dirtySize; i++)
		{
			UINT8 d = dirty[i];
			if (d)
			{
				for (unsigned j = 0; j < 8; j++)
				{
					if (d&1)
					{
						// If not at very end of region, then copy an extra 4 bytes to allow for a possible 32-bit overlap
						UINT32 toCopy = (i < dirtySize - 1 || j < 7 ? PAGE_SIZE + 4 : PAGE_SIZE);
						memcpy(pDst, pSrc, toCopy);
						copied += toCopy;
					}
					d >>= 1;
					pSrc += PAGE_SIZE;	
					pDst += PAGE_SIZE;
				}
				dirty[i] = 0;
			}
			else
			{
				pSrc += 8 * PAGE_SIZE;	
				pDst += 8 * PAGE_SIZE;
			}
		}
		return copied;
	}
}

UINT32 CTileGen::UpdateSnapshots(bool copyWhole)
{
	// Update all memory region snapshots
	UINT32 palACopied  = UpdateSnapshot(copyWhole, (UINT8*)pal[0],  (UINT8*)palRO[0],  0x020000, palDirty[0]);
	UINT32 palBCopied  = UpdateSnapshot(copyWhole, (UINT8*)pal[1],  (UINT8*)palRO[1],  0x020000, palDirty[1]);
	UINT32 vramCopied = UpdateSnapshot(copyWhole, (UINT8*)vram, (UINT8*)vramRO, 0x120000, vramDirty);
	memcpy(regsRO, regs, sizeof(regs)); // Always copy whole of regs buffer
	//printf("TileGen copied - palA:%4uK, palB:%4uK, vram:%4uK, regs:%uK\n", palACopied / 1024, palBCopied / 1024, vramCopied / 1024, sizeof(regs) / 1024);
	return palACopied + palBCopied + vramCopied + sizeof(regs);
}

void CTileGen::BeginFrame(void)
{
	// NOTE: Render2D->WriteVRAM(addr, data) is no longer being called for RAM addresses that are written
	// to and instead this class relies upon the fact that Render2D currently marks everything as dirty
	// with every frame.  If this were to change in the future then code to handle marking the correct
	// parts of the renderer as dirty would need to be added here.
	
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

UINT32 CTileGen::ReadRAM32(unsigned addr)
{
	return *(UINT32 *) &vram[addr];
}

void CTileGen::WriteRAM32(unsigned addr, UINT32 data)
{
	if (m_gpuMultiThreaded)
		MARK_DIRTY(vramDirty, addr);
	*(UINT32 *) &vram[addr] = data;
}

//TODO: 8- and 16-bit handlers have not been thoroughly tested
uint8_t CTileGen::ReadRAM8(unsigned addr)
{
  return vram[addr];
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
uint16_t CTileGen::ReadRAM16(unsigned addr)
{
  return *((uint16_t *) &vram[addr]);
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

UINT32 CTileGen::ReadRegister(unsigned reg)
{
  reg &= 0xFF;
  return regs[reg/4];
}

void CTileGen::WriteRegister(unsigned reg, UINT32 data)
{
	reg &= 0xFF;
		
	switch (reg)
	{
  case 0x00:
	case 0x08:
	case 0x0C:
	case 0x20:
	case 0x60:
	case 0x64:
	case 0x68:
	case 0x6C:
		break;
	case 0x40:	// layer A/A' color offset
	case 0x44:	// layer B/B' color offset
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
	regs[reg/4] = data;
}

void CTileGen::Reset(void)
{
	unsigned memSize = (m_gpuMultiThreaded ? MEMORY_POOL_SIZE : MEM_POOL_SIZE_RW);
	memset(memoryPool, 0, memSize);
	memset(regs, 0, sizeof(regs));
	memset(regsRO, 0, sizeof(regsRO));

	DebugLog("Tile Generator reset\n");
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void CTileGen::AttachRenderer(CRender2D *Render2DPtr)
{
	Render2D = Render2DPtr;

	// If multi-threaded, attach read-only snapshots to renderer instead of real ones
	if (m_gpuMultiThreaded)
	{
		Render2D->AttachVRAM(vramRO);
		Render2D->AttachPalette((const UINT32 **)palRO);
		Render2D->AttachRegisters(regsRO);
	}
	else
	{
		Render2D->AttachVRAM(vram);
		Render2D->AttachPalette((const UINT32 **)pal);
		Render2D->AttachRegisters(regs);
	}

	DebugLog("Tile Generator attached a Render2D object\n");
}


bool CTileGen::Init(CIRQ *IRQObjectPtr)
{
	unsigned memSize   = (m_gpuMultiThreaded ? MEMORY_POOL_SIZE : MEM_POOL_SIZE_RW);
	float	 memSizeMB = (float)memSize/(float)0x100000;
	
	// Allocate all memory for all TileGen RAM regions
	memoryPool = new(std::nothrow) UINT8[memSize];
	if (NULL == memoryPool)
		return ErrorLog("Insufficient memory for tile generator object (needs %1.1f MB).", memSizeMB);
	
	// Set up main pointers
	vram = (UINT8 *) &memoryPool[OFFSET_VRAM];
	pal[0] = (UINT32 *) &memoryPool[OFFSET_PAL_A];
	pal[1] = (UINT32 *) &memoryPool[OFFSET_PAL_B];

	// If multi-threaded, set up pointers for read-only snapshots and dirty page arrays too
	if (m_gpuMultiThreaded)
	{
		vramRO = (UINT8 *) &memoryPool[OFFSET_VRAM_RO];
		palRO[0] = (UINT32 *) &memoryPool[OFFSET_PAL_RO_A];
		palRO[1] = (UINT32 *) &memoryPool[OFFSET_PAL_RO_B];
		vramDirty = (UINT8 *) &memoryPool[OFFSET_VRAM_DIRTY];
		palDirty[0] = (UINT8 *) &memoryPool[OFFSET_PAL_A_DIRTY];
		palDirty[1] = (UINT8 *) &memoryPool[OFFSET_PAL_B_DIRTY];
	}

	// Hook up the IRQ controller
	IRQ = IRQObjectPtr;
	
	DebugLog("Initialized Tile Generator (allocated %1.1f MB and connected to IRQ controller)\n", memSizeMB);
	return OKAY;
}

CTileGen::CTileGen(const Util::Config::Node &config)
  : m_config(config),
    m_gpuMultiThreaded(config["GPUMultiThreaded"].ValueAs<bool>())
{
	IRQ = NULL;
	memoryPool = NULL;
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
		
	IRQ = NULL;
	if (memoryPool != NULL)
	{
		delete [] memoryPool;
		memoryPool = NULL;
	}
	DebugLog("Destroyed Tile Generator\n");
}
