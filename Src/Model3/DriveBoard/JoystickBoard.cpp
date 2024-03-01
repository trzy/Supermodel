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
 * JoyBoard.cpp
 *
 * Implementation of the CJoyBoard class: drive board (force feedback emulation
 * for joystick) emulation.
 *
 * NOTE: This is largely a copy of CWheelBoard as it appears to be the same drive
 * board. The joystick X axis is used rather than the wheel input. It is unknown
 * how or whether the Y axis is involved at all.
 *
 * NOTE: Simulation does not yet work. Drive board ROMs are required.
 */

#include "JoystickBoard.h"

#include "Supermodel.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

Game::DriveBoardType CJoyBoard::GetType(void)
{
  return Game::DRIVE_BOARD_JOYSTICK;
}

void CJoyBoard::Get7SegDisplays(UINT8 &seg1Digit1, UINT8 &seg1Digit2, UINT8 &seg2Digit1, UINT8 &seg2Digit2)
{
  seg1Digit1 = m_seg1Digit1;
  seg1Digit2 = m_seg1Digit2;
  seg2Digit1 = m_seg2Digit1;
  seg2Digit2 = m_seg2Digit2;
}

void CJoyBoard::SaveState(CBlockFile *SaveState)
{
  CDriveBoard::SaveState(SaveState);

  SaveState->NewBlock("JoystickBoard", __FILE__);
  SaveState->Write(&m_simulated, sizeof(m_simulated));
  if (m_simulated)
  {
    // TODO - save board simulation state
  }
  else
  {
    // Save DIP switches and digit displays
    SaveState->Write(&m_dip1, sizeof(m_dip1));
    SaveState->Write(&m_dip2, sizeof(m_dip2));

    SaveState->Write(&m_adcPortRead, sizeof(m_adcPortRead));
    SaveState->Write(&m_adcPortBit, sizeof(m_adcPortBit));
    SaveState->Write(&m_uncenterVal1, sizeof(m_uncenterVal1));
    SaveState->Write(&m_uncenterVal2, sizeof(m_uncenterVal2));
  }
}

void CJoyBoard::LoadState(CBlockFile *SaveState)
{
  CDriveBoard::LoadState(SaveState);

  if (SaveState->FindBlock("JoystickBoard") != OKAY)
  {
    ErrorLog("Unable to load joystick drive board state. Save state file is corrupt.");
    Disable();
    return;
  }

  bool wasSimulated;
  SaveState->Read(&wasSimulated, sizeof(wasSimulated));
  if (wasSimulated)
  {
    // Simulation has never existed
    ErrorLog("Save state contains unexpected data. Halting drive board emulation.");
    Disable();
    return;
  }
  else
  {
    // Load DIP switches and digit displays
    SaveState->Read(&m_dip1, sizeof(m_dip1));
    SaveState->Read(&m_dip2, sizeof(m_dip2));

    SaveState->Read(&m_adcPortRead, sizeof(m_adcPortRead));
    SaveState->Read(&m_adcPortBit, sizeof(m_adcPortBit));
    SaveState->Read(&m_uncenterVal1, sizeof(m_uncenterVal1));
    SaveState->Read(&m_uncenterVal2, sizeof(m_uncenterVal2));
  }
}

void CJoyBoard::Disable(void)
{
  SendStopAll();
  CDriveBoard::Disable();
}

void CJoyBoard::Reset(void)
{
  CDriveBoard::Reset();

  m_seg1Digit1 = 0xFF;
  m_seg1Digit2 = 0xFF;
  m_seg2Digit1 = 0xFF;
  m_seg2Digit2 = 0xFF;

  m_adcPortRead = 0;
  m_adcPortBit = 0;
  m_port42Out = 0;
  m_port45Out = 0;
  m_port46Out = 0;
  m_prev42Out = 0;
  m_prev45Out = 0;
  m_prev46Out = 0;

  m_uncenterVal1 = 0;
  m_uncenterVal2 = 0;

  m_lastConstForce = 0;
  m_lastConstForceY = 0;
  m_lastSelfCenter = 0;
  m_lastFriction = 0;
  m_lastVibrate = 0;

  m_simulated = false;  //TODO: make this run-time configurable when simulation mode is supported

  if (!m_config["ForceFeedback"].ValueAsDefault<bool>(false))
    Disable();

  // Stop any effects that may still be playing
  if (!IsDisabled())
    SendStopAll();
}

UINT8 CJoyBoard::Read(void)
{
  if (IsDisabled())
  {
    return 0xFF;
  }

  // TODO - simulate initialization sequence even when emulating to get rid of long pause at boot up (drive board can
  // carry on booting whilst game starts)
  if (m_simulated)
    return SimulateRead();
  else
    return CDriveBoard::Read();
}

void CJoyBoard::Write(UINT8 data)
{
  if (IsDisabled())
  {
    return;
  }

  //if (data >= 0x01 && data <= 0x0F ||
  //  data >= 0x20 && data <= 0x2F ||
  //  data >= 0x30 && data <= 0x3F ||
  //  data >= 0x40 && data <= 0x4F ||
  //  data >= 0x70 && data <= 0x7F)
  //  DebugLog("DriveBoard.Write(%02X)\n", data);
  if (m_simulated)
    SimulateWrite(data);
  else
  {
    CDriveBoard::Write(data);
    if (data == 0xCB)
      m_initialized = false;
  }
}

UINT8 CJoyBoard::SimulateRead(void)
{
  if (m_initialized)
  {
    switch (m_readMode)
    {
      case 0x0: return m_statusFlags;                      // Status flags
      case 0x1: return m_dip1;                             // DIP switch 1 value
      case 0x2: return m_dip2;                             // DIP switch 2 value
      case 0x3: return m_wheelCenter;                      // Wheel center
      case 0x4: return 0x80;                               // Cockpit banking center
      case 0x5: return (UINT8)m_inputs->analogJoyX->value; // Wheel position
      case 0x6: return 0x80;                               // Cockpit banking position
      case 0x7: return m_echoVal;                          // Init status/echo test
      default:  return 0xFF;
    }
  }
  else
  {
    switch (m_initState / 5)
    {
      case 0:  return 0xCF;  // Initiate start
      case 1:  return 0xCE;
      case 2:  return 0xCD;
      case 3:  return 0xCC;  // Centering wheel
      default:
        m_initialized = true;
        return 0x80;
    }
  }
}

void CJoyBoard::SimulateWrite(UINT8 cmd)
{
  // Following are commands for Scud Race.  Daytona 2 has a compatible command set while Sega Rally 2 is completely different
  // TODO - finish for Scud Race and Daytona 2
  // TODO - implement for Sega Rally 2
  UINT8 type = cmd>>4;
  UINT8 val = cmd&0xF;


    switch (type)
    {
    case 0: // 0x00-0F Play sequence
      /* TODO */
      break;
    case 1: // 0x10-1F Set centering strength
      if (val == 0)
        // Disable auto-centering
        // TODO - is 0x10 for disable?
        SendSelfCenter(0);
      else
        // Enable auto-centering (0x1 = weakest, 0xF = strongest)
        SendSelfCenter(val * 0x11);
      break;
    case 2: // 0x20-2F Friction strength
      if (val == 0)
        // Disable friction
        // TODO - is 0x20 for disable?
        SendFriction(0);
      else
        // Enable friction (0x1 = weakest, 0xF = strongest)
        SendFriction(val * 0x11);
      break;
    case 3: // 0x30-3F Uncentering (vibrate)
      if (val == 0)
        // Disable uncentering
        SendVibrate(0);
      else
        // Enable uncentering (0x1 = weakest, 0xF = strongest)
        SendVibrate(val * 0x11);
      break;
    case 4: // 0x40-4F Play power-slide sequence
      /* TODO */
      break;
    case 5: // 0x50-5F Rotate wheel right
      SendConstantForce((val + 1) * 0x5);
      break;
    case 6: // 0x60-6F Rotate wheel left
      SendConstantForce(-(val + 1) * 0x5);
      break;
    case 7: // 0x70-7F Set steering parameters
      /* TODO */
      break;
    case 8: // 0x80-8F Test Mode
      switch (val & 0x7)
      {
        case 0:  SendStopAll();                                      break;  // 0x80 Stop motor
        case 1:  SendConstantForce(20);                              break;  // 0x81 Roll wheel right
        case 2:  SendConstantForce(-20);                             break;  // 0x82 Roll wheel left
        case 3:  /* Ignore - no clutch */                            break;  // 0x83 Clutch on
        case 4:  /* Ignore - no clutch */                            break;  // 0x84 Clutch off
        case 5:  m_wheelCenter = (UINT8)m_inputs->analogJoyX->value; break;  // 0x85 Set wheel center position
        case 6:  /* Ignore */                                        break;  // 0x86 Set cockpit banking position
        case 7:  /* Ignore */                                        break;  // 0x87 Lamp on/off
      }
    case 0x9: // 0x90-9F ??? Don't appear to have any effect with Scud Race ROM
      /* TODO */
      break;
    case 0xA: // 0xA0-AF ??? Don't appear to have any effect with Scud Race ROM
      /* TODO */
      break;
    case 0xB: // 0xB0-BF Invalid command (reserved for use by PPC to send cabinet type 0xB0 or 0xB1 during initialization)
      /* Ignore */
      break;
    case 0xC: // 0xC0-CF Set board mode (0xCB = reset board)
      SendStopAll();
      if (val >= 0xB)
      {
        // Reset board
        m_initialized = false;
        m_initState = 0;
      }
      else
        m_boardMode = val;
      break;
    case 0xD: // 0xD0-DF Set read mode
      m_readMode = val & 0x7;
      break;
    case 0xE: // 0xE0-EF Invalid command
      /* Ignore */
      break;
    case 0xF: // 0xF0-FF Echo test
      m_echoVal = val;
      break;
    }

}

void CJoyBoard::RunFrame(void)
{
  if (m_simulated)
    SimulateFrame();
  else
    CDriveBoard::RunFrame();
}

void CJoyBoard::SimulateFrame(void)
{
  if (!m_initialized)
    m_initState++;
  // TODO - update m_statusFlags and play preset scripts according to board mode
}

UINT8 CJoyBoard::IORead8(UINT32 portNum)
{
  UINT8 adcVal;

  switch (portNum)
  {
  case 0x20: // DIP 1 value
    return m_dip1;
  case 0x21: // DIP 2 value
    return m_dip2;
  case 0x24: // ADC channel 1 - Y analog axis for joystick
  case 0x25: // ADC channel 2 - steering wheel position (0x00 = full left, 0x80 = center, 0xFF = full right) and X analog axis for joystick
  case 0x26: // ADC channel 3 - cockpit bank position (deluxe cabinets) (0x00 = full left, 0x80 = center, 0xFF = full right)
  case 0x27: // ADC channel 4 - not connected
    if (portNum == m_adcPortRead && m_adcPortBit-- > 0)
    {
      switch (portNum)
      {
      case 0x24: // Y analog axis for joystick
        adcVal = ReadADCChannel1();
        break;
      case 0x25: // Steering wheel for twin racing cabinets - TODO - check actual range of steering, suspect it is not really 0x00-0xFF
        adcVal = ReadADCChannel2();
        break;
      case 0x26: // Cockpit bank position for deluxe racing cabinets
        adcVal = ReadADCChannel3();
        break;
      case 0x27: // Not connected
        adcVal = ReadADCChannel4();
        break;
      default:
        DebugLog("Unhandled Z80 input on ADC port %u (at PC = %04X)\n", portNum, m_z80.GetPC());
        return 0xFF;
      }
      return (adcVal >> m_adcPortBit) & 0x01;
      }
      else
      {
        DebugLog("Unhandled Z80 input on ADC port %u (at PC = %04X)\n", portNum, m_z80.GetPC());
        return 0xFF;
      }
  case 0x28: // PPC command
    return m_dataSent;
  case 0x2c: // Encoder error reporting (kept at 0x00 for no error)
    // Bit 1 0
    //     0 0 = encoder okay, no error
    //     0 1 = encoder error 1 - overcurrent error
    //     1 0 = encoder error 2 - overheat error
    //     1 1 = encoder error 3 - encoder error, reinitializes board
    return 0x00;
  default:
    DebugLog("Unhandled Z80 input on port %u (at PC = %04X)\n", portNum, m_z80.GetPC());
    return 0xFF;
  }
}

void CJoyBoard::IOWrite8(UINT32 portNum, UINT8 data)
{
  switch (portNum)
  {
  case 0x10: // Unsure? - single byte 0x03 sent at initialization, then occasionally writes 0x07 & 0xFA to port
    return;
  case 0x11: // Interrupt control
    if (data == 0x57)
      m_allowInterrupts = true;
    else if (data == 0x53) // Strictly speaking 0x53 then 0x04
      m_allowInterrupts = false;
    return;
  case 0x1c: // Unsure? - two bytes 0xFF, 0xFF sent at initialization only
  case 0x1d: // Unsure? - two bytes 0x0F, 0x17 sent at initialization only
  case 0x1e: // Unsure? - same as port 28
  case 0x1f: // Unsure? - same as port 31
    return;
  case 0x20: // Left digit of 7-segment display 1
    m_seg1Digit1 = data;
    return;
  case 0x21: // Right digit of 7-segment display 1
    m_seg1Digit2 = data;
    return;
  case 0x22: // Left digit of 7-segment display 2
    m_seg2Digit1 = data;
    return;
  case 0x23: // Right digit of 7-segment display 2
    m_seg2Digit2 = data;
    return;
  case 0x24: // ADC channel 1 control
  case 0x25: // ADC channel 2 control
  case 0x26: // ADC channel 3 control
  case 0x27: // ADC channel 4 control
    m_adcPortRead = portNum;
    m_adcPortBit = 8;
    return;
  case 0x29: // Reply for PPC
    m_dataReceived = data;
    if (data == 0xCB)
      m_initialized = true;
    return;
  case 0x2a: // Encoder motor data (x axis)
    m_port42Out = data;
    ProcessEncoderCmdJoystick();
    return;
  case 0x2d: // Clutch/lamp control (deluxe cabinets) ( or y axis)
    m_port45Out = data;
    ProcessEncoderCmdJoystick();
    return;
  case 0x2e: // Encoder motor control
    m_port46Out = data;
    return;
  case 0xf0: // Unsure? - single byte 0xBB sent at initialization only
    return;
  case 0xf1: // Unsure? - single byte 0x4E sent regularly - some sort of watchdog?
    return;
  default:
    DebugLog("Unhandled Z80 output on port %u (at PC = %04X)\n", portNum, m_z80.GetPC());
    return;
  }
}

void CJoyBoard::ProcessEncoderCmdJoystick(void)
{
  if (m_prev42Out != m_port42Out || m_prev46Out != m_port46Out || m_prev45Out != m_port45Out)
  {
    switch (m_port46Out)
    {
      case 0xEE:
      // Apply constant force
        if (m_port42Out > 0x7f) // X
        {
          SendConstantForce(2 * (m_port42Out - 0x7f));
        }
        else if (m_port42Out < 0x7f)
        {
          SendConstantForce(2 * (m_port42Out - 0x7f));
        }
        else
        {
          SendConstantForce(0);
        }

        if (m_port45Out > 0x7f) // Y
        {
          SendConstantForceY(-2 * (m_port45Out - 0x7f));
        }
        else if (m_port45Out < 0x7f)
        {
          SendConstantForceY(-2 * (m_port45Out - 0x7f));
        }
        else
        {
          SendConstantForceY(0);
        }

        if (m_port42Out == 0x7f && m_port45Out == 0x81)
        {
          SendSelfCenter(255);
        }
        else SendSelfCenter(0);

        break;

        case 0xFF:
        // Stop all effects
        if (m_port42Out == 0xFF && m_port45Out == 0xFF)
          SendStopAll();
        break;

        case 0xcc: // init
        //42[0B] / 45[0A]
        //42[0B] / 45[0B]
        //42[FF] / 45[0B]
        //42[FF] / 45[FF]
        break;

        case 0xdd: // init
        // 42[FF] / 45[00]
        // 42[FF] / 45[FF]

        // 42[0A] / 45[00]
        // 42[0A] / 45[0A]
        break;

        case 0xce:
        // 42[7F] / 45[08]
        // 42[7F] / 45[09]
        // 42[7F] / 45[0A]
        // 42[7F] / 45[0B]
        // 42[7F] / 45[81]
        if (m_port42Out == 0x7f && m_port45Out != 0x81) // X
        {
          SendConstantForce(2 * m_port45Out);
        }

        if (m_port42Out == 0x7f && m_port45Out == 0x81)
        {
          SendSelfCenter(255);
        }
        break;

        case 0xec:
        // 42[09] / 45[81]
        // 42[2A] / 45[81]
        // 42[1B] / 45[81]
        // 42[7F] / 45[81]
        if (m_port45Out == 0x81 && m_port42Out != 0x7f) // Y
        {
          SendConstantForceY(2 * m_port42Out);
        }

        if (m_port42Out == 0x7f && m_port45Out == 0x81)
        {
          SendSelfCenter(255);
        }
        break;

        case 0x00: // init
        // 42[FF] / 45[00]
        // 42[FF] / 45[FF]
        break;

        case 0x99: // init
        // 42[B0] / 45[B0]
        // 42[80] / 45[B0]
        // 42[80] / 45[80]
        break;

        default:
        //DebugLog("Unknown = 46 [%02X] / 42 [%02X] / 45 [%02X]\n", m_port46Out, m_port42Out, m_port45Out);
        break;
    }

    m_prev42Out = m_port42Out;
    m_prev46Out = m_port46Out;
    m_prev45Out = m_port45Out;
  }
}

void CJoyBoard::SendStopAll(void)
{
  //DebugLog(">> Stop All Effects\n");

  ForceFeedbackCmd ffCmd;
  ffCmd.id = FFStop;

  m_inputs->analogJoyX->SendForceFeedbackCmd(ffCmd);
  m_inputs->analogJoyY->SendForceFeedbackCmd(ffCmd);

  m_lastConstForce = 0;
  m_lastSelfCenter = 0;
  m_lastFriction   = 0;
  m_lastVibrate    = 0;
  m_lastConstForceY = 0;
}

void CJoyBoard::SendConstantForce(INT8 val)
{
  if (val == m_lastConstForce)
    return;
  /*
  if (val > 0)
  {
    DebugLog(">> Force Right %02X [%8s", val, "");
    for (unsigned i = 0; i < 8; i++)
      DebugLog(i == 0 || i <= (val + 1) / 16 ? ">" : " ");
    DebugLog("]\n");
  }
  else if (val < 0)
  {
    DebugLog(">> Force Left  %02X [", -val);
    for (unsigned i = 0; i < 8; i++)
      DebugLog(i == 7 || i >= (val + 128) / 16 ? "<" : " ");
    DebugLog("%8s]\n", "");
  }
  else
    DebugLog(">> Stop Force     [%16s]\n", "");
  */

  ForceFeedbackCmd ffCmd;
  ffCmd.id = FFConstantForce;
  ffCmd.force = (float)val / (val >= 0 ? 127.0f : 128.0f);

  m_inputs->analogJoyX->SendForceFeedbackCmd(ffCmd);

  m_lastConstForce = val;
}

void CJoyBoard::SendConstantForceY(INT8 val)
{
  if (val == m_lastConstForceY)
    return;

  ForceFeedbackCmd ffCmd;
  ffCmd.id = FFConstantForce;
  ffCmd.force = (float)val / (val >= 0 ? 127.0f : 128.0f);
  m_inputs->analogJoyY->SendForceFeedbackCmd(ffCmd);
  m_lastConstForceY = val;
}

void CJoyBoard::SendSelfCenter(UINT8 val)
{
  if (val == m_lastSelfCenter)
    return;
  /*
  if (val == 0)
    DebugLog(">> Stop Self-Center\n");
  else
    DebugLog(">> Self-Center %02X\n", val);
  */

  ForceFeedbackCmd ffCmd;
  ffCmd.id = FFSelfCenter;
  ffCmd.force = (float)val / 255.0f;

  m_inputs->analogJoyX->SendForceFeedbackCmd(ffCmd);
  m_inputs->analogJoyY->SendForceFeedbackCmd(ffCmd);

  m_lastSelfCenter = val;
}


void CJoyBoard::SendFriction(UINT8 val)
{
  if (val == m_lastFriction)
    return;
  /*
  if (val == 0)
    DebugLog(">> Stop Friction\n");
  else
    DebugLog(">> Friction %02X\n", val);
  */

  ForceFeedbackCmd ffCmd;
  ffCmd.id = FFFriction;
  ffCmd.force = (float)val / 255.0f;
  m_inputs->analogJoyX->SendForceFeedbackCmd(ffCmd);

  m_lastFriction = val;
}

void CJoyBoard::SendVibrate(UINT8 val)
{
  if (val == m_lastVibrate)
    return;
  /*
  if (val == 0)
    DebugLog(">> Stop Vibrate\n");
  else
    DebugLog(">> Vibrate %02X\n", val);
  */

  ForceFeedbackCmd ffCmd;
  ffCmd.id = FFVibrate;
  ffCmd.force = (float)val / 255.0f;
  m_inputs->analogJoyX->SendForceFeedbackCmd(ffCmd);

  m_lastVibrate = val;
}

uint8_t CJoyBoard::ReadADCChannel1()
{
  if (m_initialized)
    return (UINT8)m_inputs->analogJoyY->value;
  else
    return 0x80; // If not initialized, return 0x80 so that ffb centering test does not fail
}

uint8_t CJoyBoard::ReadADCChannel2()
{
  if (m_initialized)
    return (UINT8)m_inputs->analogJoyX->value;
  else
    return 0x80; // If not initialized, return 0x80 so that ffb centering test does not fail
}

uint8_t CJoyBoard::ReadADCChannel3()
{
  return 0x80;
}

uint8_t CJoyBoard::ReadADCChannel4()
{
  return 0x80;
}

CJoyBoard::CJoyBoard(const Util::Config::Node &config)
  : CDriveBoard(config)
{
  m_dip1 = 0xCF;
  m_dip2 = 0xFF;

  DebugLog("Built Drive Board (Joystick)\n");
}

CJoyBoard::~CJoyBoard(void)
{

}
