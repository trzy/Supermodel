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
 */

#ifndef INCLUDED_BILLBOARD_H
#define INCLUDED_BILLBOARD_H

#include "DriveBoard.h"
#include "Util/NewConfig.h"

/*
 * CBillBoard
 */
class CBillBoard : public CDriveBoard
{
public:
  /*
   * GetType(void):
   *
   * Returns:
   *    Drive board type.
   */

  Game::DriveBoardType GetType(void);

  unsigned GetForceFeedbackStrength(void);
  void SetForceFeedbackStrength(unsigned strength);

  /*
   * SaveState(SaveState):
   *
   * Saves the bill board state.
   *
   * Parameters:
   *    SaveState Block file to save state information to.
   */
  void SaveState(CBlockFile *SaveState);

  /*
   * LoadState(SaveState):
   *
   * Restores the bill board state.
   *
   * Parameters:
   *    SaveState Block file to load save state information from.
   */
  void LoadState(CBlockFile *SaveState);

  void AttachInputs(CInputs* inputs, unsigned gameInputFlags);

  /*
   * CBillBoard(config):
   * ~CBillBoard():
   *
   * Constructor and destructor. Memory is freed by destructor.
   *
   * Paramters:
   *    config  Run-time configuration. The reference should be held because
   *            this changes at run-time.
   */
  CBillBoard(const Util::Config::Node &config);
  ~CBillBoard(void);

  /*
   * Read8(addr):
   * IORead8(portNum):
   *
   * Methods for reading from Z80's memory and IO space. Required by CBus.
   *
   * Parameters:
   *    addr    Address in memory (0-0xFFFF).
   *    portNum   Port address (0-255).
   *
   * Returns:
   *    A byte of data from the address or port.
   */
  UINT8 IORead8(UINT32 portNum);

  /*
   * Write8(addr, data):
   * IORead8(portNum, data):
   *
   * Methods for writing to Z80's memory and IO space. Required by CBus.
   *
   * Parameters:
   *    addr    Address in memory (0-0xFFFF).
   *    portNum   Port address (0-255).
   *    data    Byte to write.
   */
  void IOWrite8(UINT32 portNum, UINT8 data);

};

#endif  // INCLUDED_BILLBOARD_H
