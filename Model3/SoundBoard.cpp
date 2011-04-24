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
 Global 68K Access Handlers
 
 The 68K must interface with globally-accessible memory maps and access
 handlers.  Turbo68K uses the __cdecl calling convention. Note that this is not
 explicitly defined for the Turbo68K functions themselves but in case Turbo68K
 crashes, this may be the culprit.
******************************************************************************/

// Prototypes for access handlers
static UINT8	__cdecl	UnknownRead8(UINT32);
static UINT16	__cdecl	UnknownRead16(UINT32);
static UINT32	__cdecl	UnknownRead32(UINT32);
static void		__cdecl	UnknownWrite8(UINT32, UINT8);
static void		__cdecl	UnknownWrite16(UINT32, UINT16);
static void		__cdecl	UnknownWrite32(UINT32, UINT32);

// Memory maps for 68K
#ifdef SUPERMODEL_SOUND
struct TURBO68K_FETCHREGION mapFetch[] =
{
	{ 0x000000, 0x0FFFFF, NULL },		// SCSP1 RAM
	{ 0x200000,	0x2FFFFF, NULL },		// SCSP2 RAM
	{ 0x600000, 0x67FFFF, NULL },		// program ROM
	{ -1,       -1,       NULL }
};

struct TURBO68K_DATAREGION mapRead8[] =
{
	{ 0x000000, 0x0FFFFF, NULL, NULL },	// SCSP1 RAM
	{ 0x200000,	0x2FFFFF, NULL, NULL },	// SCSP2 RAM
	{ 0x600000, 0x67FFFF, NULL, NULL },	// program ROM
	{ 0x800000,	0x9FFFFF, NULL, NULL },	// sample ROM (low 2 MB)
	{ 0xA00000,	0xDFFFFF, NULL, NULL },	// sample ROM (bank)
	{ 0xE00000, 0xFFFFFF, NULL, NULL },	// sample ROM (bank)
	{ 0x100000, 0x10FFFF, NULL, SCSP_Master_r8 },
	{ 0x300000, 0x30FFFF, NULL, SCSP_Slave_r8 },
	{ 0x000000, 0xFFFFFF, NULL, UnknownRead8 },
	{ -1,		-1,		  NULL, NULL }
};

struct TURBO68K_DATAREGION mapRead16[] =
{
	{ 0x000000, 0x0FFFFF, NULL, NULL },	// SCSP1 RAM
	{ 0x200000,	0x2FFFFF, NULL, NULL },	// SCSP2 RAM
	{ 0x600000, 0x67FFFF, NULL, NULL },	// program ROM
	{ 0x800000,	0x9FFFFF, NULL, NULL },	// sample ROM (low 2 MB)
	{ 0xA00000,	0xDFFFFF, NULL, NULL },	// sample ROM (bank)
	{ 0xE00000, 0xFFFFFF, NULL, NULL },	// sample ROM (bank)
	{ 0x100000, 0x10FFFF, NULL, SCSP_Master_r16 },
	{ 0x300000, 0x30FFFF, NULL, SCSP_Slave_r16 },
	{ 0x000000, 0xFFFFFF, NULL, UnknownRead16 },
	{ -1,		-1,		  NULL, NULL }
};

struct TURBO68K_DATAREGION mapRead32[] =
{
	{ 0x000000, 0x0FFFFF, NULL, NULL },	// SCSP1 RAM
	{ 0x200000,	0x2FFFFF, NULL, NULL },	// SCSP2 RAM
	{ 0x600000, 0x67FFFF, NULL, NULL },	// program ROM
	{ 0x800000,	0x9FFFFF, NULL, NULL },	// sample ROM (low 2 MB)
	{ 0xA00000,	0xDFFFFF, NULL, NULL },	// sample ROM (bank)
	{ 0xE00000, 0xFFFFFF, NULL, NULL },	// sample ROM (bank)
	{ 0x100000, 0x10FFFF, NULL, SCSP_Master_r32 },
	{ 0x300000, 0x30FFFF, NULL, SCSP_Slave_r32 },
	{ 0x000000, 0xFFFFFF, NULL, UnknownRead32 },
	{ -1,		-1,		  NULL, NULL }
};

struct TURBO68K_DATAREGION mapWrite8[] =
{
	{ 0x000000, 0x0FFFFF, NULL, NULL },	// SCSP1 RAM
	{ 0x200000,	0x2FFFFF, NULL, NULL },	// SCSP2 RAM
	{ 0x100000, 0x10FFFF, NULL, SCSP_Master_w8 },
	{ 0x300000, 0x30FFFF, NULL, SCSP_Slave_w8 },
	{ 0x000000, 0xFFFFFF, NULL, UnknownWrite8 },
	{ -1,		-1,		  NULL, NULL }
};

struct TURBO68K_DATAREGION mapWrite16[] =
{
	{ 0x000000, 0x0FFFFF, NULL, NULL },	// SCSP1 RAM
	{ 0x200000,	0x2FFFFF, NULL, NULL },	// SCSP2 RAM
	{ 0x100000, 0x10FFFF, NULL, SCSP_Master_w16 },
	{ 0x300000, 0x30FFFF, NULL, SCSP_Slave_w16 },
	{ 0x000000, 0xFFFFFF, NULL, UnknownWrite16 },
	{ -1,		-1,		  NULL, NULL }
};

struct TURBO68K_DATAREGION mapWrite32[] =
{
	{ 0x000000, 0x0FFFFF, NULL, NULL },	// SCSP1 RAM
	{ 0x200000,	0x2FFFFF, NULL, NULL },	// SCSP2 RAM
	{ 0x100000, 0x10FFFF, NULL, SCSP_Master_w32 },
	{ 0x300000, 0x30FFFF, NULL, SCSP_Slave_w32 },
	{ 0x000000, 0xFFFFFF, NULL, UnknownWrite32 },
	{ -1,		-1,		  NULL, NULL }
};
#endif

static UINT8 __cdecl UnknownRead8(UINT32 addr)
{
	printf("68K read from %06X (byte)\n", addr);
	return 0;
}

static UINT16 __cdecl UnknownRead16(UINT32 addr)
{
	printf("68K read from %06X (word)\n", addr);
	return 0;
}

static UINT32 __cdecl UnknownRead32(UINT32 addr)
{
	printf("68K read from %06X (longword)\n", addr);
	return 0;
}


static void __cdecl UnknownWrite8(UINT32 addr, UINT8 data)
{
	printf("68K wrote %06X=%02X\n", addr, data);
}

static void __cdecl UnknownWrite16(UINT32 addr, UINT16 data)
{
	printf("68K wrote %06X=%04X\n", addr, data);
}

static void __cdecl UnknownWrite32(UINT32 addr, UINT32 data)
{
	printf("68K wrote %06X=%08X\n", addr, data);
}


/******************************************************************************
 Emulation Functions
******************************************************************************/
int irqLine = 0;

// SCSP callback for generating IRQs
void SCSP68KIRQCallback(int irqNum)
{
#ifdef SUPERMODEL_SOUND
	//printf("IRQ: %d\n", irqNum);
	irqLine = irqNum;
	//Turbo68KInterrupt(irqNum, TURBO68K_AUTOVECTOR);
#endif
}

// SCSP callback for running the 68K
int SCSP68KRunCallback(int numCycles)
{
#ifdef SUPERMODEL_SOUND
	for (int i = 0; i < numCycles; i++)
	{
		if (irqLine)
			Turbo68KInterrupt(irqLine,TURBO68K_AUTOVECTOR);
		Turbo68KRun(1);
	}
	return numCycles;
	//Turbo68KRun(numCycles);
	//return Turbo68KGetElapsedCycles();
#else
	return numCycles;
#endif
}

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
#ifdef SUPERMODEL_SOUND
	memcpy(ram1, soundROM, 16);	// copy 68K vector table
	Turbo68KReset();
	/*
	printf("68K PC=%06X\n", Turbo68KReadPC());
	for (int i = 0; i < 1000000; i++)
	{
		//printf("%06X\n", Turbo68KReadPC());
		Turbo68KRun(1);
	}
	*/
#endif
	DebugLog("Sound Board Reset\n");
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

// Offsets of memory regions within sound board's pool
#define OFFSET_RAM1			0			// 1 MB SCSP1 RAM
#define OFFSET_RAM2			0x100000	// 1 MB SCSP2 RAM
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
	ram1 = &memoryPool[OFFSET_RAM1];
	ram2 = &memoryPool[OFFSET_RAM2];
	
	// Initialize 68K core
#ifdef SUPERMODEL_SOUND
	mapFetch[0].ptr = mapRead8[0].ptr = mapRead16[0].ptr = mapRead32[0].ptr = 
	mapWrite8[0].ptr = mapWrite16[0].ptr = mapWrite32[0].ptr =	(UINT32)ram1 - mapFetch[0].base;;
	
	mapFetch[1].ptr = mapRead8[1].ptr = mapRead16[1].ptr = mapRead32[1].ptr = 
	mapWrite8[1].ptr = mapWrite16[1].ptr = mapWrite32[1].ptr =	(UINT32)ram2 - mapFetch[1].base;

	mapFetch[2].ptr = mapRead8[2].ptr = mapRead16[2].ptr = mapRead32[2].ptr = (UINT32)soundROM - mapFetch[2].base;
	
	mapRead8[3].ptr = mapRead16[3].ptr = mapRead32[3].ptr = (UINT32)&sampleROM[0x000000] - mapRead8[3].base;
	
	mapRead8[4].ptr = mapRead16[4].ptr = mapRead32[4].ptr = (UINT32)&sampleROM[0x200000] - mapRead8[4].base;

	mapRead8[5].ptr = mapRead16[5].ptr = mapRead32[5].ptr = (UINT32)&sampleROM[0x600000] - mapRead8[5].base;
	
	Turbo68KInit();
	Turbo68KSetFetch(mapFetch, NULL);
	Turbo68KSetReadByte(mapRead8, NULL);
	Turbo68KSetReadWord(mapRead16, NULL);
	Turbo68KSetReadLong(mapRead32, NULL);
	Turbo68KSetWriteByte(mapWrite8, NULL);
	Turbo68KSetWriteWord(mapWrite16, NULL);
	Turbo68KSetWriteLong(mapWrite32, NULL);
#endif
	
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
