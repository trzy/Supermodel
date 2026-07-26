#pragma once

#include "..\..\OSD\SDL\Types.h"
#include "..\..\BlockFile.h"

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
        UINT8   timeConstant            = 0;        // reload value
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