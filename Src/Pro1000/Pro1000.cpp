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
 * Pro1000.cpp
 *
 * Implementation of the CPro1000 class: a Real3D Pro-1000 image generator.
 *
 * The Pro-1000 was attached to a Windows NT host via SCSI. Its memory map is
 * not yet known, so it is presently modeled as a Step 1.0 Model 3 with all of
 * the Sega-specific hardware (tile generator, sound board, inputs, system
 * registers, etc.) removed.
 *
 * PowerPC Address Map (assumed to match Model 3 Step 1.0)
 * -------------------
 * 00000000-007FFFFF  RAM
 * 84000000-8400003F  Real3D Status Registers
 * 88000000-88000007  Real3D Command Port
 * 8C000000-8C3FFFFF  Real3D Culling RAM (Low)
 * 8E000000-8E0FFFFF  Real3D Culling RAM (High)
 * 94000000-940FFFFF  Real3D Texture FIFO
 * 98000000-980FFFFF  Real3D Polygon RAM
 * 9C000000-9CFFFFFF  Real3D Configuration Registers
 * 64000000-6400003F  UART (16550-style, register n at 64000000+n*8+3)
 * D0000000-D00000FF  SCSI (PCI device 11 memory BAR, programmed by firmware)
 * F0800CF8-F0800CFF  MPC105 CONFIG_ADDR
 * F0C00CF8-F0C00CFF  MPC105 CONFIG_DATA
 * F8FFF000-F8FFF0FF  MPC105 Registers
 * FF800000-FFFFFFFF  CROM
 *
 * The PowerPC resets to 0xFFF00100, so CROM is loaded at 0xFFF00000 rather
 * than being aligned to the top of the address space as on Model 3.
 *
 * There is no VROM, so the Real3D VROM texture port (0x90000000 on Model 3)
 * is not mapped.
 *
 * Endianness
 * ----------
 * As with CModel3, PowerPC RAM and ROM regions are byte reversed so that
 * aligned words can be accessed without conversion on little endian hosts.
 */

#include "Pro1000.h"

#include <new>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "Supermodel.h"
#include "ROMSet.h"
#include "OSD/Video.h"
#include "Util/ByteSwap.h"

constexpr static uint32_t RAM_SIZE          = 0x800000;   // 8MB
constexpr static uint32_t CROM_SIZE         = 0x800000;   // 8MB
constexpr static uint32_t VROM_SIZE         = 0x4000000;  // 64MB

constexpr static uint32_t CROM_BASE_ADDR    = 0xFF800000; // start of CROM in PowerPC address space
constexpr static uint32_t CROM_LOAD_ADDR    = 0xFFF00000; // where the CROM image is loaded (reset vector is at +0x100)
constexpr static uint32_t CROM_LOAD_OFFSET  = CROM_LOAD_ADDR - CROM_BASE_ADDR;

constexpr static uint32_t MEM_POOL_SIZE     = RAM_SIZE + CROM_SIZE + VROM_SIZE;
constexpr static uint32_t RAM_OFFSET        = 0;
constexpr static uint32_t CROM_OFFSET       = RAM_OFFSET + RAM_SIZE;
constexpr static uint32_t VROM_OFFSET       = CROM_OFFSET + CROM_SIZE;

constexpr static int      STEPPING          = 0x10;       // treat hardware as Model 3 Step 1.0
constexpr static unsigned PPC_FREQUENCY_MHZ = 66;         // Step 1.0 PowerPC frequency

// On Model 3, the ping-pong flip line is programmed into a tile generator
// register. There is no tile generator here, so use the value most Model 3
// games program.
constexpr static unsigned PING_PONG_FLIP_LINE = 238;

// UART registers are 8 bytes apart and occupy the least significant byte of
// each big endian 32-bit word
constexpr static uint32_t UART_BASE_ADDR    = 0x64000000;
constexpr static uint32_t UART_END_ADDR     = UART_BASE_ADDR + 8 * 8;

static inline unsigned UARTRegister(UINT32 addr)
{
  return (addr >> 3) & 7;
}

// SCSI controller is PCI device 11 (an NCR 53C825 on the real hardware;
// emulated using the 53C810). Its registers are located wherever firmware
// maps its memory BAR (0xD0000000 has been observed). A base address of 0
// means it has not been mapped yet.
constexpr static unsigned SCSI_PCI_DEVICE   = 11;

bool CPro1000::IsSCSIAddress(UINT32 addr) const
{
  UINT8 base = SCSI.GetBaseAddress();
  return base != 0 && (addr >> 24) == base && (addr & 0x00FFFF00) == 0;
}


/******************************************************************************
 Address Space Access Handlers
******************************************************************************/

UINT8 CPro1000::Read8(UINT32 addr)
{
  // RAM (most frequently accessed)
  if (addr < 0x00800000)
    return ram[addr ^ 3];

  // SCSI
  if (IsSCSIAddress(addr))
    return SCSI.ReadRegister(addr & 0xFF);

  // Other
  switch ((addr >> 24))
  {
  // CROM
  case 0xFF:
    if (addr >= CROM_BASE_ADDR)
      return crom[(addr & 0x7FFFFF) ^ 3];
    break;

  // UART
  case 0x64:
    if (addr < UART_END_ADDR && (addr & 7) == 3)
      return UART.Read(UARTRegister(addr));
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\tread8 : %08X\n", ppc_get_pc(), addr);
  return 0xFF;
}

UINT16 CPro1000::Read16(UINT32 addr)
{
  UINT16 data;

  if ((addr & 1))
  {
    data =  Read8(addr + 0) << 8;
    data |= Read8(addr + 1);
    return data;
  }

  // RAM (most frequently accessed)
  if (addr < 0x00800000)
    return *(UINT16 *) &ram[addr ^ 2];

  // SCSI
  if (IsSCSIAddress(addr))
  {
    data =  SCSI.ReadRegister((addr + 0) & 0xFF) << 8;
    data |= SCSI.ReadRegister((addr + 1) & 0xFF);
    return data;
  }

  // Other
  switch ((addr >> 24))
  {
  // CROM
  case 0xFF:
    if (addr >= CROM_BASE_ADDR)
      return *(UINT16 *) &crom[(addr & 0x7FFFFF) ^ 2];
    break;

  case 0xF0:
    // MPC105 CONFIG_DATA (F0C00CF8)
    if (((addr >> 16) & 0xFF) == 0xC0)
      return PCIBridge.ReadPCIConfigData(16, addr & 3);
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\tread16: %08X\n", ppc_get_pc(), addr);
  return 0xFFFF;
}

UINT32 CPro1000::Read32(UINT32 addr)
{
  UINT32 data;

  if ((addr & 3))
  {
    data =  Read16(addr + 0) << 16;
    data |= Read16(addr + 2);
    return data;
  }

  // RAM (most frequently accessed)
  if (addr < 0x00800000)
    return *(UINT32 *) &ram[addr];

  // SCSI
  if (IsSCSIAddress(addr))
  {
    data =  (SCSI.ReadRegister((addr + 0) & 0xFF) << 24);
    data |= (SCSI.ReadRegister((addr + 1) & 0xFF) << 16);
    data |= (SCSI.ReadRegister((addr + 2) & 0xFF) << 8);
    data |= (SCSI.ReadRegister((addr + 3) & 0xFF) << 0);
    return data;
  }

  // Other
  switch ((addr >> 24))
  {
  // CROM
  case 0xFF:
    if (addr >= CROM_BASE_ADDR)
      return *(UINT32 *) &crom[(addr & 0x7FFFFF)];
    break;

  // Real3D registers
  case 0x84:
    data = GPU.ReadRegister(addr & 0x3F);
    return FLIPENDIAN32(data);

  case 0xF0:
    // MPC105 CONFIG_DATA (F0C00CF8)
    if (((addr >> 16) & 0xFF) == 0xC0)
      return PCIBridge.ReadPCIConfigData(32, 0);
    break;

  // UART
  case 0x64:
    if (addr < UART_END_ADDR && (addr & 7) == 0)
      return 0xFFFFFF00 | UART.Read(UARTRegister(addr));
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\tread32: %08X\n", ppc_get_pc(), addr);
  return 0xFFFFFFFF;
}

UINT64 CPro1000::Read64(UINT32 addr)
{
  UINT64 data;

  data = Read32(addr + 0);
  data <<= 32;
  data |= Read32(addr + 4);
  return data;
}

void CPro1000::Write8(UINT32 addr, UINT8 data)
{
  // RAM (most frequently accessed)
  if (addr < 0x00800000)
  {
    ram[addr ^ 3] = data;
    return;
  }

  // SCSI
  if (IsSCSIAddress(addr))
  {
    SCSI.WriteRegister(addr & 0xFF, data);
    return;
  }

  // Other
  switch ((addr >> 24))
  {
  // MPC105
  case 0xF8:
    PCIBridge.WriteRegister(addr & 0xFF, data);
    return;

  // UART
  case 0x64:
    if (addr < UART_END_ADDR && (addr & 7) == 3)
    {
      UART.Write(UARTRegister(addr), data);
      return;
    }
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\twrite8 : %08X=%02X\n", ppc_get_pc(), addr, data);
}

void CPro1000::Write16(UINT32 addr, UINT16 data)
{
  if ((addr & 1))
  {
    Write8(addr + 0, data >> 8);
    Write8(addr + 1, data & 0xFF);
    return;
  }

  // RAM (most frequently accessed)
  if (addr < 0x00800000)
  {
    *(UINT16 *) &ram[addr ^ 2] = data;
    return;
  }

  // SCSI
  if (IsSCSIAddress(addr))
  {
    SCSI.WriteRegister((addr + 0) & 0xFF, data >> 8);
    SCSI.WriteRegister((addr + 1) & 0xFF, data & 0xFF);
    return;
  }

  // Other
  switch ((addr >> 24))
  {
  case 0xF0:
    // MPC105 CONFIG_DATA (F0C00CF8)
    if (((addr >> 16) & 0xFF) == 0xC0)
    {
      PCIBridge.WritePCIConfigData(16, addr & 2, data);
      return;
    }
    break;

  // MPC105
  case 0xF8:
    // Write in big endian order, like a real PowerPC
    PCIBridge.WriteRegister((addr & 0xFF) + 0, data >> 8);
    PCIBridge.WriteRegister((addr & 0xFF) + 1, data & 0xFF);
    return;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\twrite16: %08X=%04X\n", ppc_get_pc(), addr, data);
}

void CPro1000::Write32(UINT32 addr, UINT32 data)
{
  if ((addr & 3))
  {
    Write16(addr + 0, data >> 16);
    Write16(addr + 2, data);
    return;
  }

  // RAM (most frequently accessed)
  if (addr < 0x00800000)
  {
    *(UINT32 *) &ram[addr] = data;
    return;
  }

  // SCSI
  if (IsSCSIAddress(addr))
  {
    SCSI.WriteRegister((addr & 0xFF) + 0, (data >> 24) & 0xFF);
    SCSI.WriteRegister((addr & 0xFF) + 1, (data >> 16) & 0xFF);
    SCSI.WriteRegister((addr & 0xFF) + 2, (data >> 8) & 0xFF);
    SCSI.WriteRegister((addr & 0xFF) + 3, data & 0xFF);
    return;
  }

  // Other
  switch ((addr >> 24))
  {
  // Real3D trigger
  case 0x88:  // 88000000
    GPU.Flush();
    return;

  // Real3D low culling RAM
  case 0x8C:  // 8C000000-8C400000
    GPU.WriteLowCullingRAM(addr & 0x3FFFFF, FLIPENDIAN32(data));
    return;

  // Real3D high culling RAM
  case 0x8E:  // 8E000000-8E100000
    GPU.WriteHighCullingRAM(addr & 0xFFFFF, FLIPENDIAN32(data));
    return;

  // Real3D texture FIFO
  case 0x94:  // 94000000-94100000
    GPU.WriteTextureFIFO(FLIPENDIAN32(data));
    return;

  // Real3D polygon RAM
  case 0x98:  // 98000000-98400000
    GPU.WritePolygonRAM(addr & 0x3FFFFF, FLIPENDIAN32(data));
    return;

  // Real3D configuration registers
  case 0x9C:  // 9Cxxxxxx
    GPU.WriteConfigurationRegister(addr, FLIPENDIAN32(data));
    return;

  case 0xF0:
    switch ((addr >> 16) & 0xFF)
    {
    // MPC105 CONFIG_ADDR (F0800CF8)
    case 0x80:
      PCIBridge.WritePCIConfigAddress(data);
      return;

    // MPC105 CONFIG_DATA (F0C00CF8)
    case 0xC0:
      if (addr >= 0xF0C00CF8 && addr < 0xF0C00D00)
      {
        PCIBridge.WritePCIConfigData(32, 0, data);
        return;
      }
      break;

    default:
      break;
    }
    break;

  // MPC105
  case 0xF8:  // F8FFF000-F8FFF0FF
    // Write in big endian order, like a real PowerPC
    PCIBridge.WriteRegister((addr & 0xFF) + 0, (data >> 24) & 0xFF);
    PCIBridge.WriteRegister((addr & 0xFF) + 1, (data >> 16) & 0xFF);
    PCIBridge.WriteRegister((addr & 0xFF) + 2, (data >> 8) & 0xFF);
    PCIBridge.WriteRegister((addr & 0xFF) + 3, data & 0xFF);
    return;

  // UART
  case 0x64:
    if (addr < UART_END_ADDR && (addr & 7) == 0)
    {
      UART.Write(UARTRegister(addr), data & 0xFF);
      return;
    }
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\twrite32: %08X=%08X\n", ppc_get_pc(), addr, data);
}

void CPro1000::Write64(UINT32 addr, UINT64 data)
{
  Write32(addr + 0, (UINT32) (data >> 32));
  Write32(addr + 4, (UINT32) data);
}


/******************************************************************************
 Emulation and Interface Functions
******************************************************************************/

void CPro1000::SaveState(CBlockFile *SaveState)
{
  puts("Warning: Save states are not supported on the Real3D Pro-1000.");
  InfoLog("Save states are not supported on the Real3D Pro-1000.");
}

void CPro1000::LoadState(CBlockFile *SaveState)
{
  puts("Warning: Save states are not supported on the Real3D Pro-1000.");
  InfoLog("Save states are not supported on the Real3D Pro-1000.");
}

void CPro1000::SaveNVRAM(CBlockFile *NVRAM)
{
  // No NVRAM
}

void CPro1000::LoadNVRAM(CBlockFile *NVRAM)
{
  // No NVRAM
}

void CPro1000::ClearNVRAM(void)
{
  // No NVRAM
}

void CPro1000::RunFrame(void)
{
  UINT32 start = CThread::GetTicks();

  // See if currently running multi-threaded
  if (m_multiThreaded)
  {
    // If so, check all threads are up and running
    if (!StartThreads())
      goto ThreadError;

    // Wake PPC main board thread (if multi-threading GPU) so it can process a frame
    if (m_gpuMultiThreaded && !ppcBrdThreadSync->Post())
      goto ThreadError;

    // If not multi-threading GPU, then run PPC main board for a frame and sync GPUs now in this thread
    if (!m_gpuMultiThreaded)
    {
      RunMainBoardFrame();
      SyncGPUs();
    }

    // Render frame
    RenderFrame();

    // Enter notify wait critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Wait for PPC main board thread to finish its work (if it is running and hasn't finished already)
    while (m_gpuMultiThreaded && !ppcBrdThreadDone)
    {
      if (!notifySync->Wait(notifyLock))
        goto ThreadError;
    }
    ppcBrdThreadDone = false;

    // Leave notify wait critical section
    if (!notifyLock->Unlock())
      goto ThreadError;

    // If multi-threading GPU, then sync GPUs last while PPC main board thread is waiting
    if (m_gpuMultiThreaded)
      SyncGPUs();
  }
  else
  {
    // If not multi-threaded, then just process and render a single frame in this thread
    RunMainBoardFrame();
    SyncGPUs();
    RenderFrame();
  }

  timings.frameTicks = CThread::GetTicks() - start;
  timings.frameId++;
  return;

ThreadError:
  ErrorLog("Threading error in CPro1000::RunFrame: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
}

void CPro1000::RunMainBoardFrame(void)
{
  UINT32 start = CThread::GetTicks();

  /*
   * Display timings are assumed to be the same as Model 3: 57.524160 Hz with
   * 424 lines total, 384 display and 40 blanking/vsync.
   */
  unsigned mhz = m_config["PowerPCFrequency"].ValueAsDefault<unsigned>(0);
  unsigned ppcCycles    = (mhz ? mhz : PPC_FREQUENCY_MHZ) * 1000000;
  unsigned frameCycles  = (unsigned)((float)ppcCycles / 57.524160f);
  unsigned lineCycles   = frameCycles / 424;
  unsigned vBlankCycles = lineCycles * 40;

  // Scale PPC timer ratio according to speed at which the PowerPC is being
  // emulated so that the observed running frequency of the PPC timer
  // registers is more or less correct
  ppc_set_timer_ratio(ppc_get_bus_freq_multipler() * 2 * ppcCycles / ppc_get_cycles_per_sec());

  // VBlank
  if (gpusReady)
  {
    GPU.BeginVBlank();
    ppc_execute(vBlankCycles);
    GPU.EndVBlank();
  }

  // Run the PowerPC for the active display part of the frame
  for (unsigned i = 0; i < 384; i++)
  {
    if (i == PING_PONG_FLIP_LINE)
      GPU.FlipPingPongBit();
    ppc_execute(lineCycles);
  }

  timings.ppcTicks = CThread::GetTicks() - start;
}

void CPro1000::SyncGPUs(void)
{
  UINT32 start = CThread::GetTicks();

  timings.syncSize = GPU.SyncSnapshots();
  gpusReady = true;

  timings.syncTicks = CThread::GetTicks() - start;
}

void CPro1000::RenderFrame(void)
{
  UINT32 start = CThread::GetTicks();

  // Call OSD video callbacks
  if (BeginFrameVideo() && gpusReady)
  {
    GPU.BeginFrame();
    GPU.RenderFrame();
    GPU.EndFrame();
    m_superAA->Draw();
  }

  EndFrameVideo();

  timings.renderTicks = CThread::GetTicks() - start;
}

bool CPro1000::StartThreads(void)
{
  if (startedThreads)
    return true;

  // Create synchronization objects
  if (m_gpuMultiThreaded)
  {
    ppcBrdThreadSync = CThread::CreateSemaphore(0);
    if (ppcBrdThreadSync == NULL)
      goto ThreadError;
  }
  notifyLock = CThread::CreateMutex();
  if (notifyLock == NULL)
    goto ThreadError;
  notifySync = CThread::CreateCondVar();
  if (notifySync == NULL)
    goto ThreadError;

  // Reset thread flags
  pauseThreads = false;
  stopThreads = false;

  // Create PPC main board thread, if multi-threading GPU
  if (m_gpuMultiThreaded)
  {
    ppcBrdThread = CThread::CreateThread("MainBoard", StartMainBoardThread, this);
    if (ppcBrdThread == NULL)
      goto ThreadError;
  }

  startedThreads = true;
  return true;

ThreadError:
  ErrorLog("Unable to create threads and/or synchronization objects: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  DeleteThreadObjects();
  m_multiThreaded = false;
  return false;
}

bool CPro1000::PauseThreads(void)
{
  if (!startedThreads)
    return true;

  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let threads know that they should pause and wait for all of them to do so
  pauseThreads = true;
  while (ppcBrdThreadRunning)
  {
    if (!notifySync->Wait(notifyLock))
      goto ThreadError;
  }

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;
  return true;

ThreadError:
  ErrorLog("Threading error in CPro1000::PauseThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

bool CPro1000::ResumeThreads(void)
{
  if (!startedThreads)
    return true;

  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let all threads know that they can continue running
  pauseThreads = false;

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;
  return true;

ThreadError:
  ErrorLog("Threading error in CPro1000::ResumeThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

bool CPro1000::StopThreads(void)
{
  if (!startedThreads)
    return true;

  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let threads know that they should pause and wait for all of them to do so
  pauseThreads = true;
  while (ppcBrdThreadRunning)
  {
    if (!notifySync->Wait(notifyLock))
      goto ThreadError;
  }

  // Now let threads know that they should exit
  stopThreads = true;

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;

  // Resume thread and wait for it to exit
  if (ppcBrdThread != NULL)
  {
    if (ppcBrdThreadSync->Post())
      ppcBrdThread->Wait();
  }

  // Delete all thread and synchronization objects
  DeleteThreadObjects();
  startedThreads = false;
  return true;

ThreadError:
  ErrorLog("Threading error in CPro1000::StopThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

void CPro1000::DeleteThreadObjects(void)
{
  if (ppcBrdThread != NULL)
  {
    delete ppcBrdThread;
    ppcBrdThread = NULL;
  }
  if (ppcBrdThreadSync != NULL)
  {
    delete ppcBrdThreadSync;
    ppcBrdThreadSync = NULL;
  }
  if (notifyLock != NULL)
  {
    delete notifyLock;
    notifyLock = NULL;
  }
  if (notifySync != NULL)
  {
    delete notifySync;
    notifySync = NULL;
  }
}

void CPro1000::DumpTimings(void)
{
  printf("PPC:%3ums%c render:%3ums%c sync:%4uK%c%3ums%c frame:%3ums%c\n",
    timings.ppcTicks, (timings.ppcTicks > timings.renderTicks ? '!' : ','),
    timings.renderTicks, (timings.renderTicks > timings.ppcTicks ? '!' : ','),
    timings.syncSize / 1024, (timings.syncSize / 1024 > 128 ? '!' : ','),
    timings.syncTicks, (timings.syncTicks > 1 ? '!' : ','),
    timings.frameTicks, (timings.frameTicks > 16 ? '!' : ' '));
}

int CPro1000::StartMainBoardThread(void *data)
{
  // Call method on CPro1000 to run PPC main board thread
  CPro1000 *pro1000 = (CPro1000 *) data;
  return pro1000->RunMainBoardThread();
}

int CPro1000::RunMainBoardThread(void)
{
  for (;;)
  {
    bool wait = true;
    bool exit = false;
    while (wait && !exit)
    {
      // Wait on PPC main board thread semaphore
      if (!ppcBrdThreadSync->Wait())
        goto ThreadError;

      // Enter notify critical section
      if (!notifyLock->Lock())
        goto ThreadError;

      // Check threads are not being stopped or paused
      if (stopThreads)
        exit = true;
      else if (!pauseThreads)
      {
        wait = false;
        ppcBrdThreadRunning = true;
      }

      // Leave notify critical section
      if (!notifyLock->Unlock())
        goto ThreadError;
    }
    if (exit)
      return 0;

    // Process a single frame for PPC main board
    RunMainBoardFrame();

    // Enter notify critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Let other threads know processing has finished
    ppcBrdThreadRunning = false;
    ppcBrdThreadDone = true;
    if (!notifySync->SignalAll())
      goto ThreadError;

    // Leave notify critical section
    if (!notifyLock->Unlock())
      goto ThreadError;
  }

ThreadError:
  ErrorLog("Threading error in RunMainBoardThread: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return 1;
}

void CPro1000::Reset(void)
{
  // Clear memory
  memset(ram, 0, RAM_SIZE);

  // Reset all devices
  ppc_reset();
  IRQ.Reset();
  PCIBridge.Reset();
  PCIBus.Reset();
  SCSI.Reset();
  GPU.Reset();
  UART.Reset();

  gpusReady = false;

  memset(&timings, 0, sizeof(timings));

  DebugLog("Pro-1000 reset\n");
}


/******************************************************************************
 Initialization, Shutdown, and ROM Management
******************************************************************************/

const Game &CPro1000::GetGame() const
{
  return m_game;
}

Result CPro1000::LoadGame(const Game &game, const ROMSet &rom_set)
{
  m_game = Game();

  // Load CROM at its base address. Unused space reads back as all 1's, like
  // an erased EPROM.
  ROM rom = rom_set.get_rom("crom");
  if (rom.size == 0)
    return ErrorLog("No CROM defined for '%s'.", game.name.c_str());
  if (rom.size > CROM_SIZE - CROM_LOAD_OFFSET)
    return ErrorLog("CROM for '%s' is too large (%zu bytes). It must fit within 0x%08X-0xFFFFFFFF.", game.name.c_str(), rom.size, CROM_LOAD_ADDR);
  memset(crom, 0xFF, CROM_SIZE);
  rom.CopyTo(&crom[CROM_LOAD_OFFSET], rom.size);

  // Convert PowerPC ROM to little endian words
  Util::FlipEndian32(crom, CROM_SIZE);

  // Configure CPU and PCI bridge as Step 1.0
  PPC_CONFIG ppc_config;
  ppc_config.pvr = PPC_MODEL_603R;              // 66 MHz
  ppc_config.bus_frequency = BUS_FREQUENCY_66MHZ;
  ppc_config.bus_frequency_multiplier = 0x10;   // 1X multiplier
  PCIBridge.SetModel(0x105);                    // MPC105

  // Initialize CPU
  ppc_init(&ppc_config);
  ppc_attach_bus(this);
  PPCFetchRegions[0].start = 0;
  PPCFetchRegions[0].end = RAM_SIZE - 1;
  PPCFetchRegions[0].ptr = (UINT32 *) ram;
  PPCFetchRegions[1].start = CROM_BASE_ADDR;
  PPCFetchRegions[1].end = 0xFFFFFFFF;
  PPCFetchRegions[1].ptr = (UINT32 *) crom;
  PPCFetchRegions[2].start = 0;
  PPCFetchRegions[2].end = 0;
  PPCFetchRegions[2].ptr = NULL;
  ppc_set_fetch(PPCFetchRegions);

  // Initialize Real3D
  GPU.SetStepping(STEPPING);

  // Print game information
  if (!game.version.empty())
    std::cout << "    Title:          " << game.title << " (" << game.version << ")" << std::endl;
  else
    std::cout << "    Title:          " << game.title << std::endl;
  std::cout << "    ROM Set:        " << game.name << std::endl;
  std::cout << "    Developer:      " << game.manufacturer << std::endl;
  std::cout << "    Year:           " << game.year << std::endl;
  std::cout << "    Platform:       Real3D Pro-1000" << std::endl;
  std::cout << std::endl;

  m_game = game;
  return Result::OKAY;
}

void CPro1000::AttachRenderers(CRender2D *Render2DPtr, IRender3D *Render3DPtr, SuperAA *superAA)
{
  // No tile generator, so 2D renderer is unused
  GPU.AttachRenderer(Render3DPtr);
  m_superAA = superAA;
}

void CPro1000::AttachInputs(CInputs *InputsPtr)
{
  // No game inputs
}

void CPro1000::AttachOutputs(COutputs *OutputsPtr)
{
  // No outputs
}

Result CPro1000::Init(void)
{
  constexpr float memSizeMB = (float)MEM_POOL_SIZE / (float)0x100000;

  // Allocate all memory for ROMs and PPC RAM
  memoryPool = new(std::nothrow) UINT8[MEM_POOL_SIZE];
  if (NULL == memoryPool)
    return ErrorLog("Insufficient memory for Pro-1000 object (needs %1.1f MB).", memSizeMB);
  memset(memoryPool, 0, MEM_POOL_SIZE);

  // Set up pointers
  ram = &memoryPool[RAM_OFFSET];
  crom = &memoryPool[CROM_OFFSET];
  vrom = &memoryPool[VROM_OFFSET];

  // Initialize devices (PowerPC initialized after ROMs loaded)
  IRQ.Init();
  PCIBridge.Init();
  PCIBus.Init();
  SCSI.Init(this, &IRQ, 0x100); // SCSI is actually a non-maskable interrupt, so we give it a bit number outside of 8-bit range
  if (Result::OKAY != GPU.Init(vrom, this, &IRQ, 0x100)) // same for Real3D DMA interrupt
    return Result::FAIL;

  PCIBridge.AttachPCIBus(&PCIBus);
  PCIBus.AttachDevice(13, &GPU);
  PCIBus.AttachDevice(SCSI_PCI_DEVICE, &SCSI);

  DebugLog("Initialized Pro-1000 (allocated %1.1f MB)\n", memSizeMB);

  return Result::OKAY;
}

CPro1000::CPro1000(Util::Config::Node &config)
  : m_config(config),
    m_multiThreaded(config["MultiThreaded"].ValueAs<bool>()),
    m_gpuMultiThreaded(config["GPUMultiThreaded"].ValueAs<bool>()),
    memoryPool(NULL),
    ram(NULL),
    crom(NULL),
    vrom(NULL),
    gpusReady(false),
    startedThreads(false),
    pauseThreads(false),
    stopThreads(false),
    ppcBrdThread(NULL),
    ppcBrdThreadRunning(false),
    ppcBrdThreadDone(false),
    ppcBrdThreadSync(NULL),
    notifyLock(NULL),
    notifySync(NULL),
    GPU(config),
    m_superAA(nullptr)
{
  memset(PPCFetchRegions, 0, sizeof(PPCFetchRegions));
  memset(&timings, 0, sizeof(timings));
  DebugLog("Built Pro-1000\n");
}

// Dumps a memory region to a file for debugging purposes
static void Dump(const char *name, const char *file, uint8_t *buf, size_t size)
{
  FILE *fp = fopen(file, "wb");
  if (NULL != fp)
  {
    Util::FlipEndian32(buf, size);
    fwrite(buf, sizeof(UINT8), size, fp);
    fclose(fp);
    printf("Wrote %s to '%s'\n", name, file);
  }
  else
    ErrorLog("Unable to open '%s' for writing.", file);
}

CPro1000::~CPro1000(void)
{
  // Dump memory if requested
  if (m_config["DumpMemory"].ValueAsDefault<bool>(false) && memoryPool != NULL)
  {
    Dump("RAM", "ram.bin", ram, RAM_SIZE);
    Dump("CROM", "crom.bin", crom, CROM_SIZE);
  }

  // Stop all threads
  StopThreads();

  // Free memory
  if (memoryPool != NULL)
  {
    delete [] memoryPool;
    memoryPool = NULL;
  }

  ram = NULL;
  crom = NULL;
  vrom = NULL;

  DebugLog("Destroyed Pro-1000\n");
}
