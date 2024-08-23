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
 * JTAG.h
 * 
 * Header file defining the CJTAG class: the Model 3's JTAG device.
 */

#ifndef INCLUDED_JTAG_H
#define INCLUDED_JTAG_H

#include "BlockFile.h"
#include <bitset>

class CReal3D;

class CJTAGDevice
{
public:
    virtual void SaveStateToBlock(CBlockFile* SaveState) = 0;
    virtual void LoadStateFromBlock(CBlockFile* SaveState) = 0;
    virtual void CaptureDR() = 0;
    virtual void CaptureIR() = 0;
    virtual void UpdateDR() = 0;
    virtual void UpdateIR() = 0;
    virtual void Reset() = 0;
    bool Shift(bool tdi);
    bool ReadTDO();

protected:
    void SaveShiftRegister(CBlockFile* SaveState);
    void LoadShiftRegister(CBlockFile* SaveState);

    static constexpr auto MAX_REGISTER_LENGTH = 262;
    std::bitset<MAX_REGISTER_LENGTH> m_shiftReg;
    uint32_t m_shiftRegSize;
    uint8_t m_instructionReg;
};

class CASIC : public CJTAGDevice
{
public:
    enum class Name
    {
        Mercury = 0,
        Venus   = 1,
        Earth   = 2,
        Mars    = 3,
        Jupiter = 4,
        Dummy   = -1
    };

    void SaveStateToBlock(CBlockFile* SaveState);
    void LoadStateFromBlock(CBlockFile* SaveState);
    void CaptureDR();
    void CaptureIR();
    void UpdateDR();
    void UpdateIR();
    void Reset();
    void SetIDCode(uint32_t idCode) { m_idCode = idCode; }
    CASIC(CReal3D& real3D, Name deviceName);

private:
    uint32_t m_idCode = 0;
    uint32_t m_modeword = 0;
    CReal3D& m_real3D;
    Name m_deviceName = Name::Dummy;
};

class C3DRAM : public CJTAGDevice
{
public:
    void SaveStateToBlock(CBlockFile* SaveState);
    void LoadStateFromBlock(CBlockFile* SaveState);
    void CaptureDR();
    void CaptureIR();
    void UpdateDR();
    void UpdateIR();
    void Reset();
};

class CJTAG
{
public:
    void SaveState(CBlockFile* SaveState);
    void LoadState(CBlockFile* LoadState);
    bool Read();
    void Write(bool tck, bool tms, bool tdi, bool trst);
    void Reset();
    void SetStepping(int stepping);
    CJTAG(CReal3D& real3D);

private:
    enum class State
    {
        TestLogicReset, // 0
        RunTestIdle,    // 1
        SelectDRScan,   // 2
        CaptureDR,      // 3
        ShiftDR,        // 4
        Exit1DR,        // 5
        PauseDR,        // 6
        Exit2DR,        // 7
        UpdateDR,       // 8
        SelectIRScan,   // 9
        CaptureIR,      // 10
        ShiftIR,        // 11
        Exit1IR,        // 12
        PauseIR,        // 13
        Exit2IR,        // 14
        UpdateIR        // 15
    };

    static const State s_fsm[][2];

    State m_state = State::TestLogicReset;
    bool m_lastTck = false;
    bool m_tdo = false;

    CASIC m_mercury;
    CASIC m_venus;
    CASIC m_earth0, m_earth1;
    CASIC m_mars00, m_mars01, m_mars10, m_mars11;
    CASIC m_jupiter;
    C3DRAM m_3dram[8];

    // Order of JTAG devices on step 2.x video boards
    // On step 1.x we only use the first ten devices
    CJTAGDevice* m_device[17] =
    {
        &m_jupiter,
        &m_mercury,
        &m_venus,
        &m_earth0,
        &m_3dram[0],
        &m_mars00,
        &m_mars01,
        &m_3dram[1],
        &m_3dram[2],
        &m_3dram[3],
        &m_earth1,
        &m_3dram[4],
        &m_mars10,
        &m_mars11,
        &m_3dram[5],
        &m_3dram[6],
        &m_3dram[7],
    };
    uint32_t m_numDevices = 0;
};

#endif  // INCLUDED_JTAG_H
