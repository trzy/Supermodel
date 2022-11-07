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
 * DSB.cpp
 *
 * Sega Digital Sound Board (MPEG audio). Implementation of the CDSB1 and CDSB2
 * classes. Based on code donated by R. Belmont. Many Bothans died to bring us
 * this emulation.
 *
 * TODO List
 * ---------
 * - Should MPEG_SetLoop() check for loopEnd==0? This causes crashes. Usually
 *   only occurs when loopStart is also 0, and that can only be checked here.
 * - Check actual MPEG sample rate. So far, all games seem to use 32 KHz, which
 *   may be a hardware requirement, but if other sampling rates are allowable,
 *   the code here will fail (it is hard coded for 32 KHz).
 * - Should we do some bounds checking on the MPEG start/end points?
 */

#include "DSB.h"

#include "Supermodel.h"
#include "Sound/MPEG/MpegAudio.h"
#include <algorithm>

/******************************************************************************
 Resampler

 MPEG Layer 2 audio can be 32, 44.1, or 48 KHz. Here, an up-sampling algorithm
 is provided, which should work for any frequency less than 44.1 KHz and an
 output frequency of 44.1 KHz. Down-sampling is not yet implemented, but would
 work in a similar fashion. The chief difference is that the input index would
 sometimes advance by more than one for a single output sample and the
 fractions, nFrac and pFrac, would sometimes exceed 1.0.

 Up-Sampling Description
 -----------------------

 Linear interpolation is used to up-sample. Not as accurate as the Shannon
 reconstruction equation but it seems to work quite well.

 1. Linear Interpolation

 Input samples for a given frame (here, this means 1/60Hz, not to be confused
 with an MPEG frame, which is shorter) are numbered 0 ... L-1 (L samples in
 total). Output samples are 0 ... M-1.

 For two adjacent input samples at times p ("previous") and n ("next"), in[p]
 and in[n], and output out[t] at time t, linear interpolation yields:

 		out[t] = (n-t)/(n-p) * in[p] + (t-p)/(n-p) * in[n]

 Note that (n-p) = 1/fin (fin being the input sampling frequency).

 Let pFrac = (n-t)/(n-p) and nFrac = (t-p)/(n-p). As t moves from p to n, pFrac
 moves from 1 to 0 and nFrac from 0 to 1, as we expect.

 If we proceed one output sample at a time, we must add the time difference
 between output samples, 1/fout, to t. Call this delta_t. If we divide delta_t
 by (n-p), we can add it directly to nFrac and subtract from pFrac. Therefore:

 	delta = (1/fout)/(n-p) = fin/fout

 What happens when nFrac exceeds 1.0 or pFrac goes below 0.0? That can't
 be allowed to happen -- it means that we've actually moved along the line into
 the region between the next set of samples. We use pFrac < 0 as the condition
 to update the input samples.

 It so happens that when fin < fout, pFrac and nFrac will never exceed 1.0. So
 there is no need to check or mask the fixed point values when using them to
 interpolate samples.

 2. Input Buffer Overflows

 For some low sampling rates, particularly those that are a factor of 2 or 4
 smaller, it is possible that the very last sample or two needed from the input
 stream will be beyond the end. Fetching two extra samples (which can introduce
 an update lag of two samples -- imperceptible and inconsequential) fixes this,
 and so we do it.

 3. Continuity Between Frames

 The very last output sample will typically sit somewhere between two input
 samples. It is wrong to start the next frame by assuming everything is lined
 up again. The first sample of the next frame will often have to be interpol-
 ated with the last sample of the previous frame. To facilitate this, we check
 to see how many input samples remain unprocessed when up-sampling is finished,
 and then copy those to the beginning of the buffer. We then return the number
 of samples so that the buffer update function will know to skip them.

 We also must maintain the state of pFrac and nFrac to resume interpolation
 correctly. Therefore, these variables are persistent.

 4. Fixed Point Arithmetic

 Fixed point arithmetic is used to track fractions. For such numbers, the low
 8 bits represent a fraction (0x100 would be 1.0, 0x080 would be 0.5, etc.)
 and the upper bits are the integral portion.
******************************************************************************/

void CDSBResampler::Reset(void)
{
	// Initial state of fractions (24.8 fixed point)
	nFrac = 0<<8;	// fraction of next sample to use (0->1.0 as x moves p->n)
 	pFrac = 1<<8;	// previous sample (1.0->0 as x moves p->n)
}

// Mixes audio and returns number of samples copied back to start of buffer (ie. offset at which new samples should be written)
int CDSBResampler::UpSampleAndMix(float *outL, float *outR, INT16 *inL, INT16 *inR, UINT8 volumeL, UINT8 volumeR, int sizeOut, int sizeIn, int outRate, int inRate)
{
	int 	delta = (inRate<<8)/outRate;	// (1/fout)/(1/fin)=fin/fout, 24.8 fixed point
	int		outIdx = 0;
	int		inIdx = 0;
	float	v[2], musicVol;

	// Obtain program volume settings
	musicVol = (float)std::max(0,std::min(200,m_config["MusicVolume"].ValueAs<int>()));
	musicVol = musicVol * (float) (1.0 / 100.0);

	v[0] = musicVol * (float) volumeL * (float) (1.0 / (255.0*256.0)); // 256 is there to correct for fixed point interpolation below
	v[1] = musicVol * (float) volumeR * (float) (1.0 / (255.0*256.0));

	// Up-sample and mix!
	while (outIdx < sizeOut)
	{
		// nFrac, pFrac will never exceed 1.0 (0x100) (only true if delta does not exceed 1)
		INT32 leftSample  = (int)inL[inIdx]*pFrac+(int)inL[inIdx+1]*nFrac;	// left channel
		INT32 rightSample = (int)inR[inIdx]*pFrac+(int)inR[inIdx+1]*nFrac;	// right channel

		// Apply DSB volume+overall music volume setting, and mix into output
		outL[outIdx] += (float)leftSample  * v[0];
		outR[outIdx] += (float)rightSample * v[1];
		outIdx++;

		// Time step
		pFrac -= delta;
		nFrac += delta;

		// Time to move to next samples?
		if (pFrac <= 0)	// when pFrac becomes 0, advance samples, reset pFrac to 1
		{
			pFrac += (1<<8);
			nFrac -= (1<<8);
			inIdx++;	// advance samples (for upsampling only; downsampling may advance by more than one -- add delta every loop iteration)
		}
	}

	// Copy remaining "active" input samples to start of buffer
	int i = 0;
	int j = inIdx;
	while (j < sizeIn)
	{
		inL[i] = inL[j];
		inR[i] = inR[j];
		i++;
		j++;
	}
	return i;	// first free position in input buffer to copy next MPEG update to
}


/******************************************************************************
 Digital Sound Board Type 1: Z80 CPU
******************************************************************************/

UINT8 CDSB1::Read8(UINT32 addr)
{
	// ROM: 0x0000-0x7FFF
	if (addr < 0x8000)
		return progROM[addr];

	// RAM: 0x8000-0xFFFF
	return ram[addr&0x7FFF];
}

void CDSB1::Write8(UINT32 addr, UINT8 data)
{
	if (addr >= 0x8000)
		ram[addr&0x7FFF] = data;
}

void CDSB1::IOWrite8(UINT32 addr, UINT8 data)
{
	switch ((addr&0xFF))
	{
	case 0xE0:	// MPEG trigger
		mpegState = data;

		if (data == 0)	// stop
		{
			MpegDec::Stop();
			return;
		}

		if (data == 1)	// play without loop
		{
			//printf("====> Playing %06X (mpegEnd=%06X)\n", mpegStart, mpegEnd);

			usingLoopStart	= 0;		// save the settings of the MPEG currently playing
			usingLoopEnd	= 0;
			usingMPEGStart	= mpegStart;
			usingMPEGEnd	= mpegEnd;

			MpegDec::SetMemory(&mpegROM[mpegStart], mpegEnd - mpegStart, false);
			return;
		}

		if (data == 2)	// play with loop (NOTE: I don't think this actually loops b/c MPEG_PlayMemory() clears lstart)
		{
			//printf("====> Playing w/ loop %06X (mpegEnd=%06X, loopStart=%06X, loopEnd=%06X)\n", mpegStart, mpegEnd, loopStart, loopEnd);
			usingLoopStart	= 0;		// MPEG_PlayMemory() clears these settings and Z80 will set them up later
			usingLoopEnd	= 0;
			usingMPEGStart	= mpegStart;
			usingMPEGEnd	= mpegEnd;

			MpegDec::SetMemory(&mpegROM[mpegStart], mpegEnd - mpegStart, false);		// assume not looped for now
			return;
		}
		break;

	case 0xE2:	// MPEG start, high byte
		startLatch &= 0x00FFFF;
		startLatch |= ((UINT32)data) << 16;
		break;

	case 0xE3:	// MPEG start, middle byte
		startLatch &= 0xFF00FF;
		startLatch |= ((UINT32)data) << 8;
		break;

	case 0xE4:	// MPEG start, low byte
		startLatch &= 0xFFFF00;
		startLatch |= data;

		if (mpegState == 0)
		{
			mpegStart = startLatch;
			//printf("mpegStart = %08X\n", mpegStart);
		}
		else
		{
			loopStart = startLatch;
			//printf("loopStart = %08X\n", loopStart);
			// SWA: if loop end is zero, it means "keep previous end marker"
			if (loopEnd == 0)
			{
				usingLoopStart	= loopStart;
				usingLoopEnd	= mpegEnd-loopStart;
				MpegDec::UpdateMemory(&mpegROM[usingLoopStart], usingLoopEnd, true);
			}
			else
			{
				usingLoopStart	= loopStart;
				usingLoopEnd	= loopEnd-loopStart;
				MpegDec::UpdateMemory(&mpegROM[usingLoopStart], usingLoopEnd, true);
			}
		}

		break;

	case 0xE5:	// MPEG end, high byte
		endLatch &= 0x00FFFF;
		endLatch |= ((UINT32)data) << 16;
		break;

	case 0xE6:	// MPEG end, middle byte
		endLatch &= 0xFF00FF;
		endLatch |= ((UINT32)data) << 8;
		break;

	case 0xE7:	// MPEG end, low byte
		endLatch &= 0xFFFF00;
		endLatch |= data;

		if (mpegState == 0)
		{
			mpegEnd = endLatch;
			//printf("mpegEnd = %08X\n", mpegEnd);
		}
		else
		{
			loopEnd			= endLatch;
			usingLoopStart	= loopStart;
			usingLoopEnd	= loopEnd-loopStart;
			MpegDec::UpdateMemory(&mpegROM[usingLoopStart], usingLoopEnd, true);
			//printf("loopEnd = %08X\n", loopEnd);
		}
		break;

	case 0xE8:	// MPEG volume
		volume = 0x7F-data;
		//printf("Set Volume: %02X\n", volume);
		break;

	case 0xE9:	// MPEG stereo
		stereo = data;
		break;

	case 0xF0:	// command echo back
		break;

	default:
		//printf("Z80 Port %02X=%08X\n", addr, data);
		break;
	}
}

UINT8 CDSB1::IORead8(UINT32 addr)
{
	int	progress;

	switch ((addr&0xFF))
	{
	case 0xE2:	// MPEG position, high byte
		progress = MpegDec::GetPosition();
		progress += mpegStart;	// byte address currently playing
		return (progress>>16)&0xFF;

	case 0xE3:	// MPEG position, middle byte
		progress = MpegDec::GetPosition();
		progress += mpegStart;
		return (progress>>8)&0xFF;

	case 0xE4:	// MPEG position, low byte
		progress = MpegDec::GetPosition();
		progress += mpegStart;
		return progress&0xFF;

	case 0xF0:	// Latch
		UINT8 d;
		d = fifo[fifoIdxR];	// retrieve next command byte
		if (fifoIdxR != fifoIdxW)	// if these are equal, nothing has been written yet (don't advance)
		{
			fifoIdxR++;
			fifoIdxR &= FIFO_STACK_SIZE_MASK;
		}

		if (fifoIdxR == fifoIdxW)	// FIFO empty?
			status &= ~2;			// yes, indicate no commands left
		else
			status |= 2;

		Z80.SetINT(false);	// clear IRQ
		//printf("Z80: INT cleared, read from FIFO\n");
		return d;

	case 0xF1:	// Status
		/*
		 * Bit 0: Must be 1 for most games.
		 * Bit 1: Command pending (used by SWA instead of IRQ)
		 * SWA requires (status&0x38)==0 or else it loops endlessly
		 */
		return status;
	}

	//printf("Z80 Port Read %02X\n", addr);
	return 0;
}

static int Z80IRQCallback(CZ80 *Z80)
{
	return 0x38;
}

void CDSB1::SendCommand(UINT8 data)
{
	/*
	 * Commands are buffered in a FIFO. This probably does not actually exist
	 * on the real DSB but is necessary because the Z80 is not really synced
	 * up with the other CPUs and must process all commands it has received
	 * over the course of a frame at once.
	 */
	fifo[fifoIdxW++] = data;
	fifoIdxW &= FIFO_STACK_SIZE_MASK;
	//printf("Write FIFO: %02X\n", data);

	// Have we caught up to the read pointer?
#ifdef DEBUG
	if (fifoIdxW == fifoIdxR)
		printf("DSB1 FIFO overflow!\n");
#endif
}

void CDSB1::RunFrame(float *audioL, float *audioR)
{
	if (!m_config["EmulateDSB"].ValueAs<bool>())
	{
		// DSB code applies SCSP volume, too, so we must still mix
		memset(mpegL, 0, (32000/60+2)*sizeof(INT16));
		memset(mpegR, 0, (32000/60+2)*sizeof(INT16));
		retainedSamples = Resampler.UpSampleAndMix(audioL, audioR, mpegL, mpegR, 0, 0, NUM_SAMPLES_PER_FRAME, 32000/60+2, 44100, 32000);
		return;
	}

	int		cycles;
	// While FIFO not empty, fire interrupts, run for up to one frame
	for (cycles = (4000000/60); (cycles > 0) && (fifoIdxR != fifoIdxW);  )
	{
		Z80.SetINT(true);	// fire an IRQ to indicate pending command
		//printf("Z80 INT fired\n");
		cycles -= Z80.Run(500);
	}

	// Run remaining cycles
	Z80.Run(cycles);

	//printf("VOLUME=%02X STEREO=%02X\n", volume, stereo);

	// Convert volume from 0x00-0x7F -> 0x00-0xFF
	UINT8 v = (UINT8) ((float) volume * (float)(255.0/127.0));

	// Decode MPEG for this frame
	MpegDec::DecodeAudio(&mpegL[retainedSamples], &mpegR[retainedSamples], 32000 / 60 - retainedSamples + 2);
	retainedSamples = Resampler.UpSampleAndMix(audioL, audioR, mpegL, mpegR, v, v, NUM_SAMPLES_PER_FRAME, 32000/60+2, 44100, 32000);
}

void CDSB1::Reset(void)
{
	MpegDec::Stop();
	Resampler.Reset();
	retainedSamples = 0;

	memset(fifo, 0, sizeof(fifo));
	fifoIdxW = fifoIdxR = 0;

	status = 1;
	mpegState = 0;	// why doesn't RB ever init this?
	volume = 0x7F;	// full volume
	usingLoopStart = 0;

	// Even if DSB emulation is disabled, must reset to establish valid Z80 state
	Z80.Reset();
	DebugLog("DSB1 Reset\n");
}

void CDSB1::SaveState(CBlockFile *StateFile)
{
	UINT32	playOffset, endOffset;
	UINT8	isPlaying;

	StateFile->NewBlock("DSB1", __FILE__);

	// MPEG playback state
	isPlaying	= (UINT8)MpegDec::IsLoaded();
	playOffset	= (UINT32)MpegDec::GetPosition();
	endOffset	= 0;

	StateFile->Write(&isPlaying, sizeof(isPlaying));
	StateFile->Write(&playOffset, sizeof(playOffset));
	StateFile->Write(&endOffset, sizeof(endOffset));
	StateFile->Write(&usingMPEGStart, sizeof(usingMPEGStart));
	StateFile->Write(&usingMPEGEnd, sizeof(usingMPEGEnd));
	StateFile->Write(&usingLoopStart, sizeof(usingLoopStart));
	StateFile->Write(&usingLoopEnd, sizeof(usingLoopEnd));

	// MPEG board state
	StateFile->Write(ram, 0x8000);
	StateFile->Write(fifo, sizeof(fifo));
	StateFile->Write(&fifoIdxR, sizeof(fifoIdxR));
	StateFile->Write(&fifoIdxW, sizeof(fifoIdxW));
	StateFile->Write(&mpegStart, sizeof(mpegStart));
	StateFile->Write(&mpegEnd, sizeof(mpegEnd));
	StateFile->Write(&mpegState, sizeof(mpegState));
	StateFile->Write(&loopStart, sizeof(loopStart));
	StateFile->Write(&loopEnd, sizeof(loopEnd));
	StateFile->Write(&status, sizeof(status));
	StateFile->Write(&cmdLatch, sizeof(cmdLatch));
	StateFile->Write(&volume, sizeof(volume));
	StateFile->Write(&stereo, sizeof(stereo));

	// Z80 CPU state
	Z80.SaveState(StateFile, "DSB1 Z80");
}

void CDSB1::LoadState(CBlockFile *StateFile)
{
	UINT32	playOffset, endOffset;
	UINT8	isPlaying;

	if (OKAY != StateFile->FindBlock("DSB1"))
	{
		ErrorLog("Unable to load Digital Sound Board state. Save state file is corrupt.");
		return;
	}

	StateFile->Read(&isPlaying, sizeof(isPlaying));
	StateFile->Read(&playOffset, sizeof(playOffset));
	StateFile->Read(&endOffset, sizeof(endOffset));
	StateFile->Read(&usingMPEGStart, sizeof(usingMPEGStart));
	StateFile->Read(&usingMPEGEnd, sizeof(usingMPEGEnd));
	StateFile->Read(&usingLoopStart, sizeof(usingLoopStart));
	StateFile->Read(&usingLoopEnd, sizeof(usingLoopEnd));
	StateFile->Read(ram, 0x8000);
	StateFile->Read(fifo, sizeof(fifo));
	StateFile->Read(&fifoIdxR, sizeof(fifoIdxR));
	StateFile->Read(&fifoIdxW, sizeof(fifoIdxW));
	StateFile->Read(&mpegStart, sizeof(mpegStart));
	StateFile->Read(&mpegEnd, sizeof(mpegEnd));
	StateFile->Read(&mpegState, sizeof(mpegState));
	StateFile->Read(&loopStart, sizeof(loopStart));
	StateFile->Read(&loopEnd, sizeof(loopEnd));
	StateFile->Read(&status, sizeof(status));
	StateFile->Read(&cmdLatch, sizeof(cmdLatch));
	StateFile->Read(&volume, sizeof(volume));
	StateFile->Read(&stereo, sizeof(stereo));

	Z80.LoadState(StateFile, "DSB1 Z80");

	// Restart MPEG audio at the appropriate position
	if (isPlaying)
	{
		MpegDec::SetMemory(&mpegROM[usingMPEGStart], usingMPEGEnd - usingMPEGStart, false);

		if (usingLoopEnd != 0) {	// only if looping was actually enabled
			MpegDec::UpdateMemory(&mpegROM[usingLoopStart], usingLoopEnd, true);
		}

		MpegDec::SetPosition(playOffset);
	}
	else {
		MpegDec::Stop();
	}
}

// Offsets of memory regions within DSB1's pool
#define DSB1_OFFSET_RAM			0		// 32KB Z80 RAM
#define DSB1_OFFSET_MPEG_LEFT	0x8000	// 1604 bytes (48 KHz max., 1/60th second, 2 extra = 2*(48000/60+2)) left MPEG buffer
#define DSB1_OFFSET_MPEG_RIGHT	0x8644	// 1604 bytes right MPEG buffer
#define DSB1_MEMORY_POOL_SIZE	(0x8000 + 0x644 + 0x644)

bool CDSB1::Init(const UINT8 *progROMPtr, const UINT8 *mpegROMPtr)
{
	float	memSizeMB = (float)DSB1_MEMORY_POOL_SIZE/(float)0x100000;

	// Receive ROM
	progROM = progROMPtr;
	mpegROM = mpegROMPtr;

	// Allocate memory pool
	memoryPool = new(std::nothrow) UINT8[DSB1_MEMORY_POOL_SIZE];
	if (NULL == memoryPool)
		return ErrorLog("Insufficient memory for DSB1 board (needs %1.1f MB).", memSizeMB);
	memset(memoryPool, 0, DSB1_MEMORY_POOL_SIZE);

	// Set up memory pointers
	ram = &memoryPool[DSB1_OFFSET_RAM];
	mpegL = (INT16 *) &memoryPool[DSB1_OFFSET_MPEG_LEFT];
	mpegR = (INT16 *) &memoryPool[DSB1_OFFSET_MPEG_RIGHT];

	// Initialize Z80 CPU
	Z80.Init(this, Z80IRQCallback);

	retainedSamples = 0;

	return OKAY;
}

CZ80 *CDSB1::GetZ80(void)
{
	return &Z80;
}

CDSB1::CDSB1(const Util::Config::Node &config)
  : m_config(config),
    Resampler(config)
{
	progROM		= NULL;
	mpegROM		= NULL;
	memoryPool	= NULL;
	ram			= NULL;
	mpegL		= NULL;
	mpegR		= NULL;

	// must init these otherwise we end up trying to read illegal addresses
	mpegStart	= 0;
	mpegEnd		= 0;
	mpegState	= 0;
	loopStart	= 0;
	loopEnd		= 0;

	DebugLog("Built DSB1 Board\n");
}

CDSB1::~CDSB1(void)
{
	if (memoryPool != NULL)
	{
		delete [] memoryPool;
		memoryPool = NULL;
	}

	progROM	= NULL;
	mpegROM	= NULL;
	ram		= NULL;
	mpegL	= NULL;
	mpegR	= NULL;

	DebugLog("Destroyed DSB1 Board\n");
}


/******************************************************************************
 Digital Sound Board Type 2: 68K CPU
******************************************************************************/

// MPEG state machine
enum
{
	ST_IDLE = 0,
	ST_GOT14,	// start/loop addr
	ST_14_0,
	ST_14_1,
	ST_GOT24,	// end addr
	ST_24_0,
	ST_24_1,
	ST_GOT74,
	ST_GOTA0,
	ST_GOTA1,
	ST_GOTA3,
	ST_GOTA4,
	ST_GOTA5,
	ST_GOTA7,
	ST_GOTB0,
	ST_GOTB1,
	ST_GOTB2,
	ST_GOTB4,
	ST_GOTB5,
	ST_GOTB6
};

static const char *stateName[] =
{
	"idle",
	"st_got_14",
	"st_14_0",
	"st_14_1",
	"st_got24",
	"st_24_0",
	"st_24_1",
	"st_got74",
	"st_gota0",
	"st_gota1",
	"st_gota3",
	"st_gota4",
	"st_gota5",
	"st_gota7",
	"st_gotb0",
	"st_gotb1",
	"st_gotb2",
	"st_gotb4",
	"st_gotb5",
	"st_gotb6"
};

void CDSB2::WriteMPEGFIFO(UINT8 byte)
{
	//printf("fifo: %x (state %s)\n", byte, stateName[mpegState]);
	switch (mpegState)
	{
		case ST_IDLE:
			if (byte == 0x14) mpegState = ST_GOT14;
			else if (byte == 0x15) mpegState = ST_GOT14;
			else if (byte == 0x24) mpegState = ST_GOT24;
			else if (byte == 0x25) mpegState = ST_GOT24;

			else if (byte == 0x74 || byte == 0x75)	// "start play"
			{
				usingLoopStart	= 0;
				usingLoopEnd	= 0;
				usingMPEGStart	= mpegStart;
				usingMPEGEnd	= mpegEnd;
				playing			= 1;

				MpegDec::SetMemory(&mpegROM[mpegStart], mpegEnd - mpegStart, false);

				mpegState = ST_IDLE;
			}

			else if (byte == 0x84 || byte == 0x85)
			{
				MpegDec::Stop();
				playing = 0;
			}

			else if (byte == 0xa0) mpegState = ST_GOTA0;
			else if (byte == 0xa1) mpegState = ST_GOTA1;
			else if (byte == 0xA3) mpegState = ST_GOTA3;	// volume (front right?)
			else if (byte == 0xa4) mpegState = ST_GOTA4;
			else if (byte == 0xa5) mpegState = ST_GOTA5;
			else if (byte == 0xA7) mpegState = ST_GOTA7;	// volume (rear right?)

			else if (byte == 0xb0) mpegState = ST_GOTB0;
			else if (byte == 0xb1) mpegState = ST_GOTB1;
			else if (byte == 0xB2) mpegState = ST_GOTB2;	// volume (front left?)
			else if (byte == 0xb4) mpegState = ST_GOTB4;
			else if (byte == 0xb5) mpegState = ST_GOTB5;
			else if (byte == 0xB6) mpegState = ST_GOTB6;	// volume (rear left?)

			break;

		case ST_GOT14:
			mpegStart &= 0x00FFFF;
			mpegStart |= (byte<<16);
			mpegState++;
			break;
		case ST_14_0:
			mpegStart &= 0xFF00FF;
			mpegStart |= (byte<<8);
			mpegState++;
			break;
		case ST_14_1:
			mpegStart &= 0xFFFF00;
			mpegStart |= (byte);
			mpegState = ST_IDLE;

			if (playing)
			{
				usingLoopStart	= mpegStart;
				usingLoopEnd	= mpegEnd - mpegStart;
				MpegDec::UpdateMemory(&mpegROM[usingLoopStart], usingLoopEnd, true);
			}

			break;
		case ST_GOT24:
			mpegEnd &= 0x00FFFF;
			mpegEnd |= (byte<<16);
			mpegState++;
			break;
		case ST_24_0:
			mpegEnd &= 0xFF00FF;
			mpegEnd |= (byte<<8);
			mpegState++;
			break;
		case ST_24_1:
			mpegEnd &= 0xFFFF00;
			mpegEnd |= (byte);

			stereo = StereoMode::Stereo;
			mpegState = ST_IDLE;
			break;

		case ST_GOTA0:
			stereo = (byte != 0x00) ? StereoMode::MonoLeft : StereoMode::Stereo;
			mpegState = ST_IDLE;
			break;

		case ST_GOTA4:	// dayto2pe plays advertise tune from this state by writing 0x75
			mpegState = ST_IDLE;
			if (byte == 0x75)
			{
				usingLoopStart	= 0;
				usingLoopEnd	= 0;
				usingMPEGStart	= mpegStart;
				usingMPEGEnd	= mpegEnd;
				playing			= 1;
				MpegDec::SetMemory(&mpegROM[mpegStart], mpegEnd - mpegStart, false);
			}
			break;
		case ST_GOTA5:
			mpegState = ST_IDLE;
			break;
		case ST_GOTB1:
			stereo = (byte != 0x00) ? StereoMode::MonoRight : StereoMode::Stereo;
			mpegState = ST_IDLE;
			break;
		case ST_GOTB4:
			mpegState = ST_IDLE;
			if (byte == 0x96)
			{
				MpegDec::Stop();
				playing = 0;
			}
			break;
		case ST_GOTB5:
			mpegState = ST_IDLE;
			break;

		/*
		 * Speaker Volume:
		 *
		 * Daytona 2 uses B6, A7 for rear speakers; B2, A3 for front speakers.
		 *
		 * Star Wars Trilogy uses B0, A1 but B4, A4, B1, A1, B5, and A5 may be
		 * volume/speaker related.
		 *
		 * Sega Rally 2 uses B0, A0 and B4, A4 when setting all four but B0, A1
		 * when only setting two (in-game)
		 */
		case ST_GOTB6:	// rear left(?) volume
			volume[0] = byte;
			mpegState = ST_IDLE;
			break;
		case ST_GOTB0:	// left volume
			volume[0] = byte;
			//printf("Set L Volume: %02X\n", byte);
			mpegState = ST_IDLE;
			break;
		case ST_GOTA7:	// rear right(?) volume
			volume[1] = byte;
			mpegState = ST_IDLE;
			break;
		case ST_GOTA1:	// right volume
			volume[1] = byte;
			mpegState = ST_IDLE;
			break;
		/*case ST_GOTA0:
			volume[1] = byte;
			//printf("Set R Volume: %02X\n", byte);
			mpegState = ST_IDLE;
			break;*/
		case ST_GOTB2:
			mpegState = ST_IDLE;
			break;
		case ST_GOTA3:
			mpegState = ST_IDLE;
			break;
		default:
			break;
	}
}

UINT8 CDSB2::Read8(UINT32 addr)
{
	if (addr < (128*1024))
		return progROM[addr^1];

	if (addr == 0xc00001)
	{
		return cmdLatch;
	}
	if (addr == 0xc00003)	// bit 0 = command valid
	{
		return 1;
	}

	if (addr == 0xe80001)
	{
		return 0x01;	// MPEG busy status: bit 1 = busy
	}			// polled by irq2, stored | 0x10 at f01010

	if ((addr >= 0xf00000) && (addr < 0xf10000))
	{
		addr &= 0x1ffff;
		return ram[addr^1];
	}

	//printf("R8 @ %x\n", addr);
	return 0;
}

UINT16 CDSB2::Read16(UINT32 addr)
{
	if (addr < (128*1024))
	{
		return *(UINT16 *) &progROM[addr];
	}
	if ((addr >= 0xf00000) && (addr < 0xf20000))
	{
		addr &= 0x1ffff;
		return *(UINT16 *) &ram[addr];
	}

	//printf("R16 @ %x\n", addr);
	return 0;
}

UINT32 CDSB2::Read32(UINT32 addr)
{
	UINT32	hi, lo;

	if (addr < (128*1024))
	{
		hi = *(UINT16 *) &progROM[addr];
		lo = *(UINT16 *) &progROM[addr+2];
		return (hi<<16)|lo;
	}
	if ((addr >= 0xf00000) && (addr < 0xf20000))
	{
		addr &= 0x1ffff;
		hi = *(UINT16 *) &ram[addr];
		lo = *(UINT16 *) &ram[addr+2];
		return (hi<<16)|lo;
	}
	//printf("R32 @ %x\n", addr);
	return 0;
}

void CDSB2::Write8(UINT32 addr, UINT8 data)
{
	if ((addr >= 0xf00000) && (addr < 0xf20000))
	{
		addr &= 0x1ffff;
		ram[addr^1] = data;
		return;
	}

	if (addr == 0xd00001) return;

	if (addr == 0xe00003)
	{
		WriteMPEGFIFO(data);
		return;
	}

	//printf("W8: %x @ %x (PC=%x)\n", data, addr, m68k_get_reg(NULL, M68K_REG_PC));
}

void CDSB2::Write16(UINT32 addr, UINT16 data)
{
	if ((addr >= 0xf00000) && (addr < 0xf20000))
	{
		addr &= 0x1ffff;
		*(UINT16 *) &ram[addr] = data;
		return;
	}
	//printf("W16: %x @ %x\n", data, addr);
}

void CDSB2::Write32(UINT32 addr, UINT32 data)
{
	if ((addr >= 0xf00000) && (addr < 0xf20000))
	{
		addr &= 0x1ffff;
		*(UINT16 *) &ram[addr+0] = (data>>16);
		*(UINT16 *) &ram[addr+2] = data&0xFFFF;
		return;
	}
	//printf("W32: %x @ %x\n", data, addr);
}

void CDSB2::SendCommand(UINT8 data)
{
	/*
	 * Commands are buffered in a FIFO. This probably does not actually exist
	 * on the real DSB but is necessary because the 68K is not really synced
	 * up with the other CPUs and must process all commands it has received
	 * over the course of a frame at once.
	 */
	fifo[fifoIdxW++] = data;
	fifoIdxW &= FIFO_STACK_SIZE_MASK;
	//printf("Write FIFO: %02X\n", data);

	// Have we caught up to the read pointer?
#ifdef DEBUG
	if (fifoIdxW == fifoIdxR)
		printf("DSB2 FIFO overflow!\n");
#endif
}


void CDSB2::RunFrame(float *audioL, float *audioR)
{
  if (!m_config["EmulateDSB"].ValueAs<bool>())
  {
    // DSB code applies SCSP volume, too, so we must still mix
    memset(mpegL, 0, (32000/60+2) * sizeof(INT16));
    memset(mpegR, 0, (32000/60+2) * sizeof(INT16));
    retainedSamples = Resampler.UpSampleAndMix(audioL, audioR, mpegL, mpegR, 0, 0, NUM_SAMPLES_PER_FRAME, 32000/60+2, 44100, 32000);
    return;
  }

  M68KSetContext(&M68K);
  //printf("DSB2 run frame PC=%06X\n", M68KGetPC());

  // While FIFO not empty...
  while (fifoIdxR != fifoIdxW)
  {
    cmdLatch = fifo[fifoIdxR];	// retrieve next command byte
    fifoIdxR++;
    fifoIdxR &= FIFO_STACK_SIZE_MASK;

    M68KSetIRQ(1);	// indicate pending command
    //printf("68K INT fired\n");
    m_cyclesElapsedThisFrame += M68KRun(500);
  }

  // Matthew Daniels made the interesting discovery that IRQ2 may in fact be a timer interrupt
  // rather than a per-frame interrupt.For Daytona 2 and Sega Rally 2, assuming a value
  // of 1KHz fixes music fade outs and some timing issues. It is very likely this is a
  // configurable timer and we should be on the look-out for games which appear to use
  // different values. It is equally likely that all games share a similar code base and
  // use 1KHz as the timer rate.
  while (m_cyclesElapsedThisFrame < k_framePeriod)
  {
    if (m_cyclesElapsedThisFrame >= m_nextTimerInterruptCycles)
    {
      // Fire timer interrupt and schedule next one
      M68KSetIRQ(2);
      m_nextTimerInterruptCycles += k_timerPeriod;
    }
    int cyclesToRun = (std::min)(m_nextTimerInterruptCycles, k_framePeriod) - m_cyclesElapsedThisFrame;
    m_cyclesElapsedThisFrame += M68KRun(cyclesToRun);
  }
  m_cyclesElapsedThisFrame -= k_framePeriod;
  m_nextTimerInterruptCycles -= k_framePeriod;

  M68KGetContext(&M68K);

  // Decode MPEG for this frame
  MpegDec::DecodeAudio(&mpegL[retainedSamples], &mpegR[retainedSamples], 32000 / 60 - retainedSamples + 2);

  INT16 *leftChannelSource = nullptr;
  INT16 *rightChannelSource = nullptr;
  UINT8 volL = 0, volR = 0;

  switch (stereo)
  {
  default:
    case StereoMode::Stereo:
      leftChannelSource = mpegL;
      rightChannelSource = mpegR;
      volL = volume[0];
      volR = volume[1];
      break;
    case StereoMode::MonoLeft:
      leftChannelSource = mpegL;
      rightChannelSource = mpegL;
      volL = volume[0];
      volR = volume[0];
      break;
    case StereoMode::MonoRight:
      leftChannelSource = mpegR;
      rightChannelSource = mpegR;
      volL = volume[1];
      volR = volume[1];
      break;
  }

  retainedSamples = Resampler.UpSampleAndMix(audioL, audioR, leftChannelSource, rightChannelSource, volL, volR, NUM_SAMPLES_PER_FRAME, 32000/60+2, 44100, 32000);
}

void CDSB2::Reset(void)
{
	MpegDec::Stop();
	Resampler.Reset();
	retainedSamples = 0;

	memset(fifo, 0, sizeof(fifo));
	fifoIdxW = fifoIdxR = 0;

	mpegState = ST_IDLE;
	mpegStart = 0;
	mpegEnd = 0;
	playing = 0;
	volume[0] = 0xFF;	// set to max volume in case we miss the volume commands
	volume[1] = 0xFF;
	stereo = StereoMode::Stereo;

	// Even if DSB emulation is disabled, must reset to establish valid Z80 state
	M68KSetContext(&M68K);
	M68KReset();
	//printf("DSB2 PC=%06X\n", M68KGetPC());
	M68KGetContext(&M68K);

	m_cyclesElapsedThisFrame = 0;
	m_nextTimerInterruptCycles = k_timerPeriod;

	DebugLog("DSB2 Reset\n");
}

void CDSB2::SaveState(CBlockFile *StateFile)
{
	UINT32	playOffset, endOffset;
	UINT8	isPlaying;

	StateFile->NewBlock("DSB2", __FILE__);

	// MPEG playback state
	isPlaying	= (UINT8)MpegDec::IsLoaded();
	playOffset	= (UINT32)MpegDec::GetPosition();
	endOffset	= 0;

	StateFile->Write(&isPlaying, sizeof(isPlaying));
	StateFile->Write(&playOffset, sizeof(playOffset));
	StateFile->Write(&endOffset, sizeof(endOffset));
	StateFile->Write(&usingMPEGStart, sizeof(usingMPEGStart));
	StateFile->Write(&usingMPEGEnd, sizeof(usingMPEGEnd));
	StateFile->Write(&usingLoopStart, sizeof(usingLoopStart));
	StateFile->Write(&usingLoopEnd, sizeof(usingLoopEnd));

	// MPEG board state
	StateFile->Write(ram, 0x20000);
	StateFile->Write(fifo, sizeof(fifo));
	StateFile->Write(&fifoIdxR, sizeof(fifoIdxR));
	StateFile->Write(&fifoIdxW, sizeof(fifoIdxW));
	StateFile->Write(&cmdLatch, sizeof(cmdLatch));
	StateFile->Write(&mpegState, sizeof(mpegState));
	StateFile->Write(&mpegStart, sizeof(mpegStart));
	StateFile->Write(&mpegEnd, sizeof(mpegEnd));
	StateFile->Write(&playing, sizeof(playing));
	StateFile->Write(volume, sizeof(volume));
	StateFile->Write(&stereo, sizeof(stereo));

	// 68K CPU state
	M68KSetContext(&M68K);
	M68KSaveState(StateFile, "DSB2 68K");

	//DEBUG
	//printf("DSB2 PC=%06X\n", M68KGetPC());
	//printf("mpegStart=%X, mpegEnd=%X\n", mpegStart, mpegEnd);
	//printf("usingMPEGStart=%X, usingMPEGEnd=%X\n", usingMPEGStart, usingMPEGEnd);
	//printf("usingLoopStart=%X, usingLoopEnd=%X\n", usingLoopStart, usingLoopEnd);
}

void CDSB2::LoadState(CBlockFile *StateFile)
{
	UINT32	playOffset, endOffset;
	UINT8	isPlaying;

	if (OKAY != StateFile->FindBlock("DSB2"))
	{
		ErrorLog("Unable to load Digital Sound Board state. Save state file is corrupt.");
		return;
	}

	StateFile->Read(&isPlaying, sizeof(isPlaying));
	StateFile->Read(&playOffset, sizeof(playOffset));
	StateFile->Read(&endOffset, sizeof(endOffset));
	StateFile->Read(&usingMPEGStart, sizeof(usingMPEGStart));
	StateFile->Read(&usingMPEGEnd, sizeof(usingMPEGEnd));
	StateFile->Read(&usingLoopStart, sizeof(usingLoopStart));
	StateFile->Read(&usingLoopEnd, sizeof(usingLoopEnd));

	StateFile->Read(ram, 0x20000);
	StateFile->Read(fifo, sizeof(fifo));
	StateFile->Read(&fifoIdxR, sizeof(fifoIdxR));
	StateFile->Read(&fifoIdxW, sizeof(fifoIdxW));
	StateFile->Read(&cmdLatch, sizeof(cmdLatch));
	StateFile->Read(&mpegState, sizeof(mpegState));
	StateFile->Read(&mpegStart, sizeof(mpegStart));
	StateFile->Read(&mpegEnd, sizeof(mpegEnd));
	StateFile->Read(&playing, sizeof(playing));
	StateFile->Read(volume, sizeof(volume));
	StateFile->Read(&stereo, sizeof(stereo));

	M68KSetContext(&M68K);
	M68KLoadState(StateFile, "DSB2 68K");
	M68KGetContext(&M68K);

	// Technically these should be saved/restored rather than being reset but that would mean
	// the save state format has to be modified and the difference would be imperceptible anyway
	m_cyclesElapsedThisFrame = 0;
	m_nextTimerInterruptCycles = k_timerPeriod;

	// Restart MPEG audio at the appropriate position
	if (isPlaying)
	{
		MpegDec::SetMemory(&mpegROM[usingMPEGStart], usingMPEGEnd - usingMPEGStart, false);

		if (usingLoopEnd != 0) {		// only if looping was actually enabled
			MpegDec::UpdateMemory(&mpegROM[usingLoopStart], usingLoopEnd, true);
		}

		MpegDec::SetPosition(playOffset);
	}
	else {
		MpegDec::Stop();
	}

	//DEBUG
	//printf("DSB2 PC=%06X\n", M68KGetPC());
	//printf("mpegStart=%X, mpegEnd=%X\n", mpegStart, mpegEnd);
	//printf("usingMPEGStart=%X, usingMPEGEnd=%X\n", usingMPEGStart, usingMPEGEnd);
	//printf("usingLoopStart=%X, usingLoopEnd=%X\n", usingLoopStart, usingLoopEnd);

}

// Offsets of memory regions within DSB2's pool
#define DSB2_OFFSET_RAM			0		// 128KB 68K RAM
#define DSB2_OFFSET_MPEG_LEFT	0x20000	// 1604 bytes (48 KHz max., 1/60th second, 2 extra = 2*(48000/60+2)) left MPEG buffer
#define DSB2_OFFSET_MPEG_RIGHT	0x20644	// 1604 bytes right MPEG buffer
#define DSB2_MEMORY_POOL_SIZE	(0x20000 + 0x644 + 0x644)

bool CDSB2::Init(const UINT8 *progROMPtr, const UINT8 *mpegROMPtr)
{
	float memSizeMB = (float)DSB2_MEMORY_POOL_SIZE/(float)0x100000;

	// Receive ROM
	progROM = progROMPtr;
	mpegROM = mpegROMPtr;

	// Allocate memory pool
	memoryPool = new(std::nothrow) UINT8[DSB2_MEMORY_POOL_SIZE];
	if (NULL == memoryPool)
		return ErrorLog("Insufficient memory for DSB2 board (needs %1.1f MB).", memSizeMB);
	memset(memoryPool, 0, DSB2_MEMORY_POOL_SIZE);

	// Set up memory pointers
	ram = &memoryPool[DSB2_OFFSET_RAM];
	mpegL = (INT16 *) &memoryPool[DSB2_OFFSET_MPEG_LEFT];
	mpegR = (INT16 *) &memoryPool[DSB2_OFFSET_MPEG_RIGHT];

	// Initialize 68K CPU
	M68KSetContext(&M68K);
	M68KInit();
	M68KAttachBus(this);
	M68KSetIRQCallback(NULL);	// use default behavior (autovector, clear interrupt)
	M68KGetContext(&M68K);

	retainedSamples = 0;

	return OKAY;
}

M68KCtx *CDSB2::GetM68K(void)
{
	return &M68K;
}

CDSB2::CDSB2(const Util::Config::Node &config)
  : m_config(config),
    Resampler(config)
{
	progROM		= NULL;
	mpegROM		= NULL;
	memoryPool	= NULL;
	ram			= NULL;
	mpegL		= NULL;
	mpegR		= NULL;

	cmdLatch	= 0;
	mpegState	= 0;
	mpegStart	= 0;
	mpegEnd		= 0;
	playing		= 0;

	DebugLog("Built DSB2 Board\n");
}

CDSB2::~CDSB2(void)
{
	if (memoryPool != NULL)
	{
		delete [] memoryPool;
		memoryPool = NULL;
	}

	progROM	= NULL;
	mpegROM	= NULL;
	ram		= NULL;
	mpegL	= NULL;
	mpegR	= NULL;

	DebugLog("Destroyed DSB2 Board\n");
}