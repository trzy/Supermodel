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
 * Real3D.cpp
 *
 * The Model 3's Real3D-based graphics hardware. Based on the Real3D Pro-1000
 * family of image generators.
 *
 * PCI IDs
 * -------
 * It appears that accessing the PCI configuration space returns the PCI ID
 * of Mercury (0x16C311DB) on Step 1.x and the DMA device (0x178611DB) on
 * Step 2.x, while accessing the Step 2.x DMA device register returns the
 * PCI ID of Mercury. Step 2.x games by AM3 expect this behavior.
 *
 * To-Do List
 * ----------
 * - For consistency, the status registers should probably be byte reversed (this is a
 *   little endian device), forcing the Model3 Read32/Write32 handlers to
 *   manually reverse the data. This keeps with the convention for VRAM.
 * - Keep an eye out for games writing non-mipmap textures to the mipmap area.
 *   The render currently cannot cope with this.
 */

#include "Real3D.h"

#include "Supermodel.h"
#include "JTAG.h"
#include "CPU/PowerPC/ppc.h"
#include "Util/BMPFile.h"
#include <cstring>
#include <algorithm>

// Macros that divide memory regions into pages and mark them as dirty when they are written to
#define PAGE_WIDTH 12
#define PAGE_SIZE (1<<PAGE_WIDTH)
#define DIRTY_SIZE(arraySize) (1+(arraySize-1)/(8*PAGE_SIZE))
#define MARK_DIRTY(dirtyArray, addr) dirtyArray[addr>>(PAGE_WIDTH+3)] |= 1<<((addr>>PAGE_WIDTH)&7)

// Offsets of memory regions within Real3D memory pool
#define OFFSET_8C           0x0000000 // 4 MB, culling RAM low (at 0x8C000000)
#define OFFSET_8E           0x0400000 // 1 MB, culling RAM high (at 0x8E000000)
#define OFFSET_98           0x0500000 // 4 MB, polygon RAM (at 0x98000000)
#define OFFSET_TEXRAM       0x0900000 // 8 MB, texture RAM
#define OFFSET_TEXFIFO      0x1100000 // 1 MB, texture FIFO
#define MEM_POOL_SIZE_RW    (0x400000+0x100000+0x400000+0x800000+0x100000)
#define OFFSET_8C_RO        0x1200000 // 4 MB, culling RAM low (at 0x8C000000)  [read-only snapshot]
#define OFFSET_8E_RO        0x1600000 // 1 MB, culling RAM high (at 0x8E000000) [read-only snapshot]
#define OFFSET_98_RO        0x1700000 // 4 MB, polygon RAM (at 0x98000000)      [read-only snapshot]
#define OFFSET_TEXRAM_RO    0x1B00000 // 8 MB, texture RAM                      [read-only snapshot]
#define MEM_POOL_SIZE_RO    (0x400000+0x100000+0x400000+0x800000)
#define OFFSET_8C_DIRTY     0x2300000
#define OFFSET_8E_DIRTY     (OFFSET_8C_DIRTY+DIRTY_SIZE(0x400000))
#define OFFSET_98_DIRTY     (OFFSET_8E_DIRTY+DIRTY_SIZE(0x100000))
#define OFFSET_TEXRAM_DIRTY (OFFSET_98_DIRTY+DIRTY_SIZE(0x400000))
#define MEM_POOL_SIZE_DIRTY (DIRTY_SIZE(MEM_POOL_SIZE_RO))
#define MEMORY_POOL_SIZE  (MEM_POOL_SIZE_RW+MEM_POOL_SIZE_RO+MEM_POOL_SIZE_DIRTY)

static void UpdateRenderConfig(IRender3D *Render3D, uint64_t internalRenderConfig[]);


/******************************************************************************
 Save States
******************************************************************************/

void CReal3D::SaveState(CBlockFile *SaveState)
{
  SaveState->NewBlock("Real3D", __FILE__);

  SaveState->Write(memoryPool, MEM_POOL_SIZE_RW); // Don't write out read-only snapshots or dirty page arrays
  SaveState->Write(&fifoIdx, sizeof(fifoIdx));
  SaveState->Write(m_vromTextureFIFO, sizeof(m_vromTextureFIFO));

  SaveState->Write(&dmaSrc, sizeof(dmaSrc));
  SaveState->Write(&dmaDest, sizeof(dmaDest));
  SaveState->Write(&dmaLength, sizeof(dmaLength));
  SaveState->Write(&dmaData, sizeof(dmaData));
  SaveState->Write(&dmaUnknownReg, sizeof(dmaUnknownReg));
  SaveState->Write(&dmaStatus, sizeof(dmaStatus));
  SaveState->Write(&dmaConfig, sizeof(dmaConfig));

  // These used to be occupied by JTAG state
  SaveState->Write(m_internalRenderConfig, sizeof(m_internalRenderConfig));
  SaveState->Write(commandPortWritten);
  SaveState->Write(&m_pingPong, sizeof(m_pingPong));
  for (int i = 0; i < 39; i++)
  {
    uint8_t nul = 0;
    SaveState->Write(&nul, sizeof(uint8_t));
  }

  SaveState->Write(&m_vromTextureFIFOIdx, sizeof(m_vromTextureFIFOIdx));
}

void CReal3D::LoadState(CBlockFile *SaveState)
{
  if (OKAY != SaveState->FindBlock("Real3D"))
  {
    ErrorLog("Unable to load Real3D GPU state. Save state file is corrupt.");
    return;
  }

  SaveState->Read(memoryPool, MEM_POOL_SIZE_RW);

  // If multi-threaded, update read-only snapshots too
  if (m_gpuMultiThreaded)
    UpdateSnapshots(true);
  Render3D->UploadTextures(0, 0, 0, 2048, 2048);
  SaveState->Read(&fifoIdx, sizeof(fifoIdx));
  SaveState->Read(&m_vromTextureFIFO, sizeof(m_vromTextureFIFO));

  SaveState->Read(&dmaSrc, sizeof(dmaSrc));
  SaveState->Read(&dmaDest, sizeof(dmaDest));
  SaveState->Read(&dmaLength, sizeof(dmaLength));
  SaveState->Read(&dmaData, sizeof(dmaData));
  SaveState->Read(&dmaUnknownReg, sizeof(dmaUnknownReg));
  SaveState->Read(&dmaStatus, sizeof(dmaStatus));
  SaveState->Read(&dmaConfig, sizeof(dmaConfig));

  SaveState->Read(m_internalRenderConfig, sizeof(m_internalRenderConfig));
  UpdateRenderConfig(Render3D, m_internalRenderConfig);
  SaveState->Read(&commandPortWritten);
  SaveState->Read(&m_pingPong, sizeof(m_pingPong));
  for (int i = 0; i < 39; i++)
  {
    uint8_t nul;
    SaveState->Read(&nul, sizeof(uint8_t));
  }

  SaveState->Read(&m_vromTextureFIFOIdx, sizeof(m_vromTextureFIFOIdx));
}


/******************************************************************************
 Rendering
******************************************************************************/

static void UpdateRenderConfig(IRender3D *Render3D, uint64_t internalRenderConfig[])
{
  bool noSunClamp = (internalRenderConfig[0] & 0x800000) != 0 && (internalRenderConfig[1] & 0x400000) != 0;
  bool shadeIsSigned = (internalRenderConfig[0] & 0x1) == 0;
  Render3D->SetSunClamp(!noSunClamp);
  Render3D->SetSignedShade(shadeIsSigned);
}

void CReal3D::BeginVBlank(int statusCycles)
{
  // Calculate point at which status bit should change value.  Currently the same timing is used for both the status bit in ReadRegister
  // and in WriteDMARegister32/ReadDMARegister32, however it may be that they are completely unrelated.  It appears that step 1.x games
  // access just the former while step 2.x access the latter.  It is not known yet what this bit/these bits actually represent.
	statusChange = ppc_total_cycles() + statusCycles;
	m_evenFrame = !m_evenFrame;
}

void CReal3D::EndVBlank(void)
{
  error = false;  // clear error (just needs to be done once per frame)
}

uint32_t CReal3D::SyncSnapshots(void)
{
  // Update read-only copy of command port flag
  commandPortWrittenRO = commandPortWritten;
  commandPortWritten = false;

  if (!m_gpuMultiThreaded)
    return 0;

  // Update read-only queue
  queuedUploadTexturesRO = queuedUploadTextures;
  queuedUploadTextures.clear();

  // Update read-only snapshots
  return UpdateSnapshots(false);
}

uint32_t CReal3D::UpdateSnapshot(bool copyWhole, uint8_t *src, uint8_t *dst, unsigned size, uint8_t *dirty)
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
    uint32_t copied = 0;
    uint8_t *pSrc = src;
    uint8_t *pDst = dst;
    for (unsigned i = 0; i < dirtySize; i++)
    {
      uint8_t d = dirty[i];
      if (d)
      {
        for (unsigned j = 0; j < 8; j++)
        {
          if (d&1)
          {
            // If not at very end of region, then copy an extra 4 bytes to allow for a possible 32-bit overlap
            uint32_t toCopy = (i < dirtySize - 1 || j < 7 ? PAGE_SIZE + 4 : PAGE_SIZE);
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

uint32_t CReal3D::UpdateSnapshots(bool copyWhole)
{
  // Update all memory region snapshots
  uint32_t cullLoCopied  = UpdateSnapshot(copyWhole, (uint8_t*)cullingRAMLo, (uint8_t*)cullingRAMLoRO, 0x400000, cullingRAMLoDirty);
  uint32_t cullHiCopied  = UpdateSnapshot(copyWhole, (uint8_t*)cullingRAMHi, (uint8_t*)cullingRAMHiRO, 0x100000, cullingRAMHiDirty);
  uint32_t polyCopied    = UpdateSnapshot(copyWhole, (uint8_t*)polyRAM,      (uint8_t*)polyRAMRO,      0x400000, polyRAMDirty);
  uint32_t textureCopied = UpdateSnapshot(copyWhole, (uint8_t*)textureRAM,   (uint8_t*)textureRAMRO,   0x800000, textureRAMDirty);
  //printf("Read3D copied - cullLo:%4uK, cullHi:%4uK, poly:%4uK, texture:%4uK\n", cullLoCopied / 1024, cullHiCopied / 1024, polyCopied / 1024, textureCopied / 1024);
  return cullLoCopied + cullHiCopied + polyCopied + textureCopied;
}

void CReal3D::BeginFrame(void)
{
  // If multi-threaded, perform now any queued texture uploads to renderer before rendering begins
  if (m_gpuMultiThreaded)
  {
    for (const auto &it : queuedUploadTexturesRO) {
      Render3D->UploadTextures(it.level, it.x, it.y, it.width, it.height);
    }

    // done syncing data
    queuedUploadTexturesRO.clear();
  }

  Render3D->BeginFrame();
}

void CReal3D::RenderFrame(void)
{
  //if (commandPortWrittenRO)
    Render3D->RenderFrame();
}

void CReal3D::EndFrame(void)
{
  Render3D->EndFrame();
}


/******************************************************************************
 Texture Uploading and Decoding
******************************************************************************/

// Mipmap coordinates for each reduction level (within a single 2048x1024 page)

static const int mipXBase[] = { 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
static const int mipYBase[] = { 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023 };
static const int mipDivisor[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };

// Tables of texel offsets corresponding to an NxN texel texture tile

static const unsigned decode8x8[64] =
{
   1, 0, 5, 4, 9, 8,13,12,
   3, 2, 7, 6,11,10,15,14,
  17,16,21,20,25,24,29,28,
  19,18,23,22,27,26,31,30,
  33,32,37,36,41,40,45,44,
  35,34,39,38,43,42,47,46,
  49,48,53,52,57,56,61,60,
  51,50,55,54,59,58,63,62
};

static const unsigned decode8x4[32] =
{
   1, 0, 5, 4,
   3, 2, 7, 6,
   9, 8,13,12,
  11,10,15,14,
  17,16,21,20,
  19,18,23,22,
  25,24,29,28,
  27,26,31,30
};

static const unsigned decode8x2[16] =
{
   1, 0,
   3, 2,
   5, 4,
   7, 6,
   9, 8,
  11, 10,
  13, 12,
  15, 14
};

static const unsigned decode8x1[8] =
{
  1,
  3,
  0,
  2,
  5,
  7,
  4,
  6
};

void CReal3D::StoreTexture(unsigned level, unsigned xPos, unsigned yPos, unsigned width, unsigned height, const uint16_t *texData, bool sixteenBit, bool writeLSB, bool writeMSB, uint32_t &texDataOffset)
{
  uint32_t tileX = (std::min)(8u, width);
  uint32_t tileY = (std::min)(8u, height);

  texDataOffset = 0;

  if (sixteenBit)  // 16-bit textures
  {
    // Outer 2 loops: NxN tiles
    for (uint32_t y = yPos; y < (yPos + height); y += tileY)
    {
      for (uint32_t x = xPos; x < (xPos + width); x += tileX)
      {
        // Inner 2 loops: NxN texels for the current tile
        uint32_t destOffset = y * 2048 + x;
        for (uint32_t yy = 0; yy < tileY; yy++)
        {
          for (uint32_t xx = 0; xx < tileX; xx++)
          {
            if (m_gpuMultiThreaded)
              MARK_DIRTY(textureRAMDirty, destOffset * 2);
            if (tileX == 1) texData -= tileY;
            if (tileY == 1) texData -= tileX;
            if (tileX == 8)
              textureRAM[destOffset++] = texData[decode8x8[yy * tileX + xx]];
            else if (tileX == 4)
              textureRAM[destOffset++] = texData[decode8x4[yy * tileX + xx]];
            else if (tileX == 2)
              textureRAM[destOffset++] = texData[decode8x2[yy * tileX + xx]];
            else if (tileX == 1)
              textureRAM[destOffset++] = texData[decode8x1[yy * tileX + xx]];
            texDataOffset++;
          }
          destOffset += 2048 - tileX; // next line
        }
        texData += tileY * tileX; // next tile
      }
    }
  }
  else  // 8-bit textures
  {
    /*
     * 8-bit textures appear to be unpacked into 16-bit words in the
     * texture RAM. Oddly, the rows of the decoding table seem to be
     * swapped.
     */

    if (writeLSB && writeMSB)  // write to both?
      DebugLog("Observed 8-bit texture with byte_select=3!");

    // Outer 2 loops: NxN tiles
    const uint8_t byteSelect = (uint8_t)writeLSB | ((uint8_t)writeMSB << 1);
    uint16_t tempData;
    const uint16_t byteMask[4] = {0xFFFF, 0xFF00, 0x00FF, 0x0000};
    for (uint32_t y = yPos; y < (yPos + height); y += tileY)
    {
      for (uint32_t x = xPos; x < (xPos + width); x += tileX)
      {
        // Inner 2 loops: NxN texels for the current tile
        uint32_t destOffset = y * 2048 + x;
        for (uint32_t yy = 0; yy < tileY; yy++)
        {
          for (uint32_t xx = 0; xx < tileX; xx++)
          {
            if (writeLSB | writeMSB) {
              if (m_gpuMultiThreaded)
                MARK_DIRTY(textureRAMDirty, destOffset * 2);
              textureRAM[destOffset] &= byteMask[byteSelect];
              const uint8_t shift = (8 * ((xx & 1) ^ 1));
              const uint8_t index = (yy ^ 1) * tileX + (xx ^ 1) - (tileX & 1);
              if (tileX == 1) texData -= tileY;
              if (tileY == 1) texData -= tileX;
              if (tileX == 8)
                tempData = (texData[decode8x8[index] / 2] >> shift) & 0xFF;
              else if (tileX == 4)
                tempData = (texData[decode8x4[index] / 2] >> shift) & 0xFF;
              else if (tileX == 2)
                tempData = (texData[decode8x2[index] / 2] >> shift) & 0xFF;
              else if (tileX == 1)
                tempData = (texData[decode8x1[index] / 2] >> shift) & 0xFF;
              tempData |= tempData << 8;
              tempData &= byteMask[byteSelect] ^ 0xFFFF;
              textureRAM[destOffset] |= tempData;
            }
            destOffset++;
          }
          destOffset += 2048 - tileX; // next line
        }
        uint32_t offset = (std::max)(1u, (tileY * tileX) / 2);
        texData += offset; // next tile
        texDataOffset += offset; // next tile
      }
    }
  }

  // Signal to renderer that textures have changed
  // TO-DO: mipmaps? What if a game writes non-mipmap textures to mipmap area?
  if (m_gpuMultiThreaded)
  {
    // If multi-threaded, then queue calls to UploadTextures for render thread to perform at beginning of next frame
    QueuedUploadTextures upl;
    upl.level = level;
    upl.x = xPos;
    upl.y = yPos;
    upl.width = width;
    upl.height = height;
    queuedUploadTextures.push_back(upl);
  }
  else
    Render3D->UploadTextures(level, xPos, yPos, width, height);
}

/*
Texture header:
-------- -------- -------- --xxxxxx X-position
-------- -------- ----xxxx x------- Y-position
-------- -------x xx------ -------- Width
-------- ----xxx- -------- -------- Height
-------- ---x---- -------- -------- Texture page
-------- --x----- -------- -------- Write 8-bit data to the lower byte of texel
-------- -x------ -------- -------- Write 8-bit data to the upper byte of texel
-------- x------- -------- -------- Bitdepth, 0 = 8-bit, 1 = 16-bit
xxxxxxxx -------- -------- -------- Texture type:
                                      0x00 = texture with mipmaps
                                      0x01 = texture without mipmaps
                                      0x02 = only mipmaps
                                      0x80 = possibly gamma table
*/

// Texture data will be in little endian format
void CReal3D::UploadTexture(uint32_t header, const uint16_t *texData)
{
  // Position: texture RAM is arranged as 2 2048x1024 texel sheets
  uint32_t x              = 32 * (header & 0x3F);
  uint32_t y              = 32 * ((header >> 7) & 0x1F);
  uint32_t page           = (header >> 20) & 1;
  uint32_t width          = 32 << ((header >> 14) & 7);
  uint32_t height         = 32 << ((header >> 17) & 7);
  uint32_t type           = (header >> 24) & 0xFF;
  bool     sixteenBit     = (header >> 23) & 0x1;
  bool     writeUpperByte = (header >> 22) & 0x1;
  bool     writeLowerByte = (header >> 21) & 0x1;
  uint32_t offset         = 0;

  switch (type)
  {
  case 0x00:  // texture w/ mipmaps
  case 0x01:  // texture w/out mipmaps
    StoreTexture(0, x, y + (page * 1024), width, height, texData, sixteenBit, writeLowerByte, writeUpperByte, offset);
    texData += offset;
    if (type == 0x01) {
      break;
    }
  case 0x02:  // mipmaps only
  {
    for (int i = 1; width > 0 && height > 0; i++) {

      int xPos = mipXBase[i] + (x / mipDivisor[i]);
      int yPos = mipYBase[i] + (y / mipDivisor[i]);

      width /= 2;
      height /= 2;

      StoreTexture(i, xPos, yPos + (page * 1024), width, height, texData, sixteenBit, writeLowerByte, writeUpperByte, offset);
      texData += offset;
    }
    break;
  }
  case 0x80:  // MAME thinks these might be a gamma table (vf3 uploads this as the first texture)
    break;
  default:  // unknown
    DebugLog("Unknown texture format %02X\n", type);
    break;
  }
}


/******************************************************************************
 DMA Device

 Register 0xC:
 -------------
 +---+---+---+---+---+---+---+---+
 |BUS|???|???|???|???|???|???|IRQ|
 +---+---+---+---+---+---+---+---+
  BUS:  Busy (see von2 0x18A104) if 1.
  IRQ:  IRQ pending.
******************************************************************************/

void CReal3D::DMACopy(void)
{
  DebugLog("Real3D DMA copy (PC=%08X, LR=%08X): %08X -> %08X, %X %s\n", ppc_get_pc(), ppc_get_lr(), dmaSrc, dmaDest, dmaLength*4, (dmaConfig&0x80)?"(byte reversed)":"");
  //printf("Real3D DMA copy (PC=%08X, LR=%08X): %08X -> %08X, %X %s\n", ppc_get_pc(), ppc_get_lr(), dmaSrc, dmaDest, dmaLength*4, (dmaConfig&0x80)?"(byte reversed)":"");
  if ((dmaConfig&0x80)) // reverse bytes
  {
    while (dmaLength != 0)
    {
      uint32_t  data = Bus->Read32(dmaSrc);
      Bus->Write32(dmaDest, FLIPENDIAN32(data));
      dmaSrc += 4;
      dmaDest += 4;
      --dmaLength;
    }
  }
  else
  {
    while (dmaLength != 0)
    {
      Bus->Write32(dmaDest, Bus->Read32(dmaSrc));
      dmaSrc += 4;
      dmaDest += 4;
      --dmaLength;
    }
  }
}

uint8_t CReal3D::ReadDMARegister8(unsigned reg)
{
  switch (reg)
  {
  case 0xC: // status
    return dmaStatus;
  case 0xE: // configuration
    return  dmaConfig;
  default:
    break;
  }

  DebugLog("Real3D: ReadDMARegister8: reg=%X\n", reg);
  return 0;
}

void CReal3D::WriteDMARegister8(unsigned reg, uint8_t data)
{
  switch (reg)
  {
  case 0xD: // IRQ acknowledge
    if ((data&1))
    {
      dmaStatus &= ~1;
      IRQ->Deassert(dmaIRQ);
    }
    break;
  case 0xE: // configuration
    dmaConfig = data;
    break;
  default:
    DebugLog("Real3D: WriteDMARegister8: reg=%X, data=%02X\n", reg, data);
    break;
  }
  //DebugLog("Real3D: WriteDMARegister8: reg=%X, data=%02X\n", reg, data);
}

uint32_t CReal3D::ReadDMARegister32(unsigned reg)
{
  switch (reg)
  {
  case 0x14:  // command result
    return dmaData;
  default:
    break;
  }

  DebugLog("Real3D: ReadDMARegister32: reg=%X\n", reg);
  return 0;
}

void CReal3D::WriteDMARegister32(unsigned reg, uint32_t data)
{
  switch (reg)
  {
  case 0x00:  // DMA source address
    dmaSrc = data;
    break;
  case 0x04:  // DMA destination address
    dmaDest = data;
    break;
  case 0x08:  // DMA length
    dmaLength = data;
    DMACopy();
    if (dmaConfig & 1)  // only fire an IRQ if the low bit of dmaConfig is set
    {
        dmaStatus |= 1;
        IRQ->Assert(dmaIRQ);
    }
    break;
  case 0x10:  // command register
    if ((data&0x20000000)) // DMA ID command
    {
      // Games requesting PCI ID via the DMA device expect 0x16C311DB, even on step 2.x boards
      dmaData = PCIID::Step1x;
      DebugLog("Real3D: DMA ID command issued (ATTENTION: make sure we're returning the correct value), PC=%08X, LR=%08X\n", ppc_get_pc(), ppc_get_lr());
    }
    else if ((data&0x80000000))
    {
      dmaData = ReadRegister(data & 0x3F);
    }
    break;
  case 0x14:  // ?
    dmaData = 0xFFFFFFFF;
    break;
  default:
    DebugLog("Real3D: WriteDMARegister32: reg=%X, data=%08X\n", reg, data);
    break;
  }
  //DebugLog("Real3D: WriteDMARegister32: reg=%X, data=%08X\n", reg, data);
}

/******************************************************************************
 Basic Emulation Functions, Registers, Memory, and Texture FIFO
******************************************************************************/

void CReal3D::Flush(void)
{
  commandPortWritten = true;
  DebugLog("Real3D 88000000 written @ PC=%08X\n", ppc_get_pc());

  // Upload textures (if any)
  if (fifoIdx > 2) // If the texture header/data aren't present, discard the texture (prevents garbage textures in Ski Champ)
  {
    for (uint32_t i = 0; i < fifoIdx - 2; )
    {
      uint32_t size = 2+textureFIFO[i+0]/2;
      size /= 4;
      uint32_t header = textureFIFO[i+1]; // texture information header

      // Spikeout seems to be uploading 0 length textures
      if (0 == size)
      {
        DebugLog("Real3D: 0-length texture upload @ PC=%08X (%08X %08X %08X)\n", ppc_get_pc(), textureFIFO[i+0], textureFIFO[i+1], textureFIFO[i+2]);
        break;
      }

      UploadTexture(header,(uint16_t *)&textureFIFO[i+2]);
      DebugLog("Real3D: Texture upload completed: %X bytes (%X)\n", size*4, textureFIFO[i+0]);

      i += size;
    }
  }

  // Reset texture FIFO
  fifoIdx = 0;
}

void CReal3D::WriteTextureFIFO(uint32_t data)
{
  if (fifoIdx >= (0x100000/4))
  {
    if (!error)
      ErrorLog("Overflow in Real3D texture FIFO!");
    error = true;
  }
  else
    textureFIFO[fifoIdx++] = data;
}

void CReal3D::WriteTexturePort(unsigned reg, uint32_t data)
{
  if (step == 0x10)
  {
    uint32_t addr = data & 0xFFFFFF;
    uint32_t num_words = (2+vrom[addr+0]/2) / 4;
    if (!num_words)
    {
      DebugLog("Real3D: 0-length VROM texture upload @ PC=%08X (%08X)\n", ppc_get_pc(), data);
      return;
    }
    for (uint32_t i = 0; i < num_words; i++)
      WriteTextureFIFO(vrom[(addr + i) & 0xFFFFFF]);
  }
  else
  {
    if (m_vromTextureFIFOIdx == 2)
    {
      uint32_t addr = m_vromTextureFIFO[0];
      uint32_t header = m_vromTextureFIFO[1];
      UploadTexture(header, (const uint16_t *) &vrom[addr & 0xFFFFFF]);
      m_vromTextureFIFOIdx = 0;
    }
    else
      m_vromTextureFIFO[m_vromTextureFIFOIdx++] = data;
  }
}

void CReal3D::WriteLowCullingRAM(uint32_t addr, uint32_t data)
{
  if (m_gpuMultiThreaded)
    MARK_DIRTY(cullingRAMLoDirty, addr);
  cullingRAMLo[addr/4] = data;
}

void CReal3D::WriteHighCullingRAM(uint32_t addr, uint32_t data)
{
  if (m_gpuMultiThreaded)
    MARK_DIRTY(cullingRAMHiDirty, addr);
  cullingRAMHi[addr/4] = data;
}

void CReal3D::WritePolygonRAM(uint32_t addr, uint32_t data)
{
  if (m_gpuMultiThreaded)
    MARK_DIRTY(polyRAMDirty, addr);
  polyRAM[addr/4] = data;
}

// Internal registers accessible via JTAG port
void CReal3D::WriteJTAGRegister(uint64_t instruction, uint64_t data)
{
  if (instruction == CJTAG::Instruction::SetReal3DRenderConfig0)
    m_internalRenderConfig[0] = data;
  else if (instruction == CJTAG::Instruction::SetReal3DRenderConfig1)
    m_internalRenderConfig[1] = data;
  UpdateRenderConfig(Render3D, m_internalRenderConfig);
}

// Registers correspond to the Stat_Pckt in the Real3d sdk

/*
Stat Packet

0x00:   xxxx---- -------- -------- --------     spare1
        ----x--- -------- -------- --------     gp_done
        -----x-- -------- -------- --------     dp_done
        ------x- -------- -------- --------     ping_pong
        -------x -------- -------- --------     update_done
        -------- x------- -------- --------     rend_done
        -------- -xxxxxxx xxxxxxxx xxxxxxxx     tot_clks 23bit val (0x7FFFFF). This is a 33.33mhz clock value.
                                                Think this is the time the GPU takes to process the frame, used by software to
                                                estimate the frame rate.

0x01:   -------- -------- -------- --------     spare2
        -------- -xxxxxxx xxxxxxxx xxxxxxxx     vpt0_clks - not sure what this is used for (if anything). It's not used by the SDK

0x02:   -------- -------- -------- --------     spare3
        -------- -xxxxxxx xxxxxxxx xxxxxxxx     vpt1_clks - not sure what this is used for (if anything). It's not used by the SDK

0x03:   -------- -------- -------- --------     spare4
        -------- -xxxxxxx xxxxxxxx xxxxxxxx     vpt2_clks - not sure what this is used for (if anything). It's not used by the SDK

0x04:   -------- -------- -------- --------     spare5
        -------- -xxxxxxx xxxxxxxx xxxxxxxx     vpt3_clks - not sure what this is used for (if anything). It's not used by the SDK


0x05:   range0      (float) Line of sight value for priority level 0
0x06:   range1      (float) Line of sight value for priority level 1
0x07:   range2      (float) Line of sight value for priority level 2
0x08:   range3      (float) Line of sight value for priority level 3
0x09:   ls_cycle    (uint32) Think this is the frame number, don't think it's used by model3, since games never read this far into memory
*/
uint32_t CReal3D::ReadRegister(unsigned reg)
{
  DebugLog("Real3D: Read reg %X\n", reg);
  if (reg == 0)
  {
	  uint32_t ping_pong;

	  if (m_evenFrame) {
			ping_pong = (ppc_total_cycles() >= statusChange ? 0x0 : 0x02000000);
	  }
	  else {
			ping_pong = (ppc_total_cycles() >= statusChange ? 0x02000000 : 0x0);
	  }

		return 0xfdffffff | ping_pong;
  }

  else if (reg >= 20 && reg<=32) {	// line of sight registers

	int index = (reg - 20) / 4;
	float val = Render3D->GetLosValue(index);
	return *(uint32_t*)(&val);
  }

  return 0xffffffff;
}

// TODO: This returns data in the way that the PowerPC bus expects. Other functions in CReal3D should
// return data this way.
uint32_t CReal3D::ReadPCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset)
{
  uint32_t  d;

  if ((bits==8))
  {
    DebugLog("Real3D: %d-bit PCI read request for reg=%02X\n", bits, reg);
    return 0;
  }

  // This is a little endian device, must return little endian words
  switch (reg)
  {
  case 0x00:  // Device ID and Vendor ID
    d = FLIPENDIAN32(pciID);
    switch (bits)
    {
    case 8:
      d >>= (3-offset)*8; // offset will be 0-3; select appropriate byte
      d &= 0xFF;
      break;
    case 16:
      d >>= (2-offset)*8; // offset will be 0 or 2 only; select either high or low word
      d &= 0xFFFF;
      break;
    default:
      break;
    }
    DebugLog("Real3D: PCI ID read. Returning %X (%d-bits). PC=%08X, LR=%08X\n", d, bits, ppc_get_pc(), ppc_get_lr());
    return d;
  default:
    DebugLog("Real3D: PCI read request for reg=%02X (%d-bit)\n", reg, bits);
    break;
  }

  return 0;
}

void CReal3D::WritePCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset, uint32_t data)
{
  DebugLog("Real3D: PCI %d-bit write request for reg=%02X, data=%08X\n", bits, reg, data);
}

void CReal3D::Reset(void)
{
  error = false;

  m_pingPong = 0;
  commandPortWritten = false;
  commandPortWrittenRO = false;

  queuedUploadTextures.clear();
  queuedUploadTexturesRO.clear();

  fifoIdx = 0;
  m_vromTextureFIFOIdx = 0;

  dmaSrc = 0;
  dmaDest = 0;
  dmaLength = 0;
  dmaData = 0;
  dmaUnknownReg = 0;
  dmaStatus = 0;
  dmaConfig = 0;

  unsigned memSize = (m_gpuMultiThreaded ? MEMORY_POOL_SIZE : MEM_POOL_SIZE_RW);
  memset(memoryPool, 0, memSize);
  memset(m_vromTextureFIFO, 0, sizeof(m_vromTextureFIFO));
  memset(m_internalRenderConfig, 0, sizeof(m_internalRenderConfig));

  DebugLog("Real3D reset\n");
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void CReal3D::AttachRenderer(IRender3D *Render3DPtr)
{
  Render3D = Render3DPtr;

  // If mult-threaded, attach read-only snapshots to renderer instead of real ones
  if (m_gpuMultiThreaded)
    Render3D->AttachMemory(cullingRAMLoRO, cullingRAMHiRO, polyRAMRO, vrom, textureRAMRO);
  else
    Render3D->AttachMemory(cullingRAMLo, cullingRAMHi, polyRAM, vrom, textureRAM);

  Render3D->SetStepping(step);

  DebugLog("Real3D attached a Render3D object\n");
}

uint32_t CReal3D::GetASICIDCode(ASIC asic) const
{
  auto it = m_asicID.find(asic);
  return it == m_asicID.end() ? 0 : it->second;
}

void CReal3D::SetStepping(int stepping)
{
  step = stepping;
  if ((step!=0x10) && (step!=0x15) && (step!=0x20) && (step!=0x21))
  {
    DebugLog("Real3D: Unrecognized stepping: %d.%d\n", (step>>4)&0xF, step&0xF);
    step = 0x10;
  }

  // Set PCI ID
  pciID = stepping >= 0x20 ? PCIID::Step2x : PCIID::Step1x;

  // Pass to renderer
  if (Render3D != NULL)
    Render3D->SetStepping(step);

  // Set ASIC ID codes
  m_asicID.clear();
  if (step == 0x10)
  {
    m_asicID = decltype(m_asicID)
    {
      { ASIC::Mercury,  0x216c3057 },
      { ASIC::Venus,    0x116c4057 },
      { ASIC::Earth,    0x216c5057 },
      { ASIC::Mars,     0x116c6057 },
      { ASIC::Jupiter,  0x116c7057 }
    };
  }
  else if (step == 0x15)
  {
    m_asicID = decltype(m_asicID)
    {
      { ASIC::Mercury,  0x316c3057 },
      { ASIC::Venus,    0x216c4057 },
      { ASIC::Earth,    0x316c5057 },
      { ASIC::Mars,     0x216c6057 },
      { ASIC::Jupiter,  0x316c7057 }
    };
  }
  else if (step >= 0x20)
  {
    m_asicID = decltype(m_asicID)
    {
      { ASIC::Mercury,  0x416c3057 },
      { ASIC::Venus,    0x316c4057 }, // skichamp @ pc=0xa89f4, this value causes 'NO DAUGHTER BOARD' message
      { ASIC::Earth,    0x416c5057 },
      { ASIC::Mars,     0x316c6057 },
      { ASIC::Jupiter,  0x416c7057 }
    };
  }

  DebugLog("Real3D set to Step %d.%d\n", (step>>4)&0xF, step&0xF);
}

bool CReal3D::Init(const uint8_t *vromPtr, IBus *BusObjectPtr, CIRQ *IRQObjectPtr, unsigned dmaIRQBit)
{
  uint32_t memSize = (m_gpuMultiThreaded ? MEMORY_POOL_SIZE : MEM_POOL_SIZE_RW);
  float  memSizeMB = (float)memSize/(float)0x100000;

  // IRQ and bus objects
  Bus = BusObjectPtr;
  IRQ = IRQObjectPtr;
  dmaIRQ = dmaIRQBit;

  // Allocate all Real3D RAM regions
  memoryPool = new(std::nothrow) uint8_t[memSize];
  if (NULL == memoryPool)
    return ErrorLog("Insufficient memory for Real3D object (needs %1.1f MB).", memSizeMB);

  // Set up main pointers
  cullingRAMLo = (uint32_t *) &memoryPool[OFFSET_8C];
  cullingRAMHi = (uint32_t *) &memoryPool[OFFSET_8E];
  polyRAM = (uint32_t *) &memoryPool[OFFSET_98];
  textureRAM = (uint16_t *) &memoryPool[OFFSET_TEXRAM];
  textureFIFO = (uint32_t *) &memoryPool[OFFSET_TEXFIFO];

  // If multi-threaded, set up pointers for read-only snapshots and dirty page arrays too
  if (m_gpuMultiThreaded)
  {
    cullingRAMLoRO = (uint32_t *) &memoryPool[OFFSET_8C_RO];
    cullingRAMHiRO = (uint32_t *) &memoryPool[OFFSET_8E_RO];
    polyRAMRO = (uint32_t *) &memoryPool[OFFSET_98_RO];
    textureRAMRO = (uint16_t *) &memoryPool[OFFSET_TEXRAM_RO];
    cullingRAMLoDirty = (uint8_t *) &memoryPool[OFFSET_8C_DIRTY];
    cullingRAMHiDirty = (uint8_t *) &memoryPool[OFFSET_8E_DIRTY];
    polyRAMDirty = (uint8_t *) &memoryPool[OFFSET_98_DIRTY];
    textureRAMDirty = (uint8_t *) &memoryPool[OFFSET_TEXRAM_DIRTY];
  }

  // VROM pointer passed to us
  vrom = (uint32_t *) vromPtr;

  DebugLog("Initialized Real3D (allocated %1.1f MB)\n", memSizeMB);
  return OKAY;
}

CReal3D::CReal3D(const Util::Config::Node &config)
  : m_config(config),
    m_gpuMultiThreaded(config["GPUMultiThreaded"].ValueAs<bool>())
{
  Render3D = NULL;
  memoryPool = NULL;
  cullingRAMLo = NULL;
  cullingRAMHi = NULL;
  polyRAM = NULL;
  textureRAM = NULL;
  textureFIFO = NULL;
  vrom = NULL;
  error = false;
  fifoIdx = 0;
  m_vromTextureFIFO[0] = 0;
  m_vromTextureFIFO[1] = 0;
  m_vromTextureFIFOIdx = 0;
  m_internalRenderConfig[0] = 0;
  m_internalRenderConfig[1] = 0;
  DebugLog("Built Real3D\n");
}

/*
 * CReal3D::~CReal3D(void):
 *
 * Destructor.
 */
CReal3D::~CReal3D(void)
{
  // Dump memory
#if 0
  FILE  *fp;
  fp = fopen("8c000000", "wb");
  if (NULL != fp)
  {
    fwrite(cullingRAMLo, sizeof(uint8_t), 0x400000, fp);
    fclose(fp);
    printf("dumped %s\n", "8c000000");
  }
  else
    printf("unable to dump %s\n", "8c000000");
  fp = fopen("8e000000", "wb");
  if (NULL != fp)
  {
    fwrite(cullingRAMHi, sizeof(uint8_t), 0x100000, fp);
    fclose(fp);
    printf("dumped %s\n", "8e000000");
  }
  else
    printf("unable to dump %s\n", "8e000000");
  fp = fopen("98000000", "wb");
  if (NULL != fp)
  {
    fwrite(polyRAM, sizeof(uint8_t), 0x400000, fp);
    fclose(fp);
    printf("dumped %s\n", "98000000");
  }
  else
    printf("unable to dump %s\n", "98000000");
  fp = fopen("texram", "wb");
  if (NULL != fp)
  {
    fwrite(textureRAM, sizeof(uint8_t), 0x800000, fp);
    fclose(fp);
    printf("dumped %s\n", "texram");
  }
  else
    printf("unable to dump %s\n", "texram");
#endif

  // Dump textures if requested
  if (m_config["DumpTextures"].ValueAsDefault<bool>(false))
  {
    Util::WriteSurfaceToBMP<Util::T1RGB5ContourEnabled>("textures_t1rgb5_contour.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as T1RGB5 (contour bit enabled) to 'textures_t1rgb5_contour.bmp'\n");
    Util::WriteSurfaceToBMP<Util::T1RGB5ContourIgnored>("textures_t1rgb5_opaque.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as T1RGB5 (contour bit ignored) to 'textures_t1rgb5_opaque.bmp'\n");
    Util::WriteSurfaceToBMP<Util::A4L4Low>("textures_a4l4_lo.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as A4L4 (low) to 'textures_a4l4_lo.bmp'\n");
    Util::WriteSurfaceToBMP<Util::L4A4Low>("textures_l4a4_lo.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as L4A4 (low) to 'textures_l4a4_lo.bmp'\n");
    Util::WriteSurfaceToBMP<Util::A4L4High>("textures_a4l4_hi.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as A4L4 (high) to 'textures_a4l4_hi.bmp'\n");
    Util::WriteSurfaceToBMP<Util::L4A4High>("textures_l4a4_hi.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as L4A4 (high) to 'textures_l4a4_hi.bmp'\n");
    Util::WriteSurfaceToBMP<Util::L8Low>("textures_l8_lo.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as L8 (low) to 'textures_l8_lo.bmp'\n");
    Util::WriteSurfaceToBMP<Util::L8High>("textures_l8_hi.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as L8 (high) to 'textures_l8_hi.bmp'\n");
    Util::WriteSurfaceToBMP<Util::RGBA4>("textures_rgba4.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as RGBA4 to 'textures_rgba4.bmp'\n");
    Util::WriteSurfaceToBMP<Util::L4Channel0>("textures_l4_0.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as L4 (channel 0) to 'textures_l4_0.bmp'\n");
    Util::WriteSurfaceToBMP<Util::L4Channel1>("textures_l4_1.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as L4 (channel 1) to 'textures_l4_1.bmp'\n");
    Util::WriteSurfaceToBMP<Util::L4Channel2>("textures_l4_2.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as L4 (channel 2) to 'textures_l4_2.bmp'\n");
    Util::WriteSurfaceToBMP<Util::L4Channel3>("textures_l4_3.bmp", reinterpret_cast<uint8_t*>(textureRAM), 2048, 2048, false);
    printf("Wrote textures as L4 (channel 3) to 'textures_l4_3.bmp'\n");
  }

  Render3D = NULL;
  if (memoryPool != NULL)
  {
    delete [] memoryPool;
    memoryPool = NULL;
  }
  cullingRAMLo = NULL;
  cullingRAMHi = NULL;
  polyRAM = NULL;
  textureRAM = NULL;
  textureFIFO = NULL;
  vrom = NULL;
  DebugLog("Destroyed Real3D\n");
}
