/*
 * front/powerpc/4xx.c
 *
 * PowerPC 4xx Emulation.
 */

#include "drppc.h"
#include "toplevel.h"
#include "source.h"
#include "internal.h"

#include "mmap.h"

#if PPC_MODEL == PPC_MODEL_4XX

/*******************************************************************************
 SPU

 Serial Port Unit emulation. Local routines.
*******************************************************************************/

static void SPU_Write8 (UINT32 addr, UINT8 data)
{
	switch (addr & 15)
	{
	case 0: ppc.spu.spls &= ~(data & 0xF6); return;
	case 2: ppc.spu.sphs &= ~(data & 0xC0); return;
	case 4: ppc.spu.brdh = data; return;
	case 5: ppc.spu.brdl = data; return;
	case 6: ppc.spu.spctl = data; return;
	case 7: ppc.spu.sprc = data; return;
	case 8: ppc.spu.sptc = data; return;
	case 9: ppc.spu.sptb = data; return;
	}

	Error("%08X: unhandled SPU write 8 %08X = %02X\n", PC, addr, data);
}

static UINT8 SPU_Read8 (UINT32 addr)
{
	switch (addr & 15)
	{
	case 0: return(ppc.spu.spls);
	case 2: return(ppc.spu.sphs);
	case 4: return(ppc.spu.brdh);
	case 5: return(ppc.spu.brdl);
	case 6: return(ppc.spu.spctl);
	case 7: return(ppc.spu.sprc);
	case 8: return(ppc.spu.sptc);
	case 9: return(ppc.spu.sprb);
	}

	Error("%08X: unhandled SPU read 8 %08X\n", PC, addr);
}

/*******************************************************************************
 DMA

 Direct Memory Access emulation. Local routines.
*******************************************************************************/

#define DMA_CR(ch)	ppc.dcr[0xC0 + (ch << 3) + 0]
#define DMA_CT(ch)	ppc.dcr[0xC0 + (ch << 3) + 1]
#define DMA_DA(ch)	ppc.dcr[0xC0 + (ch << 3) + 2]
#define DMA_SA(ch)	ppc.dcr[0xC0 + (ch << 3) + 3]
#define DMA_CC(ch)	ppc.dcr[0xC0 + (ch << 3) + 4]
#define DMA_SR		ppc.dcr[0xE0]

/*
 * void DMA (UINT ch);
 *
 * Perform internal DMA transfer from channel ch.
 */

static void DMA (UINT ch)
{
	UINT	unit;

	if (DMA_CR(channel) & 0x000000F0)
		Error("%08X: chained DMA%u\n", PC, ch);

	unit = (DMA_CR(ch) >> 26) & 3;

	switch (unit)
	{
	case 0:		// 8-bit
	case 1:		// 16-bit
	case 2:		// 32-bit
	default:	// 256-bit
		Error("%08X: DMA%u %08X --> %08X, %08X size=%i\n", PC, ch, DMA_SA(ch), DMA_DA(ch), DMA_CT(ch), unit);
		break;
	}

	if (DMA_CR(ch) & 0x40000000)	// DIE
	{
		Error("%08X: DMA%u interrupt triggered\n", ch);
		DCR(DCR_EXISR) |= (0x00800000 >> ch);
		ppc.irq_state = 1;
	}
}

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
	memset(DCR, 0, sizeof(UINT32) * 256);

	memset(ppc.spu, 0, sizeof(SPU));

	// Init Bank registers.

	DCR(DCR_BR0) = 0xFF193FFE;
	DCR(DCR_BR1) = 0xFF013FFE;
	DCR(DCR_BR2) = 0xFF013FFE;
	DCR(DCR_BR3) = 0xFF013FFE;
	DCR(DCR_BR4) = 0xFF013FFF;
	DCR(DCR_BR5) = 0xFF013FFF;
	DCR(DCR_BR6) = 0xFF013FFF;
	DCR(DCR_BR7) = 0xFF013FFF;

	// Set PC to reset vector.

	PC = 0xFFFFFFFC;

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
	if (data & 0x80000)	// WE
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
	case SPR_PVR:
		Error("%08X: write to PVR\n", PC);
	case SPR_PIT:
		if (data)
			Error("%08X: PIT = %u\n", PC, data);
		SPR(SPR_PIT) = data;
		ppc.pit_reload = data;
		break;
	case SPR_TSR:
		SPR(SPR_TSR) &= ~data;
		return;
	}

	SPR(num) = d;
}

UINT32 GetSPR (UINT num)
{
	switch (num)
	{
	case SPR_TBLO:
		return ReadTimebaseLo();
	case SPR_TBHI:
		return ReadTimebaseHi();
	}

	return SPR(num);
}

void SetDCR (UINT num, UINT32 data)
{
	switch (num)
	{
	case DCR_DMACC0:
	case DCR_DMACC1:
	case DCR_DMACC2:
	case DCR_DMACC3:
		if (data)
			Error("%08X: chained DMA requested\n", PC);
		break;
	case DCR_DMACR0:
		DCR(num) = data;
		if(data & 0x80000000)
			DMA(0);
		return;
	case DCR_DMACR1:
		DCR(num) = data;
		if(data & 0x80000000)
			DMA(1);
		return;
	case DCR_DMACR2:
		DCR(num) = data;
		if(data & 0x80000000)
			DMAa(2);
		return;
	case DCR_DMACR3:
		DCR(num) = data;
		if(data & 0x80000000)
			DMA(3);
		return;
	case DCR_DMASR:
		DCR(num) &= ~data;
		return;
	}

	DCR(num) = data;
}

UINT32 GetDCR (UINT num)
{
	return DCR(num);
}

/*******************************************************************************
 Memory Access
*******************************************************************************/

UINT8 Read8 (UINT32 addr)
{
	if ((a >> 28) == 4)
		return SPU_Read8(addr);

	return MMap_GenericRead8(addr & 0x0FFFFFFF);
}

UINT16 Read16 (UINT32 addr)
{
	return MMap_GenericRead16(addr & 0x0FFFFFFE);
}

UINT32 Read32 (UINT32 addr)
{
	return MMap_GenericRead32(addr & 0x0FFFFFFC);
}

void Write8 (UINT32 addr, UINT8 data)
{
	if ((a >> 28) == 4)
		SPU_Write8(addr, data);
	else
		MMap_GenericWrite8(addr & 0x0FFFFFFF, data);
}

void Write16 (UINT32 addr, UINT8 data)
{
	MMap_GenericWrite16(addr & 0x0FFFFFFE, data);
}

void Write32 (UINT32 addr, UINT32 data)
{
	MMap_GenericWrite32(addr & 0x0FFFFFFC, data);
}

/*******************************************************************************
 Exception Handling
*******************************************************************************/

void CheckIRQs (void)
{
	if (MSR & 0x8000)						// Interrupts enabled
	{
		if (DCR(DCR_EXIER) & 0x10)			// Hardware interrupts enabled
		{
			if (ppc.irq_state)				// Interrupt triggered
			{
				SPR(SPR_SRR0) = PC;
				SPR(SPR_SRR1) = MSR;

				MSR &= ~0xC000;

				DCR(DCR_EXISR) |= 0x10;
				DCR(DCR_BESR) = 0;
				DCR(DCR_BEAR) = 0;

				SPR(SPR_ESR) = 0;
				SPR(SPR_DEAR) = 0;

				PC = 0xFF800500;

				ppc.irq_state = ppc.IRQCallback();
			}
		}
	}
}

#endif // PPC_MODEL_4XX
