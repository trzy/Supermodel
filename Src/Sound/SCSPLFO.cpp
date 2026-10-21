/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson
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
 * SCSPLFO.cpp
 * 
 * SCSP low frequency oscillator emulation. Included directly into SCSP.cpp. Do
 * not compile this!
 */

// One LFO per slot, shared by the pitch (PLFO) and amplitude (ALFO) modulation, which decode its 8-bit
// counter differently (manual Fig. 4.52). The waveform shapes, depths and step timing follow Mednafen's
// SCSP (ss/scsp.inc). The LFO runs for every slot whether it is playing or not, and key on
// doesn't restart it; LFORE holds the counter at 0.
struct _LFO
{
	UINT8 counter;		// position in the waveform (256 steps per cycle)
	UINT16 timeCounter;	// samples until the next step
};

// Advance one sample. LFOF sets the samples per step: 0 (0.17 Hz) to 1Fh (172 Hz), as in manual Table 4.21.
static inline void LFO_Run(_LFO *lfo, unsigned lfof, bool lfore)
{
	if (--lfo->timeCounter == 0)
	{
		lfo->counter++;
		lfo->timeCounter = (UINT16)((((8 - (lfof & 3)) << 7) >> (lfof >> 2)) - 4);
	}
	if (lfore)
		lfo->counter = 0;
}

// The noise generator (17-bit LFSR). It supplies SSCTL = 1 slot data and the noise LFO waveform, so noise
// modulation doesn't depend on LFOF or LFORE (manual p.36). It is clocked once per slot.
static inline UINT32 LFO_NextNoise(UINT32 lfsr)
{
	return (lfsr >> 1) | ((((lfsr >> 5) ^ lfsr) & 1) << 16);
}

// Pitch LFO: an offset added to the 11-bit frequency number (400h | FNS), scaled with FNS so the depth
// in cents is about the same for every note. PLFOS 7 gives about +/-210 cents, each lower level half. That is
// Mednafen's depth; the manual's Table 4.24 gives twice as much (+/-7 to +/-494 cents).
static inline int PLFO_Get(const _LFO *lfo, unsigned ws, unsigned level, unsigned fns, UINT32 lfsr)
{
	if (level == 0)
		return 0;

	const UINT8 c = lfo->counter;
	int v;
	switch (ws)
	{
	default:
	case 0:	v = (INT8)(c & ~1); break;							// saw
	case 1:	v = (c & 0x80) ? -0x80 : 0x7E; break;				// square
	case 2:	v = (INT8)((((c & 0x3F) ^ ((c & 0x40) ? 0x3F : 0x00)) ^ ((c & 0x80) ? 0x7F : 0x00)) << 1); break;	// triangle
	case 3:	v = (INT8)(lfsr & ~1); break;						// noise
	}
	v >>= (7 - level);
	return ((int)(0x40 ^ (fns >> 4)) * v) >> 6;	// signed: v is negative half the time
}

// Amplitude LFO: attenuation added to the envelope (same 0.09375 dB units), 0 = none. ALFOS 7 goes down to
// about 24 dB, each lower level half.
static inline int ALFO_Get(const _LFO *lfo, unsigned ws, unsigned level, UINT32 lfsr)
{
	if (level == 0)
		return 0;

	const UINT8 c = lfo->counter;
	unsigned v;
	switch (ws)
	{
	default:
	case 0:	v = c & ~1; break;									// saw
	case 1:	v = (c & 0x80) ? 0xFE : 0; break;					// square
	case 2:	v = (UINT8)((c ^ ((c & 0x80) ? 0xFF : 0)) << 1); break;	// triangle
	case 3:	v = lfsr & 0xFE; break;								// noise
	}
	return (int)(v >> (7 - level));
}
