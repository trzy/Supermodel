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
 * The Pro-1000 was attached to a Windows NT host via SCSI. Applications ran on
 * the host and sent data to the image generator, which had its own display.
 * The Model 3 graphics hardware is derived from it. This file documents what
 * has been learned about the hardware from its firmware. Everything here was
 * reverse engineered and should be treated as a working hypothesis unless
 * stated otherwise.
 *
 *
 * Methods
 * -------
 * The firmware (p1_p4.bin, 0x7014C bytes) is a flat big endian PowerPC image
 * loaded at 0xFFF00000. Findings come from:
 *
 *  - Disassembly with ppcd (bin64/ppcd.exe roms/P1_P4.bin -o FFF00000).
 *  - Logging unmapped accesses (DebugLog, -log-level=all) and UART output.
 *  - SCCS ID strings: each C module embeds "@(#)<file>.c <ver> <date>" in its
 *    data section. Data adjacent to an ID string belongs to that module, which
 *    attributes tables (e.g., register address tables) to source files.
 *  - Address cross-referencing: code forms addresses with "lis rX,hi" followed
 *    by "addi/ori rY,rX,lo". Collecting all such pairs reveals which code
 *    references which string or table. Error messages often contain function
 *    names (e.g., "jtag_wrt with zero bits"), which locates those functions.
 *  - Datasheets for identified parts (National SCANPSC100F, Intel 8259A,
 *    NCR 53C8xx, 16550 UART).
 *
 * Most firmware code runs from RAM. To convert, RAM = ROM - 0xFFF10000 +
 * 0x3000. Addresses below are RAM addresses unless prefixed with "ROM".
 *
 *
 * ROM Layout
 * ----------
 * ROM FFF00000-FFF0FFFF  Boot stub (reset vector at FFF00100)
 * ROM FFF10000-FFF7FFFF  Main firmware, copied to RAM 00003000
 * ROM FFF80000-FFF8FFFF  Gamma memory contents (expected; not in our dump)
 * ROM FFF90000-FFF93FFF  Soft edge blend memory contents (expected; not in dump)
 * ROM FFF94000-FFF9400F  Power-up mode words (expected; not in dump)
 *
 * The ROM is flash and the firmware can reprogram these areas (monitor
 * commands GMS, BMS, MWSV, DS). Our dump ends at ROM FFF7014C, so the rest of
 * the CROM space is filled with 0xFF (erased flash). The contents of p2.bin
 * are unknown and it is not loaded.
 *
 *
 * Boot Sequence
 * -------------
 * Boot stub (ROM):
 *  1. Reads RIF mode word (0x70000000), checks bit 13, writes 0x2000 to
 *     0x70000000 and 0x40000000.
 *  2. Initializes the UART and prints "go" four times.
 *  3. Copies ROM FFF10000-FFF7FFFC to RAM 0x3000.
 *  4. Copies 64KB from ROM FFF80000 to byte-wide memory at 0x60000000 (gamma).
 *  5. Copies 16KB from ROM FFF90000 to halfword-wide memory at 0x58000000
 *     (soft edge blend).
 *  6. Copies mode words from ROM FFF94000-FFF9400C to 0x62000000, 0x74000002,
 *     0x7400000A, 0x74000012, and ORs ROM FFF9400C | 0x100 into 0x70000000.
 *  7. Jumps to RAM 0x3000 (_start).
 *
 * _start (RAM 0x3000, startup.c):
 *  1. Prints RIF mode word (register), RIF mode word from ROM (FFF9400C),
 *     merges them, and prints PPC version.
 *  2. Initializes the 8259 (0x4AA8) and registers interrupt handlers (IR0,
 *     IR7, IR3).
 *  3. Configures PCI device 11 (SCSI, 0x4E0C).
 *  4. Resets the SCANPSC100F: writes 2 to MODE2, waits for the reset bit to
 *     clear, then writes 0x0C (status update). Registers IR4 (jtag_int).
 *  5. Initializes the SCSI controller ("ncrinit"), SCSI ID 0.
 *  6. Measures field time: clears the field counter at 0x5D960 and waits up to
 *     0x7F2C89 time base ticks for the IR0 handler to increment it. Without
 *     field syncs, it prints "NO FIELD SYNCS DETECTED", skips the steps
 *     below that touch the renderer, and goes on to read DIP switches
 *     (probably RIF register bits 23:16, active low), print the revision,
 *     and enter the main loop (". . . entering main loop . . .").
 *  7. With field syncs: prints "field time = N milliseconds", reads ASIC ID
 *     codes over JTAG and prints any that do not match (see JTAG section),
 *     falls back to the low res single pixel board configuration if
 *     detection fails, and calls write_scroll().
 *  8. Calls 0x5660 (sets up mode words via 0x130A4 and configures PCI
 *     device 12 via 0x4E78), then prints "_start: before get_hw_config".
 *     0x5660 waits for a GP sync (IR7) via 0x577C.
 *
 *
 * PowerPC Address Map
 * -------------------
 * 00000000-007FFFFF  RAM (8MB assumed)
 * 40000000           Unknown control register. Written with 0x2000 at boot.
 * 52000000           8259 interrupt acknowledge: each write is one INTA pulse
 *                    (dispatcher writes twice, then reads ISR). [Emulated]
 * 54000000-5400000F  Intel 8259A interrupt controller (int_8259.c). [Emulated]
 * 58000000-5801FFFF  Soft edge blend memory? 8K halfwords at stride 8.
 * 60000000-6007FFFF  Gamma memory? 64K bytes at stride 8 (Pluto gamma).
 * 62000000           Unknown. Loaded with a mode word from ROM at boot.
 * 64000000-6400003F  16550-style UART (uart.c). [Emulated]
 * 68000000-6800003F  SCANPSC100F JTAG scan controller (jtag_*.c). [Emulated]
 * 70000000           RIF mode word register. Bit 13 checked at boot, bit 14
 *                    toggled by boot stub, bits 23:16 probably DIP switches.
 * 72000000           Reset? Boot stub writes it in a loop on 'r' over serial.
 * 74000000-74000017  Video sync generator? Halfword registers at +2, +A, +12
 *                    loaded with mode words at boot.
 * 78000000           Free-running timer? Read by IR0 handler to time fields.
 * 80000000-          PCI device 12 memory BAR (probably Real3D). The Model 3
 *                    Real3D ranges (84, 88, 8C, 8E, 94, 98, 9C) are mapped
 *                    here provisionally.
 * C0000000           SCSI expansion ROM (PCI device 11 ROM BAR). [Not mapped]
 * D0000000-D00000FF  SCSI controller registers (PCI device 11 BAR1). [Emulated]
 * F0800CF8-F0800CFF  MPC105 CONFIG_ADDR (same as Model 3 Step 1.0)
 * F0C00CF8-F0C00CFF  MPC105 CONFIG_DATA
 * F8FFF000-F8FFF0FF  MPC105 registers
 * FF800000-FFFFFFFF  CROM (firmware at FFF00000)
 *
 * Byte-wide peripherals (8259, UART, SCANPSC100F, gamma memory) are connected
 * to D7:0 and decode address bits 3 and up: register n is the least
 * significant byte of the big endian word at base+n*8. Firmware uses both
 * byte (base+n*8+3) and word (base+n*8) accesses.
 *
 * DEADBEEF/CAFEBABE/DEADFACE test patterns written by the boot stub to 58, 60,
 * 62, 68, and 74 confirm these are distinct register/memory regions.
 *
 *
 * PCI
 * ---
 * The firmware never reads configuration space (no device probing). It
 * programs fixed device numbers through the MPC105 (CONFIG_ADDR values decoded
 * as bus 0, device 11 or 12):
 *
 *  Device 11 (SCSI, 0x4E0C):
 *    04 Command  = 0x0006 (memory space, bus master)
 *    0C          = 0x8000 (latency timer 0x80)
 *    14 BAR1     = 0xD0000000 (memory-mapped registers)
 *    30 ROM BAR  = 0xC0000001 (expansion ROM at 0xC0000000, enabled)
 *  Device 12 (probably Real3D, 0x4E78, not yet reached):
 *    04 Command  = 0x0142, status cleared
 *    10 BAR0     = 0x80000000
 *
 * The SCSI controller is an NCR 53C825 (scsi825.c, sslib25.c; "NCR825 is
 * revision 0 silicon"). It is emulated with the 53C810, which has a
 * compatible register set. Its flash ROM (expansion ROM) can be reprogrammed
 * ("test_flash_prog: erase SCSI ROM").
 *
 *
 * Interrupts
 * ----------
 * An 8259A (int_8259.c) drives the PowerPC external interrupt. Initialization
 * (0x4AA8): ICW1=0x13 (edge, single, ICW4), ICW2=0x00, ICW4=0x01 (8086 mode),
 * mask all, OCW2=0xC6 (IR6 lowest priority), OCW3=0x0B (read ISR).
 *
 * Dispatcher (0x4B24): writes 0x52000000 twice (INTA), reads ISR, scans from
 * IR7 down, calls handler[line], then issues a specific EOI (0x60|line).
 * Handlers are registered with 0x4BDC(handler, line), which also calls the
 * handler once and unmasks the line. 0x4C7C/0x4CF0 mask/unmask a line.
 *
 *  IR0  Field sync (0x4EBC): counts fields at 0x5D960, reads 0x78000000.
 *       [Emulated: one line long pulse per frame at line 383, matching when
 *       Model 3 asserts its VBlank interrupt]
 *  IR3  SCSI (0x44868, ncrintr). [Emulated]
 *  IR4  SCANPSC100F INT (0x4F98, jtag_int). Dispatches to sub-handlers
 *       registered with 0x50B4(handler, MODE1 bit). [Emulated]
 *  IR7  Geometry processor (GP) sync (0x6918, gp_sync_int.c): counts frames at
 *       0x5DB80, sets flag 0x5DB84, and keeps interval statistics (monitor
 *       "Delta Frame (GP) Sync Count"). Equivalent to the Model 3 "GP done"
 *       interrupt. 0x577C waits for it to change before returning.
 *       [Emulated: one line long pulse per frame at end of VBlank, matching
 *       when Model 3 asserts its GP done interrupt]
 *
 * The 8259 raises the CPU interrupt line only between CPU time slices (see
 * C8259::Tick()), modeling the time devices take to complete operations.
 * jtag_wrt, for example, sets a busy flag after starting a shift and would
 * miss an interrupt taken immediately.
 *
 *
 * JTAG
 * ----
 * The Real3D ASICs are configured through JTAG (mode words, ID codes) using a
 * National SCANPSC100F controller at 0x68000000, unlike Model 3 which
 * bit-bangs a TAP through system registers. jtag_wrt (0x12444, jtag_io.c)
 * navigates the TAP with TMS0 patterns (0x30 = to Shift-IR, 0x20 = to
 * Shift-DR, 0x01 = exit to Run-Test/Idle), shifts one byte per CNT32
 * interrupt using Auto TMS High on the final byte, and completes in its
 * interrupt handler (0x12630). jtag_read is at approximately 0x12790.
 *
 * ASIC name table (pointer table at 0x611AC): NOT_PRESENT, JUPITER, MERCURY,
 * VENUS, EARTH_0, MARS_00, MARS_01, EARTH_1, MARS_10, MARS_11, EARTH_2,
 * MARS_20, MARS_21, EARTH_3, MARS_30, MARS_31, PLUTO, NEPTUNE_0-3. This is a
 * naming table, not the scan chain order. Pluto is new relative to Model 3
 * and appears to handle video output/gamma. The chain depends on the hardware
 * configuration: single pixel board, dual pixel boards, or high resolution
 * (get_hw_config.c, auto_config.c). The 3D RAMs are not named.
 *
 * Expected ID codes, as printed by the firmware's ID check ("FAILED ID'S")
 * in chain order for a single pixel board configuration:
 *
 *   JUPITER   0x016C7057 or 0x0D7F1057
 *   MERCURY   0x016C3057
 *   VENUS     0x016C4057
 *   EARTH_n   0x016C5057 or 0x0D7F0057    (n = 0-3)
 *   MARS_n0   0x016C6057
 *   MARS_n1   0x016C6057
 *
 * These follow the Model 3 ID codes (e.g., Mercury 0x216C3057 on Step 1.0)
 * with version field 0. Fifteen devices are checked: Jupiter, Mercury, Venus,
 * then four groups of Earth followed by two Mars.
 *
 * The emulated chain (CPro1000::BuildScanChain()) contains these 15 ASICs
 * using the Model 3 CASIC device model.
 *
 *
 * Serial Console
 * --------------
 * The UART carries boot messages and a command monitor (rs232_ui.c, menu.c)
 * with a "command> " prompt and about 80 commands, including memory display
 * and modification (RD, RC, RP, WC, WP), mode word access (MWR, MWW), JTAG
 * (WJIR, WJDR, RJDR), ID code and hardware configuration dumps (ID, HW,
 * RIF_MODE), and rendering feature toggles. The boot stub also polls for
 * input ('r' resets, ESC prints the scratch register). Input is not yet
 * emulated.
 *
 *
 * Other Firmware Landmarks
 * ------------------------
 * 0x47D6C  printf
 * 0x8750   Disable external interrupts (clears MSR[EE]), returns old MSR
 * 0x876C   Restore MSR[EE]
 * 0x9400   Read time base (lower)
 *
 *
 * Open Questions
 * --------------
 *  - Source and rate of field syncs (IR0). Probably the video timing at
 *    0x74000000. Firmware measures 34 ms per field although frames are
 *    17.4 ms (57.52 Hz), so either the expected rate is 30 Hz (there is a 30
 *    Hz mode) or the time base runs at a different rate than assumed.
 *  - Device 12 (Real3D) register layout and the culling/polygon memory map.
 *  - Scan chain order and ID codes for each hardware configuration.
 *  - Where scan chains should be built. Currently CPro1000 builds its chain
 *    while Model 3's CJTAG builds its own. Once the Pro-1000 chain is
 *    understood, consider having CReal3D build the chain (its ASICs are the
 *    Real3D chipset) from a board configuration supplied by the system,
 *    preserving the Model 3 JTAG2 save state layout.
 *  - 0x40000000, 0x62000000, 0x72000000, 0x78000000.
 *
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

// Field sync is signaled at the start of the last active line, which is when
// Model 3 asserts its VBlank interrupt (IRQ 0x02)
constexpr static unsigned FIELD_SYNC_LINE = 383;

// UART registers are 8 bytes apart and occupy the least significant byte of
// each big endian 32-bit word
constexpr static uint32_t UART_BASE_ADDR    = 0x64000000;
constexpr static uint32_t UART_END_ADDR     = UART_BASE_ADDR + 8 * 8;

static inline unsigned UARTRegister(UINT32 addr)
{
  return (addr >> 3) & 7;
}

/*
 * Byte-wide peripherals are connected to D7:0 and decode address bits 3 and
 * up, so register n of a device is accessed as the least significant byte of
 * the big endian 32-bit word at base+n*8. Firmware uses both byte accesses
 * (at base+n*8+3) and word accesses (at base+n*8).
 */
static inline bool DecodeByteRegister(UINT32 addr, UINT32 base, unsigned numRegs, unsigned lane, unsigned *reg)
{
  if (addr < base || addr >= base + numRegs * 8 || (addr & 7) != lane)
    return false;
  *reg = (addr - base) >> 3;
  return true;
}

constexpr static uint32_t PIC_BASE_ADDR     = 0x54000000; // 8259 (2 registers: A0)
constexpr static uint32_t INTA_ADDR         = 0x52000000; // any write performs one 8259 INTA pulse
constexpr static uint32_t JTAG_BASE_ADDR    = 0x68000000; // SCANPSC100F (8 registers: A2:0)

// 8259 input lines (IR mask bits)
constexpr static uint16_t PIC_LINE_FIELD_SYNC = 1 << 0;  // handler counts fields
constexpr static uint16_t PIC_LINE_SCSI     = 1 << 3;     // handler is ncrintr
constexpr static uint16_t PIC_LINE_JTAG     = 1 << 4;     // handler is jtag_int
constexpr static uint16_t PIC_LINE_GP_SYNC  = 1 << 7;     // handler counts geometry processor frames
constexpr static uint16_t PIC_LINE_NONE     = 0;          // not connected

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

  // Byte-wide peripherals
  unsigned reg;
  if (DecodeByteRegister(addr, PIC_BASE_ADDR, 2, 3, &reg))
    return PIC.Read(reg);
  if (DecodeByteRegister(addr, JTAG_BASE_ADDR, 8, 3, &reg))
    return JTAG.Read(reg);

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

  // Byte-wide peripherals
  unsigned reg;
  if (DecodeByteRegister(addr, PIC_BASE_ADDR, 2, 0, &reg))
    return 0xFFFFFF00 | PIC.Read(reg);
  if (DecodeByteRegister(addr, JTAG_BASE_ADDR, 8, 0, &reg))
    return 0xFFFFFF00 | JTAG.Read(reg);

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

  // Byte-wide peripherals
  unsigned reg;
  if (DecodeByteRegister(addr, PIC_BASE_ADDR, 2, 3, &reg))
  {
    PIC.Write(reg, data);
    return;
  }
  if (DecodeByteRegister(addr, JTAG_BASE_ADDR, 8, 3, &reg))
  {
    JTAG.Write(reg, data);
    return;
  }

  // 8259 interrupt acknowledge
  if ((addr >> 24) == (INTA_ADDR >> 24))
  {
    PIC.Acknowledge();
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

  // Byte-wide peripherals
  unsigned reg;
  if (DecodeByteRegister(addr, PIC_BASE_ADDR, 2, 0, &reg))
  {
    PIC.Write(reg, data & 0xFF);
    return;
  }
  if (DecodeByteRegister(addr, JTAG_BASE_ADDR, 8, 0, &reg))
  {
    JTAG.Write(reg, data & 0xFF);
    return;
  }

  // 8259 interrupt acknowledge
  if ((addr >> 24) == (INTA_ADDR >> 24))
  {
    PIC.Acknowledge();
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
    for (unsigned i = 0; i < vBlankCycles / lineCycles; i++)
    {
      PIC.Tick();
      ppc_execute(lineCycles);
    }

    // Geometry processor sync at end of VBlank, which is when Model 3 asserts
    // its GP done interrupt (IRQ 0x08). Deasserted after first active line.
    PIC.Assert(PIC_LINE_GP_SYNC);

    GPU.EndVBlank();
  }

  // Run the PowerPC for the active display part of the frame
  for (unsigned i = 0; i < 384; i++)
  {
    if (i == PING_PONG_FLIP_LINE)
      GPU.FlipPingPongBit();
    if (i == FIELD_SYNC_LINE)
      PIC.Assert(PIC_LINE_FIELD_SYNC);
    PIC.Tick();
    ppc_execute(lineCycles);
    if (i == 0)
      PIC.Deassert(PIC_LINE_GP_SYNC);     // one line long pulse
    if (i == FIELD_SYNC_LINE)
      PIC.Deassert(PIC_LINE_FIELD_SYNC);  // one line long pulse
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
  PIC.Reset();
  JTAG.Reset();
  m_scanChain.Reset();
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

/*
 * Single pixel board configuration, in the order and with the ID codes the
 * firmware's ID check expects. Only the first ASIC of each type forwards its
 * mode word to the Real3D object (as on Model 3). Whether the 3D-RAMs, Pluto,
 * or Neptune are on this chain is not yet known.
 */
void CPro1000::BuildScanChain(void)
{
  struct ASICInfo
  {
    CASIC::Name name;
    uint32_t    idCode;
  };

  static const ASICInfo s_chain[] =
  {
    { CASIC::Name::Jupiter, 0x016C7057 },
    { CASIC::Name::Mercury, 0x016C3057 },
    { CASIC::Name::Venus,   0x016C4057 },
    { CASIC::Name::Earth,   0x016C5057 },   // EARTH_0
    { CASIC::Name::Mars,    0x016C6057 },   // MARS_00
    { CASIC::Name::Dummy,   0x016C6057 },   // MARS_01
    { CASIC::Name::Dummy,   0x016C5057 },   // EARTH_1
    { CASIC::Name::Dummy,   0x016C6057 },   // MARS_10
    { CASIC::Name::Dummy,   0x016C6057 },   // MARS_11
    { CASIC::Name::Dummy,   0x016C5057 },   // EARTH_2
    { CASIC::Name::Dummy,   0x016C6057 },   // MARS_20
    { CASIC::Name::Dummy,   0x016C6057 },   // MARS_21
    { CASIC::Name::Dummy,   0x016C5057 },   // EARTH_3
    { CASIC::Name::Dummy,   0x016C6057 },   // MARS_30
    { CASIC::Name::Dummy,   0x016C6057 },   // MARS_31
  };

  std::vector<CJTAGDevice *> devices;
  m_asics.clear();
  for (auto &info: s_chain)
  {
    m_asics.emplace_back(std::make_unique<CASIC>(GPU, info.name));
    m_asics.back()->SetIDCode(info.idCode);
    devices.push_back(m_asics.back().get());
  }
  m_scanChain = CJTAGScanChain(devices);
  JTAG.AttachScanChain(&m_scanChain);
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
  PCIBridge.Init();
  PCIBus.Init();
  SCSI.Init(this, &PIC, PIC_LINE_SCSI);
  if (Result::OKAY != GPU.Init(vrom, this, &PIC, PIC_LINE_NONE)) // Real3D DMA interrupt line unknown
    return Result::FAIL;
  BuildScanChain();
  JTAG.AttachInterruptOutput([this](bool level)
  {
    if (level)
      PIC.Assert(PIC_LINE_JTAG);
    else
      PIC.Deassert(PIC_LINE_JTAG);
  });

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
