/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * 
 * Rendering
 * ---------
 * Supermodel draws one frame for each emulated frame. RenderFrame() tells
 * the renderer to draw the scene that is in Real3D memory. In multi-threaded
 * mode, the renderer reads the read-only snapshots, which SyncSnapshots()
 * updates once for each frame. No command starts the drawing.
 *
 * Model 3:
 *
 * Two signals tell when the data for a frame is complete:
 *
 *   Command port (0x88000000)  Flush(). The CPU wrote all the data for the
 *                              frame.
 *   Tile generator             TilegenDrawFrame(). The tile generator draws
 *                              a frame.
 *
 * When both signals occur for a frame, DrawFrame() latches the block culling
 * bit (Mercury mode word, bit 2). This bit can stop the display of the 3D
 * scene. DrawFrame() does not draw. Signals that occur after the ping-pong
 * flip apply to the next frame: BeginVBlank() then applies the update
 * buffers, uploads the textures if the command port was written, and calls
 * DrawFrame() if both signals occurred.
 *
 * Textures:
 *
 *   Texture FIFO (0x94000000)  WriteTextureFIFO() keeps the words. The
 *                              textures are uploaded when the command port
 *                              is written (before the flip) or in
 *                              BeginVBlank() (after the flip).
 *   Texture port (0x90000000)  WriteTexturePort(). Step 1.0: the data is the
 *                              word address of a texture block in VROM, and
 *                              the block goes into the texture FIFO. Step
 *                              2.x: two words (address and header), and the
 *                              texture is uploaded immediately. Addresses
 *                              below 0x100000 are in polygon RAM.
 *
 * Texture RAM is 8 MB: two pages of 2048x1024 16-bit texels.
 *
 * Real3D Pro-1000 (hypothesis):
 *
 * The Pro-1000 has no tile generator. Thus TilegenDrawFrame() does not occur,
 * DrawFrame() does not occur, and block culling stays off. The firmware
 * writes the command port (REND_UPDATE_COMPL) after it processes the update
 * from the host. This uploads the textures. Supermodel draws each frame from
 * memory, as for Model 3.
 *
 * The hypothesis is that the Pro-1000 draws continuously, one frame for each
 * field, and that REND_UPDATE_COMPL marks the end of an update. The firmware
 * sends no command to draw. For each frame, it processes the update from the
 * host, writes REND_UPDATE_COMPL, waits for the geometry processor sync, and
 * reports "render done" in the status packet. The host reads the status
 * packet before it sends the next frame.
 *
 * Difference: Supermodel draws at the end of each emulated frame, not when
 * REND_UPDATE_COMPL is written. The update buffers (see below) hold only the
 * writes that occur after the flip. Writes before the flip go directly to
 * memory. Thus a frame can show part of an update that is not complete. This
 * was not seen in the SDK demos.
 *
 * Update Buffers
 * --------------
 * Model 3:
 *
 * The Real3D draws a frame while the CPU sends the data for the next frame.
 * The ping_pong status bit flips during the frame, often at about 66% of the
 * frame time (the line is a tile generator register). After the flip, writes
 * to high culling RAM and polygon RAM must not change the frame that is
 * drawn. Supermodel keeps these writes in update buffers in low culling RAM
 * and applies them at the start of the next VBlank (BeginVBlank()).
 *
 * The configuration registers (0x9C000000, WriteConfigurationRegister()) give
 * the sizes of the buffers in 32-bit words:
 *
 *   reg 0 (pingPongMemSize)           The high culling RAM buffer is words
 *                                     0 to reg 0 - 1 of low culling RAM.
 *   reg 8 (pingPongAndUpdateMemSize)  The polygon RAM buffer is words reg 0
 *                                     to reg 8 - 1 of low culling RAM.
 *
 * All Model 3 games that were examined write these registers when they boot,
 * before they write data after a flip: Virtua Fighter 3 and Bass Fishing
 * (Step 1.0), Scud Race and The Lost World (Step 1.5), Sega Rally 2, Harley-
 * Davidson, Ski Champ, Fighting Vipers 2, and Virtual On 2 (Step 2.0),
 * Daytona USA 2, Spikeout, Emergency Call Ambulance, Magical Truck Adventure,
 * La Machine Guns, Dirt Devils, and Star Wars Trilogy (Step 2.1). The values
 * are 0x5FFC to 0xBFFC for reg 0 and 0x18000 to 0x40000 for reg 8.
 *
 * A buffer is a list of blocks (UpdateBlock). A block has a start address, an
 * end address, and the data for these consecutive addresses. The buffers
 * operate only if the registers give an area for them (reg 8 > reg 0). A
 * buffer cannot go outside its area. If a buffer is full, it is applied
 * immediately and the write goes directly to memory. Writes from a buffer
 * cannot go outside the memory that they update.
 *
 * Real3D Pro-1000:
 *
 * The Pro-1000 has the same architecture. Its SDK gives a "ping-pong memory"
 * in high culling RAM (PRO_PINGPONG_BASE = 0x8E000000) and an "update memory"
 * (PRO_AbbreviatedConfigureMemory()), and its status packet has a ping_pong
 * bit. The same three values configure the buffers, but the firmware does not
 * write them to 0x9C000000. It sends them to the Mercury ASIC through JTAG
 * (instruction 0x09). In Supermodel, an update of that JTAG register writes
 * the configuration registers (see "Update Memory Configuration" in
 * Pro1000.cpp). The firmware sends values from the SCSI ROM at boot. The host
 * sends values when it starts (the SDK demos send 0x8000, 0, 0x20000).
 * Supermodel does not have the real SCSI ROM. The synthetic SCSI ROM
 * (SyntheticSCSIROM.cpp) contains valid values (0x7FFC, 0x4, 0x20000). Thus
 * the configuration registers are valid after boot.
 *
 * The time of the flip on the Pro-1000 is not known: it has no tile
 * generator. Supermodel flips the bit at the line that most Model 3 games
 * program (PING_PONG_FLIP_LINE in Pro1000.cpp).
 *
 * Without configuration:
 *
 * The update buffers are always emulated. They operate only when the
 * configuration registers give an area for them (reg 8 > reg 0). Before the
 * configuration registers are written (they are zero after reset), there are
 * no buffers and all writes go directly to memory. Earlier versions put both
 * buffers at the start of low culling RAM when the registers were zero. On
 * the Pro-1000, the firmware writes scene data there directly. This data
 * changed the buffers and caused writes outside polygon RAM (a crash).
 *
 * TODO: Physical Ping-Pong Memory (hypothesis, not emulated)
 *
 * Supermodel has one high culling RAM (0x8E000000) that is separate from low
 * culling RAM. The ping_pong bit flips, but no memory changes. The evidence
 * below shows that the hardware is different.
 *
 * The SDK (PRO_AbbreviatedConfigureMemory(), Developer's Guide 4.0e) gives
 * the two sizes:
 *
 *  - Ping-pong memory (reg 0): the culling memory where viewports,
 *    animation sequences, and databases are stored. The API objects for
 *    viewports, matrices, databases, animation sequences, and indirection
 *    tables are "ping-pong data blocks".
 *  - Update memory (reg 8 - reg 0): the largest amount of data that the
 *    application sends to the renderer. The default (PRO_UPDATE_MEMSIZE) is
 *    96K words (393216 bytes). The application can change it.
 *
 * 1. Two ping-pong memories (strong evidence)
 *
 * There are two physical ping-pong memories, A and B. The renderer draws
 * from one of them. A write to 0x8E000000 (culling address 0x800000 + i)
 * goes only to the other one. They swap at a frame boundary after an update
 * is complete (REND_UPDATE_COMPL). Thus data that must stay in ping-pong
 * memory must be written to both memories. Evidence:
 *
 *  - Model 3 games write their ping-pong data again in each frame, or they
 *    write it exactly two times in two consecutive frames. Data that was
 *    not written for 60 frames or more, at frame 1800 of the attract mode:
 *    Scud Race: 590 words written exactly two times, all one frame apart,
 *    and no word written only once. Virtua Fighter 3: 2927 words written
 *    exactly two times, 2926 of them one frame apart. Daytona USA 2: 632
 *    words written exactly two times, all one frame apart.
 *  - The SDK demo Bayou writes all of its ping-pong data (2266 words, words
 *    0 to 0x7FFF) again in each update. It does not send an update in every
 *    frame. Thus the swap probably occurs after an update, not in each
 *    frame.
 *  - The Pro-1000 firmware writes each word to 0x8E000000 two times, with an
 *    update complete and a frame boundary between the writes (0x10984, the
 *    monitor write command at 0xFDE4).
 *  - The firmware rejects writes to 0x8E000000 above reg 0 ("PPO"). The
 *    0x8E000000 data of Bayou ends at reg 0 (0x8000).
 *  - Mercury mode word bit 4 is "Block PingPong" and bit 5 is "Force
 *    PingPong" (bit names in the Pro-1000 firmware). The firmware does not
 *    set them. Only the monitor can.
 *
 * 2. Ping-pong memory A is low culling RAM (weak evidence)
 *
 * One of the two memories is possibly at the start of low culling RAM
 * (0x8C000000, low culling words 0 to reg 0 - 1). Two versions:
 *
 *  a. The windows swap. At any time, one memory is at 0x8C000000 and the
 *     other is at 0x8E000000: A at 0x8C000000 and B at 0x8E000000, then after
 *     a flip, B at 0x8C000000 and A at 0x8E000000.
 *  b. 0x8C000000 is always memory A. 0x8E000000 changes between A and B at
 *     each flip. When 0x8E000000 is A, the two windows get the same memory.
 *     The location of B is not known (it possibly has no CPU address).
 *
 * Evidence (see "Update Memory Configuration" in Pro1000.cpp):
 *
 *  - With reg 0 = 0x45, Bayou writes a complete scene (a viewport, a pointer
 *    list, a matrix, and a culling node) to low culling words 0-0x44 through
 *    0x8C000000. Its pointers are culling addresses 0x800000 + i, which are
 *    correct only if word i of that area is culling address 0x800000 + i.
 *    But there is no evidence that the renderer draws this scene: Bayou
 *    replaces it soon after through 0x8E000000.
 *  - Hosts put their own culling data above reg 8 (Asia: 0x20000, Bayou:
 *    0x40000). The Model 3 games that were examined do not write low culling
 *    RAM below word 0x20000.
 *
 * Problem: the Pro-1000 shows a spinning logo at power-on. The firmware
 * loads the culling block of the SCSI ROM through 0x8C000000 one time
 * (rend_init), and it does not write the scene through 0x8E000000. If
 * 0x8C000000 gets only one ping-pong memory, the logo is in only one memory
 * and is not shown in each frame. Thus part 2 is possibly not correct, or
 * the hardware writes the culling block to both memories in a way that is
 * not known.
 *
 * To do:
 *
 *  1. Find how the flip operates (which memory each window and the renderer
 *     use, where the second memory is, and what swaps them), then emulate
 *     the two ping-pong memories.
 *  2. If part 2 is correct, keep both update buffers in update memory (low
 *     culling words reg 0 to reg 8 - 1). Do not use words 0 to reg 0 - 1:
 *     they are then a ping-pong memory.
 */

#include "Real3D.h"

#include "Supermodel.h"
#include "JTAG.h"
#include "CPU/PowerPC/ppc.h"
#include "Util/BMPFile.h"
#include "Util/BitCast.h"
#include <cstring>
#include <algorithm>

// Macros that divide memory regions into pages and mark them as dirty when they are written to
#define PAGE_WIDTH 12
#define PAGE_SIZE (1<<PAGE_WIDTH)
#define DIRTY_SIZE(arraySize) (1+((arraySize)-1)/(8*PAGE_SIZE))
#define MARK_DIRTY(dirtyArray, addr) dirtyArray[(addr)>>(PAGE_WIDTH+3)] |= 1<<(((addr)>>PAGE_WIDTH)&7)

// Sizes of memory regions (bytes). Polygon RAM size is in the hardware
// configuration (Real3DConfig).
static constexpr uint32_t CULLING_RAM_LO_SIZE = 0x400000;  // 4 MB at 0x8C000000
static constexpr uint32_t CULLING_RAM_HI_SIZE = 0x100000;  // 1 MB at 0x8E000000
static constexpr uint32_t TEXTURE_RAM_SIZE    = 0x800000;  // 8 MB
static constexpr uint32_t TEXTURE_FIFO_SIZE   = 0x100000;  // 1 MB

// The memory pool holds, in this sequence: the writable regions (culling RAM
// low, culling RAM high, polygon RAM, texture RAM, texture FIFO), the
// read-only snapshots of the first four regions, and the dirty page arrays.
// Save states contain the writable regions. With the Model 3 configuration,
// the layout is the same as in earlier versions, so save states stay
// compatible.


/******************************************************************************
 Save States
******************************************************************************/

void CReal3D::SaveState(CBlockFile *SaveState)
{
  SaveState->NewBlock("Real3D", __FILE__);

  SaveState->Write(memoryPool, m_layout.sizeRW); // Don't write out read-only snapshots or dirty page arrays
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
  SaveState->Write(&m_modeword, sizeof(m_modeword));
  SaveState->Write(&m_vromTextureFIFOIdx, sizeof(m_vromTextureFIFOIdx));
  SaveState->Write(&m_configRegisters, sizeof(m_configRegisters));

  uint32_t polyUpdateBlockOffset = -1;
  uint32_t highCullUpdateBlockOffset = -1;

  // calculate block offsets in 32bit words
  if (m_polyUpdateBlock) {
      auto pStartAddr = cullingRAMLo + m_configRegisters.pingPongMemSize;
      polyUpdateBlockOffset = (uint32_t)((uint32_t*)m_polyUpdateBlock - pStartAddr);
  }
  if (m_highRamUpdateBlock) {
      auto pStartAddr = cullingRAMLo;
      highCullUpdateBlockOffset = (uint32_t)((uint32_t*)m_highRamUpdateBlock - pStartAddr);
  }

  SaveState->Write(&polyUpdateBlockOffset, sizeof(polyUpdateBlockOffset));
  SaveState->Write(&highCullUpdateBlockOffset, sizeof(highCullUpdateBlockOffset));
}

void CReal3D::LoadState(CBlockFile *SaveState)
{
  if (Result::OKAY != SaveState->FindBlock("Real3D"))
  {
    ErrorLog("Unable to load Real3D GPU state. Save state file is corrupt.");
    return;
  }

  SaveState->Read(memoryPool, m_layout.sizeRW);

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
  SaveState->Read(&commandPortWritten);
  SaveState->Read(&m_pingPong, sizeof(m_pingPong));
  SaveState->Read(&m_modeword, sizeof(m_modeword));
  Render3D->SetSunClamp((m_modeword[static_cast<int>(CASIC::Name::Mars)] & 0x40000) == 0);
  Render3D->SetBlockCulling((m_modeword[static_cast<int>(CASIC::Name::Mercury)] & 4) == 4);
  SaveState->Read(&m_vromTextureFIFOIdx, sizeof(m_vromTextureFIFOIdx));
  SaveState->Read(&m_configRegisters, sizeof(m_configRegisters));

  uint32_t polyUpdateBlockOffset = 0;
  uint32_t highCullUpdateBlockOffset = 0;
  SaveState->Read(&polyUpdateBlockOffset, sizeof(polyUpdateBlockOffset));
  SaveState->Read(&highCullUpdateBlockOffset, sizeof(highCullUpdateBlockOffset));

  // use -1 to indicate null pointer
  if (polyUpdateBlockOffset == (uint32_t)-1) { m_polyUpdateBlock = nullptr;}
  else                                       { m_polyUpdateBlock = (UpdateBlock*)(cullingRAMLo + m_configRegisters.pingPongMemSize + polyUpdateBlockOffset); }

  if (highCullUpdateBlockOffset == (uint32_t)-1) { m_highRamUpdateBlock = nullptr; }
  else                                           { m_highRamUpdateBlock = (UpdateBlock*)(cullingRAMLo + highCullUpdateBlockOffset); }
  
}


/******************************************************************************
 Rendering
******************************************************************************/


void CReal3D::BeginVBlank()
{
    // need to grab a copy at the start of the frame so we can see if it's flipped
    m_pingPongCopy = m_pingPong;

    // sync any update buffers
    SyncBufferedMem(m_highRamUpdateBlock, cullingRAMLo, cullingRAMHi, CULLING_RAM_HI_SIZE / 4, cullingRAMHiDirty);
    SyncBufferedMem(m_polyUpdateBlock, cullingRAMLo + m_configRegisters.pingPongMemSize, polyRAM, m_hwConfig.polygonRAMSize / 4, polyRAMDirty);

    // update write pointers
    m_polyUpdateBlock = nullptr;
    m_highRamUpdateBlock = nullptr;

    // update any textures if they were written after the ping_pong time.
    if (commandPortWritten) {
        FlushTextures();
    }

    // check to see if a complete frame was written after the ping_pong time
    if (m_tilegenDrawFrame && commandPortWritten) {
        DrawFrame();
    }
}

void CReal3D::EndVBlank(void)
{
  error = false;  // clear error (just needs to be done once per frame)
}

void CReal3D::FlipPingPongBit(void)
{
    m_pingPong = !m_pingPong;

    // this is the last time writes can happen for this frame, any writes after are buffered for the next frame
    // so we reset drawing flags here
    m_tilegenDrawFrame = false;
    commandPortWritten = false;
}

uint32_t CReal3D::SyncSnapshots(void)
{
  if (!m_gpuMultiThreaded)
    return 0;

  // Update read-only queue
  queuedUploadTexturesRO = queuedUploadTextures;
  queuedUploadTextures.clear();

  Render3D->SetBlockCulling(m_blockCullingRO);

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

void CReal3D::SyncBufferedMem(UpdateBlock* updateBlock, uint32_t* updateBuffer, uint32_t* dst, uint32_t dstWords, uint8_t* dirty)
{
    if (updateBlock) {                          // this is a pointer to the end of the buffer, or null if no data

        auto ub = (UpdateBlock*)updateBuffer;   // this takes us back to the start

        while (true) {

            auto count      = ub->Count();
            auto startAddr  = ub->startAddr;

            for (auto i = 0u; i < count; i++) {

                auto addr = startAddr + i;
                if (addr >= dstWords) {         // AddToUpdateBuffer() prevents this; do not write outside memory
                    break;
                }
                dst[addr] = ub->data[i];

                if (m_gpuMultiThreaded) {
                    MARK_DIRTY(dirty, (addr*4));
                }
            }

            if (ub == updateBlock) break;      // last block

            ub = ub->Next();    
        }
    }
}

void CReal3D::FlushTextures()
{
    // Upload textures (if any)
    if (fifoIdx > 2) // If the texture header/data aren't present, discard the texture (prevents garbage textures in Ski Champ)
    {
        for (uint32_t i = 0; i < fifoIdx - 2; )
        {
            uint32_t size = 2 + textureFIFO[i + 0] / 2;
            size /= 4;
            uint32_t header = textureFIFO[i + 1]; // texture information header

            // Spikeout seems to be uploading 0 length textures
            if (0 == size)
            {
                DebugLog("Real3D: 0-length texture upload @ PC=%08X (%08X %08X %08X)\n", ppc_get_pc(), textureFIFO[i + 0], textureFIFO[i + 1], textureFIFO[i + 2]);
                break;
            }

            UploadTexture(header, (uint16_t*)&textureFIFO[i + 2]);
            DebugLog("Real3D: Texture upload completed: %X bytes (%X)\n", size * 4, textureFIFO[i + 0]);

            i += size;
        }
    }

    // Reset texture FIFO
    fifoIdx = 0;
}

bool CReal3D::PollPingPong()
{
    return m_pingPong != m_pingPongCopy;
}

void CReal3D::DrawFrame()
{
    // on real h/w the frame should start drawing here
    // we are drawing at the end of the frame time which does the same

    m_tilegenDrawFrame = false;
    commandPortWritten = false;

    // we need the block culling state when a frame has been triggered for rendering
    m_blockCullingRO = (m_modeword[static_cast<int>(CASIC::Name::Mercury)] & 4) == 4;

    // todo figure out what exactly happens if a frame hasn't been triggered for rendering
    // are we just drawing the previous frame that's left in the buffer
    // or are we triggering white to draw

    if (!m_gpuMultiThreaded) {
        Render3D->SetBlockCulling(m_blockCullingRO);
    }
}

void CReal3D::TilegenDrawFrame(uint32_t flags)
{
    m_tilegenDrawFrame = true;

    bool pingPongFlipped = PollPingPong();

    if (!pingPongFlipped) {
        if (commandPortWritten) {
            DrawFrame();
        }
    }
}

uint32_t CReal3D::UpdateSnapshots(bool copyWhole)
{
  // Update all memory region snapshots
  uint32_t cullLoCopied  = UpdateSnapshot(copyWhole, (uint8_t*)cullingRAMLo, (uint8_t*)cullingRAMLoRO, 0x400000, cullingRAMLoDirty);
  uint32_t cullHiCopied  = UpdateSnapshot(copyWhole, (uint8_t*)cullingRAMHi, (uint8_t*)cullingRAMHiRO, 0x100000, cullingRAMHiDirty);
  uint32_t polyCopied    = UpdateSnapshot(copyWhole, (uint8_t*)polyRAM,      (uint8_t*)polyRAMRO,      m_hwConfig.polygonRAMSize, polyRAMDirty);
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

static constexpr int mipXBase[] = { 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
static constexpr int mipYBase[] = { 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023 };
static constexpr int mipDivisor[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };

// Tables of texel offsets corresponding to an NxN texel texture tile

static constexpr unsigned decode8x8[64] =
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

static constexpr unsigned decode8x4[32] =
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

static constexpr unsigned decode8x2[16] =
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

void CReal3D::StoreTexture(unsigned level, unsigned xPos, unsigned yPos, unsigned width, unsigned height, const uint16_t *texData, bool sixteenBit, bool writeLSB, bool writeMSB, uint32_t &texDataOffset)
{
  const uint32_t tileX = (std::min)(8u, width);
  const uint32_t tileY = (std::min)(8u, height);

  const unsigned* const decode = (tileX == 8) ? decode8x8 : (tileX == 4) ? decode8x4 : (tileX == 2) ? decode8x2 : nullptr;

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

            textureRAM[destOffset++] = texData[decode[yy * tileX + xx]];

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
    static constexpr uint16_t byteMask[4] = {0xFFFF, 0xFF00, 0x00FF, 0x0000};
    const uint32_t offset = std::max(1u, (tileY * tileX) / 2);
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

              uint16_t tempData = (texData[decode[index] / 2] >> shift) & 0xFF;

              tempData |= tempData << 8;
              tempData &= byteMask[byteSelect] ^ 0xFFFF;
              textureRAM[destOffset] |= tempData;
            }
            destOffset++;
          }
          destOffset += 2048 - tileX; // next line
        }
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
    [[fallthrough]];
  case 0x02:  // mipmaps only
  {
    for (int i = 1; width >=4 && height >=4; i++) {

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
      uint32_t data = Bus->Read32(dmaSrc);
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

uint8_t CReal3D::ReadDMARegister8(unsigned reg) const
{
  switch (reg)
  {
  case 0xC: // status
    return dmaStatus;
  case 0xE: // configuration
    return dmaConfig;
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

uint32_t CReal3D::ReadDMARegister32(unsigned reg) const
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

void CReal3D::WriteConfigurationRegister(unsigned reg, uint32_t data)
{
    reg = reg & 0xF;

    // reg 0 = ping pong buffer size (high culling RAM) in 32-bit words
    // reg 4 = flags, probably overload mode etc
    // reg 8 = combined size of the ping pong buffer and the update buffer in 32-bit words

    m_configRegisters.regs[reg / 4] = data;
    DebugLog("Real3D: Configuration register %X = %08X\n", reg, data);
}

/******************************************************************************
 Basic Emulation Functions, Registers, Memory, and Texture FIFO
******************************************************************************/

void CReal3D::Flush(void)
{
  commandPortWritten = true;
  DebugLog("Real3D 88000000 written @ PC=%08X\n", ppc_get_pc());
  if (!PollPingPong()) {
      FlushTextures();
      if (m_tilegenDrawFrame) {
          DrawFrame();
      }
  }
}

void CReal3D::WriteTextureFIFO(uint32_t data)
{
  PollPingPong();

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
  if (m_hwConfig.singleWordTexturePort)
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

      // first 4 MB maps to polygon RAM rather than VROM
      // Daytona 2 PE uses zeroed data to replace unused Dreamcast logo textures
      if (addr < 0x100000)
        UploadTexture(header, (const uint16_t*)&polyRAM[addr & 0xFFFFFF]);
      else
        UploadTexture(header, (const uint16_t*)&vrom[addr & 0xFFFFFF]);
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

// Returns true if writes go to the update buffers now: the ping-pong bit
// flipped in this frame, and the configuration registers give an update
// buffer area.
bool CReal3D::UpdateBuffersActive(void) const
{
  return m_pingPong != m_pingPongCopy &&
         m_configRegisters.pingPongAndUpdateMemSize > m_configRegisters.pingPongMemSize;
}

// Adds a write to an update buffer. Consecutive addresses go into the same
// block. Returns false if the buffer has no space; then the caller must apply
// the buffer and write directly.
bool CReal3D::AddToUpdateBuffer(UpdateBlock *&block, uint32_t *bufferStart, uint32_t *bufferEnd, uint32_t addr, uint32_t data)
{
  if (block && block->lastAddr + 1 == addr)
  {
    // Extend the current block
    if (&block->data[block->Count()] + 1 > bufferEnd)
      return false;
    block->lastAddr = addr;
    block->data[addr - block->startAddr] = data;
    return true;
  }

  // Start a new block
  UpdateBlock *next = block ? block->Next() : (UpdateBlock *) bufferStart;
  if ((uint32_t *) next + UpdateBlock::HeaderSize() + 1 > bufferEnd)
    return false;
  block = next;
  block->startAddr = addr;
  block->lastAddr = addr;
  block->data[0] = data;
  return true;
}

void CReal3D::WriteHighCullingRAM(uint32_t addr, uint32_t data)
{
  if (addr >= CULLING_RAM_HI_SIZE)
    return;

  // If ping pong has elapsed we buffer this data, else we write it normally.
  // The buffer for high culling RAM is at the start of low culling RAM, below
  // the buffer for polygon RAM.
  if (UpdateBuffersActive())
  {
    uint32_t *start = cullingRAMLo;
    uint32_t *end = cullingRAMLo + std::min<uint32_t>(m_configRegisters.pingPongMemSize, CULLING_RAM_LO_SIZE / 4);
    if (AddToUpdateBuffer(m_highRamUpdateBlock, start, end, addr / 4, data))
      return;

    // The buffer is full: apply it now, so this write stays after the earlier
    // writes
    DebugLog("Real3D: High culling RAM update buffer is full\n");
    SyncBufferedMem(m_highRamUpdateBlock, start, cullingRAMHi, CULLING_RAM_HI_SIZE / 4, cullingRAMHiDirty);
    m_highRamUpdateBlock = nullptr;
  }

  if (m_gpuMultiThreaded)
    MARK_DIRTY(cullingRAMHiDirty, addr);
  cullingRAMHi[addr/4] = data;
}

uint32_t CReal3D::ReadPolygonRAM(uint32_t addr) const
{
  if (addr >= m_hwConfig.installedPolygonRAMSize)
    return 0;
  return polyRAM[addr / 4];
}

void CReal3D::WritePolygonRAM(uint32_t addr, uint32_t data)
{
  if (addr >= m_hwConfig.installedPolygonRAMSize)
    return;

  // If ping pong has elapsed we buffer this data, else we write it normally.
  // The buffer for polygon RAM is in low culling RAM, between the two sizes
  // that the configuration registers give.
  if (UpdateBuffersActive())
  {
    uint32_t *start = cullingRAMLo + std::min<uint32_t>(m_configRegisters.pingPongMemSize, CULLING_RAM_LO_SIZE / 4);
    uint32_t *end = cullingRAMLo + std::min<uint32_t>(m_configRegisters.pingPongAndUpdateMemSize, CULLING_RAM_LO_SIZE / 4);
    if (AddToUpdateBuffer(m_polyUpdateBlock, start, end, addr / 4, data))
      return;

    // The buffer is full: apply it now, so this write stays after the earlier
    // writes
    DebugLog("Real3D: Polygon RAM update buffer is full\n");
    SyncBufferedMem(m_polyUpdateBlock, start, polyRAM, m_hwConfig.polygonRAMSize / 4, polyRAMDirty);
    m_polyUpdateBlock = nullptr;
  }

  if (m_gpuMultiThreaded)
    MARK_DIRTY(polyRAMDirty, addr);
  polyRAM[addr / 4] = data;
}

void CReal3D::WriteJTAGModeword(CASIC::Name device, uint32_t data)
{
    if (device == CASIC::Name::Dummy)
        return;

    m_modeword[static_cast<int>(device)] = data;

    switch (device)
    {
    case CASIC::Name::Mercury:
        // block culling (disables rendering for 3d h/w)
        break;
    case CASIC::Name::Mars:
        Render3D->SetSunClamp((data & 0x40000) == 0);
        break;
    default:
        break;
    }
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
	  uint32_t ping_pong = (m_pingPong ? 0x02000000 : 0x0);
	  return 0xfdffffff | ping_pong;
  }

  else if (reg >= 20 && reg<=32) {	// line of sight registers

	int index = (reg - 20) / 4;
	float val = Render3D->GetLosValue(index);
	return Util::FloatAsUint32(val);
  }

  return 0xffffffff;
}

// TODO: This returns data in the way that the PowerPC bus expects. Other functions in CReal3D should
// return data this way.
uint32_t CReal3D::ReadPCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset) const
{
  uint32_t  d;

  if (bits == 8)
  {
    DebugLog("Real3D: %d-bit PCI read request for reg=%02X\n", bits, reg);
    return 0;
  }

  // This is a little endian device, must return little endian words
  switch (reg)
  {
  case 0x00:  // Device ID and Vendor ID
    d = FLIPENDIAN32(m_hwConfig.pciID);
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
  m_tilegenDrawFrame = false;
  m_blockCullingRO = false;

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

  for (auto& r : m_configRegisters.regs) {
      r = 0;
  }

  m_polyUpdateBlock = nullptr;
  m_highRamUpdateBlock = nullptr;

  unsigned memSize = (m_gpuMultiThreaded ? m_layout.total : m_layout.sizeRW);
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

  // If mult-threaded, attach read-only snapshots to renderer instead of real
  // ones. If polygon RAM replaces VROM, the renderer must read the snapshot
  // of polygon RAM through both pointers.
  if (m_gpuMultiThreaded)
    Render3D->AttachMemory(cullingRAMLoRO, cullingRAMHiRO, polyRAMRO, m_hwConfig.vromIsPolygonRAM ? polyRAMRO : vrom, textureRAMRO);
  else
    Render3D->AttachMemory(cullingRAMLo, cullingRAMHi, polyRAM, vrom, textureRAM);

  Render3D->SetVROMIsPolygonRAM(m_hwConfig.vromIsPolygonRAM);

  DebugLog("Real3D attached a Render3D object\n");
}

Real3DConfig Real3DConfig::Model3(Game::Stepping stepping)
{
  Real3DConfig config;
  bool step2x = stepping == Game::Stepping::MODEL3_2_0 || stepping == Game::Stepping::MODEL3_2_1;
  config.pciID = step2x ? CReal3D::PCIID::Step2x : CReal3D::PCIID::Step1x;
  config.singleWordTexturePort = stepping == Game::Stepping::MODEL3_1_0;
  return config;
}

Real3DConfig Real3DConfig::Pro1000(uint32_t installedPolygonRAMSize)
{
  Real3DConfig config;
  config.polygonRAMSize = 0x4000000;
  config.installedPolygonRAMSize = installedPolygonRAMSize;
  config.vromIsPolygonRAM = true;
  config.pciID = CReal3D::PCIID::Step1x;
  config.singleWordTexturePort = true;
  return config;
}

Result CReal3D::Init(const uint8_t *vromPtr, IBus *BusObjectPtr, IIRQController *IRQObjectPtr, uint16_t dmaIRQBit, const Real3DConfig &hwConfig)
{
  // Memory layout for the hardware configuration
  m_hwConfig = hwConfig;
  m_hwConfig.installedPolygonRAMSize = std::min(m_hwConfig.installedPolygonRAMSize, m_hwConfig.polygonRAMSize);
  MemoryLayout &l = m_layout;
  l.cullingRAMLo = 0;
  l.cullingRAMHi = l.cullingRAMLo + CULLING_RAM_LO_SIZE;
  l.polyRAM = l.cullingRAMHi + CULLING_RAM_HI_SIZE;
  l.textureRAM = l.polyRAM + m_hwConfig.polygonRAMSize;
  l.textureFIFO = l.textureRAM + TEXTURE_RAM_SIZE;
  l.sizeRW = l.textureFIFO + TEXTURE_FIFO_SIZE;
  l.cullingRAMLoRO = l.sizeRW;
  l.cullingRAMHiRO = l.cullingRAMLoRO + CULLING_RAM_LO_SIZE;
  l.polyRAMRO = l.cullingRAMHiRO + CULLING_RAM_HI_SIZE;
  l.textureRAMRO = l.polyRAMRO + m_hwConfig.polygonRAMSize;
  l.sizeRO = CULLING_RAM_LO_SIZE + CULLING_RAM_HI_SIZE + m_hwConfig.polygonRAMSize + TEXTURE_RAM_SIZE;
  l.cullingRAMLoDirty = l.sizeRW + l.sizeRO;
  l.cullingRAMHiDirty = l.cullingRAMLoDirty + DIRTY_SIZE(CULLING_RAM_LO_SIZE);
  l.polyRAMDirty = l.cullingRAMHiDirty + DIRTY_SIZE(CULLING_RAM_HI_SIZE);
  l.textureRAMDirty = l.polyRAMDirty + DIRTY_SIZE(m_hwConfig.polygonRAMSize);
  l.sizeDirty = DIRTY_SIZE(l.sizeRO);
  l.total = l.sizeRW + l.sizeRO + l.sizeDirty;

  uint32_t memSize = (m_gpuMultiThreaded ? l.total : l.sizeRW);
  float  memSizeMB = (float)memSize/(float)0x100000;

  // IRQ and bus objects
  Bus = BusObjectPtr;
  IRQ = IRQObjectPtr;
  dmaIRQ = dmaIRQBit;

  // Allocate all Real3D RAM regions
  memoryPool = new(std::nothrow) uint8_t[memSize];
  if (NULL == memoryPool)
    return ErrorLog("Insufficient memory for Real3D object (needs %1.1f MB).", memSizeMB);

  // new[] does not initialize the memory. Clear it, or the renderer reads
  // random values where a program writes no data.
  memset(memoryPool, 0, memSize);

  // Set up main pointers
  cullingRAMLo = (uint32_t *) &memoryPool[l.cullingRAMLo];
  cullingRAMHi = (uint32_t *) &memoryPool[l.cullingRAMHi];
  polyRAM = (uint32_t *) &memoryPool[l.polyRAM];
  textureRAM = (uint16_t *) &memoryPool[l.textureRAM];
  textureFIFO = (uint32_t *) &memoryPool[l.textureFIFO];

  // If multi-threaded, set up pointers for read-only snapshots and dirty page arrays too
  if (m_gpuMultiThreaded)
  {
    cullingRAMLoRO = (uint32_t *) &memoryPool[l.cullingRAMLoRO];
    cullingRAMHiRO = (uint32_t *) &memoryPool[l.cullingRAMHiRO];
    polyRAMRO = (uint32_t *) &memoryPool[l.polyRAMRO];
    textureRAMRO = (uint16_t *) &memoryPool[l.textureRAMRO];
    cullingRAMLoDirty = (uint8_t *) &memoryPool[l.cullingRAMLoDirty];
    cullingRAMHiDirty = (uint8_t *) &memoryPool[l.cullingRAMHiDirty];
    polyRAMDirty = (uint8_t *) &memoryPool[l.polyRAMDirty];
    textureRAMDirty = (uint8_t *) &memoryPool[l.textureRAMDirty];
  }

  // VROM pointer passed to us. If there is no VROM, polygon RAM replaces it:
  // the same word address gives the same location through either pointer.
  if (m_hwConfig.vromIsPolygonRAM)
    vrom = polyRAM;
  else
    vrom = (uint32_t *) vromPtr;

  DebugLog("Initialized Real3D (allocated %1.1f MB)\n", memSizeMB);
  return Result::OKAY;
}

CReal3D::CReal3D(const Util::Config::Node &config)
  : m_config(config),
    m_gpuMultiThreaded(config["GPUMultiThreaded"].ValueAs<bool>())
{
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
    fwrite(polyRAM, sizeof(uint8_t), m_hwConfig.polygonRAMSize, fp);
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

  Render3D = nullptr;
  delete [] memoryPool;
  memoryPool = nullptr;
  cullingRAMLo = nullptr;
  cullingRAMHi = nullptr;
  polyRAM = nullptr;
  textureRAM = nullptr;
  textureFIFO = nullptr;
  vrom = nullptr;

  m_polyUpdateBlock = nullptr;
  m_highRamUpdateBlock = nullptr;

  DebugLog("Destroyed Real3D\n");
}
