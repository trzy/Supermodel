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
 * Model2DriveBoard.h
 *
 * The Model 2 racing games' wheel drive board (MAME's sj25_0207_01: Daytona
 * USA's 838-10646, Sega Rally's): a Z80 at 4 MHz running the drive program
 * (Daytona: "HANDLE CONTROL PROGRAM", EPR-16488A), two 315-5296 I/O chips and
 * an MSM6253 ADC. Unlike the Model 3 board it drives the wheel's motor itself:
 *
 *   IN 27      the command from the main board (Daytona: the I/O board's
 *              shared RAM byte 11; the 2A games: I/O port E)
 *   OUT 23     the reply (not connected to the main board here, as in MAME)
 *   IN 26      DIP switches: 7-6 equal for normal operation, 3 power-on
 *              calibration, 5-4 the display, 2-0 the strength (inverted)
 *   IN 28-2B, 48-4B  the I/O chips' "SEGA" ID, checked at power-on
 *   OUT/IN 80  the ADC (channel select, then the 8 bits serially, MSB first):
 *              the wheel's position
 *   OUT 42-43  the 7-segment display
 *   OUT 46     the motor: power in bits 5-0 (inverted), bit 7 cleared to turn
 *              the wheel left, bit 6 cleared to turn it right
 *
 * The work is done in the maskable interrupt (IM 1), which the program's
 * timeouts put at about 1 kHz (976 ticks per calibration step, 1000 for the
 * motor test): taken as 4 MHz / 4096. At power-on the board turns the wheel
 * left and right to find its ends and centre, as the cabinet does; without a
 * force feedback wheel that moves, it gives up and drives no force.
 *
 * The ROM is optional: without it the game runs without the board.
 */

#ifndef INCLUDED_MODEL2DRIVEBOARD_H
#define INCLUDED_MODEL2DRIVEBOARD_H

#include "Model3/DriveBoard/DriveBoard.h"

class CModel2DriveBoard : public CDriveBoard
{
public:
  CModel2DriveBoard(const Util::Config::Node &config);

  Game::DriveBoardType GetType(void) const override { return Game::DRIVE_BOARD_WHEEL; }
  unsigned GetForceFeedbackStrength(void) const override;
  void SetForceFeedbackStrength(unsigned strength) override;

  void SaveState(CBlockFile *SaveState) override;
  void LoadState(CBlockFile *SaveState) override;
  void Reset(void) override;
  void RunFrame(void) override;

  UINT8 Read8(UINT32 addr) override;
  UINT8 IORead8(UINT32 portNum) override;
  void IOWrite8(UINT32 portNum, UINT8 data) override;

protected:
  void Disable(void) override;

private:
  static constexpr int TIMER_CYCLES = 4096;   // Z80 cycles per interrupt

  void SendForce(INT8 force);
  void StopForce(void);

  UINT8 m_motor = 0xFF;         // port 46
  UINT8 m_adcValue = 0x80;      // the value being shifted out
  UINT8 m_adcBit = 0;           // bits left to shift out
  UINT8 m_display[2] = { 0xFF, 0xFF };
  int   m_timerCycles = TIMER_CYCLES;
  INT8  m_lastForce = 0;
};

/*
 * The EPR-18261 drive board of Indy 500, Sega Touring Car Championship, Over
 * Rev and Super GT 24h (Le Mans 24 on Model 3 has the same program): the Model
 * 3 board's hardware (Z80 at 8 MHz, the CTC's NMI, the same ports), but a
 * direct drive motor with a power each way instead of Model 3's servo board:
 *
 *   IN 28      the command from the main board (I/O port E)
 *   OUT 29     the reply, on the main board's I/O port D (the game reads the
 *              wheel's centre from it)
 *   IN 20, 21  DIP switches (all off)
 *   OUT/IN 24  the ADC: the wheel's position (channel select, then the 8 bits
 *              serially, MSB first, as Model 3's board)
 *   OUT 2E     the power turning the wheel left, OUT 2A right: 6 bits,
 *              inverted, in reverse bit order (0x37: 4; FF: off). Both at once
 *              brake the motor.
 */
class CModel2DirectDriveBoard : public CDriveBoard
{
public:
  CModel2DirectDriveBoard(const Util::Config::Node &config);

  Game::DriveBoardType GetType(void) const override { return Game::DRIVE_BOARD_WHEEL; }

  void SaveState(CBlockFile *SaveState) override;
  void LoadState(CBlockFile *SaveState) override;
  void Reset(void) override;
  void RunFrame(void) override;

  UINT8 IORead8(UINT32 portNum) override;
  void IOWrite8(UINT32 portNum, UINT8 data) override;

protected:
  void Disable(void) override;

private:
  static int Power(UINT8 port);

  void SendForce(INT8 force);
  void StopForce(void);

  UINT8 m_left = 0xFF, m_right = 0xFF;    // ports 2E and 2A
  UINT8 m_adcValue = 0x80;
  UINT8 m_adcBit = 0;
  INT8  m_lastForce = 0;
};

#endif  // INCLUDED_MODEL2DRIVEBOARD_H
