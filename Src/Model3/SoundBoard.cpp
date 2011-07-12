/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski 
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
 * SoundBoard.cpp
 * 
 * Model 3 sound board. Implementation of the CSoundBoard class. This class can
 * only be instantiated once because it relies on global variables (the non-OOP
 * 68K core).
 */

#include "Supermodel.h"

//TEMP: these need to be dynamically allocated in the memory pool
static INT16	leftBuffer[44100/60],rightBuffer[44100/60];
static FILE		*soundFP;

/******************************************************************************
 68K Access Handlers
******************************************************************************/

// Memory regions passed out of CSoundBoard object for global access handlers
static UINT8		*sbRAM1, *sbRAM2;
static const UINT8	*sbSoundROM, *sbSampleROM;

static UINT8 Read8(UINT32 a)
{ 
	// SCSP RAM 1
	if ((a >= 0x000000) && (a <= 0x0FFFFF))
		return sbRAM1[a^1];
	
	// SCSP RAM 2
	else if ((a >= 0x200000) && (a <= 0x2FFFFF))
		return sbRAM2[(a-0x200000)^1];
		
	// Program ROM
	else if ((a >= 0x600000) && (a <= 0x67FFFF))
		return sbSoundROM[(a-0x600000)^1];
	
	// Sample ROM (low 2MB, fixed)
	else if ((a >= 0x800000) && (a <= 0x9FFFFF))
		return sbSampleROM[(a-0x800000)^1];
		
	// Sample ROM (bank)
	else if ((a >= 0xA00000) && (a <= 0xDFFFFF))
		return sbSampleROM[(a-0x800000)^1];
		
	// Sample ROM (bank)
	else if ((a >= 0xE00000) && (a <= 0xFFFFFF))
		return sbSampleROM[(a-0x800000)^1];
		
	// SCSP (Master)
	else if ((a >= 0x100000) && (a <= 0x10FFFF))
		return SCSP_Master_r8(a);
		
	// SCSP (Slave)
	else if ((a >= 0x300000) && (a <= 0x30FFFF))
		return SCSP_Slave_r8(a);
		
	// Unknown
	else
	{
		printf("68K: Unknown read8 %06X\n", a);
		return 0;
	}
}

static UINT16 Read16(UINT32 a) 
{ 
	// SCSP RAM 1
	if ((a >= 0x000000) && (a <= 0x0FFFFF))
		return *(UINT16 *) &sbRAM1[a];
	
	// SCSP RAM 2
	else if ((a >= 0x200000) && (a <= 0x2FFFFF))
		return *(UINT16 *) &sbRAM2[(a-0x200000)];
		
	// Program ROM
	else if ((a >= 0x600000) && (a <= 0x67FFFF))
		return *(UINT16 *) &sbSoundROM[(a-0x600000)];
	
	// Sample ROM (low 2MB, fixed)
	else if ((a >= 0x800000) && (a <= 0x9FFFFF))
		return *(UINT16 *) &sbSampleROM[(a-0x800000)];
		
	// Sample ROM (bank)
	else if ((a >= 0xA00000) && (a <= 0xDFFFFF))
		return *(UINT16 *) &sbSampleROM[(a-0x800000)];
		
	// Sample ROM (bank)
	else if ((a >= 0xE00000) && (a <= 0xFFFFFF))
		return *(UINT16 *) &sbSampleROM[(a-0x800000)];
		
	// SCSP (Master)
	else if ((a >= 0x100000) && (a <= 0x10FFFF))
		return SCSP_Master_r16(a);
		
	// SCSP (Slave)
	else if ((a >= 0x300000) && (a <= 0x30FFFF))
		return SCSP_Slave_r16(a);
		
	// Unknown
	else
	{
		printf("68K: Unknown read16 %06X\n", a);
		return 0;
	}
}

static UINT32 Read32(UINT32 a) 
{ 
	// SCSP RAM 1
	if ((a >= 0x000000) && (a <= 0x0FFFFF))
		return (Read16(a)<<16)|Read16(a+2);
	
	// SCSP RAM 2
	else if ((a >= 0x200000) && (a <= 0x2FFFFF))
		return (Read16(a)<<16)|Read16(a+2);
		
	// Program ROM
	else if ((a >= 0x600000) && (a <= 0x67FFFF))
		return (Read16(a)<<16)|Read16(a+2);
	
	// Sample ROM (low 2MB, fixed)
	else if ((a >= 0x800000) && (a <= 0x9FFFFF))
		return (Read16(a)<<16)|Read16(a+2);
		
	// Sample ROM (bank)
	else if ((a >= 0xA00000) && (a <= 0xDFFFFF))
		return (Read16(a)<<16)|Read16(a+2);
		
	// Sample ROM (bank)
	else if ((a >= 0xE00000) && (a <= 0xFFFFFF))
		return (Read16(a)<<16)|Read16(a+2);
		
	// SCSP (Master)
	else if ((a >= 0x100000) && (a <= 0x10FFFF))
		return SCSP_Master_r32(a);
		
	// SCSP (Slave)
	else if ((a >= 0x300000) && (a <= 0x30FFFF))
		return SCSP_Slave_r32(a);
		
	// Unknown
	else
	{
		printf("68K: Unknown read32 %06X\n", a);
		return 0;
	}
}

static void Write8 (unsigned int a,unsigned char d)  
{ 
	
	// SCSP RAM 1
	if ((a >= 0x000000) && (a <= 0x0FFFFF))
		sbRAM1[a^1] = d;
	
	// SCSP RAM 2
	else if ((a >= 0x200000) && (a <= 0x2FFFFF))
		sbRAM2[(a-0x200000)^1] = d;
		
	// SCSP (Master)
	else if ((a >= 0x100000) && (a <= 0x10FFFF))
		SCSP_Master_w8(a,d);
		
	// SCSP (Slave)
	else if ((a >= 0x300000) && (a <= 0x30FFFF))
		SCSP_Slave_w8(a,d);
		
	// Unknown
	else
		printf("68K: Unknown write8 %06X=%02X\n", a, d);
}

static void Write16(unsigned int a,unsigned short d) 
{ 
		
	// SCSP RAM 1
	if ((a >= 0x000000) && (a <= 0x0FFFFF))
		*(UINT16 *) &sbRAM1[a] = d;
	
	// SCSP RAM 2
	else if ((a >= 0x200000) && (a <= 0x2FFFFF))
		*(UINT16 *) &sbRAM2[(a-0x200000)] = d;
		
	// SCSP (Master)
	else if ((a >= 0x100000) && (a <= 0x10FFFF))
		SCSP_Master_w16(a,d);
		
	// SCSP (Slave)
	else if ((a >= 0x300000) && (a <= 0x30FFFF))
		SCSP_Slave_w16(a,d);
		
	// Unknown
	else
		printf("68K: Unknown write16 %06X=%04X\n", a, d);
	
}

static void Write32(unsigned int a,unsigned int d)
{
	// SCSP RAM 1
	if ((a >= 0x000000) && (a <= 0x0FFFFF))
	{
		Write16(a,d>>16);
		Write16(a+2,d&0xFFFF);
	}
		
	// SCSP RAM 2
	else if ((a >= 0x200000) && (a <= 0x2FFFFF))
	{
		Write16(a,d>>16);
		Write16(a+2,d&0xFFFF);
	}
		
	// SCSP (Master)
	else if ((a >= 0x100000) && (a <= 0x10FFFF))
		SCSP_Master_w32(a,d);
		
	// SCSP (Slave)
	else if ((a >= 0x300000) && (a <= 0x30FFFF))
		SCSP_Slave_w32(a,d);
		
	// Unknown
	else
		printf("68K: Unknown write32 %06X=%08X\n", a, d);
}


/******************************************************************************
 SCSP 68K Callbacks
 
 The SCSP emulator drives the 68K via callbacks.
******************************************************************************/

// Status of IRQ pins (IPL2-0) on 68K
static int	irqLine = 0;

// Interrupt acknowledge callback (TODO: don't need this, default behavior in M68K.cpp is fine)
int IRQAck(int irqLevel)
{
	M68KSetIRQ(0);
	irqLine = 0;
	return M68K_IRQ_AUTOVECTOR;
}

// SCSP callback for generating IRQs
void SCSP68KIRQCallback(int irqLevel)
{
	/*
	 * IRQ arbitration logic: only allow higher priority IRQs to be asserted or
	 * 0 to clear pending IRQ.
	 */
	if ((irqLevel>irqLine) || (0==irqLevel))
	{
		irqLine = irqLevel;	
		
	}
	M68KSetIRQ(irqLine);
}

// SCSP callback for running the 68K
int SCSP68KRunCallback(int numCycles)
{
	return M68KRun(numCycles);
}


/******************************************************************************
 Sound Board Emulation
******************************************************************************/

void CSoundBoard::WriteMIDIPort(UINT8 data)
{
	SCSP_MidiIn(data);
}

void CSoundBoard::RunFrame(void)
{
#ifdef SUPERMODEL_SOUND
	SCSP_Update();
	
	// Output to binary file
	INT16	s;
	for (int i = 0; i < 44100/60; i++)
	{
		s = ((UINT16)leftBuffer[i]>>8) | ((leftBuffer[i]&0xFF)<<8);
		fwrite(&s, sizeof(INT16), 1, soundFP);	// left channel
		s = ((UINT16)rightBuffer[i]>>8) | ((rightBuffer[i]&0xFF)<<8);
		fwrite(&s, sizeof(INT16), 1, soundFP);	// right channel
	}
#endif
}

void CSoundBoard::Reset(void)
{
	memcpy(ram1, soundROM, 16);	// copy 68K vector table
	M68KReset();
	DebugLog("Sound Board Reset\n");
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

// Offsets of memory regions within sound board's pool
#define OFFSETsbRAM1			0			// 1 MB SCSP1 RAM
#define OFFSETsbRAM2			0x100000	// 1 MB SCSP2 RAM
#define MEMORY_POOL_SIZE	(0x100000+0x100000)

BOOL CSoundBoard::Init(const UINT8 *soundROMPtr, const UINT8 *sampleROMPtr, CIRQ *ppcIRQObjectPtr, unsigned soundIRQBit)
{
	float	memSizeMB = (float)MEMORY_POOL_SIZE/(float)0x100000;
	
	// Attach IRQ controller
	ppcIRQ = ppcIRQObjectPtr;
	ppcSoundIRQBit = soundIRQBit;
	
	// Receive sound ROMs
	soundROM = soundROMPtr;
	sampleROM = sampleROMPtr;
	
	// Allocate all memory for RAM
	memoryPool = new(std::nothrow) UINT8[MEMORY_POOL_SIZE];
	if (NULL == memoryPool)
		return ErrorLog("Insufficient memory for sound board (needs %1.1f MB).", memSizeMB);
	memset(memoryPool, 0, MEMORY_POOL_SIZE);
	
	// Set up memory pointers
	ram1 = &memoryPool[OFFSETsbRAM1];
	ram2 = &memoryPool[OFFSETsbRAM2];
	
	// Make global copies of memory pointers for 68K access handlers
	sbRAM1 = ram1;
	sbRAM2 = ram2;
	sbSoundROM = soundROM;
	sbSampleROM = sampleROM;

	// Initialize 68K core
	M68KInit();
	M68KSetIRQCallback(IRQAck);
	M68KSetFetch8Callback(Read8);
	M68KSetFetch16Callback(Read16);
	M68KSetFetch32Callback(Read32);
	M68KSetRead8Callback(Read8);
	M68KSetRead16Callback(Read16);
	M68KSetRead32Callback(Read32);
	M68KSetWrite8Callback(Write8);
	M68KSetWrite16Callback(Write16);
	M68KSetWrite32Callback(Write32);
		
	// Initialize SCSPs
	SCSP_SetBuffers(leftBuffer, rightBuffer, 44100/60);
	SCSP_SetCB(SCSP68KRunCallback, SCSP68KIRQCallback, ppcIRQ, ppcSoundIRQBit);
	SCSP_Init(2);
	SCSP_SetRAM(0, ram1);
	SCSP_SetRAM(1, ram2);
	
	// Binary logging
#ifdef SUPERMODEL_SOUND
	soundFP = fopen("sound.bin","wb");	// delete existing file
	fclose(soundFP);
	soundFP = fopen("sound.bin","ab");	// append mode
#endif

	return OKAY;
}

CSoundBoard::CSoundBoard(void)
{
	memoryPool = NULL;
	ram1 = NULL;
	ram2 = NULL;
	
	DebugLog("Built Sound Board\n");
}

static void Reverse16(UINT8 *buf, unsigned size)
{
	unsigned	i;
	UINT8		tmp;
	
	for (i = 0; i < size; i += 2)
	{
		tmp = buf[i+0];
		buf[i+0] = buf[i+1];
		buf[i+1] = tmp;
	}
}

CSoundBoard::~CSoundBoard(void)
{	
#ifdef SUPERMODEL_SOUND
	// close binary log file
	fclose(soundFP);
//#if 0
	FILE	*fp;
	
	Reverse16(ram1, 0x100000);
	Reverse16(ram2, 0x100000);
	fp = fopen("scspRAM1", "wb");
	if (NULL != fp)
	{
		fwrite(ram1, sizeof(UINT8), 0x100000, fp);
		fclose(fp);
		printf("dumped %s\n", "scspRAM1");
		
	}
	fp = fopen("scspRAM2", "wb");
	if (NULL != fp)
	{
		fwrite(ram2, sizeof(UINT8), 0x100000, fp);
		fclose(fp);
		printf("dumped %s\n", "scspRAM2");
		
	}
//#endif
#endif

	SCSP_Deinit();
	
	if (memoryPool != NULL)
	{
		delete [] memoryPool;
		memoryPool = NULL;
	}
	ram1 = NULL;
	ram2 = NULL;
	DebugLog("Destroyed Sound Board\n");
}
