/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
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

#include "Z80CTC.h"

// info here http://www.z80.info/zip/z80ctc.pdf
// this isn't complete emulation, but hopefully enough for model3

/*

    x------- Interupt			                1 enables, 0 disables
    -x------ Mode 				                1 counter mode, 0 timer mode
    --x----- Prescaler value		            1 = 256, 0 = 16
    ---x---- Clock / trigger edge selection	    1 = rising edge, 0 falling edge
    ----x--- Timer trigger			            1 = clk / trg pulse starts timer, 0 = automatic trigger when time constant is loaded
    -----x-- Time constant			            1 = time constant follows, 0 = no time constant follows
    ------x- Reset				                1 = software reset, 0 = continued operation
    -------x Control or vector		            1 = control word, 0 = vector

*/

void Z80CTC::Write(UINT32 channel, UINT8 value)
{
    auto& chn = m_ch[channel];

    if (value & 0x01) {  // D0 = 1 control word
        chn.control                 = value;
        chn.interruptEnabled        = (value & 0x80);
        chn.counterMode             = (value & 0x40);
        chn.prescaler256            = (value & 0x20);
        chn.triggerRising           = (value & 0x10);
        chn.manualTrigger           = (value & 0x08);
        chn.waitingForTimeConstant  = (value & 0x04);

        bool softwareReset = (value & 0x02);

        if (softwareReset) {
            chn.counter = 0;
            chn.running = false;
        }
    }
    else {
        // D0 = vector or time constant
        if (chn.waitingForTimeConstant) {

            bool automaticTrigger = !chn.manualTrigger;

            chn.timeConstant            = value ? value : 256;      // in the spec is value = 0 it's assumed to be 256
            chn.counter                 = chn.timeConstant;
            chn.waitingForTimeConstant  = false;
            chn.running                 = automaticTrigger;
        }
    }
}

UINT32 Z80CTC::CalcFrequency(UINT32 channel, UINT32 inputFrequency)
{
    auto& chn = m_ch[channel];

    bool timerMode = !chn.counterMode;

    // in timer mode we divide by the system clock

    if (timerMode) {
        UINT32  prescaler   = chn.prescaler256 ? 256 : 16;
        auto    result      = inputFrequency / (chn.timeConstant * prescaler);
        return result;
    }
    else {
        // i guess this depends on how stuff is wired together, for but us input comes from previous channel
        if (chn.triggerRising) {
            auto result = CalcFrequency(channel - 1, inputFrequency);

            return result /= chn.counter;
        }
    }

    // if we got here we are unsupported as of now
    return 0;
}

bool Z80CTC::TimerRunning(UINT32 channel) const
{
    return m_ch[channel].running;
}

void Z80CTC::Reset()
{
    for (auto& c : m_ch) {
        c = Channel{};
    }
}

void Z80CTC::SaveState(CBlockFile* saveState) const
{
    saveState->Write(m_ch, sizeof(m_ch));
}

void Z80CTC::LoadState(CBlockFile * saveState)
{
    saveState->Read(m_ch, sizeof(m_ch));
}
