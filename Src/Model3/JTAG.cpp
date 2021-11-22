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
 * JTAG.cpp
 * 
 * Model 3's JTAG test access port (TAP). This is accessed through the system
 * register space and is connected to the Real3D chipset and possibly other
 * devices. Hence, it is emulated as an independent module.
 *
 * It is unclear which exact JTAG standard the device conforms to (and it 
 * probably doesn't matter), so we assume IEEE 1149.1-1990 here.
 */

#include "JTAG.h"

#include "Supermodel.h"
#include "Real3D.h"

#include <iostream>

// Finite state machine. Each state has two possible next states.
const CJTAG::State CJTAG::s_fsm[][2] =
{
  // tms = 0      tms = 1
  { RunTestIdle,  TestLogicReset }, // 0  Test-Logic/Reset
  { RunTestIdle,  SelectDRScan },   // 1  Run-Test/Idle
  { CaptureDR,    SelectIRScan },   // 2  Select-DR-Scan
  { ShiftDR,      Exit1DR },        // 3  Capture-DR
  { ShiftDR,      Exit1DR },        // 4  Shift-DR
  { PauseDR,      UpdateDR },       // 5  Exit1-DR
  { PauseDR,      Exit2DR },        // 6  Pause-DR
  { ShiftDR,      UpdateDR },       // 7  Exit2-DR
  { RunTestIdle,  SelectDRScan },   // 8  Update-DR
  { CaptureIR,    TestLogicReset }, // 9  Select-IR-Scan
  { ShiftIR,      Exit1IR },        // 10 Capture-IR
  { ShiftIR,      Exit1IR },        // 11 Shift-IR
  { PauseIR,      UpdateIR },       // 12 Exit1-IR
  { PauseIR,      Exit2IR },        // 13 Pause-IR
  { ShiftIR,      UpdateIR },       // 14 Exit2-IR
  { RunTestIdle,  SelectDRScan }    // 15 Update-IR
};

const char *CJTAG::s_state[] =
{
  "Test-Logic/Reset",
  "Run-Test/Idle",
  "Select-DR-Scan",
  "Capture-DR",
  "Shift-DR",
  "Exit1-DR",
  "Pause-DR",
  "Exit2-DR",
  "Update-DR",
  "Select-IR-Scan",
  "Capture-IR",
  "Shift-IR",
  "Exit1-IR",
  "Pause-IR",
  "Exit2-IR",
  "Update-IR"
};

static void SaveBitRegister(CBlockFile *SaveState, const Util::BitRegister &reg)
{
  uint16_t size = (uint16_t)reg.Size() + 1; // include null terminator
  SaveState->Write(&size, sizeof(size));
  SaveState->Write(reg.ToBinaryString());
}

static void LoadBitRegister(CBlockFile *SaveState, Util::BitRegister *reg)
{
  uint16_t size;
  SaveState->Read(&size, sizeof(size));
  char *str = new char[size];
  SaveState->Read(str, size);
  reg->Set(str);
  delete [] str;
}

void CJTAG::SaveState(CBlockFile *SaveState)
{
  SaveState->NewBlock("JTAG", __FILE__);
  SaveBitRegister(SaveState, m_instructionShiftReg);
  SaveBitRegister(SaveState, m_dataShiftReg);
  SaveState->Write(&m_instructionReg, sizeof(m_instructionReg));
  SaveState->Write(&m_state, sizeof(m_state));
  SaveState->Write(&m_lastTck, sizeof(m_lastTck));
  SaveState->Write(&m_tdo, sizeof(m_tdo));
}

void CJTAG::LoadState(CBlockFile *SaveState)
{
  if (OKAY != SaveState->FindBlock("JTAG"))
  {
    ErrorLog("Unable to load JTAG state. Save state file is corrupt.");
    return;
  }
  LoadBitRegister(SaveState, &m_instructionShiftReg);
  LoadBitRegister(SaveState, &m_dataShiftReg);
  SaveState->Read(&m_instructionReg, sizeof(m_instructionReg));
  SaveState->Read(&m_state, sizeof(m_state));
  SaveState->Read(&m_lastTck, sizeof(m_lastTck));
  SaveState->Read(&m_tdo, sizeof(m_tdo));
}

uint8_t CJTAG::Read()
{
  return m_tdo;
}

void CJTAG::LoadASICIDCodes()
{
  /*
   * ID code retrieval has not been carefully studied but based on observation,
   * it appears that the ID codes are loaded on logic reset (Step 2.x games and
   * some 1.x games rely on this) as well as instruction 0x06318fc63fff. Some 
   * games rely on both (e.g., von2).
   */
  m_dataShiftReg.SetZeros();
  m_dataShiftReg.Insert(2 + 0*32 + 0, Util::Hex(m_real3D.GetASICIDCode(CReal3D::ASIC::Jupiter)));
  m_dataShiftReg.Insert(2 + 1*32 + 0, Util::Hex(m_real3D.GetASICIDCode(CReal3D::ASIC::Mercury)));
  m_dataShiftReg.Insert(2 + 2*32 + 0, Util::Hex(m_real3D.GetASICIDCode(CReal3D::ASIC::Venus)));
  m_dataShiftReg.Insert(2 + 3*32 + 0, Util::Hex(m_real3D.GetASICIDCode(CReal3D::ASIC::Earth)));
  m_dataShiftReg.Insert(2 + 4*32 + 1, Util::Hex(m_real3D.GetASICIDCode(CReal3D::ASIC::Mars)));
  m_dataShiftReg.Insert(2 + 5*32 + 1, Util::Hex(m_real3D.GetASICIDCode(CReal3D::ASIC::Mars))); 
}

void CJTAG::Write(uint8_t tck, uint8_t tms, uint8_t tdi, uint8_t trst)
{
  tck = !!tck;
  tms = !!tms;
  tdi = !!tdi;
  trst = !!trst;
  
  //TODO: is trst used anywhere? If so, need to emulate.
  //if (!trst)
  //  printf("TRST=0\n");
  //printf("%d trst=%d tms=%d tdi=%d\n", tck, trst, tms, tdi);
  
  // Transitions occur on rising edge
  uint8_t lastTck = m_lastTck;
  m_lastTck = tck;
  if (!tck || lastTck != 0)
    return;

  // Current state logic
  switch (m_state)
  {
  default:
    break;
  case State::TestLogicReset:
    LoadASICIDCodes();
    break;
  case State::CaptureDR:
    if (m_instructionReg == Instruction::ReadASICIDCodes)
      LoadASICIDCodes();
    break;
  case State::ShiftDR:
    m_tdo = m_dataShiftReg.ShiftOutRight(tdi);
    break;
  case State::UpdateDR:
    if (m_instructionReg == Instruction::SetReal3DRenderConfig0 || m_instructionReg == Instruction::SetReal3DRenderConfig1)
    {
      uint64_t data = m_dataShiftReg.GetBits(0, 42);
      m_real3D.WriteJTAGRegister(m_instructionReg, data);
    }
    //std::cout << "DR = " << m_dataShiftReg << std::endl;
    break;
  case State::CaptureIR:
    // Load lower 2 bits with 01 as per IEEE 1149.1-1990
    m_instructionShiftReg.Insert(44, "01");
    break;
  case State::ShiftIR:
    m_tdo = m_instructionShiftReg.ShiftOutRight(tdi);
    break;
  case State::UpdateIR:
    // Latch the instruction register (technically, this should occur on
    // falling edge of clock as per the spec)
    m_instructionReg = m_instructionShiftReg.GetBits();
    //std::cout << "IR = " << Util::Hex(m_instructionReg, 12) << std::endl;
    break;
  }
  
  // Go to next state
  m_state = s_fsm[m_state][tms];
  //printf("  -> %s\n", s_state[m_state]);
}

void CJTAG::Reset()
{
  m_state = State::TestLogicReset;
  DebugLog("JTAG reset\n");
}

CJTAG::CJTAG(CReal3D &real3D)
  : m_real3D(real3D),
    m_instructionShiftReg(46, 0),
    m_dataShiftReg(197, 0)
{
  DebugLog("Built JTAG logic\n");
}