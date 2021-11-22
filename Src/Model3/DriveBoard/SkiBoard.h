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
 * SkiBoard.h
 *
 * Header for the CSkiBoard (rumble skipad emulation) class.
 */

#ifndef INCLUDED_SKIBOARD_H
#define INCLUDED_SKIBOARD_H

#include "DriveBoard.h"
#include "Util/NewConfig.h"
#include "Game.h"

/*
 * CSkiBoard
 */
class CSkiBoard : public CDriveBoard
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
   * Saves the drive board state.
   *
   * Parameters:
   *    SaveState Block file to save state information to.
   */
  //void SaveState(CBlockFile *SaveState);

  /*
   * LoadState(SaveState):
   *
   * Restores the drive board state.
   *
   * Parameters:
   *    SaveState Block file to load save state information from.
   */
  void LoadState(CBlockFile *SaveState);

  /*
   * Init(romPtr):
   *
   * Initializes (and "attaches") the drive board. This should be called
   * before other members.
   *
   * Parameters:
   *    romPtr    Pointer to the drive board ROM (Z80 program). If this
   *          is NULL, then the drive board will not be emulated.
   *
   * Returns:
   *    FAIL if the drive board could not be initialized (prints own error
   *    message), otherwise OKAY. If the drive board is not attached
   *    because no ROM was passed to it, no error is generated and the
   *    drive board is silently disabled (detached).
   */
  bool Init(const UINT8 *romPtr);


  /*
   * Reset(void):
   *
   * Resets the drive board.
   */
  void Reset(void);

  /*
   * Read():
   *
   * Reads data from the drive board.
   *
   * Returns:
   *    Data read.
   */
  UINT8 Read(void);

  /*
   * Write(data):
   *
   * Writes data to the drive board.
   *
   * Parameters:
   *    data  Data to send.
   */
  void Write(UINT8 data);

  /*
   * RunFrame(void):
   *
   * Emulates a single frame's worth of time on the drive board.
   */
  void RunFrame(void);

  /*
   * CWheelBoard(config):
   * ~CWheelBoard():
   *
   * Constructor and destructor. Memory is freed by destructor.
   *
   * Paramters:
   *    config  Run-time configuration. The reference should be held because
   *            this changes at run-time.
   */
  CSkiBoard(const Util::Config::Node &config);
  ~CSkiBoard(void);

  // needed by abstract class
  UINT8 Read8(UINT32 addr);
  void Write8(UINT32 addr, UINT8 data);

protected:
  void Disable(void);

private:

  // Feedback state
  UINT8 m_lastVibrate;    // Last vibrate command sent

  UINT8 SimulateRead(void);

  void SimulateWrite(UINT8 data);

  void SendVibrate(UINT8 val);

};

#endif  // INCLUDED_SKIBOARD_H
