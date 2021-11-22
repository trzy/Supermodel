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
 * DriveBoard.cpp
 *
 * Implementation of the CDriveBoard class: drive board (force feedback)
 * emulation.
 *
 * NOTE: Simulation does not yet work. Drive board ROMs are required.
 *
 * State Management:
 * -----------------
 * - IsAttached: This indicates whether the drive board is present for the game
 *   and should be emulated, if possible. It is determined by whether a ROM was
 *   passed to the initializer. Entirely simulated implementations of the drive
 *   board should still take a ROM, even if it contains no data. The attached
 *   should be set only *once* at initialization and preferably by the base
 *   CDriveBoard class. Do not change this flag during run-time!
 * - Disabled: This state indicates emulation should not proceed. Force
 *   feedback must be completely halted. This flag is used only to disable the
 *   board due to run-time errors and must *not* be re-enabled. It must not be
 *   used to "temporarily" disable the board. Only the Reset() method may
 *   enable emulation and then only if the board is attached. A valid reason
 *   for disabling the board during run-time is e.g., if a loaded save state is
 *   incompatible (wrong format or because it was saved while the board was
 *   disabled, rendering its state data invalid).
 * - IsDisabled: This method is used internally only and should be used to test
 *   whether emulation should occur. It is the combination of attachment and
 *   enabled state.
 * - Disable: Use this to disable the board. Drive board implementations should
 *   override this to send stop commands to force feedback motors and then call
 *   CDriveBoard::Disable() to update the disabled flag.
 */

#include "DriveBoard.h"

#include "Supermodel.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

#define ROM_SIZE 0x9000
#define RAM_SIZE 0x2000 // Z80 RAM

static_assert(sizeof(bool) == 1, "bools must be copied to uint8_t"); // Save state code relies on this -- we must fix this so that bools are copied to uint8_t explicitly

void CDriveBoard::SaveState(CBlockFile* SaveState)
{
    SaveState->NewBlock("DriveBoard.2", __FILE__);

    // Check board is attached and enabled
    bool enabled = !IsDisabled();
    SaveState->Write(&enabled, sizeof(enabled));
    if (enabled)
    {
        // Check if simulated
        SaveState->Write(&m_simulated, sizeof(m_simulated));
        if (m_simulated)
        {
            // TODO - save board simulation state
        }
        else
        {
            // Save RAM state
            SaveState->Write(m_ram, RAM_SIZE);

            // Save interrupt and input/output state
            SaveState->Write(&m_initialized, sizeof(m_initialized));
            SaveState->Write(&m_allowInterrupts, sizeof(m_allowInterrupts));
            SaveState->Write(&m_dataSent, sizeof(m_dataSent));
            SaveState->Write(&m_dataReceived, sizeof(m_dataReceived));

            // Save CPU state
            m_z80.SaveState(SaveState, "DriveBoard Z80");
        }
    }
}

void CDriveBoard::LoadState(CBlockFile* SaveState)
{
    if (SaveState->FindBlock("DriveBoard.2") != OKAY)
    {
        ErrorLog("Unable to load base drive board state. Save state file is corrupt.");
        Disable();
        return;
    }

    // Check that board was enabled in saved state
    bool isEnabled = !IsDisabled();
    bool wasEnabled = false;
    bool wasSimulated = false;
    SaveState->Read(&wasEnabled, sizeof(wasEnabled));
    if (wasEnabled)
    {
        // Simulated?
        SaveState->Read(&wasSimulated, sizeof(wasSimulated));
        if (wasSimulated)
        {
            // Derived classes may have simulation state (e.g., SkiBoard)
        }
        else
        {
            // Load RAM state
            SaveState->Read(m_ram, RAM_SIZE);

            // Load interrupt and input/output state
            SaveState->Read(&m_initialized, sizeof(m_initialized));
            SaveState->Read(&m_allowInterrupts, sizeof(m_allowInterrupts));
            SaveState->Read(&m_dataSent, sizeof(m_dataSent));
            SaveState->Read(&m_dataReceived, sizeof(m_dataReceived));

            // Load CPU state
            // TODO: we should have a way to check whether this succeeds... make CZ80::LoadState() return a bool
            m_z80.LoadState(SaveState, "DriveBoard Z80");
        }
    }

    // If the board was not in the same activity and simulation state when the
    // save file was generated, we cannot safely resume and must disable it
    if (wasEnabled != isEnabled || wasSimulated != m_simulated)
    {
        Disable();
        ErrorLog("Halting drive board emulation due to mismatch in active and restored states.");
    }
}

void CDriveBoard::LoadLegacyState(const LegacyDriveBoardState &state, CBlockFile *SaveState)
{
  static_assert(RAM_SIZE == sizeof(state.ram),"Ram sizes must match");
  memcpy(m_ram, state.ram, RAM_SIZE);
  m_initialized = state.initialized;
  m_allowInterrupts = state.allowInterrupts;
  m_dataSent = state.dataSent;
  m_dataReceived = state.dataReceived;
  m_z80.LoadState(SaveState, "DriveBoard Z80");
}

Game::DriveBoardType CDriveBoard::GetType(void)
{
  return Game::DRIVE_BOARD_NONE;
}

bool CDriveBoard::IsAttached(void)
{
  return m_attached;
}

bool CDriveBoard::IsSimulated(void)
{
  return m_simulated;
}

bool CDriveBoard::IsDisabled(void)
{
  bool enabled = !m_disabled;
  return !(m_attached && enabled);
}

void CDriveBoard::Disable(void)
{
  m_disabled = true;
}

void CDriveBoard::GetDIPSwitches(UINT8& dip1, UINT8& dip2)
{
  dip1 = m_dip1;
  dip2 = m_dip2;
}

void CDriveBoard::GetDIPSwitches(UINT8& dip1)
{
  dip1 = m_dip1;
}

void CDriveBoard::SetDIPSwitches(UINT8 dip1, UINT8 dip2)
{
  m_dip1 = dip1;
  m_dip2 = dip2;
}

void CDriveBoard::SetDIPSwitches(UINT8 dip1)
{
  m_dip1 = dip1;
}

unsigned CDriveBoard::GetForceFeedbackStrength()
{
  return ((~(m_dip1 >> 2)) & 7) + 1;
}

void CDriveBoard::SetForceFeedbackStrength(unsigned strength)
{
  m_dip1 = (m_dip1 & 0xE3) | (((~(strength - 1)) & 7) << 2);
}

CZ80* CDriveBoard::GetZ80(void)
{
  return &m_z80;
}

void CDriveBoard::AttachInputs(CInputs* inputs, unsigned gameInputFlags)
{
  m_inputs = inputs;
  m_inputFlags = gameInputFlags;

  DebugLog("DriveBoard attached inputs\n");
}

void CDriveBoard::AttachOutputs(COutputs* outputs)
{
  m_outputs = outputs;

  DebugLog("DriveBoard attached outputs\n");
}


UINT8 CDriveBoard::Read(void)
{
  if (IsDisabled())
  {
    return 0xFF;
  }
  return m_dataReceived;
}

void CDriveBoard::Write(UINT8 data)
{
  m_dataSent = data;
}

UINT8 CDriveBoard::Read8(UINT32 addr)
{
  // TODO - shouldn't end of ROM be 0x7FFF not 0x8FFF?
  if (addr < ROM_SIZE)      // ROM is 0x0000-0x8FFF
    return m_rom[addr];
  else if (addr >= 0xE000)  // RAM is 0xE000-0xFFFF
    return m_ram[(addr - 0xE000) & 0x1FFF];
  else
  {
    //DebugLog("Unhandled Z80 read of %08X (at PC = %04X)\n", addr, m_z80.GetPC());
    return 0xFF;
  }
}

void CDriveBoard::Write8(UINT32 addr, UINT8 data)
{
  if (addr >= 0xE000)  // RAM is 0xE000-0xFFFF
    m_ram[(addr - 0xE000) & 0x1FFF] = data;
#ifdef DEBUG
  else
    DebugLog("Unhandled Z80 write to %08X (at PC = %04X)\n", addr, m_z80.GetPC());
#endif
}

UINT8 CDriveBoard::IORead8(UINT32 portNum)
{
  return 0xff;
}

void CDriveBoard::IOWrite8(UINT32 portNum, UINT8 data)
{
}

void CDriveBoard::RunFrame(void)
{
  if (IsDisabled())
  {
    return;
  }

  // Assuming Z80 runs @ 4.0MHz and NMI triggers @ 60.0KHz for WheelBoard and JoystickBoard
  // Assuming Z80 runs @ 8.0MHz and INT triggers @ 60.0KHz for BillBoard
  // TODO - find out if Z80 frequency is correct and exact frequency of NMI interrupts (just guesswork at the moment!)
  int cycles = (int)(m_z80Clock * 1000000 / 60);
  int loopCycles = 10000;
  while (cycles > 0)
  {
    if (m_allowInterrupts)
    {
      if(m_z80NMI)
        m_z80.TriggerNMI();
      else
        m_z80.SetINT(true);
    }
    cycles -= m_z80.Run(std::min<int>(loopCycles, cycles));
  }
}

bool CDriveBoard::Init(const UINT8* romPtr)
{
    // This constructor must present a valid ROM
    if (!romPtr)
    {
      return ErrorLog("Internal error: no drive board ROM supplied.");
    }

    // Assign ROM (note that the ROM data has not yet been loaded)
    m_rom = romPtr;

    // Allocate memory for RAM
    m_ram = new (std::nothrow) UINT8[RAM_SIZE];
    if (NULL == m_ram)
    {
        float ramSizeMB = (float)RAM_SIZE / (float)0x100000;
        return ErrorLog("Insufficient memory for drive board (needs %1.1f MB).", ramSizeMB);
    }
    memset(m_ram, 0, RAM_SIZE);

    // Initialize Z80
    m_z80.Init(this, NULL);

    // We are attached
    m_attached = true;

    return OKAY;
}

// Dummy drive board (not attached)
bool CDriveBoard::Init(void)
{
  // Use an empty dummy ROM otherwise so debugger doesn't crash if it
  // tries to read our memory or if we accidentally run the Z80 (which
  // should never happen in a detached board state).
  if (!m_dummyROM)
  {
    uint8_t *rom = new (std::nothrow) uint8_t[ROM_SIZE];
    if (NULL == rom)
    {
      return ErrorLog("Insufficient memory for drive board.");
    }
    memset(rom, 0xFF, ROM_SIZE);
    m_dummyROM = rom;
  }
  bool result = Init(m_dummyROM);
  m_attached = false; // force detached
  return result;
}

void CDriveBoard::Reset()
{
    m_initialized = false;
    m_initState = 0;
    m_readMode = 0;
    m_boardMode = 0;
    m_wheelCenter = 0x80;
    m_allowInterrupts = false;
    m_dataSent = 0;
    m_dataReceived = 0;
    m_z80.Reset();        // always reset to provide a valid Z80 state

    // Configure options (cannot be done in Init() because command line settings weren't yet parsed)
    SetForceFeedbackStrength(m_config["ForceFeedbackStrength"].ValueAsDefault<unsigned>(5));

    // Enable only if attached -- **this is the only place this flag should ever be cleared**
    if (m_attached)
      m_disabled = false;
}


CDriveBoard::CDriveBoard(const Util::Config::Node& config)
  : m_config(config),
    m_attached(false),
    m_disabled(true), // begin in disabled state -- can be enabled only 1) if attached and 2) by successful reset
    m_simulated(false),
    m_initialized(false),
    m_allowInterrupts(false),
    m_dataSent(0),
    m_dataReceived(0),
    m_dip1(0x00),
    m_dip2(0x00),
    m_initState(0),
    m_statusFlags(0),
    m_boardMode(0),
    m_readMode(0),
    m_wheelCenter(0),
    m_cockpitCenter(0),
    m_echoVal(0),
    m_rom(NULL),
    m_ram(NULL),
    m_dummyROM(NULL),
    m_z80Clock(4.0),
    m_z80NMI(true),
    m_inputs(NULL),
    m_inputFlags(0),
    m_outputs(NULL)
{
  DebugLog("Built Drive Board\n");
}

CDriveBoard::~CDriveBoard(void)
{
  if (m_ram != NULL)
  {
    delete[] m_ram;
    m_ram = NULL;
  }
  if (m_dummyROM != NULL)
  {
    delete [] m_dummyROM;
    m_dummyROM = NULL;
  }
  m_rom = NULL;
  m_inputs = NULL;
  m_outputs = NULL;

  DebugLog("Destroyed Drive Board\n");
}
