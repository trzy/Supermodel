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

#include "BillBoard.h"

#include "Supermodel.h"

#include <cstdio>
#include <cmath>
#include <algorithm>


Game::DriveBoardType CBillBoard::GetType(void)
{
  return Game::DRIVE_BOARD_BILLBOARD;
}

unsigned CBillBoard::GetForceFeedbackStrength()
{
  return 0;
}

void CBillBoard::SetForceFeedbackStrength(unsigned strength)
{

}

void CBillBoard::SaveState(CBlockFile* SaveState)
{
  CDriveBoard::SaveState(SaveState);
  SaveState->NewBlock("BillBoard", __FILE__);
  SaveState->Write(&m_dip1, sizeof(m_dip1));
}

void CBillBoard::LoadState(CBlockFile* SaveState)
{
  CDriveBoard::LoadState(SaveState);
  if (SaveState->FindBlock("BillBoard") != OKAY)
  {
    ErrorLog("Unable to load billboard state. Save state file is corrupt.");
    return;
  }
  SaveState->Read(&m_dip1, sizeof(m_dip1));
}

void CBillBoard::AttachInputs(CInputs* inputs, unsigned gameInputFlags)
{

}

UINT8 CBillBoard::IORead8(UINT32 portNum)
{
  switch (portNum)
  {
  case 0x20:
    // return the dipswitch
    // 0x80 : test all segments
    return m_dip1;
  case 0x21:
    //DebugLog("     Bill R portnum=%X  m_dataSent=%X\n", portNum, m_dataSent);
    return m_dataSent;
  case 0x26:
    //DebugLog("          Bill R portnum=%X  m_dataSent=%X\n", portNum, m_dataSent);
    // 0xf0 or 0x0f = no more test lamp
    return 0xff;
  default:
    DebugLog("Unhandled Z80 input on port %u (at PC = %04X)\n", portNum, m_z80.GetPC());
    return 0xff;
  }
}

void CBillBoard::IOWrite8(UINT32 portNum, UINT8 data)
{
  switch (portNum)
  {
  case 0x22: // P1 Digit 1
    //DebugLog("Bill W 0x22 <- %X\n", data);
    if (m_outputs != NULL)
      m_outputs->SetValue(OutputBill1, data);
    m_dataReceived = data;
    break;
  case 0x23: // P1 Digit 2
    //DebugLog("Bill W 0x23 <- %X\n", data);
    if (m_outputs != NULL)
      m_outputs->SetValue(OutputBill2, data);
    m_dataReceived = data;
    break;
  case 0x24: // P2 Digit 1
    //DebugLog("Bill W 0x24 <- %X\n", data);
    if (m_outputs != NULL)
      m_outputs->SetValue(OutputBill3, data);
    m_dataReceived = data;
    break;
  case 0x25: // P2 Digit 2
    //DebugLog("Bill W 0x25 <- %X\n", data);
    if (m_outputs != NULL)
      m_outputs->SetValue(OutputBill4, data);
    m_dataReceived = data;
    break;
  case 0x26: // lamp P1 P2
    //DebugLog("Bill W 0x26 <- %X\n", data);
    if (m_outputs != NULL)
      m_outputs->SetValue(OutputBill5, data);
    m_dataReceived = data;
    break;
  case 0x28:
    //DebugLog("Bill W 0x28 <- %X\n", data);
    if (data == 0x03)
      m_allowInterrupts = true;
    break;
  case 0x2e:
    //DebugLog("Bill W 0x2e <- %X\n", data);
    if (data == 0x00)
      m_initialized = true;
    return;
  default:
    DebugLog("Unhandled Z80 output on port %u (at PC = %04X)\n", portNum, m_z80.GetPC());
    break;
  }
}



CBillBoard::CBillBoard(const Util::Config::Node& config)
  : CDriveBoard(config)
{
  m_dip1 = 0x0f;
  m_simulated = false;
  m_z80Clock = 8.0;
  m_z80NMI = false;

  DebugLog("Built Drive Board (billboard)\n");
}

CBillBoard::~CBillBoard(void)
{

}
