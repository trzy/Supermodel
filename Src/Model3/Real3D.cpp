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
 * It appears that Step 2.0 returns a different PCI ID depending on whether
 * the PCI configuration space or DMA register are accessed. For example,
 * Virtual On 2 expects 0x178611DB from the PCI configuration header but 
 * 0x16C311DB from the DMA device. 
 *
 * To-Do List
 * ----------
 * - For consistency, the status registers should probably be byte reversed (this is a
 *   little endian device), forcing the Model3 Read32/Write32 handlers to
 *   manually reverse the data. This keeps with the convention for VRAM.
 * - Keep an eye out for games writing non-mipmap textures to the mipmap area.
 *   The render currently cannot cope with this.
 */

#include "Supermodel.h"
#include "Model3/JTAG.h"
#include "Util/BMPFile.h"
#include <cstring>

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
#ifndef NEW_FRAME_TIMING
  // Calculate point at which status bit should change value.  Currently the same timing is used for both the status bit in ReadRegister
  // and in WriteDMARegister32/ReadDMARegister32, however it may be that they are completely unrelated.  It appears that step 1.x games
  // access just the former while step 2.x access the latter.  It is not known yet what this bit/these bits actually represent.
  statusChange = ppc_total_cycles() + statusCycles;
#else
  // Buffers are swapped at a specific point in the frame if a flush (command
  // port write) was performed
  if (commandPortWritten)
  {
    m_pingPong ^= 0x02000000;
    commandPortWritten = false;
  }
#endif
}

void CReal3D::EndVBlank(void)
{
  error = false;  // clear error (just needs to be done once per frame)
}

uint32_t CReal3D::SyncSnapshots(void)
{
  // Update read-only copy of command port flag
  commandPortWrittenRO = commandPortWritten;
#ifndef NEW_FRAME_TIMING
  commandPortWritten = false;
#endif

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
static const int mipXBase[11] =
{ 
  1024, // 1024/2
  1536, // 512/2
  1792, // 256/2
  1920, // ...
  1984, 
  2016, 
  2032, 
  2040, 
  2044, 
  2046, 
  2047 
};

static const int mipYBase[11] =
{
  512, 
  768, 
  896, 
  960, 
  992, 
  1008,
  1016, 
  1020, 
  1022, 
  1023, 
  0 
};

// Mipmap reduction factors
static const int mipDivisor[9] = { 2, 4, 8, 16, 32, 64, 128, 256, 512 };

// Table of texel offsets corresponding to an 8x8 texel texture tile
static const unsigned decode[64] =
{
   0, 1, 4, 5, 8, 9,12,13,
   2, 3, 6, 7,10,11,14,15,
  16,17,20,21,24,25,28,29,
  18,19,22,23,26,27,30,31,
  32,33,36,37,40,41,44,45,
  34,35,38,39,42,43,46,47,
  48,49,52,53,56,57,60,61,
  50,51,54,55,58,59,62,63
};

static void StoreTexelByte(uint16_t *texel, uint32_t byteSelect, uint8_t byte)
{
  if ((byteSelect & 1)) // write to LSB
    *texel = (*texel & 0xFF00) | byte;
  if ((byteSelect & 2)) // write to MSB
    *texel = (*texel & 0x00FF) | (uint16_t(byte) << 8);
}   

void CReal3D::StoreTexture(unsigned level, unsigned xPos, unsigned yPos, unsigned width, unsigned height, const uint16_t *texData, uint32_t header)
{
  if ((header & 0x00800000))  // 16-bit textures
  {
    // Outer 2 loops: 8x8 tiles
    for (uint32_t y = yPos; y < (yPos+height); y += 8)
    {
      for (uint32_t x = xPos; x < (xPos+width); x += 8)
      {
        // Inner 2 loops: 8x8 texels for the current tile
        uint32_t destOffset = y*2048+x;
        for (uint32_t yy = 0; yy < 8; yy++)
        {
          for (uint32_t xx = 0; xx < 8; xx++)
          { 
            if (m_gpuMultiThreaded)
              MARK_DIRTY(textureRAMDirty, destOffset * 2);
            textureRAM[destOffset++] = texData[decode[(yy*8+xx)^1]];
          }

          destOffset += 2048-8; // next line
        }
        texData += 8*8; // next tile
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

    uint32_t byteSelect = (header>>21)&3; // which byte to unpack to
    if (byteSelect == 3)  // write to both?
      DebugLog("Observed 8-bit texture with byte_select=3!");
  
    // Outer 2 loops: 8x8 tiles
    for (uint32_t y = yPos; y < (yPos+height); y += 8)
    {
      for (uint32_t x = xPos; x < (xPos+width); x += 8)
      {
        // Inner 2 loops: 8x8 texels for the current tile
        uint32_t destOffset = y*2048+x;
        for (uint32_t yy = 0; yy < 8; yy++)
        {
          for (uint32_t xx = 0; xx < 8; xx += 2)
          {
            uint8_t byte1 = texData[decode[(yy^1)*8+((xx+0)^1)]/2]>>8;
            uint8_t byte2 = texData[decode[(yy^1)*8+((xx+1)^1)]/2]&0xFF;
            if (m_gpuMultiThreaded)
              MARK_DIRTY(textureRAMDirty, destOffset * 2);
            StoreTexelByte(&textureRAM[destOffset], byteSelect, byte1);
            ++destOffset;
            if (m_gpuMultiThreaded)
              MARK_DIRTY(textureRAMDirty, destOffset * 2);
            StoreTexelByte(&textureRAM[destOffset], byteSelect, byte2);
            ++destOffset;
          }
          destOffset += 2048-8;
        }
        texData += 8*8/2; // next tile
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

// Texture data will be in little endian format
void CReal3D::UploadTexture(uint32_t header, const uint16_t *texData)
{
  // Position: texture RAM is arranged as 2 2048x1024 texel sheets
  uint32_t x = 32*(header&0x3F);
  uint32_t y = 32*((header>>7)&0x1F);
  uint32_t page = (header>>20)&1;
  y += page*1024; // treat page as additional Y bit (one 2048x2048 sheet)
  
  // Texture size and bit depth
  uint32_t width = 32<<((header>>14)&7);
  uint32_t height  = 32<<((header>>17)&7);
  uint32_t bytesPerTexel;
  if ((header&0x00800000))  // 16 bits per texel
    bytesPerTexel = 2;
  else            // 8 bits
  {
    bytesPerTexel = 1;
    //printf("8-bit textures!\n");
  }
  
  // Mipmaps
  uint32_t mipYPos = 32*((header>>7)&0x1F);
  
  // Process texture data
  DebugLog("Real3D: Texture upload: pos=(%d,%d) size=(%d,%d), %d-bit\n", x, y, width, height, bytesPerTexel*8);
  //printf("Real3D: Texture upload: pos=(%d,%d) size=(%d,%d), %d-bit\n", x, y, width, height, bytesPerTexel*8);
  switch ((header>>24)&0xFF)
  {
  case 0x00:  // texture w/ mipmaps
  {
    StoreTexture(0, x, y, width, height, texData, header);
    uint32_t mipWidth = width;
    uint32_t mipHeight = height;
    uint32_t mipNum = 0;

    while((mipHeight>8) && (mipWidth>8))
    {
      if (bytesPerTexel == 1)
        texData += (mipWidth*mipHeight)/2;
      else
        texData += (mipWidth*mipHeight);
      mipWidth /= 2;
      mipHeight /= 2;
      uint32_t mipX = mipXBase[mipNum] + (x / mipDivisor[mipNum]);
      uint32_t mipY = mipYBase[mipNum] + (mipYPos / mipDivisor[mipNum]);
      if(page)
        mipY += 1024;
      mipNum++;
	  StoreTexture(mipNum, mipX, mipY, mipWidth, mipHeight, (uint16_t *)texData, header);
    }
    break;
  }
  case 0x01:  // texture w/out mipmaps
    StoreTexture(0, x, y, width, height, texData, header);
    break;
  case 0x02:  // mipmaps only
  {
    uint32_t mipWidth = width;
    uint32_t mipHeight = height;
    uint32_t mipNum = 0;
    while((mipHeight>8) && (mipWidth>8))
    {
      mipWidth /= 2;
      mipHeight /= 2;
      uint32_t mipX = mipXBase[mipNum] + (x / mipDivisor[mipNum]);
      uint32_t mipY = mipYBase[mipNum] + (mipYPos / mipDivisor[mipNum]);
      if(page)
        mipY += 1024;
      mipNum++;
	  StoreTexture(mipNum, mipX, mipY, mipWidth, mipHeight, texData, header);
      if (bytesPerTexel == 1)
        texData += (mipWidth*mipHeight)/2;
      else
        texData += (mipWidth*mipHeight);
    }
    break;
  }
  case 0x80:  // MAME thinks these might be a gamma table
    /*
    printf("Special texture format 0x80:\n");
    for (int i = 0; i < 32*32; i++)
    {
      printf("  %02x=%02x\n", i, texData[i]);
    }
    */
    break;
  default:  // unknown
    DebugLog("Unknown texture format %02X\n", header>>24);
    //printf("unknown texture format %02X\n", header>>24);
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
    dmaStatus |= 1;
    IRQ->Assert(dmaIRQ);
    break;
  case 0x10:  // command register
    if ((data&0x20000000))
    {
      dmaData = 0x16C311DB; // Virtual On 2 expects this from DMA
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
  if (fifoIdx > 0)
  {
    for (uint32_t i = 0; i < fifoIdx; )
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

// Registers seem to range from 0x00 to around 0x3C but they are not understood
uint32_t CReal3D::ReadRegister(unsigned reg)
{
  DebugLog("Real3D: Read reg %X\n", reg);
  if (reg == 0)
  {
#ifndef NEW_FRAME_TIMING
    uint32_t status = (ppc_total_cycles() >= statusChange ? 0x0 : 0x02000000);
    return 0xfdffffff | status;
#else
    return 0xfdffffff | m_pingPong;
#endif
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
  dmaStatus = 0;
  dmaUnknownReg = 0;
  
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
  if (step < 0x20)      
    pciID = 0x16C311DB; // vendor 0x11DB = Sega
  else
    pciID = 0x178611DB;
    
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
  uint32_t memSize = (m_config["GPUMultiThreaded"].ValueAs<bool>() ? MEMORY_POOL_SIZE : MEM_POOL_SIZE_RW);
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
  Util::WriteSurfaceToBMP<Util::A1RGB5>("textures.bmp", reinterpret_cast<uint8_t *>(textureRAM), 2048, 2048, false);
#endif
Util::WriteSurfaceToBMP<Util::A1RGB5>("textures.bmp", reinterpret_cast<uint8_t *>(textureRAM), 2048, 2048, false);

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
