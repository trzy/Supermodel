/*
 * front/powerpc/6xx.c
 *
 * PowerPC 6xx Emulation.
 */

/*
 * NOTE: It temporarily covers PowerPC Gekko emulation too. There's no support
 * for DMA, nor for the custom graphics ports.
 */

#include <string.h>	// memset

#include "drppc.h"
#include "toplevel.h"
#include "source.h"
#include "internal.h"

#include "mmap.h"

#if PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO

/*******************************************************************************
 Model-Specific Management
*******************************************************************************/

INT InitModel (DRPPC_CFG * cfg, UINT32 pvr)
{
	/* Uhm. Do nothing. */

	return DRPPC_OKAY;
}

INT ResetModel (void)
{
	memset(&F(0), 0, sizeof(FPR) * 32);

#if PPC_MODEL == PPC_MODEL_GEKKO
	memset(&F_IS_PS(0), 0, sizeof(BOOL));
#endif

	memset(&SR(0), 0, sizeof(UINT32) * 16);

	MSR = 0x40;
	FPSCR = 0;
	DEC = 0xFFFFFFFF;

	ppc.reserved_bit = 0xFFFFFFFF;
	ppc.dec_expired = FALSE;

	/*
	 * Move PC to the reset vector.
	 */

	PC = 0xFFF00100;

	return DRPPC_OKAY;
}

void ShutdownModel (void)
{
	/* Uhm. Do nothing. */
}

/*******************************************************************************
 Register Access
*******************************************************************************/

void SetMSR (UINT32 data)
{
	if ((MSR ^ data) & 0x20000)
		Error("%08X: switched TGPRs on\n", PC);
	if (data & 0x80000)
		Error("%08X: PowerPC halted\n", PC);

	MSR = data;
}

UINT32 GetMSR (void)
{
	return MSR;
}

void SetSPR (UINT num, UINT32 data)
{
	switch (num)
	{
	case SPR_DEC:
		if ((data & 0x80000000) && !(SPR(SPR_DEC) & 0x80000000))
			Error("%08X: DEC intentionally triggers IRQ\n", PC);
		SPR(SPR_DEC) = data;
		break;
	case SPR_PVR:
		Error("%08X: write to PVR\n", PC);
	case SPR_TBL_W:
	case SPR_TBL_R:	// special 603e case
		WriteTimebaseLo(data);
		return;
	case SPR_TBU_R:
	case SPR_TBU_W:	// special 603e case
		WriteTimebaseHi(data);

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	case SPR_DMA_U:
	case SPR_DMA_L:
		Error("%08X: Gekko DMA unimplemented\n", PC);
#endif

	}

	SPR(num) = data;
}

UINT32 GetSPR (UINT num)
{
	switch (num)
	{
	case SPR_TBL_R:
		Error("%08X: GetSPR from TBL_R\n", PC);
	case SPR_TBU_R:
		Error("%08X: GetSPR from TBU_R\n", PC);
	case SPR_TBL_W:
		Error("%08X: GetSPR from TBL_W\n", PC);
	case SPR_TBU_W:
		Error("%08X: GetSPR from TBU_W\n", PC);
	}

	return SPR(num);
}

/*******************************************************************************
 Memory Access
*******************************************************************************/

// NOTE: no TLB emulation.

#define ITLB_ENABLED	(MSR & 0x20)
#define DTLB_ENABLED	(MSR & 0x10)

UINT8 Read8 (UINT32 addr)
{
	return MMap_GenericRead8(addr);
}

UINT16 Read16 (UINT32 addr)
{
	return MMap_GenericRead16(addr);
}

UINT32 Read32 (UINT32 addr)
{
	return MMap_GenericRead32(addr);
}

UINT64 Read64 (UINT32 addr)
{
	return ((UINT64)MMap_GenericRead32(addr + 0) << 32) |
			(UINT64)MMap_GenericRead32(addr + 4);
}

void Write8 (UINT32 addr, UINT8 data)
{
	MMap_GenericWrite8(addr, data);
}

void Write16 (UINT32 addr, UINT16 data)
{
	MMap_GenericWrite16(addr, data);
}

void Write32 (UINT32 addr, UINT32 data)
{
	MMap_GenericWrite32(addr, data);
}

void Write64 (UINT32 addr, UINT64 data)
{
	MMap_GenericWrite32(addr + 0, (UINT32)(data >> 32));
	MMap_GenericWrite32(addr + 4, (UINT32)data);
}

/*******************************************************************************
 Exception Handling
*******************************************************************************/

void CheckIRQs (void)
{
	if (MSR & 0x8000)					// Interrupts enabled
	{
		if (ppc.irq_state)				// External IRQ
		{
			SPR(SPR_SRR0) = PC;
			SPR(SPR_SRR1) = MSR | 2;

#if (PPC_MODEL == PPC_MODEL_GEKKO)
			ppc.pc = (MSR & 0x40) ? 0x80000500 : 0x00000500; // hack
#else
			ppc.pc = (MSR & 0x40) ? 0xFFF00500 : 0x00000500;
#endif
			MSR = (MSR & 0x11040) | ((MSR >> 16) & 1);

			ppc.irq_state = ppc.IRQCallback();

			UpdatePC();
		}
		else if (ppc.dec_expired)		// Decrementer exception
		{
			SPR(SPR_SRR0) = PC;
			SPR(SPR_SRR1) = MSR;

#if (PPC_MODEL == PPC_MODEL_GEKKO)
			PC = (MSR & 0x40) ? 0x80000900 : 0x00000900; // hack
#else
			PC = (MSR & 0x40) ? 0xFFF00900 : 0x00000900;
#endif
			MSR = (MSR & 0x11040) | ((MSR >> 16) & 1);

			ppc.dec_expired = FALSE;

			UpdatePC();
		}
	}
}

#endif // PPC_MODEL_6XX or PPC_MODEL_GEKKO
