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
 * SkiBoard.cpp
 *
 * Implementation of the CSkiBoard class: rumble ski pad emulation
 * emulation.
 *
 * NOTE: Simulation does not yet work. Drive board ROMs are required.
 */

#include "SkiBoard.h"

#include "Supermodel.h"
#include "Inputs/Input.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

Game::DriveBoardType CSkiBoard::GetType(void)
{
  return Game::DRIVE_BOARD_SKI;
}

unsigned CSkiBoard::GetForceFeedbackStrength()
{
  return 0;
}

void CSkiBoard::SetForceFeedbackStrength(unsigned strength)
{

}

void CSkiBoard::LoadState(CBlockFile *SaveState)
{
  CDriveBoard::LoadState(SaveState);

  if (!IsDisabled())
    SendVibrate(0);
}

void CSkiBoard::Disable(void)
{
  SendVibrate(0);
  CDriveBoard::Disable();
}

bool CSkiBoard::Init(const UINT8 *romPtr)
{
  bool result = CDriveBoard::Init(romPtr);
  m_simulated = true;
  return result;
}

void CSkiBoard::Reset(void)
{
  CDriveBoard::Reset();
  m_lastVibrate = 0;

  if (!m_config["ForceFeedback"].ValueAsDefault<bool>(false))
    Disable();

  // Stop any effects that may still be playing
  if (!IsDisabled())
    SendVibrate(0);
}

UINT8 CSkiBoard::Read(void)
{
  if (IsDisabled())
  {
    return 0xFF;
  }

  // TODO - simulate initialization sequence even when emulating to get rid of long pause at boot up (drive board can
  // carry on booting whilst game starts)
  return SimulateRead();
}

void CSkiBoard::Write(UINT8 data)
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
}

UINT8 CSkiBoard::SimulateRead(void)
{
  if (m_initialized)
  {
    switch (m_readMode)
    {
      // Note about Driveboard error :
      // The value returned by case 0x0 is the error value displayed when error occurs.
      // In service menu->output test, you will see "foot ctrl clutch" occilating free/lock due to 
      // the incrementation of m_statusFlags (000x0000 : x=1=lock x=0=free)
      
      case 0x0: return m_statusFlags++;       // Status flags - Increment every time to bypass driveboard error
      case 0x1: return m_dip1;                // DIP switch 1 value
      case 0x4: return 0xFF;                  // Foot sensor Left=0xf0 Right=0x0f Both=0xff
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

void CSkiBoard::SimulateWrite(UINT8 cmd)
{
  // Following are commands for Scud Race.  Daytona 2 has a compatible command set while Sega Rally 2 is completely different
  // TODO - finish for Scud Race and Daytona 2
  // TODO - implement for Sega Rally 2
  UINT8 type = cmd>>4;
  UINT8 val = cmd&0xF;

  // Ski Champ vibration pad

    switch (type)
    {
    case 0x00: // nothing to do ?
        break;
    case 0x01: // full stop ?
      if (val == 0)
        SendVibrate(0);
      break;
    case 0x04:
      SendVibrate(val*0x11);
      break;
    case 0x08:
      if (val == 0x08)
      {
        // test driveboard passed ?
      }
      break;
    case 0x0A: // only when test in service menu
      switch (val)
      {
        case 3: // clutch
          break;
        case 5: // motor test
          SendVibrate(val * 0x11);
          break;
        case 6: // end motor test
          SendVibrate(0);
          break;
      }
      break;
    case 0x0C: // game state or reset driveboard ?
      switch (val)
      {
        case 1:
          // in game
          break;
        case 2:
          // game ready
          break;
        case 3:
          // test mode
          break;
      }
      break;
    case 0x0D: // 0xD0-DF Set read mode
      m_readMode = val & 0x7;
      break;
    default:
      //DebugLog("Skipad unknown command %02X , val= %02X\n",type,val);
      break;
    }

}

void CSkiBoard::RunFrame(void)
{
  if (m_simulated)
  {
    if (!m_initialized)
      m_initState++;
  }
}

UINT8 CSkiBoard::Read8(UINT32 addr)
{
  return 0xFF;
}

void CSkiBoard::Write8(UINT32 addr, UINT8 data)
{
}

void CSkiBoard::SendVibrate(UINT8 val)
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
  m_inputs->skiX->SendForceFeedbackCmd(ffCmd);

  m_lastVibrate = val;
}


CSkiBoard::CSkiBoard(const Util::Config::Node& config)
  : CDriveBoard(config),
    m_lastVibrate(0)
{
  m_attached = false;
  m_simulated = false;
  m_initialized = false;
  m_dip1 = 0xCF;

  DebugLog("Built Drive Board (ski pad)\n");
}

CSkiBoard::~CSkiBoard(void)
{

}
