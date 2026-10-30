
/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
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
 * SCSP.cpp
 *
 * WARNING: Here be dragons! Tread carefully. Changing the state variables
 * may break save state support.
 *
 * SCSP (Sega Custom Sound Processor) emulation. This code was generously
 * donated by ElSemi. Interfaces directly to the 68K processor through
 * callbacks. Some minor interface changes were made (external global variables
 * were removed).
 *
 * The MIDI input buffer has been increased from 8 (which I assume is the
 * actual size) in order to accommodate Model 3's PowerPC/68K communication.
 * There is probably tight synchronization between the CPUs, with PowerPC-side
 * interrupts being generated to fill the MIDI buffer as the 68K pulls data
 * out, or there may be a UART with a large FIFO buffer. This can be simulated
 * by increasing the MIDI buffer (MIDI_STACK_SIZE).
 *
 * Supermodel sends a whole frame's MIDI commands during VBlank (up to 128
 * PowerPC MIDI interrupts) before the 68K runs, so the buffer must hold a
 * frame's worth of bytes. Sega Rally 2 sends more than 255 in a burst.
 *
 * To-Do List
 * ----------
 * - Wrap up into an object.
 */


/*
	SEGA Custom Sound Processor (SCSP) Emulation
	by ElSemi.
	Driven by MC68000
*/

/* MAME SCSP conversion by Paul Prosser (aka Conversus W. Vans). Code by R.Belmont and ElSemi.
Fixes made: buggy timing, better envelope processing, better FM support, a more reasonable DSP emulation and all kinds of improvements.
But the new core isn't without its to-do list:
- Fix low sound volume when changing music in Harley-Davidson & LA Riders (Maybe the MVOL code from MAME will fix this?)
- Check that music in Fighting Vipers 2 Bahn's stage no longer hangs (it used to need ElSemi's older DSP
  register handling, which has been removed now that DSP memory access follows the manual).

Since I (Paul) started this code overhaul in November 2019, I have also removed some obsolete features like the REVERB software effect.
It doesn't sound good at all.

Anyways credit to R. Belmont and ElSemi for the code, and for being awesome emulation Gods.
*/

#include "SCSP.h"

#include "Supermodel.h"
#include "SCSPDSP.h"
#include "OSD/Thread.h"


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>


static const Util::Config::Node *s_config = 0;
static bool s_multiThreaded = false;

// Scales each SCSP's output by its MVOL master volume (0-15), like MAME. Not from MAME: the define, off by default, as the
// volume levels were tuned without it, and games that set MVOL < 15 (or never set it: it starts at 0, which is silent)
// would get quieter.
//#define APPLY_MVOL

// These globals control the operation of the SCSP, they are no longer extern and are set through SCSP_SetBuffers(). --Bart
static float* bufferfl;
static float* bufferfr;
static float* bufferrl;
static float* bufferrr;
static int length;	// capacity of the buffers in samples

static constexpr double srate=44100;


#define ICLIP16(x) (((x)<-32768)?-32768:(((x)>32767)?32767:(x)))
#define ICLIP18(x) (((x)<-131072)?-131072:(((x)>131071)?131071:(x)))

#ifndef BYTE
#define BYTE UINT8
#endif

#ifndef WORD
#define WORD UINT16
#endif

#ifndef DWORD
#define DWORD UINT32
#endif

static CMutex *MIDILock;	// for safe access to the MIDI FIFOs
static int (*Run68kCB)(int cycles);
static void (*Int68kCB)(int irq);

#define MIDI_STACK_SIZE			0x1000
#define MIDI_STACK_SIZE_MASK	(MIDI_STACK_SIZE-1)

#define MIDI_OUT_STACK_MASK		15	// not from MAME: 16 entries (MAME has 32), as the size is part of the save state

static BYTE MidiOutStack[MIDI_OUT_STACK_MASK+1];
static BYTE MidiOutW=0,MidiOutR=0;
static BYTE MidiStack[MIDI_STACK_SIZE];
static BYTE MidiOutFill;
static BYTE MidiInFill;
static UINT16 MidiW=0,MidiR=0;
static bool MidiInOverflow=false;	// MIOVF: a byte arrived while the input FIFO was full
static BYTE HasSlaveSCSP=0;
static BYTE MidiInOverflow=0;		// MIOVF: a byte arrived while the input FIFO was full
static int MidiOutDrain=0;			// samples until the next byte leaves the MIDI output FIFO

#define MIDI_OUT_FIFO_SIZE		4		// manual: 4 byte MIDI-OUT buffer
#define MIDI_OUT_SAMPLES_PER_BYTE	14	// 10 bits at 31.25 kbaud = 320 us = ~14 samples

#define SOUND_RAM_MASK	0xFFFFF		// each SCSP addresses 1 MB of sound memory (2 x 4 Mbit, MEM4MB=1)

static INT32 EG_TABLE[0x400];

static constexpr float SDLT[8] = { -1000000.0f,-36.0f,-30.0f,-24.0f,-18.0f,-12.0f,-6.0f,0.0f };
static int LPANTABLE[0x10000];
static int RPANTABLE[0x10000];

static int TimCnt[3];		// timer A-C counts (8 bit)
static int TimReload[3];	// TIMx value written, loaded on the next prescaler clock (-1: none)
static int lastdiff = 0;	// 68K cycles run past the end of the previous sample, taken off the next one
static UINT32 SampleCounter;	// samples since reset, clocks the envelope generators

#define SHIFT	12
#define FIX(v)	((UINT32) ((float) (1<<SHIFT)*(v)))

#define ADDR_SHIFT	14	// fraction bits of the sample position (as on the chip, see SCSP_Step)
#define FM_DELAY    5	// a slot's output reaches the sound stack 5 slot cycles later (manual Fig. 4.30/4.31)


#include "SCSPLFO.cpp"

/*
	SCSP features 32 programmable slots
	that can generate FM and PCM (from ROM/RAM) sound
*/
//SLOT PARAMETERS
#define KEYONEX(slot)   ((slot->data[0x0] >> 0x0) & 0x1000)
#define KEYONB(slot)    ((slot->data[0x0] >> 0x0) & 0x0800)
#define SBCTL(slot)     ((slot->data[0x0] >> 0x9) & 0x0003)
#define SSCTL(slot)     ((slot->data[0x0] >> 0x7) & 0x0003)
#define LPCTL(slot)     ((slot->data[0x0] >> 0x5) & 0x0003)
#define PCM8B(slot)     ((slot->data[0x0] >> 0x0) & 0x0010)

#define SA(slot)        (((slot->data[0x0] & 0xF) << 16) | (slot->data[0x1]))

#define LSA(slot)       (slot->data[0x2])

#define LEA(slot)       (slot->data[0x3])

#define D2R(slot)       ((slot->data[0x4] >> 0xB) & 0x001F)
#define D1R(slot)       ((slot->data[0x4] >> 0x6) & 0x001F)
#define EGHOLD(slot)    ((slot->data[0x4] >> 0x0) & 0x0020)
#define AR(slot)        ((slot->data[0x4] >> 0x0) & 0x001F)

#define LPSLNK(slot)    ((slot->data[0x5] >> 0x0) & 0x4000)
#define KRS(slot)       ((slot->data[0x5] >> 0xA) & 0x000F)
#define DL(slot)        ((slot->data[0x5] >> 0x5) & 0x001F)
#define RR(slot)        ((slot->data[0x5] >> 0x0) & 0x001F)

#define STWINH(slot)    ((slot->data[0x6] >> 0x0) & 0x0200)
#define SDIR(slot)      ((slot->data[0x6] >> 0x0) & 0x0100)
#define TL(slot)        ((slot->data[0x6] >> 0x0) & 0x00FF)

#define MDL(slot)       ((slot->data[0x7] >> 0xC) & 0x000F)
#define MDXSL(slot)     ((slot->data[0x7] >> 0x6) & 0x003F)
#define MDYSL(slot)     ((slot->data[0x7] >> 0x0) & 0x003F)

#define OCT(slot)       ((slot->data[0x8] >> 0xB) & 0x000F)
#define FNS(slot)       ((slot->data[0x8] >> 0x0) & 0x03FF)

#define LFORE(slot)     ((slot->data[0x9] >> 0x0) & 0x8000)
#define LFOF(slot)      ((slot->data[0x9] >> 0xA) & 0x001F)
#define PLFOWS(slot)    ((slot->data[0x9] >> 0x8) & 0x0003)
#define PLFOS(slot)     ((slot->data[0x9] >> 0x5) & 0x0007)
#define ALFOWS(slot)    ((slot->data[0x9] >> 0x3) & 0x0003)
#define ALFOS(slot)     ((slot->data[0x9] >> 0x0) & 0x0007)

#define ISEL(slot)      ((slot->data[0xA] >> 0x3) & 0x000F)
#define IMXL(slot)      ((slot->data[0xA] >> 0x0) & 0x0007)

#define DISDL(slot)     ((slot->data[0xB] >> 0xD) & 0x0007)
#define DIPAN(slot)     ((slot->data[0xB] >> 0x8) & 0x001F)
#define EFSDL(slot)     ((slot->data[0xB] >> 0x5) & 0x0007)
#define EFPAN(slot)     ((slot->data[0xB] >> 0x0) & 0x001F)




typedef enum {ATTACK,DECAY1,DECAY2,RELEASE} _STATE;
struct _EG
{
	int att;		// attenuation, 0 (full volume) to 3FFh, 0.09375 dB per step
	_STATE state;
};

struct _SLOT
{
	union
	{
		WORD data[0x10];	//only 0x1a bytes used
		BYTE datab[0x20];
	};
	BYTE active;	//this slot is currently playing
	BYTE *base;		//samples base address
	DWORD cur_addr;	//current play address (18.14)
	DWORD nxt_addr;	//next sample, for interpolation (18.14)
	BYTE Back;
	_EG EG;			//Envelope
	_LFO LFO;		// pitch and amplitude LFO
};

#define DAC18B(scsp)	((scsp->data[0]>>0x0)&0x0100)
#define RBL(scsp)		((scsp->data[1]>>0x7)&0x0003)
#define RBP(scsp)		((scsp->data[1]>>0x0)&0x007F)

struct _SCSP
{
	union
	{
		WORD data[0x30/2];
		BYTE datab[0x30];
	};
	_SLOT Slots[32];
	signed short RINGBUF[64];
	unsigned char BUFPTR;
	signed short DELAYBUF[FM_DELAY];
	BYTE DELAYVALID[FM_DELAY];	// 0 = slot had STWINH set, don't write this entry to the stack
	BYTE DELAYPTR;
	unsigned char *SCSPRAM;
	char Master;
	UINT32 LFSR;	// noise generator (SSCTL = 1 data and the noise LFO waveform)
	_SCSPDSP DSP;
} SCSPs[MAX_SCSP],*SCSP=SCSPs;




void CheckPendingIRQ()
{
	_SCSP *chip = SCSPs;	// only the master SCSP drives the 68K interrupt lines

	// MIDI input interrupt (bit 3) is level triggered: pending while the input FIFO holds data
	if (MidiW != MidiR)
		chip->data[0x20 / 2] |= 0x0008;
	else
		chip->data[0x20 / 2] &= ~0x0008;

	// MIDI output interrupt (bit 9): pending while the output FIFO is empty
	if (MidiOutFill == 0)
		chip->data[0x20 / 2] |= 0x0200;
	else
		chip->data[0x20 / 2] &= ~0x0200;

	DWORD pend = chip->data[0x20 / 2] & chip->data[0x1e / 2] & 0x7ff;

	// The level of each source comes from SCILV0-2. Bits 0-6 have their own level; timer B, timer C,
	// MIDI out and the 1 sample interrupt (bits 7-10) share the level of bit 7 (manual Fig. 4.65).
	unsigned int irqLevel = 0;
	for (int bit = 0; bit <= 10; bit++)
	{
		if (pend & (1 << bit))
		{
			int lvBit = bit < 7 ? bit : 7;
			unsigned int level = ((chip->data[0x24 / 2] >> lvBit) & 1) | (((chip->data[0x26 / 2] >> lvBit) & 1) << 1) | (((chip->data[0x28 / 2] >> lvBit) & 1) << 2);
			if (level > irqLevel)
				irqLevel = level;
		}
	}

	Int68kCB(irqLevel);
}

// Envelope generator, following Mednafen's hardware-tested SCSP. The level is an attenuation (0 = full volume,
// 3FFh = silent). The rates are the 5-bit register values plus the key scaling, and each rate clocks the EG at its
// own interval. Attack starts at 280h (-60 dB) and approaches 0 exponentially (Corlett's AICA notes describe the
// same start level and step); decay and release add a fixed amount. A slot stops sounding at 3C0h.

// Key rate scaling: KRS + octave, limited to 0-Fh, added to every rate. KRS = Fh: off.
static unsigned EG_KeyScale(_SLOT *slot)
{
	if (KRS(slot) == 0xF)
		return 0;
	int scale = (int)KRS(slot) + ((OCT(slot) ^ 8) - 8);
	return scale < 0 ? 0 : (scale > 0xF ? 0xF : (unsigned)scale);
}

static unsigned EG_Rate(_SLOT *slot, _STATE state)
{
	switch (state)
	{
	case ATTACK:	return AR(slot);
	case DECAY1:	return D1R(slot);
	case DECAY2:	return D2R(slot);
	default:		return RR(slot);
	}
}

// One sample of the envelope of one slot. sc is the sample counter, scxc marks the bits that changed in it.
static void EG_Run(_SLOT *slot, UINT32 sc, UINT32 scxc)
{
	const int level = slot->EG.att;
	const unsigned rateNoScale = EG_Rate(slot, slot->EG.state);
	const unsigned keyScale = EG_KeyScale(slot);
	const unsigned rate = (std::min)(0x1Fu, rateNoScale + keyScale);
	const unsigned rateWBT = (0x18 - (std::min)(0x18u, rate)) >> 1;
	const unsigned shiftAdj = (rate < 0x18 && (rate & 1)) ? 1 : 0;

	// A rate of 0 never clocks; neither does an attack fast enough to be instant (it started at 0)
	bool clock = (((scxc | (scxc >> shiftAdj)) >> (rateWBT + shiftAdj)) & 1) && rateNoScale != 0;
	if (slot->EG.state == ATTACK && rateNoScale + keyScale >= 0x20)
		clock = false;

	if (clock)
	{
		const int incBase = (slot->EG.state == ATTACK) ? ~level : 16;
		const unsigned ermaxo = (std::max)(0x18u, (std::min)(0x1Eu, rate));
		const unsigned srac = ((0x20 - ermaxo) >> 1) + (ermaxo & 1 & (sc >> (rateWBT + 1)));
		int newLevel = level + (incBase >> srac);
		slot->EG.att = (std::max)(0, (std::min)(0x3FF, newLevel));
	}

	// Phase changes use the level from before this step
	if (slot->EG.state == DECAY1)
	{
		if ((unsigned)(level >> 5) == DL(slot))
			slot->EG.state = DECAY2;
	}
	else if (slot->EG.state == ATTACK)
	{
		if (!LPSLNK(slot) && level == 0)	// with LPSLNK, decay starts when the loop start is passed (SCSP_UpdateSlot)
			slot->EG.state = DECAY1;
	}
}

// Attenuation the EG applies now (EGHOLD: full volume during the attack)
static int EG_Level(_SLOT *slot)
{
	return (slot->EG.state == ATTACK && EGHOLD(slot)) ? 0 : slot->EG.att;
}

void SCSP_StopSlot(_SLOT *slot,int keyoff);

// Phase step per sample (ADDR_SHIFT fraction bits): the 11-bit frequency number plus the pitch LFO offset,
// shifted by the octave
static DWORD SCSP_Step(_SLOT *slot, int plfo)
{
	return ((UINT32)((0x400 | FNS(slot)) + plfo) << (OCT(slot) ^ 8)) >> (18 - ADDR_SHIFT);
}


void SCSP_StartSlot(_SLOT *slot)
{
	UINT32 start_offset;

	slot->active = 1;
	slot->Back = 0;
	slot->nxt_addr = 1 << ADDR_SHIFT;
	slot->cur_addr = 0;
	start_offset = PCM8B(slot) ? SA(slot) : SA(slot) & 0xFFFFE;	// 16-bit samples are word aligned anywhere in the 1 MB
	slot->base = SCSP->SCSPRAM + start_offset;
	slot->EG.state = ATTACK;
	slot->EG.att = (AR(slot) + EG_KeyScale(slot) >= 0x20) ? 0 : 0x280;	// instant attack, or start at -60 dB
}

void SCSP_StopSlot(_SLOT *slot,int keyoff)
{
	// KYONB is left as written: registers read back what was written to them
	if(keyoff)
	{
		slot->EG.state=RELEASE;
	}
	else
		slot->active=0;
}

Result SCSP_Init(const Util::Config::Node &config, int n)
{
	s_config = &config;
	s_multiThreaded = config["MultiThreaded"].ValueAs<bool>();

	if(n==2)
	{
		SCSP=SCSPs+1;
		memset(SCSP,0,sizeof(_SCSP));
		SCSP->Master=0;
		HasSlaveSCSP=1;
		SCSPDSP_Init(&SCSP->DSP);

	}
	SCSP=SCSPs+0;
	memset(SCSP,0,sizeof(_SCSP));
	SCSPDSP_Init(&SCSP->DSP);
	SCSP->Master=1;
	MidiR=MidiW=0;
	MidiInOverflow=false;
	MidiOutR=MidiOutW=0;
	MidiOutFill=0;

	for (int i = 0; i < 0x400; ++i) {
		float envDB = ((float)(3 * (i - 0x3ff))) / 32.0f;
		float scale = (float)(1 << SHIFT);
		EG_TABLE[i] = (INT32)(pow(10.0, envDB / 20.0)*scale);
	}


	for(int i=0;i<0x10000;++i)
	{
		int iTL =(i>>0x0)&0xff;
		int iPAN=(i>>0x8)&0x1f;
		int iSDL=(i>>0xd)&0x07;

		float TL=1.0;
		float SegaDB=0;
		if(iTL&0x01) SegaDB-=0.4f;
		if(iTL&0x02) SegaDB-=0.8f;
		if(iTL&0x04) SegaDB-=1.5f;
		if(iTL&0x08) SegaDB-=3.0f;
		if(iTL&0x10) SegaDB-=6.0f;
		if(iTL&0x20) SegaDB-=12.0f;
		if(iTL&0x40) SegaDB-=24.0f;
		if(iTL&0x80) SegaDB-=48.0f;

		TL=powf(10.0f,SegaDB/20.0f);

		float PAN=1.0;

		SegaDB=0;
		if(iPAN&0x1) SegaDB-=3.0f;
		if(iPAN&0x2) SegaDB-=6.0f;
		if(iPAN&0x4) SegaDB-=12.0f;
		if(iPAN&0x8) SegaDB-=24.0f;

		if(iPAN==0xf) PAN=0.0;
		else if(iPAN==0x1f) PAN=0.0;
		else PAN=powf(10.0f,SegaDB/20.0f);

		float LPAN,RPAN;

		if(iPAN<0x10)
		{
			LPAN=PAN;
			RPAN=1.0;
		}
		else
		{
			RPAN=PAN;
			LPAN=1.0;
		}

		float SDL=1.0;
		if(iSDL)
			SDL=powf(10.0f,(SDLT[iSDL])/20.0f);
		else
			SDL=0.0;

		LPANTABLE[i]=FIX((4.0*LPAN*TL*SDL));
		RPANTABLE[i]=FIX((4.0*RPAN*TL*SDL));
	}

	for (int c = 0; c < MAX_SCSP; ++c)
	{
		for (int i = 0; i < 32; ++i)
		{
			SCSPs[c].Slots[i].EG.state = RELEASE;	// idle slots must be KEY_ON-able (key on starts slots in RELEASE)
			SCSPs[c].Slots[i].EG.att = 0x3FF;
			SCSPs[c].Slots[i].LFO.counter = 0;
			SCSPs[c].Slots[i].LFO.timeCounter = 1;
		}
		SCSPs[c].LFSR = 1;
	}
	MidiInOverflow = 0;
	MidiOutDrain = 0;

	SCSPs->data[0x20 / 2] = 0;
	for (int t = 0; t < 3; t++)
	{
		TimCnt[t] = 0;
		TimReload[t] = -1;
	}
	SampleCounter = 0;
	lastdiff = 0;

	// MIDI FIFO mutex
	MIDILock = CThread::CreateMutex();
	if (NULL == MIDILock)
	{
		return ErrorLog("Unable to create MIDI mutex!");
	}

	return Result::OKAY;
}

void SCSP_SetRAM(int n,unsigned char *r)
{
	SCSPs[n].SCSPRAM=r;
	SCSPs[n].DSP.SCSPRAM=(unsigned short*) r;
}

void SCSP_UpdateSlotReg(int s,int r)
{
	struct _SLOT *slot = SCSP->Slots + s;
	int sl;
	switch (r & 0x3f)
	{
	case 0:
	case 1:
		if (KEYONEX(slot))
		{
			for (sl = 0; sl < 32; ++sl)
			{
				struct _SLOT *s2 = SCSP->Slots + sl;
				{
					if (KEYONB(s2) && s2->EG.state == RELEASE)
					{
						SCSP_StartSlot(s2);
					}
					if (!KEYONB(s2))
					{
						SCSP_StopSlot(s2, 1);
					}
				}
			}
			slot->data[0] &= ~0x1000;
		}
		break;
	}
}

// DMA between sound memory and the SCSP's own registers (manual 4.2, "DMA Transfer Register"). Done at once.
static void SCSP_DoDMA(void)
{
	UINT32 memAddr = (SCSP->data[0x12 / 2] & 0xFFFE) | ((UINT32)(SCSP->data[0x14 / 2] & 0xF000) << 4);	// DMEA[19:1]
	UINT32 regAddr = SCSP->data[0x14 / 2] & 0x0FFE;										// DRGA[11:1]
	UINT32 words = (SCSP->data[0x16 / 2] & 0x0FFE) / 2;									// DTLG[11:1]
	bool gate = (SCSP->data[0x16 / 2] & 0x4000) != 0;	// DGATE: transfer zeros
	bool toMemory = (SCSP->data[0x16 / 2] & 0x2000) != 0;	// DDIR: 1 = registers -> sound memory

	for (UINT32 i = 0; i < words; i++, memAddr += 2, regAddr += 2)
	{
		UINT32 reg = regAddr & 0xFFE;
		if (reg >= 0xEE4 || (reg >= 0x412 && reg <= 0x416))	// past the register area, or the DMA registers themselves
			continue;
		UINT16 *mem = (UINT16 *) &SCSP->SCSPRAM[memAddr & SOUND_RAM_MASK & ~1];
		if (toMemory)
			*mem = gate ? 0 : SCSP_r16(reg);
		else
			SCSP_w16(reg, gate ? 0 : *mem);
	}

	SCSP->data[0x16 / 2] &= ~0x1000;	// DEXE clears itself when done
	SCSP->data[0x20 / 2] |= 0x0010;		// DMA end interrupt (bit 4)
	SCSP->data[0x2c / 2] |= 0x0010;
	if (SCSP->Master)
		CheckPendingIRQ();
}

void SCSP_UpdateReg(int reg)
{
	switch(reg&0x3f)
	{
		case 0x0: // MVOL is applied in SCSP_DoMasterSamples (if APPLY_MVOL is defined)
			//m_stream->set_output_gain(0, MVOL() / 15.0);
			//m_stream->set_output_gain(1, MVOL() / 15.0);
			//break;
		case 0x2:
		case 0x3:
			SCSP->DSP.RBL = (8 * 1024) << RBL(SCSP); // 8 / 16 / 32 / 64 kwords
			SCSP->DSP.RBP = RBP(SCSP);
			break;

		case 0x6:	// MOBUF (low byte only; a write to the high byte doesn't send anything)
			if (SCSP->Master)
				SCSP_MidiOutW(SCSP->data[0x6/2]&0xff);
			break;

		case 8:
		case 9:
			SCSP->data[0x8 / 2] &= 0xf800;
			break;
		case 0x16:	// DGATE, DDIR, DEXE, DTLG
		case 0x17:
			if (SCSP->data[0x16 / 2] & 0x1000)
				SCSP_DoDMA();
			break;
		case 0x1e:	// SCIEB
		case 0x1f:
		case 0x20:	// SCIPD (only bit 5 is writable, see SCSP_w8/w16)
		case 0x21:
			if (SCSP->Master)
				CheckPendingIRQ();
			break;
		case 0x18:	// TIMx (low byte): counting restarts from the new value
			if(SCSP->Master)
			{
				TimReload[0] = SCSPs->data[0x18/2] & 0xff;	// loaded on the next prescaler clock (SCSP_TimersAddTicks)
			}
			break;
		case 0x1A:	// TIMx (low byte): counting restarts from the new value
			if(SCSP->Master)
			{
				TimReload[1] = SCSPs->data[0x1A/2] & 0xff;	// loaded on the next prescaler clock (SCSP_TimersAddTicks)
			}
			break;
		case 0x1C:	// TIMx (low byte): counting restarts from the new value
			if(SCSP->Master)
			{
				TimReload[2] = SCSPs->data[0x1C/2] & 0xff;	// loaded on the next prescaler clock (SCSP_TimersAddTicks)
			}
			break;
		// TODO (verified via spec, not from MAME): SCIPD (0x20) is read only, except that writes can set bit 5 (raising that interrupt)
		case 0x22:	//SCIRE
		case 0x23:
			if(SCSP->Master)
			{
				SCSP->data[0x20 / 2] &= ~SCSP->data[0x22 / 2];	// only clears

				CheckPendingIRQ();
			}
			break;
		case 0x2e:	// MCIRE
		case 0x2f:
			SCSP->data[0x2c / 2] &= ~SCSP->data[0x2e / 2];
			break;
	}
}

// Readable bits of the common control registers (400h-42Fh). Write only registers read as 0 (manual p.35).
static const UINT16 s_commonReadMask[0x18] =
{
	0x00F0,	// 400 M4/DB/MVOL write only, VER readable
	0x0000,	// 402 RBL, RBP
	0x1FFF,	// 404 MIDI flags, MIBUF
	0x0000,	// 406 MOBUF
	0x07FF,	// 408 CA (+ SGC/EG monitor bits); MSLC is write only
	0x0000, 0x0000, 0x0000, 0x0000,	// 40A-410
	0x0000,	// 412 DMEA
	0x0000,	// 414 DMEA, DRGA
	0x7000,	// 416 DGATE, DDIR, DEXE (DTLG write only)
	0x0000, 0x0000, 0x0000,	// 418-41C timers
	0x07FF,	// 41E SCIEB
	0x07FF,	// 420 SCIPD
	0x0000,	// 422 SCIRE
	0x0000, 0x0000, 0x0000,	// 424-428 SCILV0-2
	0x07FF,	// 42A MCIEB
	0x07FF,	// 42C MCIPD
	0x0000,	// 42E MCIRE
};

// Read a common control register word (reg = even offset 00h-2Eh). popMidi: this read takes the byte from MIBUF.
static UINT16 SCSP_ReadCommonReg(int reg, bool popMidi)
{
	switch (reg)
	{
	case 0x04:	// MIDI status and input buffer
	{
		if (SCSP != SCSPs)		// the MIDI FIFOs belong to the master SCSP
			return 0x0900;		// slave: input and output FIFO empty

		if (s_multiThreaded)
			MIDILock->Lock();

		UINT16 v = 0;
		if (MidiOutFill >= MIDI_OUT_FIFO_SIZE)	v |= 0x1000;	// MOFULL
		if (MidiOutFill == 0)					v |= 0x0800;	// MOEMP
		if (MidiInOverflow)						v |= 0x0400;	// MIOVF
		if (((MidiW + 1) & MIDI_STACK_SIZE_MASK) == MidiR)	v |= 0x0200;	// MIFULL
		if (MidiW == MidiR)						v |= 0x0100;	// MIEMP
		else
		{
			v |= MidiStack[MidiR];
			if (popMidi)
			{
				++MidiR;
				MidiR &= MIDI_STACK_SIZE_MASK;
				MidiInOverflow = 0;
			}
		}

		if (s_multiThreaded)
			MIDILock->Unlock();

		if (popMidi)
			CheckPendingIRQ();
		return v;
	}

	case 0x08:	// slot status of the slot selected by MSLC
	{
		// MSLC     |  CA   |SGC|EG
		// f e d c b a 9 8 7 6 5 4 3 2 1 0
		BYTE MSLC = (SCSP->data[0x8 / 2] >> 11) & 0x1f;
		_SLOT *slot = SCSP->Slots + MSLC;
		unsigned int SGC = (slot->EG.state) & 3;
		unsigned int CA = (slot->cur_addr >> (ADDR_SHIFT + 12)) & 0xf;
		unsigned int EG = (EG_Level(slot) >> 5) & 0x1f;	// attenuation, 0 = full volume
    /* note: according to the manual MSLC is write only, CA, SGC and EG read only.  */
		// not from MAME: keep MSLC in the register (MAME latches it, which would change the save state), so later reads
		// monitor the same slot (SCSP_r8/r16 mask it out of the value read)
		// TODO: MAME (458507e06b) updates this only once per sample, so a read right after selecting a slot returns the
		// old value. Depends on 68K timing, and needs a save state change
		return (CA << 7) | (SGC << 5) | EG;		// MSLC stays as written
	}

	default:
		return SCSP->data[reg / 2] & s_commonReadMask[reg / 2];
	}
}

// Only bit 5 of SCIPD and MCIPD can be written, and writing 1 raises the CPU interrupt (manual p.96)
static void SCSP_FixPendingWrite(int reg, UINT16 oldSCIPD, UINT16 oldMCIPD)
{
	if ((reg & ~1) == 0x20)
		SCSP->data[0x20 / 2] = oldSCIPD | (SCSP->data[0x20 / 2] & 0x0020);
	else if ((reg & ~1) == 0x2c)
		SCSP->data[0x2c / 2] = oldMCIPD | (SCSP->data[0x2c / 2] & 0x0020);
}

static UINT16 SCSP_ReadDSPWord(unsigned int addr);
static void SCSP_WriteDSPWord(unsigned int addr, UINT16 val);

void SCSP_w8(unsigned int addr,unsigned char val)
{
	addr&=0xffff;
	if(addr<0x400)
	{
		int slot=addr/0x20;
		addr&=0x1f;
		*(unsigned char *) &(SCSP->Slots[slot].datab[addr^1]) = val;
		SCSP_UpdateSlotReg(slot,(addr^1)&0x1f);
	}
	else if(addr<0x600)
	{
		if (addr < 0x430)	// common control registers end at 42Fh
		{
			int reg = (addr & 0x3f) ^ 1;
			UINT16 oldSCIPD = SCSP->data[0x20 / 2], oldMCIPD = SCSP->data[0x2c / 2];
			SCSP->datab[reg] = val;
			SCSP_FixPendingWrite(reg, oldSCIPD, oldMCIPD);
			SCSP_UpdateReg(reg);
		}
	}
	else if(addr<0x680)	// sound stack, 64 words (big endian bytes)
	{
		signed short &w = SCSP->RINGBUF[(addr-0x600)/2];
		if (addr & 1)
			w = (signed short)((w & 0xFF00) | val);
		else
			w = (signed short)((w & 0x00FF) | (val << 8));
	}
	else if(addr<0x700)
		;	// nothing here
	else
	{
		// read-modify-write of the 16-bit word
		UINT16 w = SCSP_ReadDSPWord(addr & ~1);
		if (addr & 1)
			w = (w & 0xFF00) | val;
		else
			w = (w & 0x00FF) | (val << 8);
		SCSP_WriteDSPWord(addr & ~1, w);
	}
}
// DSP area 700h-EE3h, word access
static UINT16 SCSP_ReadDSPWord(unsigned int addr)
{
	UINT16 v = 0;
	if (addr < 0x780)	//COEF
		v = *((UINT16 *)(SCSP->DSP.COEF + (addr - 0x700) / 2));
	else if (addr < 0x7c0)
		v = *((UINT16 *)(SCSP->DSP.MADRS + (addr - 0x780) / 2));
	else if (addr < 0x800)	// 7C0h-7FFh: nothing here (not a MADRS mirror, see SCSP_WriteDSPWord)
		v = 0;
	else if (addr < 0xC00)
		v = *((UINT16 *)(SCSP->DSP.MPRO + (addr - 0x800) / 2));
	// Internal buffers (manual Fig. 4.7): each TEMP/MEMS entry is two words, bits [7:0] then bits [23:8];
	// each MIXS entry is bits [3:0] then bits [19:4]
	else if (addr < 0xE00)
	{
		INT32 t = SCSP->DSP.TEMP[(addr >> 2) & 0x7f];
		v = (addr & 2) ? (UINT16)(t >> 8) : (UINT16)(t & 0xff);
	}
	else if (addr < 0xE80)
	{
		INT32 m = SCSP->DSP.MEMS[(addr >> 2) & 0x1f];
		v = (addr & 2) ? (UINT16)(m >> 8) : (UINT16)(m & 0xff);
	}
	else if (addr < 0xEC0)
	{
		INT32 m = SCSP->DSP.MIXS[(addr >> 2) & 0xf];
		v = (addr & 2) ? (UINT16)(m >> 4) : (UINT16)(m & 0xf);
	}
	else if (addr < 0xEE0)
		v = *((UINT16 *)(SCSP->DSP.EFREG + (addr - 0xec0) / 2));
	// EXTS (EE0h-EE3h) can't be accessed
	return v;
}

static void SCSP_WriteDSPWord(unsigned int addr, UINT16 val)
{
	if (addr < 0x780)  //COEF
		*((UINT16 *)(SCSP->DSP.COEF + (addr - 0x700) / 2)) = val;
	else if (addr < 0x7c0)
		*((UINT16 *)(SCSP->DSP.MADRS + (addr - 0x780) / 2)) = val;
	else if (addr < 0x800)
		;	// 7C0h-7FFh: nothing here (manual Fig. 4.5: MADRS is 32 words at 780h-7BFh). This is not a MADRS
			// mirror: VF3 and Sega Rally 2 write their delay taps to 780h-7BFh and then clear 7C0h-7FFh,
			// which with a mirror wipes the taps and silences their reverb.
	else if (addr < 0xC00)
		*((UINT16 *)(SCSP->DSP.MPRO + (addr - 0x800) / 2)) = val;
	else if (addr < 0xE00)
	{
		INT32 &t = SCSP->DSP.TEMP[(addr >> 2) & 0x7f];
		t = (addr & 2) ? (((INT32)(INT16)val << 8) | (t & 0xff)) : ((t & ~0xff) | (val & 0xff));
	}
	else if (addr < 0xE80)
	{
		INT32 &m = SCSP->DSP.MEMS[(addr >> 2) & 0x1f];
		m = (addr & 2) ? (((INT32)(INT16)val << 8) | (m & 0xff)) : ((m & ~0xff) | (val & 0xff));
	}
	else if (addr < 0xEC0)
	{
		INT32 &m = SCSP->DSP.MIXS[(addr >> 2) & 0xf];
		m = (addr & 2) ? (((INT32)(INT16)val << 4) | (m & 0xf)) : ((m & ~0xf) | (val & 0xf));
	}
	else if (addr < 0xEE0)
		*((UINT16 *)(SCSP->DSP.EFREG + (addr - 0xec0) / 2)) = val;

	if (addr == 0xBF0)
		SCSPDSP_Start(&SCSP->DSP);
}

void SCSP_w16(unsigned int addr,unsigned short val)
{
	addr&=0xffff;
	if(addr<0x400)
	{
		if ((addr&0x1f) == 0x1e)	// verified via spec, not from MAME: crosses into the next slot (or the common registers)
		{
			SCSP_w16(addr, val >> 16);
			SCSP_w16(addr + 2, val & 0xffff);
			return;
		}
		// otherwise write both halves before the updates, as a key on (offset 0) uses SA from the second half
		int slot=addr/0x20;
		addr&=0x1f;
		*(unsigned short *) &(SCSP->Slots[slot].datab[addr]) = val;
		SCSP_UpdateSlotReg(slot,addr&0x1f);
	}
	else if(addr<0x600)
	{
		// not from MAME: as two 16-bit writes, which keeps them inside the registers (same result as updating after both
		// halves, as an update that reads the other half is redone by that half's own update)
		//SCSP_w16(addr, val >> 16);
		//SCSP_w16(addr + 2, val & 0xffff);
		if (addr < 0x430)
		{
			int reg = addr & 0x3e;
			UINT16 oldSCIPD = SCSP->data[0x20 / 2], oldMCIPD = SCSP->data[0x2c / 2];
			SCSP->data[reg / 2] = val;
			SCSP_FixPendingWrite(reg, oldSCIPD, oldMCIPD);
			SCSP_UpdateReg(reg);
		}
	}
	else if (addr < 0x680)	// sound stack is 600h-67Fh (64 words)
		SCSP->RINGBUF[(addr - 0x600) / 2] = val;
	else if (addr < 0x700)
		;	// nothing here
	else
	{
		SCSP_WriteDSPWord(addr, val);
	}
}
void SCSP_w32(unsigned int addr,unsigned int val)
{
	// big endian: the high word goes to the lower address
	SCSP_w16(addr, (unsigned short)(val >> 16));
	SCSP_w16(addr + 2, (unsigned short)(val & 0xFFFF));
}
unsigned char SCSP_r8(unsigned int addr)
{
	unsigned char v=0;
	addr&=0xffff;
	if(addr<0x400)
	{
		int slot=addr/0x20;
		addr&=0x1f;
		v=*(unsigned char *) &(SCSP->Slots[slot].datab[addr^1]);
	}
	else if(addr<0x600)
	{
		if (addr < 0x430)
		{
			// MIBUF is the low byte of 404h: only a read of that byte takes data from the FIFO
			UINT16 w = SCSP_ReadCommonReg(addr & 0x3e, (addr & 1) != 0);
			v = (addr & 1) ? (w & 0xff) : (w >> 8);
		}
	}
	else if(addr<0x680)
	{
		UINT16 w = SCSP->RINGBUF[(addr - 0x600) / 2];
		v = (addr & 1) ? (w & 0xff) : (w >> 8);
	}
	else if(addr<0x700)
		v=0;
	else
	{
		UINT16 w = SCSP_ReadDSPWord(addr & ~1);
		v = (addr & 1) ? (w & 0xff) : (w >> 8);
	}
	return v;
}
unsigned short SCSP_r16(unsigned int addr)
{
	unsigned short v=0;
	addr&=0xffff;
	if(addr<0x400)
	{
		int slot=addr/0x20;
		addr&=0x1f;
		v=*(unsigned short *) &(SCSP->Slots[slot].datab[addr]);
	}
	else if(addr<0x600)
	{
		if (addr < 0x430)
			v = SCSP_ReadCommonReg(addr & 0x3e, true);
	}
	else if (addr < 0x680)
		v = SCSP->RINGBUF[(addr - 0x600) / 2];
	else if (addr < 0x700)
		v = 0;
	else
		v = SCSP_ReadDSPWord(addr);
	return v;
}
unsigned int SCSP_r32(unsigned int addr)
{
	// big endian: the high word comes from the lower address
	return ((unsigned int) SCSP_r16(addr) << 16) | SCSP_r16(addr + 2);
}

void SCSP_TimersAddTicks(int ticks)
{
	// 8-bit up counters (Mednafen, hardware tested). The prescaler clocks a timer on samples where the sample
	// counter is a multiple of 2^TxCTL. A TIMx write is loaded on the next clock, which uses that clock up, so the
	// interrupt comes (256 - TIMx) clocks after the write, one more than the manual's (255 - TIMx); VF3's music
	// runs fast without it. The interrupt is requested when the count reaches FFh, and the counter then wraps and
	// carries on.
	static const UINT16 pendBit[3] = { 0x40, 0x80, 0x100 };
	for (int n = 0; n < ticks; n++)
	{
		const UINT32 sc = SampleCounter + n;
		for (int t = 0; t < 3; t++)
		{
			unsigned ctl = (SCSPs->data[(0x18 + 2 * t) / 2] >> 8) & 0x7;
			if (sc & ((1u << ctl) - 1))
				continue;
			if (TimReload[t] >= 0)
			{
				TimCnt[t] = TimReload[t];
				TimReload[t] = -1;
			}
			else
				TimCnt[t] = (TimCnt[t] + 1) & 0xFF;
			if (TimCnt[t] == 0xFF)
				SCSPs->data[0x20 / 2] |= pendBit[t];
		}
	}
}

// One sample of one slot. plfo and alfo are this sample's LFO outputs and noise the noise generator value.
// Returns the slot output; *stackValue receives the value written to the sound stack.
signed int inline SCSP_UpdateSlot(_SCSP *chip, _SLOT *slot, int plfo, int alfo, UINT16 noise, signed short *stackValue)
{
	signed int sample;
	const DWORD step = SCSP_Step(slot, plfo);
	DWORD addr1, addr2, addr_select;
	DWORD *addr[2] = { &addr1, &addr2 };
	DWORD *slot_addr[2] = { &(slot->cur_addr), &(slot->nxt_addr) };

	// FM: the average of the two selected sound stack entries shifts the read address (manual Table 4.17:
	// the maximum shift is 2^MDL addresses for MDL 5-F, MDL 0-4 is no modulation). Only 10 bits of shift are
	// valid, so a shift past 1K words wraps around (Fig. 4.41).
	INT32 fmShift = 0;
	if (MDL(slot) >= 5)
	{
		INT32 smp = (chip->RINGBUF[(chip->BUFPTR + MDXSL(slot)) & 63] + chip->RINGBUF[(chip->BUFPTR + MDYSL(slot)) & 63]) / 2;
		smp <<= 0xA;
		smp >>= 0x19 - MDL(slot);	// full scale input (+/-32768) -> +/-2^MDL
		fmShift = ((smp & 0x7FF) ^ 0x400) - 0x400;	// wraps as an 11-bit signed value
	}

	// Sound memory byte addresses of this sample and the next (for interpolation). SA is a byte address in the
	// 1 MB sound memory and the read address wraps around within it.
	UINT32 start = (UINT32)(slot->base - chip->SCSPRAM);
	UINT32 pos1 = slot->cur_addr >> ADDR_SHIFT;
	UINT32 pos2 = slot->nxt_addr >> ADDR_SHIFT;
	if (PCM8B(slot)) {
		addr1 = (start + pos1 + fmShift) & SOUND_RAM_MASK;
		addr2 = (start + pos2 + fmShift) & SOUND_RAM_MASK;
	}
	else {
		addr1 = (start + 2 * (pos1 + fmShift)) & (SOUND_RAM_MASK & ~1);
		addr2 = (start + 2 * (pos2 + fmShift)) & (SOUND_RAM_MASK & ~1);
	}

	// SBCTL converts the sample data as it is read: bit 0 inverts bits 14-0, bit 1 inverts the sign bit. For
	// waveforms this happens before interpolation, otherwise unsigned data would glitch at every zero crossing.
	static const UINT16 sbXor[4] = { 0x0000, 0x7FFF, 0x8000, 0xFFFF };
	const UINT16 xorValue = sbXor[SBCTL(slot)];

	switch (SSCTL(slot))
	{
	case 0:	// wave form data from sound memory
	{
		INT16 p1, p2;
		if (PCM8B(slot))	//8 bit
		{
			p1 = (INT16)(((UINT16)chip->SCSPRAM[addr1 ^ 1] << 8) ^ xorValue);
			p2 = (INT16)(((UINT16)chip->SCSPRAM[addr2 ^ 1] << 8) ^ xorValue);
		}
		else	//16 bit
		{
			p1 = (INT16)(*(UINT16 *) &(chip->SCSPRAM[addr1]) ^ xorValue);
			p2 = (INT16)(*(UINT16 *) &(chip->SCSPRAM[addr2]) ^ xorValue);
		}
		signed int fpart = slot->cur_addr & ((1 << ADDR_SHIFT) - 1);
		sample = ((int)p1 * ((1 << ADDR_SHIFT) - fpart) + (int)p2 * fpart) >> ADDR_SHIFT;
		break;
	}
	case 1:	// internally generated noise (manual Table 4.12)
		sample = (INT16)(noise ^ xorValue);
		break;
	default:	// 2: internally generated zeros, 3: not allowed
		sample = (INT16)xorValue;
		break;
	}

	if (slot->Back)
		slot->cur_addr -= step;
	else
		slot->cur_addr += step;
	slot->nxt_addr = slot->cur_addr + (1 << ADDR_SHIFT);

	addr1 = slot->cur_addr >> ADDR_SHIFT;
	addr2 = slot->nxt_addr >> ADDR_SHIFT;

	if (addr1 >= LSA(slot) && !(slot->Back))
	{
		if (LPSLNK(slot) && slot->EG.state == ATTACK)
			slot->EG.state = DECAY1;
	}


	for (addr_select = 0; addr_select < 2; addr_select++)
	{
		INT32 rem_addr;
		switch (LPCTL(slot))
		{
		case 0: //no loop
			if (*addr[addr_select] >= LSA(slot) && *addr[addr_select] >= LEA(slot))
			{
				SCSP_StopSlot(slot, 0);
			}
			break;
		case 1: //normal loop
			if (*addr[addr_select] >= LEA(slot))
			{
				rem_addr = *slot_addr[addr_select] - (LEA(slot) << ADDR_SHIFT);
				*slot_addr[addr_select] = (LSA(slot) << ADDR_SHIFT) + rem_addr;
			}
			break;
		case 2: //reverse loop
			if ((*addr[addr_select] >= LSA(slot)) && !(slot->Back))
			{
				rem_addr = *slot_addr[addr_select] - (LSA(slot) << ADDR_SHIFT);
				*slot_addr[addr_select] = (LEA(slot) << ADDR_SHIFT) - rem_addr;
				slot->Back = 1;
			}
			else if ((*addr[addr_select] < LSA(slot) || (*slot_addr[addr_select] & 0x80000000)) && slot->Back)
			{
				rem_addr = (LSA(slot) << ADDR_SHIFT) - *slot_addr[addr_select];
				*slot_addr[addr_select] = (LEA(slot) << ADDR_SHIFT) - rem_addr;
			}
			break;
		case 3: //ping-pong
			if (*addr[addr_select] >= LEA(slot)) //reached end, reverse till start
			{
				rem_addr = *slot_addr[addr_select] - (LEA(slot) << ADDR_SHIFT);
				*slot_addr[addr_select] = (LEA(slot) << ADDR_SHIFT) - rem_addr;
				slot->Back = 1;
			}
			else if ((*addr[addr_select] < LSA(slot) || (*slot_addr[addr_select] & 0x80000000)) && slot->Back)//reached start or negative
			{
				rem_addr = (LSA(slot) << ADDR_SHIFT) - *slot_addr[addr_select];
				*slot_addr[addr_select] = (LSA(slot) << ADDR_SHIFT) + rem_addr;
				slot->Back = 0;
			}
			break;
		}
	}

	// SDIR: output the sound data without EG, TL and ALFO (manual p.72)
	if (!SDIR(slot))
	{
		const int att = (std::min)(0x3FF, EG_Level(slot) + alfo);	// the amplitude LFO adds to the EG attenuation
		sample = (sample * EG_TABLE[0x3FF - att]) >> SHIFT;
	}

	// sound stack value (after TL, which SDIR also bypasses)
	{
		UINT16 Enc = ((SDIR(slot) ? 0 : TL(slot)) << 0x0) | (0x7 << 0xd);
		*stackValue = (signed short)((sample * LPANTABLE[Enc]) >> (SHIFT + 1));
	}

	return sample;
}

// One slot of one SCSP: generate it, mix it into the direct and DSP outputs and write the sound stack
static void SCSP_StepSlot(_SCSP *chip, int sl, float balance, signed int &outL, signed int &outR)
{
	_SLOT *slot = chip->Slots + sl;
	signed short stackValue = 0;	// a slot that isn't playing outputs 0

	// The envelope runs for every slot, playing or not. A slot stops reading its waveform once the envelope
	// reaches 3C0h.
	if (slot->EG.att >= 0x3C0)
		slot->active = 0;
	EG_Run(slot, SampleCounter, (SampleCounter ^ (SampleCounter - 1)) & (SampleCounter ^ 1));

	// The LFO and the noise generator run for every slot. The pitch LFO and the noise data are taken before
	// they step, the amplitude LFO after (Mednafen's order).
	const int plfo = PLFO_Get(&slot->LFO, PLFOWS(slot), PLFOS(slot), FNS(slot), chip->LFSR);
	const UINT16 noise = (UINT16)(chip->LFSR << 8);
	LFO_Run(&slot->LFO, LFOF(slot), LFORE(slot) != 0);
	chip->LFSR = LFO_NextNoise(chip->LFSR);
	const int alfo = ALFO_Get(&slot->LFO, ALFOWS(slot), ALFOS(slot), chip->LFSR);

	if (slot->active)
	{
		signed int sample = (int)(balance*(float)SCSP_UpdateSlot(chip, slot, plfo, alfo, noise, &stackValue));
		const int tl = SDIR(slot) ? 0 : TL(slot);	// SDIR bypasses TL
		UINT16 Enc;

		Enc = (tl << 0x0) | ((IMXL(slot)) << 0xd);
		SCSPDSP_SetSample(&chip->DSP, (sample*LPANTABLE[Enc]) >> (SHIFT - 2), ISEL(slot));
		Enc = (tl << 0x0) | ((DIPAN(slot)) << 0x8) | ((DISDL(slot)) << 0xd);
		outL += (sample*LPANTABLE[Enc]) >> SHIFT;
		outR += (sample*RPANTABLE[Enc]) >> SHIFT;
	}

	// The output reaches the sound stack FM_DELAY slot cycles later (manual Fig. 4.31). STWINH: no write.
	chip->DELAYBUF[chip->DELAYPTR] = stackValue;
	chip->DELAYVALID[chip->DELAYPTR] = !STWINH(slot);
	int oldest = (chip->DELAYPTR + 1) % FM_DELAY;
	if (chip->DELAYVALID[oldest])
		chip->RINGBUF[(chip->BUFPTR + 64 - (FM_DELAY - 1)) & 63] = chip->DELAYBUF[oldest];
	chip->DELAYPTR = (chip->DELAYPTR + 1) % FM_DELAY;
	chip->BUFPTR = (chip->BUFPTR + 1) & 63;
}

void SCSP_DoMasterSamples(int nsamples)
{
	constexpr int slice = 11289600 / 44100;	// 68K clocked at 11.2896MHz (45.1584MHz OSC / 4), which is 256 cycles/sample

	/*
	 * Compute relative master/slave SCSP balance (note: master is often used
	 * for the front speakers). Equal balance is a 1.0 scale factor for both.
	 * When one SCSP is fully attenuated, the other's samples will be multiplied
	 * by 2.
	 */
	float balance = std::max(-100.f,std::min(100.f,s_config->Get("Balance").ValueAs<float>()));
	balance *= 0.01f;
	float masterBalance = 1.0f + balance;
	float slaveBalance = 1.0f - balance;

	float* buffl = bufferfl;
	float* buffr = bufferfr;
	float* bufrl = bufferrl;
	float* bufrr = bufferrr;

	/*
	 * Generate samples
	 */
	for (INT32 s = 0; s < nsamples; ++s)
	{
		signed int smpfl = 0, smpfr = 0;
		signed int smprl = 0, smprr = 0;

		for (INT32 sl = 0; sl < 32; ++sl)
		{
			SCSP_StepSlot(&SCSPs[0], sl, masterBalance, smpfl, smpfr);
			if (HasSlaveSCSP)
				SCSP_StepSlot(&SCSPs[1], sl, slaveBalance, smprl, smprr);
		}

		SCSPDSP_Step(&SCSPs[0].DSP);
		if (HasSlaveSCSP)
			SCSPDSP_Step(&SCSPs[1].DSP);

		for (INT32 i = 0; i < 16; ++i)
		{
			_SLOT *slot = SCSPs[0].Slots + i;
			if (EFSDL(slot))
			{
				UINT16 Enc = ((EFPAN(slot)) << 0x8) | ((EFSDL(slot)) << 0xd);
				smpfl += (int)(masterBalance*(float)(((SCSPs[0].DSP.EFREG[i] * LPANTABLE[Enc]) >> SHIFT)));
				smpfr += (int)(masterBalance*(float)(((SCSPs[0].DSP.EFREG[i] * RPANTABLE[Enc]) >> SHIFT)));
			}
			if (HasSlaveSCSP)
			{
				_SLOT *slot1 = SCSPs[1].Slots + i;
				if (EFSDL(slot1))
				{
					UINT16 Enc = ((EFPAN(slot1)) << 0x8) | ((EFSDL(slot1)) << 0xd);
					smprl += (int)(slaveBalance*(float)(((SCSPs[1].DSP.EFREG[i] * LPANTABLE[Enc]) >> SHIFT)));
					smprr += (int)(slaveBalance*(float)(((SCSPs[1].DSP.EFREG[i] * RPANTABLE[Enc]) >> SHIFT)));
				}
			}
		}

		if (DAC18B((&SCSPs[0])))
		{
			// 18-bit output is 4x the 16-bit level here. On the DAC both have the same full scale, but
			// Supermodel's mix (including the Digital Sound Board's music volume) is balanced to this level.
			smpfl = ICLIP18(smpfl);
			smpfr = ICLIP18(smpfr);

			*buffl++ = (float)smpfl;
			*buffr++ = (float)smpfr;
		}
		else
		{
			smpfl = ICLIP16(smpfl >> 2);
			smpfr = ICLIP16(smpfr >> 2);

			*buffl++ = (float)smpfl;
			*buffr++ = (float)smpfr;
		}

		if (HasSlaveSCSP)
		{
			if (DAC18B((&SCSPs[1])))
			{
				smprl = ICLIP18(smprl);
				smprr = ICLIP18(smprr);

				*bufrl++ = (float)smprl;
				*bufrr++ = (float)smprr;
			}
			else
			{
				smprl = ICLIP16(smprl >> 2);
				smprr = ICLIP16(smprr >> 2);

				*bufrl++ = (float)smprl;
				*bufrr++ = (float)smprr;
			}
		}
		else
		{
			*bufrl++ = (float)smprl;
			*bufrr++ = (float)smprr;
		}

#ifdef APPLY_MVOL
		// as MAME's output gain, so after the clipping above ([-1]: each buffer pointer has just advanced by one)
		{
			const float masterVol = (float)MVOL((&SCSPs[0])) / 15.0f;
			buffl[-1] *= masterVol;
			buffr[-1] *= masterVol;
			if (HasSlaveSCSP)
			{
				const float slaveVol = (float)MVOL((&SCSPs[1])) / 15.0f;
				bufrl[-1] *= slaveVol;
				bufrr[-1] *= slaveVol;
			}
		}
#endif

		SCSP_TimersAddTicks(1);

		// one sample interval interrupt (SCIPD bit 10)
		SCSPs[0].data[0x20 / 2] |= 0x0400;

		// MIDI output: nothing receives it on Model 3, so the FIFO just drains at the MIDI baud rate
		if (MidiOutFill != 0 && --MidiOutDrain <= 0)
		{
			SCSP_MidiOutR();
			MidiOutDrain = MIDI_OUT_SAMPLES_PER_BYTE;
		}

		CheckPendingIRQ();
		lastdiff = Run68kCB(slice - lastdiff);
		SampleCounter++;
	}
}

void SCSP_Update(int numSamples)
{
	SCSP_DoMasterSamples((std::min)(numSamples, length));
}

void SCSP_SetCB(int (*Run68k)(int cycles),void (*Int68k)(int irq))
{
	Int68kCB=Int68k;
	Run68kCB=Run68k;
}

// NOTE (not from/unlike MAME): bytes arrive instantly instead of at the serial MIDI rate, and the FIFO is bigger than the
// hardware's, on purpose (see MIDI_STACK_SIZE at the top of this file)
void SCSP_MidiIn(BYTE val)
{
	/*
	 * MIDI FIFO critical section
	 */
	if (s_multiThreaded)
		MIDILock->Lock();

	if (((MidiW + 1) & MIDI_STACK_SIZE_MASK) == MidiR)
	{
		MidiInOverflow = 1;		// FIFO full: the byte is lost (MIOVF)
		static bool reported = false;
		if (!reported)
		{
			ErrorLog("SCSP MIDI input FIFO overflow: MIDI commands were lost");
			reported = true;
		}
	}
	else
	{
		MidiStack[MidiW++]=val;
		MidiW&=MIDI_STACK_SIZE_MASK;
	}

	if (s_multiThreaded)
		MIDILock->Unlock();
}
void SCSP_MidiOutW(BYTE val)
{
	/*
	 * MIDI FIFO critical section
	 */
	if (s_multiThreaded)
		MIDILock->Lock();

	if (MidiOutFill < MIDI_OUT_FIFO_SIZE)
	{
		if (MidiOutFill == 0)
			MidiOutDrain = MIDI_OUT_SAMPLES_PER_BYTE;
		MidiOutStack[MidiOutW++]=val;		// own buffer: this used to overwrite the MIDI input FIFO
		MidiOutW&=15;
		++MidiOutFill;
	}

	if (s_multiThreaded)
		MIDILock->Unlock();
}

unsigned char SCSP_MidiOutR()
{
	unsigned char val;

	if(MidiOutR==MidiOutW)	// I don't think this needs to be a critical section...
		return 0xff;

	/*
	 * MIDI FIFO critical section
	 */
	if (s_multiThreaded)
		MIDILock->Lock();

	val=MidiOutStack[MidiOutR++];
	MidiOutR&=15;
	--MidiOutFill;

	if (s_multiThreaded)
		MIDILock->Unlock();

	return val;
}
void SCSP_Master_w8(unsigned int addr,unsigned char val)
{
	SCSP=SCSPs+0;
	SCSP_w8(addr,val);
}

void SCSP_Master_w16(unsigned int addr,unsigned short val)
{
	SCSP=SCSPs+0;
	SCSP_w16(addr,val);
}

void SCSP_Master_w32(unsigned int addr,unsigned int val)
{
	SCSP=SCSPs+0;
	SCSP_w32(addr,val);
}

void SCSP_Slave_w8(unsigned int addr,unsigned char val)
{
	SCSP=SCSPs+1;
	SCSP_w8(addr,val);
}

void SCSP_Slave_w16(unsigned int addr,unsigned short val)
{
	SCSP=SCSPs+1;
	SCSP_w16(addr,val);
}

void SCSP_Slave_w32(unsigned int addr,unsigned int val)
{
	SCSP=SCSPs+1;
	SCSP_w32(addr,val);
}

unsigned char SCSP_Master_r8(unsigned int addr)
{
	SCSP=SCSPs+0;
	return SCSP_r8(addr);
}

unsigned short SCSP_Master_r16(unsigned int addr)
{
	SCSP=SCSPs+0;
	return SCSP_r16(addr);
}

unsigned int SCSP_Master_r32(unsigned int addr)
{
	SCSP=SCSPs+0;
	return SCSP_r32(addr);
}

unsigned char SCSP_Slave_r8(unsigned int addr)
{
	SCSP=SCSPs+1;
	return SCSP_r8(addr);
}

unsigned short SCSP_Slave_r16(unsigned int addr)
{
	SCSP=SCSPs+1;
	return SCSP_r16(addr);
}

unsigned int SCSP_Slave_r32(unsigned int addr)
{
	SCSP=SCSPs+1;
	return SCSP_r32(addr);
}


/******************************************************************************
 Supermodel Interface Functions
******************************************************************************/

// Save state helpers: write or read a variable or array as raw bytes
template <typename T> static void SaveVar(CBlockFile *StateFile, const T &v) { StateFile->Write(&v, sizeof(v)); }
template <typename T> static void LoadVar(CBlockFile *StateFile, T &v) { StateFile->Read(&v, sizeof(v)); }

static const char *s_stateBlockName = "SCSP x 2 v6";	// renamed when the format changed: older states are rejected

void SCSP_SaveState(CBlockFile *StateFile)
{
	StateFile->NewBlock(s_stateBlockName, __FILE__);

	// Global state. Tables built by SCSP_Init() (EG_TABLE, pan tables) are not saved.
	SaveVar(StateFile, MidiStack);
	SaveVar(StateFile, MidiW);
	SaveVar(StateFile, MidiR);
	SaveVar(StateFile, MidiInOverflow);
	SaveVar(StateFile, MidiOutStack);
	SaveVar(StateFile, MidiOutW);
	SaveVar(StateFile, MidiOutR);
	SaveVar(StateFile, MidiOutFill);
	SaveVar(StateFile, MidiOutDrain);
	SaveVar(StateFile, TimCnt);
	SaveVar(StateFile, SampleCounter);
	SaveVar(StateFile, lastdiff);

	for (int i = 0; i < 2; i++)
	{
		_SCSP &chip = SCSPs[i];
		SaveVar(StateFile, chip.datab);
		SaveVar(StateFile, chip.RINGBUF);
		SaveVar(StateFile, chip.BUFPTR);
		SaveVar(StateFile, chip.DELAYBUF);
		SaveVar(StateFile, chip.DELAYVALID);
		SaveVar(StateFile, chip.DELAYPTR);
		SaveVar(StateFile, chip.Master);
		SaveVar(StateFile, chip.LFSR);

		for (int j = 0; j < 32; j++)
		{
			_SLOT &slot = chip.Slots[j];
			UINT32 baseOffset = (UINT32) (slot.base - chip.SCSPRAM);
			UINT8 egState = slot.EG.state;
			SaveVar(StateFile, slot.datab);
			SaveVar(StateFile, slot.active);
			SaveVar(StateFile, baseOffset);
			SaveVar(StateFile, slot.cur_addr);
			SaveVar(StateFile, slot.nxt_addr);
			SaveVar(StateFile, slot.Back);

			// EG
			SaveVar(StateFile, slot.EG.att);
			SaveVar(StateFile, egState);

			// LFO
			SaveVar(StateFile, slot.LFO.counter);
			SaveVar(StateFile, slot.LFO.timeCounter);
		}

		// DSP
		SaveVar(StateFile, chip.DSP.RBP);
		SaveVar(StateFile, chip.DSP.RBL);
		SaveVar(StateFile, chip.DSP.COEF);
		SaveVar(StateFile, chip.DSP.MADRS);
		SaveVar(StateFile, chip.DSP.MPRO);
		SaveVar(StateFile, chip.DSP.TEMP);
		SaveVar(StateFile, chip.DSP.MEMS);
		SaveVar(StateFile, chip.DSP.DEC);
		SaveVar(StateFile, chip.DSP.MIXS);
		SaveVar(StateFile, chip.DSP.EXTS);
		SaveVar(StateFile, chip.DSP.EFREG);
		SaveVar(StateFile, chip.DSP.Stopped);
		SaveVar(StateFile, chip.DSP.LastStep);
		SaveVar(StateFile, chip.DSP.ACC);
		SaveVar(StateFile, chip.DSP.INPUTS);
		SaveVar(StateFile, chip.DSP.FRC_REG);
		SaveVar(StateFile, chip.DSP.Y_REG);
		SaveVar(StateFile, chip.DSP.ADRS_REG);
		SaveVar(StateFile, chip.DSP.MEMVAL);
		SaveVar(StateFile, chip.DSP.RWADDR);
		SaveVar(StateFile, chip.DSP.WRITEVAL);
		SaveVar(StateFile, chip.DSP.READPENDING);
		SaveVar(StateFile, chip.DSP.WRITEPENDING);
	}

	// Timers (separate block so that "SCSP x 2 v4" states from before this timer model still load)
	StateFile->NewBlock("SCSP timers", __FILE__);
	SaveVar(StateFile, TimCnt);
	SaveVar(StateFile, TimReload);
}

void SCSP_LoadState(CBlockFile *StateFile)
{
	if (Result::OKAY != StateFile->FindBlock(s_stateBlockName))
	{
		ErrorLog("Unable to load SCSP state. Save state file is corrupt or from an older version.");
		return;
	}

	LoadVar(StateFile, MidiStack);
	LoadVar(StateFile, MidiW);
	LoadVar(StateFile, MidiR);
	LoadVar(StateFile, MidiInOverflow);
	LoadVar(StateFile, MidiOutStack);
	LoadVar(StateFile, MidiOutW);
	LoadVar(StateFile, MidiOutR);
	LoadVar(StateFile, MidiOutFill);
	LoadVar(StateFile, MidiOutDrain);
	LoadVar(StateFile, TimCnt);
	LoadVar(StateFile, SampleCounter);
	LoadVar(StateFile, lastdiff);
	MidiW &= MIDI_STACK_SIZE_MASK;
	MidiR &= MIDI_STACK_SIZE_MASK;
	MidiOutW &= 15;
	MidiOutR &= 15;

	for (int i = 0; i < 2; i++)
	{
		_SCSP &chip = SCSPs[i];
		LoadVar(StateFile, chip.datab);
		LoadVar(StateFile, chip.RINGBUF);
		LoadVar(StateFile, chip.BUFPTR);
		LoadVar(StateFile, chip.DELAYBUF);
		LoadVar(StateFile, chip.DELAYVALID);
		LoadVar(StateFile, chip.DELAYPTR);
		LoadVar(StateFile, chip.Master);
		LoadVar(StateFile, chip.LFSR);
		chip.LFSR &= 0x1FFFF;
		if (chip.LFSR == 0)
			chip.LFSR = 1;
		chip.BUFPTR &= 63;
		chip.DELAYPTR %= FM_DELAY;

		for (int j = 0; j < 32; j++)
		{
			_SLOT &slot = chip.Slots[j];
			UINT32 baseOffset;
			UINT8 egState;
			LoadVar(StateFile, slot.datab);
			LoadVar(StateFile, slot.active);
			LoadVar(StateFile, baseOffset);
			LoadVar(StateFile, slot.cur_addr);
			LoadVar(StateFile, slot.nxt_addr);
			LoadVar(StateFile, slot.Back);
			slot.base = &chip.SCSPRAM[baseOffset & SOUND_RAM_MASK];

			// EG
			LoadVar(StateFile, slot.EG.att);
			LoadVar(StateFile, egState);
			slot.EG.state = (_STATE) (egState & 3);

			// LFO
			LoadVar(StateFile, slot.LFO.counter);
			LoadVar(StateFile, slot.LFO.timeCounter);
			if (slot.LFO.timeCounter == 0)
				slot.LFO.timeCounter = 1;
		}

		// DSP
		LoadVar(StateFile, chip.DSP.RBP);
		LoadVar(StateFile, chip.DSP.RBL);
		LoadVar(StateFile, chip.DSP.COEF);
		LoadVar(StateFile, chip.DSP.MADRS);
		LoadVar(StateFile, chip.DSP.MPRO);
		LoadVar(StateFile, chip.DSP.TEMP);
		LoadVar(StateFile, chip.DSP.MEMS);
		LoadVar(StateFile, chip.DSP.DEC);
		LoadVar(StateFile, chip.DSP.MIXS);
		LoadVar(StateFile, chip.DSP.EXTS);
		LoadVar(StateFile, chip.DSP.EFREG);
		LoadVar(StateFile, chip.DSP.Stopped);
		LoadVar(StateFile, chip.DSP.LastStep);
		LoadVar(StateFile, chip.DSP.ACC);
		LoadVar(StateFile, chip.DSP.INPUTS);
		LoadVar(StateFile, chip.DSP.FRC_REG);
		LoadVar(StateFile, chip.DSP.Y_REG);
		LoadVar(StateFile, chip.DSP.ADRS_REG);
		LoadVar(StateFile, chip.DSP.MEMVAL);
		LoadVar(StateFile, chip.DSP.RWADDR);
		LoadVar(StateFile, chip.DSP.WRITEVAL);
		LoadVar(StateFile, chip.DSP.READPENDING);
		LoadVar(StateFile, chip.DSP.WRITEPENDING);
	}

	if (Result::OKAY == StateFile->FindBlock("SCSP timers"))
	{
		LoadVar(StateFile, TimCnt);
		LoadVar(StateFile, TimReload);
	}
	else
	{
		// Older state: the counts had 8 fraction bits
		for (int t = 0; t < 3; t++)
		{
			TimCnt[t] = (TimCnt[t] >> 8) & 0xFF;
			TimReload[t] = -1;
		}
	}
}

void SCSP_SetBuffers(float *leftBufferPtr, float *rightBufferPtr, float* leftRearBufferPtr, float* rightRearBufferPtr, int bufferLength)
{
	bufferfl = leftBufferPtr;
	bufferfr = rightBufferPtr;
	bufferrl = leftRearBufferPtr;
	bufferrr = rightRearBufferPtr;

	length = bufferLength;
}

void SCSP_Deinit(void)
{
	delete MIDILock;
	MIDILock = NULL;
}
