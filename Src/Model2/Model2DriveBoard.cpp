/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
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
 * Model2DriveBoard.cpp
 *
 * The Model 2 wheel drive board. See Model2DriveBoard.h.
 */

#include "Model2DriveBoard.h"

#include "Supermodel.h"
#include "Inputs/Inputs.h"

#include <algorithm>

CModel2DriveBoard::CModel2DriveBoard(const Util::Config::Node &config)
  : CDriveBoard(config)
{
  m_z80Clock = 4.0f;
  // normal operation, power-on calibration, the display shows the wheel's
  // position; the strength comes from the ForceFeedbackStrength setting
  m_dip1 = 0xF8;
  m_dip2 = 0xFF;
}

// The strength switches (2-0, inverted) add to the motor's power: 1-8 as
// Model 3's setting, 1 the weakest
unsigned CModel2DriveBoard::GetForceFeedbackStrength(void) const
{
  return ((~m_dip1) & 7) + 1;
}

void CModel2DriveBoard::SetForceFeedbackStrength(unsigned strength)
{
  strength = std::clamp(strength, 1u, 8u);
  m_dip1 = (m_dip1 & 0xF8) | ((~(strength - 1)) & 7);
}

void CModel2DriveBoard::SaveState(CBlockFile *SaveState)
{
  CDriveBoard::SaveState(SaveState);
  SaveState->NewBlock("Model2DriveBoard", __FILE__);
  SaveState->Write(m_motor);
  SaveState->Write(m_adcValue);
  SaveState->Write(m_adcBit);
  SaveState->Write(m_display, sizeof(m_display));
  SaveState->Write(m_timerCycles);
}

void CModel2DriveBoard::LoadState(CBlockFile *SaveState)
{
  CDriveBoard::LoadState(SaveState);
  if (SaveState->FindBlock("Model2DriveBoard") != Result::OKAY)
  {
    Disable();
    return;
  }
  SaveState->Read(m_motor);
  SaveState->Read(m_adcValue);
  SaveState->Read(m_adcBit);
  SaveState->Read(m_display, sizeof(m_display));
  SaveState->Read(m_timerCycles);
  m_lastForce = 0;
  if (!IsDisabled())
    StopForce();
}

void CModel2DriveBoard::Reset(void)
{
  CDriveBoard::Reset();
  m_motor = 0xFF;
  m_adcValue = 0x80;
  m_adcBit = 0;
  m_display[0] = m_display[1] = 0xFF;
  m_timerCycles = TIMER_CYCLES;
  m_lastForce = 0;
  if (!m_config["ForceFeedback"].ValueAsDefault<bool>(false))
    Disable();
  if (!IsDisabled())
    StopForce();
}

void CModel2DriveBoard::Disable(void)
{
  StopForce();
  CDriveBoard::Disable();
}

void CModel2DriveBoard::RunFrame(void)
{
  if (IsDisabled())
    return;

  // The next command the main board wrote, shown to the Z80 for this whole
  // frame (the program takes a command once it reads the same one twice)
  {
    std::lock_guard<std::mutex> lock(m_commandMutex);
    if (!m_commands.empty())
    {
      m_dataSent = m_commands.front();
      m_commands.pop_front();
    }
  }

  // The frame's interrupts, the motor sampled at each: the wheel gets the
  // frame's average torque
  int force = 0, samples = 0;
  int frameCycles = GetFramecycles();
  while (frameCycles > 0)
  {
    int cycles = m_z80.Run(std::min(frameCycles, m_timerCycles));
    frameCycles -= cycles;
    m_timerCycles -= cycles;
    if (m_timerCycles <= 0)
    {
      m_timerCycles += TIMER_CYCLES;
      m_z80.SetINT(true);
      int power = (~m_motor) & 0x3F;
      if (!(m_motor & 0x80))
        force -= power;             // left: the position goes down
      else if (!(m_motor & 0x40))
        force += power;
      samples++;
    }
  }
  if (samples)
    SendForce((INT8) std::clamp(force * 127 / (0x3F * samples), -127, 127));
}

UINT8 CModel2DriveBoard::Read8(UINT32 addr)
{
  if (addr < 0x8000)
    return m_rom[addr];
  if (addr >= 0xE000)
    return m_ram[addr - 0xE000];
  return 0xFF;
}

UINT8 CModel2DriveBoard::IORead8(UINT32 portNum)
{
  portNum &= 0xFF;
  switch (portNum)
  {
  case 0x26:                        // DIP switches
    return m_dip1;
  case 0x27:                        // command
    return m_dataSent;
  case 0x28: case 0x29: case 0x2A: case 0x2B:
  case 0x48: case 0x49: case 0x4A: case 0x4B:
    return "SEGA"[portNum & 3];     // the 315-5296s' ID
  case 0x80: case 0x81: case 0x82: case 0x83:
    // ADC: the selected channel's value, a bit at a time, MSB first
    if (m_adcBit == 0)
      return 0xFF;
    m_adcBit--;
    return (m_adcValue >> m_adcBit) & 1;
  default:
    return 0xFF;
  }
}

void CModel2DriveBoard::IOWrite8(UINT32 portNum, UINT8 data)
{
  portNum &= 0xFF;
  switch (portNum)
  {
  case 0x23:                        // reply
    m_dataReceived = data;
    break;
  case 0x42: case 0x43:             // 7-segment display
    m_display[portNum - 0x42] = data;
    break;
  case 0x46:                        // motor
    m_motor = data;
    break;
  case 0x80: case 0x81: case 0x82: case 0x83:
    // ADC channel select: channel 0 is the wheel
    m_adcValue = (portNum == 0x80 && m_inputs && m_inputs->steering) ? (UINT8) m_inputs->steering->value : 0x80;
    m_adcBit = 8;
    break;
  default:                          // 00: watchdog; the I/O chips' set-up
    break;
  }
}

void CModel2DriveBoard::SendForce(INT8 force)
{
  if (force == m_lastForce || !m_inputs || !m_inputs->steering)
    return;
  ForceFeedbackCmd ffCmd{};
  ffCmd.id = FFConstantForce;
  ffCmd.force = (float) force / 127.0f;
  m_inputs->steering->SendForceFeedbackCmd(ffCmd);
  m_lastForce = force;
}

void CModel2DriveBoard::StopForce(void)
{
  if (m_inputs && m_inputs->steering)
  {
    ForceFeedbackCmd ffCmd{};
    ffCmd.id = FFStop;
    m_inputs->steering->SendForceFeedbackCmd(ffCmd);
  }
  m_lastForce = 0;
}


/******************************************************************************
 The EPR-18261 board (Indy 500, Touring Car, Over Rev, Super GT 24h)
******************************************************************************/

CModel2DirectDriveBoard::CModel2DirectDriveBoard(const Util::Config::Node &config)
  : CDriveBoard(config)
{
  m_dip1 = 0xFF;                    // all off (the strength is ForceFeedbackStrength's gain)
  m_dip2 = 0xFF;
}

// A motor's power: the port's 6 bits inverted, in reverse order
int CModel2DirectDriveBoard::Power(UINT8 port)
{
  int bits = (~port) & 0x3F, power = 0;
  for (int i = 0; i < 6; i++)
    if (bits & (1 << i))
      power |= 0x20 >> i;
  return power;
}

void CModel2DirectDriveBoard::SaveState(CBlockFile *SaveState)
{
  CDriveBoard::SaveState(SaveState);
  SaveState->NewBlock("Model2DirectDriveBoard", __FILE__);
  SaveState->Write(m_left);
  SaveState->Write(m_right);
  SaveState->Write(m_adcValue);
  SaveState->Write(m_adcBit);
}

void CModel2DirectDriveBoard::LoadState(CBlockFile *SaveState)
{
  CDriveBoard::LoadState(SaveState);
  if (SaveState->FindBlock("Model2DirectDriveBoard") != Result::OKAY)
  {
    Disable();
    return;
  }
  SaveState->Read(m_left);
  SaveState->Read(m_right);
  SaveState->Read(m_adcValue);
  SaveState->Read(m_adcBit);
  m_lastForce = 0;
  if (!IsDisabled())
    StopForce();
}

void CModel2DirectDriveBoard::Reset(void)
{
  CDriveBoard::Reset();
  m_left = m_right = 0xFF;
  m_adcValue = 0x80;
  m_adcBit = 0;
  m_lastForce = 0;
  if (!m_config["ForceFeedback"].ValueAsDefault<bool>(false))
    Disable();
  if (!IsDisabled())
    StopForce();
}

void CModel2DirectDriveBoard::Disable(void)
{
  StopForce();
  CDriveBoard::Disable();
}

void CModel2DirectDriveBoard::RunFrame(void)
{
  if (IsDisabled())
    return;

  {
    std::lock_guard<std::mutex> lock(m_commandMutex);
    if (!m_commands.empty())
    {
      m_dataSent = m_commands.front();
      m_commands.pop_front();
    }
  }

  // The frame's NMIs (the CTC's), the motor sampled at each: the wheel gets
  // the frame's average torque
  int force = 0, samples = 0;
  int frameCycles = GetFramecycles();
  while (frameCycles > 0)
  {
    if (m_allowNMIInterupts)
    {
      int cycles = m_z80.Run(std::min<int>(frameCycles, m_nmiTimerVal));
      frameCycles -= cycles;
      m_nmiTimerVal -= cycles;
      if (m_nmiTimerVal <= 0)
      {
        m_z80.TriggerNMI();
        int freq = (int) m_z80Clock * 1000000;
        m_nmiTimerVal = freq / m_z80CTC.CalcFrequency(1, freq);
        force += Power(m_right) - Power(m_left);
        samples++;
      }
    }
    else
      frameCycles -= m_z80.Run(frameCycles);
  }

  // The game's pushes reach about 16 (strength 0-7 plus the board's base, DIP
  // switches off): that is full force
  constexpr int FULL = 16;
  if (samples)
    SendForce((INT8) std::clamp(force * 127 / (FULL * samples), -127, 127));
}

UINT8 CModel2DirectDriveBoard::IORead8(UINT32 portNum)
{
  portNum &= 0xFF;
  switch (portNum)
  {
  case 0x20:                        // DIP switches
    return m_dip1;
  case 0x21:
    return m_dip2;
  case 0x24: case 0x25: case 0x26: case 0x27:
    // ADC: the selected channel's value, a bit at a time, MSB first
    if (m_adcBit == 0)
      return 0xFF;
    m_adcBit--;
    return (m_adcValue >> m_adcBit) & 1;
  case 0x28:                        // command
    return m_dataSent;
  case 0x2C:                        // motor errors: none
    return 0x00;
  default:
    return 0xFF;
  }
}

void CModel2DirectDriveBoard::IOWrite8(UINT32 portNum, UINT8 data)
{
  CDriveBoard::IOWrite8(portNum, data);   // the CTC
  portNum &= 0xFF;
  switch (portNum)
  {
  case 0x24: case 0x25: case 0x26: case 0x27:
    // ADC channel select: 24 is the wheel
    m_adcValue = (portNum == 0x24 && m_inputs && m_inputs->steering) ? (UINT8) m_inputs->steering->value : 0x80;
    m_adcBit = 8;
    break;
  case 0x29:                        // reply
    m_dataReceived = data;
    break;
  case 0x2A:                        // right
    m_right = data;
    break;
  case 0x2E:                        // left
    m_left = data;
    break;
  default:
    break;
  }
}

void CModel2DirectDriveBoard::SendForce(INT8 force)
{
  if (force == m_lastForce || !m_inputs || !m_inputs->steering)
    return;
  ForceFeedbackCmd ffCmd{};
  ffCmd.id = FFConstantForce;
  ffCmd.force = (float) force / 127.0f;
  m_inputs->steering->SendForceFeedbackCmd(ffCmd);
  m_lastForce = force;
}

void CModel2DirectDriveBoard::StopForce(void)
{
  if (m_inputs && m_inputs->steering)
  {
    ForceFeedbackCmd ffCmd{};
    ffCmd.id = FFStop;
    m_inputs->steering->SendForceFeedbackCmd(ffCmd);
  }
  m_lastForce = 0;
}
