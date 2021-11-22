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
 * DriveBoard.h
 *
 * Header for the CDriveBoard (force feedback emulation) class.
 * Abstract base class defining the common interface for wheel, joystick, ski, bill board types.
 */

#ifndef INCLUDED_DRIVEBOARD_H
#define INCLUDED_DRIVEBOARD_H

#include "CPU/Bus.h"
#include "CPU/Z80/Z80.h"
#include "Inputs/Inputs.h"
#include "OSD/Outputs.h"
#include "Util/NewConfig.h"
#include "Game.h"

/*
 * CDriveBoard
 */
class CDriveBoard : public IBus
{
public:
  /*
   * GetType(void):
   *
   * Returns:
   *    Drive board type.
   */
  virtual Game::DriveBoardType GetType(void);

  /*
   * IsAttached(void):
   *
   * Returns:
   *    True if the drive board is "attached" and should be emulated,
   *    otherwise false.
   */
  virtual bool IsAttached(void);

  /*
   * IsSimulated(void):
   *
   * Returns:
   *    True if the drive board is being simulated rather than actually
   *    emulated, otherwise false.
   */
  virtual bool IsSimulated(void);

  /*
   * GetDIPSwitches(dip1, dip2):
   *
   * Reads the two sets of DIP switches on the drive board.
   *
   * Parameters:
   *    dip1  Reference of variable to store DIP switch 1 to.
   *    dip2  DIP switch 2.
   */
  virtual void GetDIPSwitches(UINT8 &dip1, UINT8 &dip2);
  virtual void GetDIPSwitches(UINT8& dip1);

  /*
   * SetDIPSwitches(dip1, dip2):
   *
   * Sets the DIP switches.
   *
   * Parameters:
   *    dip1  DIP switch 1 value.
   *    dip2  DIP switch 2 value.
   */
  virtual void SetDIPSwitches(UINT8 dip1, UINT8 dip2);
  virtual void SetDIPSwitches(UINT8 dip1);

  /*
   * GetForceFeedbackStrength(void):
   *
   * Returns:
   *    Strength of the force feedback based on drive board DIP switches (1-8).
   */
  virtual unsigned GetForceFeedbackStrength(void);

  /*
   * SetForceFeedbackStrength(strength):
   *
   * Sets the force feedback strength (modifies the DIP switch setting).
   *
   * Parameters:
   *    strength  A value ranging from 1 to 8.
   */
  virtual void SetForceFeedbackStrength(unsigned strength);


  /*
   * GetZ80(void):
   *
   * Returns:
   *    The Z80 object.
   */
  virtual CZ80 *GetZ80(void);

  /*
   * SaveState(SaveState):
   *
   * Saves the drive board state.
   *
   * Parameters:
   *    SaveState Block file to save state information to.
   */
  virtual void SaveState(CBlockFile *SaveState);

  /*
   * LoadState(SaveState):
   *
   * Restores the drive board state.
   *
   * Parameters:
   *    SaveState Block file to load save state information from.
   */
  virtual void LoadState(CBlockFile *SaveState);

  /*
   * Init(romPtr):
   *
   * Initializes (and "attaches") the drive board. This should be called
   * before other members.
   *
   * Parameters:
   *    romPtr    Pointer to the drive board ROM (Z80 program).
   *
   * Returns:
   *    FAIL if the drive board could not be initialized (prints own error
   *    message), otherwise OKAY.
   */
  virtual bool Init(const UINT8 *romPtr);

  /*
   * Init(void):
   *
   * Initializes a dummy drive board in a detached state. This should be called
   * before other members. This initializer is provided in case a CDriveBoard
   * object or pointer is needed but no drive board actually exists.
   */
  bool Init(void);

  /*
   * AttachInputs(InputsPtr, gameInputFlags):
   *
   * Attaches inputs to the drive board (for access to the steering wheel
   * position).
   *
   * Parameters:
   *    inputs      Pointer to the input object.
   *    gameInputFlags  The current game's input flags.
   */
  virtual void AttachInputs(CInputs *inputs, unsigned gameInputFlags);

  virtual void AttachOutputs(COutputs *outputs);

  /*
   * Reset(void):
   *
   * Resets the drive board.
   */
  virtual void Reset(void);

  /*
   * Read():
   *
   * Reads data from the drive board.
   *
   * Returns:
   *    Data read.
   */
  virtual UINT8 Read(void);

  /*
   * Write(data):
   *
   * Writes data to the drive board.
   *
   * Parameters:
   *    data  Data to send.
   */
  virtual void Write(UINT8 data);

  /*
   * RunFrame(void):
   *
   * Emulates a single frame's worth of time on the drive board.
   */
  virtual void RunFrame(void);

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
  CDriveBoard(const Util::Config::Node& config);
  virtual ~CDriveBoard(void);

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
  virtual UINT8 Read8(UINT32 addr);
  virtual UINT8 IORead8(UINT32 portNum);

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
  virtual void Write8(UINT32 addr, UINT8 data);
  virtual void IOWrite8(UINT32 portNum, UINT8 data);


protected:

  struct LegacyDriveBoardState
  {
    uint8_t   dip1;
    uint8_t   dip2;
    uint8_t   ram[0x2000];
    uint8_t   initialized;
    uint8_t   allowInterrupts;
    uint8_t   dataSent;
    uint8_t   dataReceived;
    uint16_t  adcPortRead;
    uint8_t   adcPortBit;
    uint8_t   uncenterVal1;
    uint8_t   uncenterVal2;
  };

  // Disable the drive board (without affecting attachment state). Used internally only to disable emulation.
  virtual void Disable(void);

  // Whether disabled and/or not attached -- used to determine whether to carry out emulation
  bool IsDisabled(void);

  // Attempt to load drive board data from old save states (prior to drive board refactor)
  void LoadLegacyState(const LegacyDriveBoardState &state, CBlockFile *SaveState);


    const Util::Config::Node& m_config;

    bool m_attached;    // True if drive board is attached
    bool m_disabled;    // True if emulation is internally disabled (e.g., by loading an incompatible save state). Can only be enabled once and if attached.
    bool m_simulated;   // True if drive board should be simulated rather than emulated

    // Emulation state
    bool m_initialized;     // True if drive board has finished initialization
    bool m_allowInterrupts; // True if drive board has enabled NMI interrupts

    UINT8 m_dataSent;       // Last command sent by main board
    UINT8 m_dataReceived;   // Data to send back to main board

    UINT8 m_dip1;           // Value of DIP switch 1
    UINT8 m_dip2;           // Value of DIP switch 2

    // Simulation state
    UINT8 m_initState;
    UINT8 m_statusFlags;
    UINT8 m_boardMode;
    UINT8 m_readMode;
    UINT8 m_wheelCenter;
    UINT8 m_cockpitCenter;
    UINT8 m_echoVal;

    const UINT8* m_rom;     // 32k ROM
    UINT8* m_ram;           // 8k RAM

    const UINT8* m_dummyROM;

    CZ80 m_z80;             // Z80 CPU
    float m_z80Clock;       // Z80 clock frequency
    bool m_z80NMI;          // Non Masquable Interrupt or Interrupt

    CInputs* m_inputs;
    unsigned m_inputFlags;

    COutputs* m_outputs;
};

#endif  // INCLUDED_DRIVEBOARD_H
