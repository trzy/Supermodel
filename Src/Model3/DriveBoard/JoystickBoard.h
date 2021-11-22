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
 * JoyBoard.h
 *
 * Header for the CJoyBoard (force feedback emulation for joystick) class.
 */

#ifndef INCLUDED_JOYBOARD_H
#define INCLUDED_JOYBOARD_H

#include "DriveBoard.h"
#include "Util/NewConfig.h"
#include "Game.h"

/*
 * CJoyBoard
 */
class CJoyBoard : public CDriveBoard
{
public:
  /*
   * GetType(void):
   *
   * Returns:
   *    Drive board type.
   */
  Game::DriveBoardType GetType(void);

  /*
   * Get7SegDisplays(seg1Digit1, seg1Digit2, seg2Digit1, seg2Digit2):
   *
   * Reads the 7-segment displays.
   *
   * Parameters:
   *    seg1Digit1  Reference of variable to store digit 1 of the first 7-
   *          segment display to.
   *    seg1Digit2  First display, second digit.
   *    seg2Digit1  Second display, first digit.
   *    seg2Digit2  Second display, second digit.
   */
  void Get7SegDisplays(UINT8 &seg1Digit, UINT8 &seg1Digit2, UINT8 &seg2Digit1, UINT8 &seg2Digit2);


  /*
   * SaveState(SaveState):
   *
   * Saves the drive board state.
   *
   * Parameters:
   *    SaveState Block file to save state information to.
   */
  void SaveState(CBlockFile *SaveState);

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
   * CDriveBoard(config):
   * ~CDriveBoard():
   *
   * Constructor and destructor. Memory is freed by destructor.
   *
   * Paramters:
   *    config  Run-time configuration. The reference should be held because
   *            this changes at run-time.
   */
  CJoyBoard(const Util::Config::Node &config);
  ~CJoyBoard(void);

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

protected:
  void Disable(void);

private:

  UINT8 m_seg1Digit1;   // Current value of left digit on 7-segment display 1
  UINT8 m_seg1Digit2;   // Current value of right digit on 7-segment display 1
  UINT8 m_seg2Digit1;   // Current value of left digit on 7-segment display 2
  UINT8 m_seg2Digit2;     // Current value of right digit on 7-segment display 2

  UINT16 m_adcPortRead;   // ADC port currently reading from
  UINT8 m_adcPortBit;     // Bit number currently reading on ADC port

  UINT8 m_port42Out;      // Last value sent to Z80 I/O port 42 (encoder motor data)
  UINT8 m_port46Out;      // Last value sent to Z80 I/O port 46 (encoder motor control)
  UINT8 m_port45Out;      // Last value sent to Z80 I/O port 42 (up-down motor data)

  UINT8 m_prev42Out;      // Previous value sent to Z80 I/O port 42
  UINT8 m_prev46Out;      // Previous value sent to Z80 I/O port 46
  UINT8 m_prev45Out;      // Previous value sent to Z80 I/O port 45 (up-down)

  UINT8 m_uncenterVal1;   // First part of pending uncenter command
  UINT8 m_uncenterVal2;   // Second part of pending uncenter command

  // Feedback state
  INT8 m_lastConstForce;  // Last constant force command sent
  INT8 m_lastConstForceY;  // Last constant force command sent y axis
  UINT8 m_lastSelfCenter; // Last self center command sent
  UINT8 m_lastFriction;   // Last friction command sent
  UINT8 m_lastVibrate;    // Last vibrate command sent

  UINT8 SimulateRead(void);

  void SimulateWrite(UINT8 data);

  void SimulateFrame(void);

  void ProcessEncoderCmdJoystick(void);

  void SendStopAll(void);

  void SendConstantForce(INT8 val);

  void SendConstantForceY(INT8 val);

  void SendSelfCenter(UINT8 val);

  void SendFriction(UINT8 val);

  void SendVibrate(UINT8 val);

  uint8_t ReadADCChannel1();

  uint8_t ReadADCChannel2();

  uint8_t ReadADCChannel3();

  uint8_t ReadADCChannel4();
};

#endif  // INCLUDED_JOYBOARD_H
