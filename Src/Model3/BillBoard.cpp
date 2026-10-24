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
 * BillBoard.cpp
 *
 * Implementation of the CBillBoard class
 * emulation.
 *
 */

/*
** digits format
**
**       a
**    -------
**   |       |
** f |       | b
**   |   g   |
**    -------
**   |       |
** e |       | c
**   |       |
**    -------
**       d     (O) h
**
**   h g f e d c b a
** msb             lsb
**
** 0 switch on
** 1 switch off
**
**
** lamps
**   x x x x x x P2 P1
** 0 switch on
** 1 switch off
**
**/

/*
** Hardware (Sega 837-11854 billboard control board, EPR-18022)
**
** Z80 at 8 MHz (32 MHz crystal / 4), ROM 0000-7FFF, RAM E000-FFFF.
** Sega 315-5338A I/O chip at ports 20-2F:
**   20  port A  DIP switches (in)
**   21  port B  command from the main board (in)
**   22  port C  P1 right digit (out)   23  port D  P1 left digit (out)
**   24  port E  P2 right digit (out)   25  port F  P2 left digit (out)
**               (left/right from the ROM's scrolling text; output names BillDigit1-4 follow the ports)
**   26  port G  winner lamps (out)
**   28, 2E      I/O chip set-up, written once at start
** The Z80 runs in interrupt mode 1. A counter divides the crystal by 65536 and a flip-flop latches each
** period as an interrupt request until the Z80 takes it: 488.28 Hz, independent of the main board. The
** program counts these ticks for all its animations. There is no Z80 CTC and no NMI.
**/

#include "BillBoard.h"
#include "Supermodel.h"
#include <algorithm>
#include <cstring>

Result CBillBoard::Init(const UINT8 *romPtr)
{
  m_rom = romPtr;
  m_attached = romPtr != nullptr;
  m_z80.Init(this, nullptr);  // mode 1 interrupts; with no callback the Z80 clears the request when it takes it
  if (m_attached)
    DebugLog("Billboard attached\n");
  return Result::OKAY;
}

bool CBillBoard::IsAttached(void) const
{
  return m_attached;
}

void CBillBoard::AttachOutputs(COutputs *outputs)
{
  m_outputs = outputs;
}

void CBillBoard::Reset(void)
{
  std::memset(m_ram, 0, sizeof(m_ram));
  m_command = 0;
  {
    std::lock_guard<std::mutex> lock(m_commandMutex);
    m_commands.clear();
    m_lastCommand = 0;
  }
  m_irqCycles = IRQ_PERIOD;
  m_cycleCarry = 0;
  m_z80.Reset();
}

void CBillBoard::Write(UINT8 data)
{
  std::lock_guard<std::mutex> lock(m_commandMutex);
  if (data != m_lastCommand)
  {
    if (m_commands.size() >= 16)
      m_commands.pop_front();
    m_commands.push_back(data);
    m_lastCommand = data;
  }
}

void CBillBoard::RunFrame(void)
{
  if (!IsAttached())
    return;

  // The next command the main board wrote, shown to the Z80 for this whole frame
  {
    std::lock_guard<std::mutex> lock(m_commandMutex);
    if (!m_commands.empty())
    {
      m_command = m_commands.front();
      m_commands.pop_front();
    }
  }

  // Run the frame in slices up to each interrupt. The request stays latched until the Z80 takes it.
  int frameCycles = FRAME_CYCLES + m_cycleCarry;
  while (frameCycles > 0)
  {
    int cycles = m_z80.Run(std::min(frameCycles, m_irqCycles));
    frameCycles -= cycles;
    m_irqCycles -= cycles;
    if (m_irqCycles <= 0)
    {
      m_z80.SetINT(true);
      m_irqCycles += IRQ_PERIOD;
    }
  }
  m_cycleCarry = frameCycles;
}

void CBillBoard::SaveState(CBlockFile *SaveState)
{
  SaveState->NewBlock("BillBoard.3", __FILE__);

  bool enabled = IsAttached();
  SaveState->Write(enabled);
  if (enabled)
  {
    SaveState->Write(m_ram);
    SaveState->Write(m_dip);
    SaveState->Write(m_command);
    SaveState->Write(m_irqCycles);
    SaveState->Write(m_cycleCarry);
    m_z80.SaveState(SaveState, "BillBoard Z80");
  }
}

void CBillBoard::LoadState(CBlockFile *SaveState)
{
  // The billboard only drives the outputs, so a state saved with the outputs set differently doesn't need to
  // stop it: without a billboard in the state it restarts its program, and a state saved with one is skipped
  // when the billboard isn't emulated.
  bool wasEnabled = false;
  if (SaveState->FindBlock("BillBoard.3") == Result::OKAY)
    SaveState->Read(wasEnabled);
  if (!IsAttached())
    return;
  if (!wasEnabled)
  {
    Reset();
    return;
  }

  SaveState->Read(m_ram);
  SaveState->Read(m_dip);
  SaveState->Read(m_command);
  SaveState->Read(m_irqCycles);
  SaveState->Read(m_cycleCarry);

  m_irqCycles = std::max(1, std::min(IRQ_PERIOD, m_irqCycles));
  m_cycleCarry = std::max(-1000, std::min(0, m_cycleCarry));

  {
    std::lock_guard<std::mutex> lock(m_commandMutex);
    m_commands.clear();
    m_lastCommand = m_command;
  }

  m_z80.LoadState(SaveState, "BillBoard Z80");
}

CZ80 *CBillBoard::GetZ80(void)
{
  return &m_z80;
}

UINT8 CBillBoard::Read8(UINT32 addr)
{
  addr &= 0xFFFF;
  if (addr < ROM_SIZE)
    return m_rom ? m_rom[addr] : 0xFF;
  if (addr >= 0xE000)
    return m_ram[addr - 0xE000];
  return 0xFF;
}

void CBillBoard::Write8(UINT32 addr, UINT8 data)
{
  addr &= 0xFFFF;
  if (addr >= 0xE000)
    m_ram[addr - 0xE000] = data;
}

UINT8 CBillBoard::IORead8(UINT32 portNum)
{
  switch (portNum & 0xFF)
  {
  case 0x20:  // port A: DIP switches (0x80: test all segments)
    return m_dip;
  case 0x21:  // port B: command from the main board
    return m_command;
  case 0x26:  // port G: lamps, read back before changing the two lamp bits
    return 0xFF;
  default:
    DebugLog("Billboard: unhandled Z80 input on port %02X (at PC = %04X)\n", portNum & 0xFF, m_z80.GetPC());
    return 0xFF;
  }
}

void CBillBoard::IOWrite8(UINT32 portNum, UINT8 data)
{
  EOutputs output;
  switch (portNum & 0xFF)
  {
  case 0x22: output = OutputBill1; break;   // P1 right digit
  case 0x23: output = OutputBill2; break;   // P1 left digit
  case 0x24: output = OutputBill3; break;   // P2 right digit
  case 0x25: output = OutputBill4; break;   // P2 left digit
  case 0x26: output = OutputBill5; break;   // lamps (bit 0: P1, bit 1: P2, 0 = lit)
  case 0x28:  // I/O chip set-up (written once at start)
  case 0x2E:
    return;
  default:
    DebugLog("Billboard: unhandled Z80 output on port %02X (at PC = %04X)\n", portNum & 0xFF, m_z80.GetPC());
    return;
  }

  if (m_outputs != nullptr)
    m_outputs->SetValue(output, data);
}

CBillBoard::CBillBoard(void)
{
  DebugLog("Built billboard\n");
}

CBillBoard::~CBillBoard(void)
{
}
