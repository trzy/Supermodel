/*
 * front/powerpc/interp/i_branch.c
 *
 * Branch instructions handlers.
 */

#include "../powerpc.h"
#include "../internal.h"

/*******************************************************************************
 Local Routines
*******************************************************************************/

static BOOL CHECK_BI(UINT bi)
{
	UINT t = bi >> 2;
	UINT b = bi & 3;

	switch (b)
	{
	case 0:  return GET_CR_LT(t) != 0;
	case 1:  return GET_CR_GT(t) != 0;
	case 2:  return GET_CR_EQ(t) != 0;
	default: return GET_CR_SO(t) != 0;
	}
}

static UINT32 ppc_bo_0000(UINT32 bi){ return(--CTR != 0 && CHECK_BI(bi) == 0); }
static UINT32 ppc_bo_0001(UINT32 bi){ return(--CTR == 0 && CHECK_BI(bi) == 0); }
static UINT32 ppc_bo_001z(UINT32 bi){ return(CHECK_BI(bi) == 0); }
static UINT32 ppc_bo_0100(UINT32 bi){ return(--CTR != 0 && CHECK_BI(bi) != 0); }
static UINT32 ppc_bo_0101(UINT32 bi){ return(--CTR == 0 && CHECK_BI(bi) != 0); }
static UINT32 ppc_bo_011z(UINT32 bi){ return(CHECK_BI(bi) != 0); }
static UINT32 ppc_bo_1z00(UINT32 bi){ return(--CTR != 0); }
static UINT32 ppc_bo_1z01(UINT32 bi){ return(--CTR == 0); }
static UINT32 ppc_bo_1z1z(UINT32 bi){ return(1); }

static UINT32 (* ppc_bo[16])(UINT32 bi) =
{
	ppc_bo_0000, ppc_bo_0001, ppc_bo_001z, ppc_bo_001z,
	ppc_bo_0100, ppc_bo_0101, ppc_bo_011z, ppc_bo_011z,
	ppc_bo_1z00, ppc_bo_1z01, ppc_bo_1z1z, ppc_bo_1z1z,
	ppc_bo_1z00, ppc_bo_1z01, ppc_bo_1z1z, ppc_bo_1z1z,
};

/*******************************************************************************
 Instruction Handlers
*******************************************************************************/

INT I_Bx (UINT32 op)
{
	UINT32 lr = ppc.pc;

	ppc.pc = (((INT32)op << 6) >> 6) & ~3;

	if((op & _AA) == 0)
		ppc.pc += lr - 4;

	if(op & _LK)
		LR = lr;

	UpdatePC();

	return 1;
}

INT I_Bcx (UINT32 op)
{
	UINT32 lr = ppc.pc;
	BOOL cond = ppc_bo[_BO >> 1](_BI);

	if(cond)
	{
		ppc.pc = SIMM & ~3;
		if((op & _AA) == 0)
			ppc.pc += lr - 4;
		UpdatePC();
	}

	if(op & _LK)
		LR = lr;

	return 1;
}

INT I_Bcctrx (UINT32 op)
{
	UINT32 lr = ppc.pc;
	BOOL cond = ppc_bo[_BO >> 1](_BI);

	if(cond)
	{
		ppc.pc = CTR & ~3;
		UpdatePC();
	}

	if(op & _LK)
		LR = lr;

	return 1;
}

INT I_Bclrx (UINT32 op)
{
	UINT32 lr = ppc.pc;
	BOOL cond = ppc_bo[_BO >> 1](_BI);

	if(cond)
	{
		ppc.pc = LR & ~3;
		UpdatePC();
	}

	if(op & _LK)
		LR = lr;

	return 1;
}

INT I_Rfi (UINT32 op)
{
	ppc.pc = ppc.spr[SPR_SRR0];
	SetMSR(ppc.spr[SPR_SRR1]);

	UpdatePC();

	return 1;
}

#if PPC_MODEL == PPC_MODEL_4XX

INT I_Rfci (UINT32 op)
{
	ppc.pc = ppc.spr[SPR_SRR2];
	SetMSR(ppc.spr[SPR_SRR3]);

	UpdatePC();

	return 1;
}

#endif // PPC_MODEL == PPC_MODEL_4XX

INT I_Sc (UINT32 op)
{
	SPR(SPR_SRR0) = ppc.pc;
	SPR(SPR_SRR1) = ppc.msr;// & 0x87C0FF73;

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	ppc.pc = (ppc.msr & 0x40) ? 0x80000C00 : 0xC00;	// hack
#else
	ppc.pc = (ppc.msr & 0x40) ? 0xFFF00C00 : 0xC00;
#endif
	ppc.msr = (ppc.msr & 0x11040) | ((ppc.msr >> 16) & 1);

	UpdatePC();

	return 1;
}

INT I_Tw (UINT32 op)
{
	Print("%08X: unhandled instruction tw\n", PC);

	return 1;
}

INT I_Twi (UINT32 op)
{
	Print("%08X: unhandled instruction twi\n", PC);

	return 1;
}
