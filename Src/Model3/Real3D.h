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
 * Real3D.h
 * 
 * Header file defining the CReal3D class: the Model 3's Real3D-based graphics
 * hardware.
 */

#ifndef INCLUDED_REAL3D_H
#define INCLUDED_REAL3D_H

#include "IRQ.h"
#include "PCI.h"
#include "CPU/Bus.h"
#include "Graphics/IRender3D.h"
#include "Util/NewConfig.h"

#include <cstdint>
#include <unordered_map>

/* 
 * QueuedUploadTextures:
 *
 * When rendering is multi-threaded, this struct is used to represent a postponed
 * call to CRender3D::UploadTextures that will be performed by the render thread
 * at the beginning of the next frame, rather than directly in the PPC thread.
 */
struct QueuedUploadTextures
{
  unsigned level;	// mipmap level of the texture, saves calculating this later
  unsigned x;
  unsigned y;
  unsigned width;
  unsigned height;
};

/*
 * CReal3D:
 *
 * Model 3 Real3D-based graphics hardware. This class manages the hardware
 * state and drives the rendering process (scene database traversal). Actual
 * rasterization and matrix transformations are carried out by the graphics
 * engine.
 */
class CReal3D: public IPCIDevice
{
public:
  /*
   * PCI IDs
   *
   * The CReal3D object must be configured with the PCI ID of the ASIC directly
   * connected to the PCI slot; 0x16c311db for Step 1.x, 0x178611db for Step
   * 2.x. Requesting PCI ID via the DMA device on Step 2.x appears to return
   * the PCI ID of Mercury which is the ASIC connected to the PCI slot on Step
   * 1.x, hence why some Step 2.x games appear to expect the 1.x ID.
   *
   * The vendor ID code 0x11db is Sega's.
   */
  enum PCIID: uint32_t
  {
    Step1x = 0x16C311DB,  // Step 1.x
    Step2x = 0x178611DB   // Step 2.x
  };

  /*
   * ASIC Names
   *
   * These were determined from Virtual On, which prints them out if any of the
   * ID codes are incorrect. ID codes depend on stepping.
   */
  enum ASIC
  {
    Mercury,
    Venus,
    Earth,
    Mars,
    Jupiter
  };

  /*
   * SaveState(SaveState):
   *
   * Saves an image of the current device state.
   *
   * Parameters:
   *    SaveState   Block file to save state information to.
   */
  void SaveState(CBlockFile *SaveState);

  /*
   * LoadState(SaveState):
   *
   * Loads and a state image.
   *
   * Parameters:
   *    SaveState   Block file to load state information from.
   */
  void LoadState(CBlockFile *SaveState);

  /*
   * BeginVBlank(void):
   *
   * Must be called before the VBlank starts.
   */
  void BeginVBlank(int statusCycles);
  
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
  uint32_t SyncSnapshots(void);

  /*
   * BeginFrame(void):
   *
   * Prepares to render a new frame.  Must be called once per frame prior to
   * drawing anything and must only access read-only snapshots and variables
   * since it may be running in a separate thread.
   */
  void BeginFrame(void);
  
  /*
   * RenderFrame(void):
   *
   * Traverses the scene database and renders a frame.  Must be called after
   * BeginFrame() but before EndFrame() and must only access read-only snapshots
   * and variables since it may be running in a separate thread.
   */
  void RenderFrame(void);

  /*
   * EndFrame(void):
   *
   * Signals the end of rendering for this frame.  Must be called last during
   * the frame and must only access read-only snapshots and variables since it
   * may be running in a separate thread.
   */
  void EndFrame(void);
  
  /*
   * Flush(void):
   *
   * Triggers the beginning of a new frame. All textures in the texture FIFO
   * are uploaded and the FIFO is reset. On the real device, this seems to 
   * cause a frame to be rendered as well but this is not performed here.
   *
   * This should be called when the command port is written.
   */
  void Flush(void);
   
  /*
   * ReadDMARegister8(reg):
   * ReadDMARegister32(reg):
   *
   * Reads from a DMA register. Multi-byte reads are returned as little
   * endian and must be flipped if called by a big endian device.
   *
   * Parameters:
   *    reg   Register number to read from (0-0xFF only).
   *
   * Returns:
   *    Data of the requested size, in little endian.
   */
  uint8_t ReadDMARegister8(unsigned reg);
  uint32_t ReadDMARegister32(unsigned reg);
  
  /*
   * WriteDMARegister8(reg, data):
   * WriteDMARegister32(reg, data);
   *
   * Write to a DMA register. Multi-byte writes by big endian devices must be
   * byte reversed (this is a little endian device).
   *
   * Parameters:
   *    reg   Register number to read from (0-0xFF only).
   *    data  Data to write.
   */
  void WriteDMARegister8(unsigned reg, uint8_t data);
  void WriteDMARegister32(unsigned reg, uint32_t data);
  
  /*
   * WriteLowCullingRAM(addr, data):
   *
   * Writes the low culling RAM region. Because this is a little endian
   * device, big endian devices/buses have to take care to manually reverse
   * the data before writing. 
   * 
   * Parameters:
   *    addr  Word (32-bit) aligned address ranging from 0 to 0x3FFFFC.
   *          User must ensure address is properly clamped.
   *    data  Data to write.
   */
  void WriteLowCullingRAM(uint32_t addr, uint32_t data);
  
  /*
   * WriteHighCullingRAM(addr, data):
   *
   * Writes the high culling RAM region. Because this is a little endian
   * device, big endian devices/buses have to take care to manually reverse
   * the data before writing. 
   * 
   * Parameters:
   *    addr  Word (32-bit) aligned address ranging from 0 to 0xFFFFC.
   *          User must ensure address is properly clamped.
   *    data  Data to write.
   */
  void WriteHighCullingRAM(uint32_t addr, uint32_t data);
  
  /*
   * WriteTextureFIFO(data):
   *
   * Writes to the 1MB texture FIFO. Because this is a little endian device,
   * big endian devices/buses have to take care to manually reverse the data
   * before writing.
   *
   * Parameters:
   *    data  Data to write.
   */
  void WriteTextureFIFO(uint32_t data);
  
  /*
   * WriteTexturePort(reg, data):
   *
   * Writes to the VROM texture ports. Register 0 is the word-granular VROM
   * address of the texture, register 4 is the texture information header,
   * and register 8 is the size of the texture in words.
   *
   * Parameters:
   *    reg   Register number: must be 0, 4, 8, 0xC, 0x10, or 0x14 only.
   *    data  The 32-bit word to write to the register. A write to
   *          register 8 triggers the upload.
   */
  void WriteTexturePort(unsigned reg, uint32_t data);
  
  /*
   * WritePolygonRAM(addr, data):
   *
   * Writes the polygon RAM region. Because this is a little endian device,
   * big endian devices/buses have to take care to manually reverse the data
   * before writing. 
   * 
   * Parameters:
   *    addr  Word (32-bit) aligned address ranging from 0 to 0x3FFFFC.
   *          User must ensure address is properly clamped.
   *    data  Data to write.
   */
  void WritePolygonRAM(uint32_t addr, uint32_t data);
  
  /*
   * WriteJTAGRegister(instruction, data):
   *
   * Write to an internal register using the JTAG interface. This is intended
   * to be called from the JTAG emulation for instructions that are known to
   * poke the internal state of Real3D ASICs.
   *
   * Parameters:
   *    instruction   Value of the JTAG instruction register.
   *    data          Data written.
   */
  void WriteJTAGRegister(uint64_t instruction, uint64_t data);

  /*
   * ReadRegister(reg):
   *
   * Reads one of the status registers.
   *
   * Parameters:
   *    reg   Register offset (32-bit aligned). From 0x00 to 0x3C.
   *
   * Returns:
   *    The 32-bit status register.
   */
  uint32_t ReadRegister(unsigned reg);
  
  /*
   * ReadPCIConfigSpace(device, reg, bits, offset):
   *
   * Reads a PCI configuration space register. See CPCIDevice definition for
   * more details.
   *
   * Parameters:
   *    device  Device number (ignored, not needed).
   *    reg     Register number.
   *    bits    Bit width of access (8, 16, or 32 only).;
   *    offset  Byte offset within register, aligned to the specified bit
   *            width, and offset from the 32-bit aligned base of the
   *            register number.
   *
   * Returns:
   *    Register data.
   */
  uint32_t ReadPCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned width);
  
  /*
   * WritePCIConfigSpace(device, reg, bits, offset, data):
   *
   * Writes to a PCI configuration space register. See CPCIDevice definition
   * for more details.
   *
   * Parameters:
   *    device  Device number (ignored, not needed).
   *    reg     Register number.
   *    bits    Bit width of access (8, 16, or 32 only).
   *    offset  Byte offset within register, aligned to the specified bit
   *            width, and offset from the 32-bit aligned base of the
   *            register number.
   *    data    Data.
   */
  void WritePCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned width, uint32_t data);
  
  /*
   * Reset(void):
   *
   * Resets the Real3D device. Must be called before reading/writing the
   * device.
   */
  void Reset(void);
  
  /*
   * AttachRenderer(render3DPtr):
   *
   * Attaches a 3D renderer for the Real3D to use. This function will
   * immediately pass along the information that a CRender3D object needs to
   * work with.
   *
   * Parameters:
   *    Render3DPtr   Pointer to a 3D renderer object.
   */
  void AttachRenderer(IRender3D *Render3DPtr);
  
  /*
   * GetASICIDCodes(asic):
   *
   * Obtain ASIC ID code for the specified ASIC under the currently configured
   * hardware stepping.
   *
   * Parameters:
   *    asic  ASIC ID.
   *
   * Returns:
   *    The ASIC ID code. Undefined for invalid ASIC ID.
   */
  uint32_t GetASICIDCode(ASIC asic) const;

  /*
   * SetStepping(stepping):
   *
   * Sets the Model 3 hardware stepping, which also determines the Real3D
   * functionality. The default is Step 1.0. This should be called prior to 
   * any other emulation functions and after Init().
   *
   * Parameters:
   *    stepping    0x10 for Step 1.0, 0x15 for Step 1.5, 0x20 for Step 2.0, or
   *                0x21 for Step 2.1. Anything else defaults to 1.0.
   */
  void SetStepping(int stepping);

  
  /*
   * Init(vromPtr, BusObjectPtr, IRQObjectPtr, dmaIRQBit):
   *
   * One-time initialization of the context. Must be called prior to all
   * other members. Connects the Real3D device to its video ROM and allocates
   * memory for RAM regions.
   *
   * Parameters:
   *    vromPtr       A pointer to video ROM (with each 32-bit word in
   *                  its native little endian format).
   *    BusObjectPtr  Pointer to the bus that the 53C810 has control
   *                  over. Used to read/write memory.
   *    IRQObjectPtr  Pointer to the IRQ controller. Used to trigger SCSI
   *                  and DMA interrupts.
   *    dmaIRQBit     IRQ identifier bit to pass along to IRQ controller
   *                  when asserting interrupts.
   *
   * Returns:
   *    OKAY if successful otherwise FAIL (not enough memory). Prints own
   *    errors.
   */
  bool Init(const uint8_t *vromPtr, IBus *BusObjectPtr, CIRQ *IRQObjectPtr, unsigned dmaIRQBit);
   
  /*
   * CReal3D(config):
   * ~CReal3D(void):
   *
   * Constructor and destructor.
   *
   * Paramters:
   *    config  Run-time configuration. The reference should be held because
   *            this changes at run-time.
   */
  CReal3D(const Util::Config::Node &config);
  ~CReal3D(void);
  
private:
  // Private member functions
  void      DMACopy(void);
  void      StoreTexture(unsigned level, unsigned xPos, unsigned yPos, unsigned width, unsigned height, const uint16_t *texData, bool sixteenBit, bool writeLSB, bool writeMSB, uint32_t &texDataOffset);

  void      UploadTexture(uint32_t header, const uint16_t *texData);
  uint32_t  UpdateSnapshots(bool copyWhole);
  uint32_t  UpdateSnapshot(bool copyWhole, uint8_t *src, uint8_t *dst, unsigned size, uint8_t *dirty);

  // Config 
  const Util::Config::Node &m_config;
  const bool                m_gpuMultiThreaded;

  // Renderer attached to the Real3D
  IRender3D *Render3D;
  
  // Data passed from Model 3 object
  const uint32_t  *vrom;  // Video ROM
  int             step;   // hardware stepping (as in GameInfo structure)
  uint32_t        pciID;  // PCI vendor and device ID
  
  // Error flag (to limit errors to once per frame)
  bool error; // true if an error occurred this frame

  // Real3D memory
  uint8_t   *memoryPool;        // all memory allocated here
  uint32_t  *cullingRAMLo;      // 4MB of culling RAM at 8C000000
  uint32_t  *cullingRAMHi;      // 1MB of culling RAM at 8E000000
  uint32_t  *polyRAM;           // 4MB of polygon RAM at 98000000
  uint16_t  *textureRAM;        // 8MB of internal texture RAM
  uint32_t  *textureFIFO;       // 1MB texture FIFO at 0x94000000
  uint32_t  fifoIdx;            // index into texture FIFO
  uint32_t  m_vromTextureFIFO[2];
  uint32_t  m_vromTextureFIFOIdx;
  
  // Read-only snapshots
  uint32_t  *cullingRAMLoRO;    // 4MB of culling RAM at 8C000000 [read-only snapshot]
  uint32_t  *cullingRAMHiRO;    // 1MB of culling RAM at 8E000000 [read-only snapshot]
  uint32_t  *polyRAMRO;         // 4MB of polygon RAM at 98000000 [read-only snapshot]
  uint16_t  *textureRAMRO;      // 8MB of internal texture RAM    [read-only snapshot]
  
  // Arrays to keep track of dirty pages in memory regions
  uint8_t   *cullingRAMLoDirty;
  uint8_t   *cullingRAMHiDirty;
  uint8_t   *polyRAMDirty;
  uint8_t   *textureRAMDirty;

  // Queued texture uploads
  std::vector<QueuedUploadTextures> queuedUploadTextures;
  std::vector<QueuedUploadTextures> queuedUploadTexturesRO;  // Read-only copy of queue
  
  // Big endian bus object for DMA memory access
  IBus  *Bus;
  
  // IRQ handling
  CIRQ    *IRQ;   // IRQ controller
  uint32_t  dmaIRQ; // IRQ bit to use when calling IRQ handler
  
  // DMA device
  uint32_t  dmaSrc;
  uint32_t  dmaDest;
  uint32_t  dmaLength;
  uint32_t  dmaData;
  uint32_t  dmaUnknownReg;
  uint8_t   dmaStatus;
  uint8_t   dmaConfig;
  
  // Command port
  bool  commandPortWritten;
  bool  commandPortWrittenRO; // Read-only copy of flag
  
  // Status and command registers
  uint32_t m_pingPong;
  uint64_t statusChange = 0;
  bool m_evenFrame = false;
  
  // Internal ASIC state
  std::unordered_map<ASIC, uint32_t> m_asicID;
  uint64_t m_internalRenderConfig[2];
};


#endif  // INCLUDED_REAL3D_H
