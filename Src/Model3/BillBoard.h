/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2021 Bart Trzynadlowski, Nik Henson, Ian Curtis,
 **                     Harry Tuttle, and Spindizzi
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
 * BillBoard.h
 *
 * Header for the CBillBoard class (BillBoard emulation).
 *
 * The billboard (Sega 837-11854, VF3 and other versus cabinets) is its own board, not a drive board: it has
 * a Z80 (8 MHz, from a 32 MHz crystal), 8 KB of RAM, a Sega 315-5338A I/O chip and a counter that interrupts
 * the Z80 at 32 MHz / 65536 (488.28 Hz). There is no Z80 CTC and no NMI. The main board sends it commands
 * through the same output port it uses for a drive board.
 */

#ifndef INCLUDED_BILLBOARD_H
#define INCLUDED_BILLBOARD_H

#include "CPU/Bus.h"
#include "CPU/Z80/Z80.h"
#include "OSD/Outputs.h"
#include "BlockFile.h"
#include "Types.h"

#include <deque>
#include <mutex>

/*
 * CBillBoard
 */
class CBillBoard final : public IBus
{
public:
  /*
   * Init(romPtr):
   *
   * One-time initialization. Must be called first.
   *
   * Parameters:
   *    romPtr  Billboard program ROM (32 KB used), or NULL if the game has no billboard or there are no
   *            outputs to show it on. Without a ROM the board is not attached and does nothing.
   *
   * Returns:
   *    OKAY.
   */
  Result Init(const UINT8 *romPtr);

  /*
   * IsAttached(void):
   *
   * Returns:
   *    True if the billboard is emulated. This doesn't change after Init(), so the billboard thread can rely
   *    on it.
   */
  bool IsAttached(void) const;

  /*
   * AttachOutputs(outputs):
   *
   * Sets the outputs the digits and lamps are sent to.
   */
  void AttachOutputs(COutputs *outputs);

  /*
   * Reset(void):
   *
   * Resets the Z80 and the interrupt timer.
   */
  void Reset(void);

  /*
   * RunFrame(void):
   *
   * Runs the billboard for one frame, interrupting the Z80 every IRQ_PERIOD cycles.
   */
  void RunFrame(void);

  /*
   * Write(data):
   *
   * A command byte from the main board (the drive board output port). May be called from another thread.
   */
  void Write(UINT8 data);

  /*
   * SaveState(SaveState):
   * LoadState(SaveState):
   *
   * Saves and restores the billboard state.
   */
  void SaveState(CBlockFile *SaveState);
  void LoadState(CBlockFile *SaveState);

  /*
   * GetZ80(void):
   *
   * Returns:
   *    The billboard's Z80 (for the debugger).
   */
  CZ80 *GetZ80(void);

  /*
   * Read8(addr):
   * Write8(addr, data):
   * IORead8(portNum):
   * IOWrite8(portNum, data):
   *
   * The Z80's memory and I/O space (IBus).
   */
  UINT8 Read8(UINT32 addr) override;
  void Write8(UINT32 addr, UINT8 data) override;
  UINT8 IORead8(UINT32 portNum) override;
  void IOWrite8(UINT32 portNum, UINT8 data) override;

  CBillBoard(void);
  ~CBillBoard(void);

private:
  static constexpr unsigned ROM_SIZE = 0x8000;    // 0000-7FFF
  static constexpr unsigned RAM_SIZE = 0x2000;    // E000-FFFF

  // The interrupt comes from a counter dividing the 32 MHz crystal by 65536: one interrupt every
  // 65536 / 4 = 16384 cycles of the 8 MHz Z80 (488.28 Hz). Measured on hardware from the speed of the
  // "SEGA" scroll (250 ticks per step).
  static constexpr int IRQ_PERIOD = 16384;
  static constexpr int FRAME_CYCLES = (int)(8000000.0 / 57.524);   // Z80 cycles per video frame

  bool m_attached = false;      // the billboard is emulated
  const UINT8 *m_rom = nullptr;
  UINT8 m_ram[RAM_SIZE] = {};
  CZ80 m_z80;
  COutputs *m_outputs = nullptr;

  UINT8 m_dip = 0x0F;           // DIP switches (port A)
  UINT8 m_command = 0;          // command shown to the Z80 (port B)

  // Commands written by the main board, each shown to the Z80 for a whole frame (the program only takes a
  // command once it has read the same value at two interrupts in a row)
  std::mutex m_commandMutex;
  std::deque<UINT8> m_commands;
  UINT8 m_lastCommand = 0;

  int m_irqCycles = IRQ_PERIOD; // Z80 cycles until the next interrupt
  int m_cycleCarry = 0;         // cycles the Z80 ran past the end of the previous frame (0 or negative)
};

#endif  // INCLUDED_BILLBOARD_H
