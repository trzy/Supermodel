/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2024 The Supermodel Team
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
 * JTAG.cpp
 * 
 * Model 3's JTAG test access port (TAP). This is accessed through the system
 * register space and is connected to the Real3D chipset and possibly other
 * devices. Hence, it is emulated as an independent module.
 *
 * It is unclear which exact JTAG standard the device conforms to (and it 
 * probably doesn't matter), so we assume IEEE 1149.1-1990 here.
 * 
 * All of the video board ASICs and the 3D-RAM chips are JTAG-compatible; their
 * JTAG lines are daisy chained together. Usually only one device is accessed at
 * a time (the other devices are given the BYPASS instruction) but some
 * procedures such as reading ID codes and performing boundary scan tests call
 * for accessing most or all of the devices at once.
 * 
 * 3D-RAM chips are only used for boundary scan tests (currently unemulated).
 */

#include "JTAG.h"

#include "Supermodel.h"
#include "Real3D.h"

bool CJTAGDevice::Shift(bool tdi)
{
    bool tdo = m_shiftReg[0];
    m_shiftReg >>= 1;
    m_shiftReg[m_shiftRegSize - 1] = tdi;
    return tdo;
}

bool CJTAGDevice::ReadTDO()
{
    return m_shiftReg[0];
}

void CJTAGDevice::SaveShiftRegister(CBlockFile* SaveState)
{
    SaveState->Write(m_shiftReg.to_string());
}

void CJTAGDevice::LoadShiftRegister(CBlockFile* SaveState)
{
    char str[MAX_REGISTER_LENGTH + 1];      // add one char for null terminator
    SaveState->Read(str, sizeof(str));
    std::bitset<MAX_REGISTER_LENGTH> tempReg{ str };
    m_shiftReg = tempReg;
}

void CASIC::SaveStateToBlock(CBlockFile* SaveState)
{
    SaveShiftRegister(SaveState);
    SaveState->Write(&m_shiftRegSize, sizeof(m_shiftRegSize));
    SaveState->Write(&m_instructionReg, sizeof(m_instructionReg));
    SaveState->Write(&m_idCode, sizeof(m_idCode));
    SaveState->Write(&m_modeword, sizeof(m_modeword));
}

void CASIC::LoadStateFromBlock(CBlockFile* SaveState)
{
    LoadShiftRegister(SaveState);
    SaveState->Read(&m_shiftRegSize, sizeof(m_shiftRegSize));
    SaveState->Read(&m_instructionReg, sizeof(m_instructionReg));
    SaveState->Read(&m_idCode, sizeof(m_idCode));
    SaveState->Read(&m_modeword, sizeof(m_modeword));
}

void CASIC::CaptureDR()
{
    switch (m_instructionReg)
    {
    case 3:                     // IDCODE
        m_shiftRegSize = 32;
        m_shiftReg = m_idCode;
        break;
    case 8:                     // MODEWORD
        m_shiftRegSize = 32;
        m_shiftReg = m_modeword;
        break;
    default:                    // BYPASS
        m_shiftRegSize = 1;
        m_shiftReg = 0;
        break;
    }
}

void CASIC::CaptureIR()
{
    m_shiftRegSize = 5;
    m_shiftReg = 1;
}

void CASIC::UpdateDR()
{
    switch (m_instructionReg)
    {
    case 8:                     // MODEWORD
        m_modeword = m_shiftReg.to_ulong();
        m_real3D.WriteJTAGModeword(m_deviceName, m_modeword);
        break;
    default:
        break;
    }
}

void CASIC::UpdateIR()
{
    m_instructionReg = (uint8_t)m_shiftReg.to_ulong();
}

void CASIC::Reset()
{
    m_shiftRegSize = 32;
    m_shiftReg = m_idCode;
    m_instructionReg = 3;   // IDCODE
}

CASIC::CASIC(CReal3D& real3D, CASIC::Name deviceName) :
    m_real3D(real3D),
    m_deviceName(deviceName)
{

}

void C3DRAM::SaveStateToBlock(CBlockFile* SaveState)
{
    SaveShiftRegister(SaveState);
    SaveState->Write(&m_shiftRegSize, sizeof(m_shiftRegSize));
    SaveState->Write(&m_instructionReg, sizeof(m_instructionReg));
}

void C3DRAM::LoadStateFromBlock(CBlockFile* SaveState)
{
    LoadShiftRegister(SaveState);
    SaveState->Read(&m_shiftRegSize, sizeof(m_shiftRegSize));
    SaveState->Read(&m_instructionReg, sizeof(m_instructionReg));
}

void C3DRAM::CaptureDR()
{
    switch (m_instructionReg)
    {
    default:                // BYPASS
        m_shiftRegSize = 1;
        m_shiftReg = 0;
        break;
    }
}

void C3DRAM::CaptureIR()
{
    m_shiftRegSize = 4;
    m_shiftReg = 0b1001;
}

void C3DRAM::UpdateDR()
{
    switch (m_instructionReg)
    {
        // no instructions implemented yet
    }
}

void C3DRAM::UpdateIR()
{
    m_instructionReg = (uint8_t)m_shiftReg.to_ulong();
}

void C3DRAM::Reset()
{
    m_shiftRegSize = 1;
    m_shiftReg = 0;
    m_instructionReg = 15;  // BYPASS
}

// Finite state machine. Each state has two possible next states.
const CJTAGScanChain::State CJTAGScanChain::s_fsm[][2] =
{
    // tms = 0				tms = 1
    { State::RunTestIdle,	State::TestLogicReset },	// 0  Test-Logic/Reset
    { State::RunTestIdle,	State::SelectDRScan },		// 1  Run-Test/Idle
    { State::CaptureDR,		State::SelectIRScan },		// 2  Select-DR-Scan
    { State::ShiftDR,		State::Exit1DR },			// 3  Capture-DR
    { State::ShiftDR,		State::Exit1DR },			// 4  Shift-DR
    { State::PauseDR,		State::UpdateDR },			// 5  Exit1-DR
    { State::PauseDR,		State::Exit2DR },			// 6  Pause-DR
    { State::ShiftDR,		State::UpdateDR },			// 7  Exit2-DR
    { State::RunTestIdle,	State::SelectDRScan },		// 8  Update-DR
    { State::CaptureIR,		State::TestLogicReset },	// 9  Select-IR-Scan
    { State::ShiftIR,		State::Exit1IR },			// 10 Capture-IR
    { State::ShiftIR,		State::Exit1IR },			// 11 Shift-IR
    { State::PauseIR,		State::UpdateIR },			// 12 Exit1-IR
    { State::PauseIR,		State::Exit2IR },			// 13 Pause-IR
    { State::ShiftIR,		State::UpdateIR },			// 14 Exit2-IR
    { State::RunTestIdle,	State::SelectDRScan }		// 15 Update-IR
};

bool CJTAGScanChain::Clock(bool tms, bool tdi)
{
    // TDO changes on the falling edge of TCK and is sampled by the master on
    // the rising edge, so the value returned is the one presented after the
    // previous cycle
    bool tdo = m_tdo;
    RisingEdge(tms, tdi);
    FallingEdge();
    return tdo;
}

void CJTAGScanChain::RisingEdge(bool tms, bool tdi)
{
    switch (m_state)
    {
    case State::TestLogicReset:
        for (auto device : m_devices)
            device->Reset();
        break;
    case State::CaptureDR:
        for (auto device : m_devices)
            device->CaptureDR();
        break;
    case State::CaptureIR:
        for (auto device : m_devices)
            device->CaptureIR();
        break;
    case State::ShiftDR:
    case State::ShiftIR:
        for (auto device : m_devices)
            tdi = device->Shift(tdi);
        break;
    default:
        break;
    }

    // Go to next state
    m_state = s_fsm[static_cast<int>(m_state)][tms];
}

void CJTAGScanChain::FallingEdge()
{
    switch (m_state)
    {
    case State::ShiftDR:
    case State::ShiftIR:
        if (!m_devices.empty())
            m_tdo = m_devices.back()->ReadTDO();
        break;
    case State::UpdateDR:
        for (auto device : m_devices)
            device->UpdateDR();
        break;
    case State::UpdateIR:
        for (auto device : m_devices)
            device->UpdateIR();
        break;
    default:
        break;
    }
}

bool CJTAGScanChain::TDO() const
{
    return m_tdo;
}

void CJTAGScanChain::ResetDevices()
{
    for (auto device : m_devices)
        device->Reset();
}

void CJTAGScanChain::Reset()
{
    m_state = State::TestLogicReset;
    ResetDevices();
}

CJTAGScanChain::State CJTAGScanChain::GetState() const
{
    return m_state;
}

void CJTAGScanChain::SetState(State state, bool tdo)
{
    m_state = state;
    m_tdo = tdo;
}

CJTAGScanChain::CJTAGScanChain(std::vector<CJTAGDevice*> devices)
  : m_devices(std::move(devices))
{
}

void CJTAG::SaveState(CBlockFile* SaveState)
{
    CJTAGScanChain::State state = m_scanChain.GetState();
    bool tdo = m_scanChain.TDO();
    SaveState->NewBlock("JTAG2", __FILE__);
    SaveState->Write(&state, sizeof(state));
    SaveState->Write(&m_lastTck, sizeof(m_lastTck));
    SaveState->Write(&tdo, sizeof(tdo));
    SaveState->Write(&m_numDevices, sizeof(m_numDevices));
    for (auto& device : m_device)
    {
        device->SaveStateToBlock(SaveState);
    }
}

void CJTAG::LoadState(CBlockFile* LoadState)
{
    if (Result::OKAY != LoadState->FindBlock("JTAG2"))
    {
        ErrorLog("Unable to load JTAG state. Save state file is corrupt.");
        return;
    }
    CJTAGScanChain::State state;
    bool tdo;
    LoadState->Read(&state, sizeof(state));
    LoadState->Read(&m_lastTck, sizeof(m_lastTck));
    LoadState->Read(&tdo, sizeof(tdo));
    LoadState->Read(&m_numDevices, sizeof(m_numDevices));
    for (auto& device : m_device)
    {
        device->LoadStateFromBlock(LoadState);
    }
    BuildScanChain();
    m_scanChain.SetState(state, tdo);
}

bool CJTAG::Read() const
{
    return m_scanChain.TDO();
}

void CJTAG::Write(bool tck, bool tms, bool tdi, bool trst)
{
    if (!trst)
    {
        m_scanChain.ResetDevices();
        return;
    }

    if (m_lastTck == tck)
        return;
    m_lastTck = tck;

    if (tck)
        m_scanChain.RisingEdge(tms, tdi);
    else
        m_scanChain.FallingEdge();
}

void CJTAG::Reset()
{
    m_scanChain.Reset();
    for (auto& device : m_device)   // includes devices not on this stepping's chain
        device->Reset();
    DebugLog("JTAG reset\n");
}

void CJTAG::BuildScanChain()
{
    m_scanChain = CJTAGScanChain(std::vector<CJTAGDevice*>(m_device, m_device + m_numDevices));
}

void CJTAG::SetStepping(int stepping)
{
    switch (stepping)
    {
    case 0x21:
        m_mercury.SetIDCode(0x416c3057);
        m_venus.SetIDCode(0x316c4057);
        m_earth0.SetIDCode(0x516c5057);
        m_earth1.SetIDCode(0x516c5057);
        m_mars00.SetIDCode(0x316c6057);
        m_mars01.SetIDCode(0x316c6057);
        m_mars10.SetIDCode(0x316c6057);
        m_mars11.SetIDCode(0x316c6057);
        m_jupiter.SetIDCode(0x516c7057);
        m_numDevices = 17;
        break;
    case 0x20:
        m_mercury.SetIDCode(0x416c3057);
        m_venus.SetIDCode(0x316c4057);
        m_earth0.SetIDCode(0x416c5057);
        m_earth1.SetIDCode(0x416c5057);
        m_mars00.SetIDCode(0x316c6057);
        m_mars01.SetIDCode(0x316c6057);
        m_mars10.SetIDCode(0x316c6057);
        m_mars11.SetIDCode(0x316c6057);
        m_jupiter.SetIDCode(0x416c7057);
        m_numDevices = 17;
        break;
    case 0x15:
        m_mercury.SetIDCode(0x316c3057);
        m_venus.SetIDCode(0x216c4057);
        m_earth0.SetIDCode(0x316c5057);
        m_mars00.SetIDCode(0x216c6057);
        m_mars01.SetIDCode(0x216c6057);
        m_jupiter.SetIDCode(0x316c7057);
        m_numDevices = 10;
        break;
    case 0x10:
    default:
        m_mercury.SetIDCode(0x216c3057);
        m_venus.SetIDCode(0x116c4057);
        m_earth0.SetIDCode(0x216c5057);
        m_mars00.SetIDCode(0x116c6057);
        m_mars01.SetIDCode(0x116c6057);
        m_jupiter.SetIDCode(0x116c7057);
        m_numDevices = 10;
        break;
    }
    BuildScanChain();
}

// Only the first ASIC of each type has its modeword written to the Real3D object
CJTAG::CJTAG(CReal3D& real3D) :
    m_mercury(real3D, CASIC::Name::Mercury),
    m_venus(real3D, CASIC::Name::Venus),
    m_earth0(real3D, CASIC::Name::Earth),
    m_earth1(real3D, CASIC::Name::Dummy),
    m_mars00(real3D, CASIC::Name::Mars),
    m_mars01(real3D, CASIC::Name::Dummy),
    m_mars10(real3D, CASIC::Name::Dummy),
    m_mars11(real3D, CASIC::Name::Dummy),
    m_jupiter(real3D, CASIC::Name::Jupiter)
{
    DebugLog("Built JTAG logic\n");
}
