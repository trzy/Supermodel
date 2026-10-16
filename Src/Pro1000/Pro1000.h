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
 * Pro1000.h
 *
 * Header file defining the CPro1000 class.
 */

#ifndef INCLUDED_PRO1000_H
#define INCLUDED_PRO1000_H

#include "Game.h"
#include "Pro1000/8259.h"
#include "Pro1000/SCANPSC100F.h"
#include "Pro1000/SCSITCPBridge.h"
#include "Pro1000/SCSITCPBridgeProtocol.h"
#include "Pro1000/SCSIHostAdapter.h"
#include "Pro1000/UART.h"
#include "Model3/53C810.h"
#include "Model3/JTAG.h"
#include "Model3/IEmulator.h"
#include "Model3/MPC10x.h"
#include "Model3/PCI.h"
#include "Model3/Real3D.h"
#include "CPU/Bus.h"
#include "CPU/PowerPC/ppc.h"
#include "Util/NewConfig.h"
#include "Graphics/SuperAA.h"
#include "OSD/Thread.h"
#include <memory>
#include <vector>

/*
 * CPro1000:
 *
 * A Real3D Pro-1000 image generator. Very little is known about this hardware,
 * so for now, it is modeled as a stripped-down Step 1.0 Model 3 board: a
 * PowerPC 603 with RAM, flash ROM, an MPC105 PCI bridge, an NCR 53C810 SCSI
 * controller, and the Real3D graphics hardware.
 *
 * Inherits IBus in order to pass the address space handlers to devices that
 * may need them (CPU, DMA, etc.)
 *
 * NOTE: Currently NOT re-entrant due to a non-OOP PowerPC core. Do NOT create
 * more than one CPro1000 (or CModel3) object!
 */
class CPro1000: public IEmulator, public IBus
{
public:
  // IEmulator interface
  bool PauseThreads(void);
  bool ResumeThreads(void);
  void SaveState(CBlockFile *SaveState);
  void LoadState(CBlockFile *SaveState);
  void SaveNVRAM(CBlockFile *NVRAM);
  void LoadNVRAM(CBlockFile *NVRAM);
  void ClearNVRAM(void);
  void RunFrame(void);
  void RenderFrame(void);
  void Reset(void);
  const Game &GetGame(void) const;
  Result LoadGame(const Game &game, const ROMSet &rom_set);
  void AttachRenderers(CRender2D *Render2DPtr, IRender3D *Render3DPtr, SuperAA *superAA);
  void AttachInputs(CInputs *InputsPtr);
  void AttachOutputs(COutputs *OutputsPtr);
  void AttachConsoleInput(IConsoleInput *input) override;
  Result Init(void);

  // IBus interface
  UINT8 Read8(UINT32 addr);
  UINT16 Read16(UINT32 addr);
  UINT32 Read32(UINT32 addr);
  UINT64 Read64(UINT32 addr);
  void Write8(UINT32 addr, UINT8 data);
  void Write16(UINT32 addr, UINT16 data);
  void Write32(UINT32 addr, UINT32 data);
  void Write64(UINT32 addr, UINT64 data);

  /*
   * DumpTimings(void):
   *
   * Prints all timings for the most recent frame to the console, for debugging purposes.
   */
  void DumpTimings(void);

  /*
   * CPro1000(config):
   * ~CPro1000(void):
   *
   * Constructor and destructor. Constructor performs a bare-bones
   * initialization of object; does not perform any memory allocation or any
   * actions that can fail. The destructor will deallocate memory and free
   * resources used by the object (and its child objects).
   *
   * Paramters:
   *    config  Run-time configuration. The reference should be held because
   *            this changes at run-time.
   */
  CPro1000(Util::Config::Node &config);
  ~CPro1000(void);

private:
  /*
   * Timings within a frame, for debugging purposes
   */
  struct FrameTimings
  {
    UINT32 ppcTicks;
    UINT32 syncSize;
    UINT32 syncTicks;
    UINT32 renderTicks;
    UINT32 frameTicks;
    UINT64 frameId;
  };

  bool IsSCSIAddress(UINT32 addr) const;          // True if address falls within SCSI controller's PCI-mapped register window

  void BuildScanChain(void);                    // Creates JTAG devices and connects them to the scan controller
  void RunMainBoardFrame(void);                 // Runs PPC main board for a frame
  void SyncGPUs(void);                          // Sync's up GPUs in preparation for rendering - must be called when PPC is not running

  bool    StartThreads(void);                   // Starts all threads
  bool    StopThreads(void);                    // Stops all threads
  void    DeleteThreadObjects(void);            // Deletes all threads and synchronization objects

  static int StartMainBoardThread(void *data);  // Callback to start PPC main board thread
  int     RunMainBoardThread(void);             // Runs PPC main board thread (sync'd in step with render thread)

  // Runtime configuration
  Util::Config::Node &m_config;
  bool m_multiThreaded;
  bool m_gpuMultiThreaded;

  // Game information
  Game m_game;

  // Emulated memory regions
  UINT8   *memoryPool;  // single allocated region for all ROM and system RAM
  UINT8   *ram;         // 4 MB PowerPC RAM
  UINT8   *flash;       // 8 MB flash ROM window
  UINT8   *scsiROM;     // 512 KB SCSI ROM (flash on SCSI controller's expansion ROM interface)

  // PowerPC
  PPC_FETCH_REGION  PPCFetchRegions[3];

  // Multiple threading
  bool        gpusReady;            // True if GPUs are ready to render
  bool        startedThreads;       // True if threads have been created and started
  bool        pauseThreads;         // True if threads should pause
  bool        stopThreads;          // True if threads should stop
  CThread     *ppcBrdThread;        // PPC main board thread
  bool        ppcBrdThreadRunning;  // Flag to indicate PPC main board thread is currently processing
  bool        ppcBrdThreadDone;     // Flag to indicate PPC main board thread has finished processing

  // Thread synchronization objects
  CSemaphore  *ppcBrdThreadSync;
  CMutex      *notifyLock;
  CCondVar    *notifySync;

  // Frame timings
  FrameTimings timings;

  // Devices
  C8259       PIC;          // Intel 8259A interrupt controller
  CSCANPSC100F JTAG;        // SCANPSC100F JTAG scan controller
  CMPC10x     PCIBridge;    // MPC105 PCI/bridge/memory controller
  CPCIBus     PCIBus;       // PCI bus
  C53C810     SCSI;         // SCSI controller (NCR 53C825 on real hardware, emulated as 53C810)
  CSCSIHostAdapter SCSIHostAdapter; // host computer's SCSI host adapter (initiator on the SCSI bus)
  std::unique_ptr<CSCSITCPBridge> m_scsiTCPBridge;  // connects host computer programs to the host adapter (destroyed before it)
  CReal3D     GPU;          // Real3D graphics hardware
  std::vector<std::unique_ptr<CASIC>> m_asics;  // Real3D ASICs on the JTAG scan chain
  C3DRAM      m_3dram[4];                       // 3D-RAMs on the JTAG scan chain
  CJTAGScanChain m_scanChain;                   // JTAG scan chain driven by JTAG controller
  CUART       UART;         // 16550-style serial port
  SuperAA     *m_superAA;
};


#endif  // INCLUDED_PRO1000_H
