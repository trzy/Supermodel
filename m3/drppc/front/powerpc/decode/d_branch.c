/*
 * front/powerpc/decode/d_branch.c
 *
 * Branch instruction decoders.
 */

#include "drppc.h"
#include "../powerpc.h"
#include "../internal.h"
#include "middle/ir.h"

/*******************************************************************************
 Local Routines
*******************************************************************************/

static void PlaceCondInTEMP0 (UINT bo, UINT bi)
{
	switch (bo >> 1)
	{
	case 0x0:			// --CTR != 0 && CR == 0
		IR_EncodeAddi(DFLOW_BIT_CTR, DFLOW_BIT_CTR, -1);
		IR_EncodeCmpi(IR_CONDITION_NE, DFLOW_BIT_TEMP0, DFLOW_BIT_CTR, 0);
		IR_EncodeCmpi(IR_CONDITION_EQ, DFLOW_BIT_TEMP1, DFLOW_BIT_CR0_LT + bi, 0);
		IR_EncodeAnd(DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP1);
		break;
	case 0x1:			// --CTR == 0 && CR == 0
		IR_EncodeAddi(DFLOW_BIT_CTR, DFLOW_BIT_CTR, -1);
		IR_EncodeCmpi(IR_CONDITION_EQ, DFLOW_BIT_TEMP0, DFLOW_BIT_CTR, 0);
		IR_EncodeCmpi(IR_CONDITION_EQ, DFLOW_BIT_TEMP1, DFLOW_BIT_CR0_LT + bi, 0);
		IR_EncodeAnd(DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP1);
		break;
	case 0x4:			// --CTR != 0 && CR != 0
		IR_EncodeAddi(DFLOW_BIT_CTR, DFLOW_BIT_CTR, -1);
		IR_EncodeCmpi(IR_CONDITION_NE, DFLOW_BIT_TEMP0, DFLOW_BIT_CTR, 0);
		IR_EncodeCmpi(IR_CONDITION_NE, DFLOW_BIT_TEMP1, DFLOW_BIT_CR0_LT + bi, 0);
		IR_EncodeAnd(DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP1);
		break;
	case 0x5:			// --CTR == 0 && CR == 0
		IR_EncodeAddi(DFLOW_BIT_CTR, DFLOW_BIT_CTR, -1);
		IR_EncodeCmpi(IR_CONDITION_NE, DFLOW_BIT_TEMP0, DFLOW_BIT_CTR, 0);
		IR_EncodeCmpi(IR_CONDITION_EQ, DFLOW_BIT_TEMP1, DFLOW_BIT_CR0_LT + bi, 0);
		IR_EncodeAnd(DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP1);
		break;
	case 0x8: case 0xC:	// --CTR != 0
		IR_EncodeAddi(DFLOW_BIT_CTR, DFLOW_BIT_CTR, -1);
		IR_EncodeCmpi(IR_CONDITION_NE, DFLOW_BIT_TEMP0, DFLOW_BIT_CTR, 0);
		break;
	case 0x9: case 0xD:	// --CTR == 0
		IR_EncodeAddi(DFLOW_BIT_CTR, DFLOW_BIT_CTR, -1);
		IR_EncodeCmpi(IR_CONDITION_EQ, DFLOW_BIT_TEMP0, DFLOW_BIT_CTR, 0);
		break;
	case 0x2: case 0x3:	// CR == 0
		IR_EncodeCmpi(IR_CONDITION_EQ, DFLOW_BIT_TEMP0, DFLOW_BIT_CR0_LT + bi, 0);
		break;
	case 0x6: case 0x7:	// CR != 0
		IR_EncodeCmpi(IR_CONDITION_NE, DFLOW_BIT_TEMP0, DFLOW_BIT_CR0_LT + bi, 0);
		break;
	case 0xA: case 0xB: case 0xE: case 0xF:	// Always
		IR_EncodeLoadi(DFLOW_BIT_TEMP0, 1);
		break;
	default:
		ASSERT(0);
		break;
	}
}

/*******************************************************************************
 Decode Handlers
*******************************************************************************/

INT D_Bx(UINT32 op)
{
	UINT32	target;

	// Compute the target address
	target = (((INT32)op << 6) >> 6) & ~3;
	if ((op & _AA) == 0)
		target += ppc.pc - 4;

	// Update LR if LK bit is set
	if (op & _LK)
		IR_EncodeLoadi(DFLOW_BIT_LR, ppc.pc);

	// Do the jump
	IR_EncodeLoadi(ID_TEMP(0), target);
	IR_EncodeBranch(ID_TEMP(0));

	return DRPPC_TERMINATOR;
}

INT D_Bcx(UINT32 op)
{
	UINT32	target;
	UINT	bo = _BO;
	UINT	bi = _BI;

	// Compute the target address
	target = SIMM & ~3;
	if ((op & _AA) == 0)
		target += ppc.pc - 4;

	// Update LR if LK bit is set
	if (op & _LK)
		IR_EncodeLoadi(DFLOW_BIT_LR, ppc.pc);

	// Compute the condition and place the result in TEMP0
	PlaceCondInTEMP0(bo, bi);

	// Upload the target address to TEMP1
	IR_EncodeLoadi(DFLOW_BIT_TEMP1, target);

	// Upload the next address to TEMP2
	IR_EncodeLoadi(DFLOW_BIT_TEMP2, ppc.pc);

	// Now branch to target
	IR_EncodeBCond(DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP1, DFLOW_BIT_TEMP2);

	return DRPPC_TERMINATOR;
}

INT D_Bcctrx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_TERMINATOR;
}

INT D_Bclrx(UINT32 op)
{
	UINT32	lr = ppc.pc;
	UINT	bo = _BO;
	UINT	bi = _BI;

	// Compute the condition and place the result in TEMP0
	PlaceCondInTEMP0(bo, bi);

	// Upload the target address to TEMP1
	IR_EncodeMove(DFLOW_BIT_TEMP1, DFLOW_BIT_LR);

	// Upload the next address to TEMP2
	IR_EncodeLoadi(DFLOW_BIT_TEMP2, ppc.pc);

	// Now branch to LR
	IR_EncodeBCond(DFLOW_BIT_TEMP0, DFLOW_BIT_TEMP1, DFLOW_BIT_TEMP2);

	// Update LR if LK bit is set
	if (op & _LK)
		IR_EncodeLoadi(DFLOW_BIT_LR, ppc.pc);

	return DRPPC_TERMINATOR;
}

INT D_Rfi(UINT32 op)
{
	// TEMP0 = SRR1
	IR_EncodeLoadPtr32(ID_TEMP(0), (UINT32)&SPR(SPR_SRR1));
	// TEMP1 = SRR0
	IR_EncodeLoadPtr32(ID_TEMP(1), (UINT32)&SPR(SPR_SRR0));

	// MSR = TEMP0
	IR_EncodeStorePtr32(ID_TEMP(0), (UINT32)&MSR);			// FIXME!
	// PC = TEMP1
	IR_EncodeBranch(ID_TEMP(1));

	return DRPPC_TERMINATOR;
}

INT D_Sc(UINT32 op)
{
	ASSERT(0);
	return DRPPC_TERMINATOR;
}

INT D_Tw(UINT32 op)
{
	ASSERT(0);
	return DRPPC_TERMINATOR;
}

INT D_Twi(UINT32 op)
{
	ASSERT(0);
	return DRPPC_TERMINATOR;
}

#if PPC_MODEL == PPC_MODEL_4XX

INT D_Rfci(UINT32 op)
{
	ASSERT(0);
	return DRPPC_TERMINATOR;
}

#endif // PPC_MODEL_4XX
