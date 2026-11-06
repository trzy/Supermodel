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

#pragma once

#include "Types.h"
#include "BlockFile.h"

class Z80CTC
{
public:

    void    Write(UINT32 channel, UINT8 value);
    UINT32  CalcFrequency(UINT32 channel, UINT32 inputFrequency);
    bool    TimerRunning(UINT32 channel) const;
    void    Reset();
    void    SaveState(CBlockFile* saveState) const;
    void    LoadState(CBlockFile* saveState);

private:

    struct Channel
    {
        UINT8   control                 = 0;        // last control word
        UINT16  timeConstant            = 0;        // reload value
        UINT16  counter                 = 0;        // current down-counter
        bool    interruptEnabled        = false;
        bool    counterMode             = false;
        bool    prescaler256            = false;
        bool    triggerRising           = false;
        bool    manualTrigger           = false;
        bool    waitingForTimeConstant  = false;
        bool    running                 = false;
    };

    Channel m_ch[4];
};