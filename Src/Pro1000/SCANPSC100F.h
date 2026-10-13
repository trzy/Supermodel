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
 * SCANPSC100F.h
 *
 * Header file defining the CSCANPSC100F class: National Semiconductor
 * SCANPSC100F Embedded Boundary Scan Controller, a parallel bus to JTAG (IEEE
 * 1149.1) scan chain bridge.
 *
 * Naming conventions follow the datasheet, which names serial signals from
 * the controller's point of view: TDO is the controller's *output* (driving
 * the TDI of the first device in the scan chain) and TDI is the controller's
 * *input* (driven by the TDO of the last device in the chain).
 */

#ifndef INCLUDED_SCANPSC100F_H
#define INCLUDED_SCANPSC100F_H

#include "Model3/JTAG.h"
#include <cstdint>
#include <functional>

/*
 * CSCANPSC100F:
 *
 * Registers are selected with a 3-bit address (A2:0). Shifting through the
 * scan chain is performed instantaneously whenever the datasheet's TCK
 * control logic would allow TCK to run. Only the TMS0 chain is connected.
 * Pseudo-random pattern generation (PRPG), serial signature compaction (SSC),
 * loop-around/loop-back, and freeze modes are not emulated.
 */
class CSCANPSC100F
{
public:
  uint8_t Read(unsigned reg);
  void Write(unsigned reg, uint8_t data);

  /*
   * Reset(void):
   *
   * Equivalent to asserting the RST pin.
   */
  void Reset(void);

  /*
   * AttachScanChain(chain):
   *
   * Attaches the scan chain connected to TMS0. With no chain attached, TDI
   * reads 1 (the pin has a pull-up).
   */
  void AttachScanChain(IJTAGScanChain *chain);

  /*
   * AttachInterruptOutput(callback):
   *
   * Sets the function called when the level of the INT pin changes.
   */
  void AttachInterruptOutput(std::function<void(bool)> callback);

  CSCANPSC100F(void);

private:
  // 2 x 8-bit FIFO used by each shifter/buffer
  struct ShifterBuffer
  {
    uint8_t   data[2] = { 0, 0 };
    unsigned  count = 0;        // number of bytes in FIFO

    bool Full() const { return count == 2; }
    bool Empty() const { return count == 0; }
    void Push(uint8_t byte);
    uint8_t Pop();
    void Clear() { count = 0; }
  };

  void    SynchronousReset(void);
  void    WriteCounter(uint8_t data);
  void    RunTCK(void);
  bool    TCKEnabled(void) const;
  void    EndOfByte(bool terminalCount);
  uint8_t LiveStatus(void) const;
  void    UpdateInterrupt(void);

  uint8_t   m_mode0;
  uint8_t   m_mode1;
  bool      m_continuousUpdate;
  uint8_t   m_statusLatch;          // MODE2 bits 7-3

  uint32_t  m_counter;              // CNT32
  bool      m_counterLoaded;
  uint32_t  m_counterLoadValue;
  unsigned  m_counterWriteIdx;      // bytes written so far (0-3) when loading CNT32
  unsigned  m_cnt3;                 // 3-bit TCK counter used for byte alignment

  ShifterBuffer m_tdo;
  ShifterBuffer m_tms0;
  ShifterBuffer m_tms1;
  ShifterBuffer m_tdi;
  uint8_t   m_tdiShift;             // TDI byte being assembled (new bits enter at MSB)
  uint8_t   m_lastTDIRead;

  bool      m_tdoPin;               // outputs retain their last state when not shifting
  bool      m_tms0Pin;

  bool      m_intPin;
  std::function<void(bool)> m_intCallback;
  IJTAGScanChain *m_chain;
};

#endif  // INCLUDED_SCANPSC100F_H
