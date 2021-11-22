/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2017 Bart Trzynadlowski, Nik Henson, Ian Curtis
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
 * JTAG.h
 * 
 * Header file defining the CJTAG class: the Model 3's JTAG device.
 */

#ifndef INCLUDED_JTAG_H
#define INCLUDED_JTAG_H

#include "BlockFile.h"
#include "Util/BitRegister.h"

class CReal3D;

class CJTAG
{
public:
  enum Instruction: uint64_t
  {
    ReadASICIDCodes         = 0x06318fc63fff,
    SetReal3DRenderConfig0  = 0x3fffffd1ffff,
    SetReal3DRenderConfig1  = 0x3ffffffe8fff
  };

  void SaveState(CBlockFile *SaveState);
  void LoadState(CBlockFile *SaveState);
  uint8_t Read();
  void Write(uint8_t tck, uint8_t tms, uint8_t tdi, uint8_t trst);
  void Reset();
  CJTAG(CReal3D &real3D);

private:
  void LoadASICIDCodes();

  enum State: uint8_t
  {
    TestLogicReset, // 0
    RunTestIdle,    // 1
    SelectDRScan,   // 2
    CaptureDR,      // 3
    ShiftDR,        // 4
    Exit1DR,        // 5
    PauseDR,        // 6
    Exit2DR,        // 7
    UpdateDR,       // 8
    SelectIRScan,   // 9
    CaptureIR,      // 10
    ShiftIR,        // 11
    Exit1IR,        // 12
    PauseIR,        // 13
    Exit2IR,        // 14
    UpdateIR        // 15
  };
  
  static const State s_fsm[][2];
  static const char *s_state[];

  CReal3D &m_real3D;
  Util::BitRegister m_instructionShiftReg;
  Util::BitRegister m_dataShiftReg;
  uint64_t m_instructionReg = 0;
  State m_state = State::TestLogicReset;
  uint8_t m_lastTck = 0;
  uint8_t m_tdo = 0;
};

#endif  // INCLUDED_JTAG_H
