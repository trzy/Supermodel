/*
 * front/powerpc/decode/d_arith.c
 *
 * Decode handlers for ALU instructions.
 */

#include "drppc.h"
#include "../powerpc.h"
#include "../internal.h"
#include "middle/ir.h"

/*******************************************************************************
 Local Routines
*******************************************************************************/

static void EncodeCmpS (UINT crn, UINT ra, UINT rb)
{
	IR_EncodeCmp(IR_CONDITION_LTS,	ID_CR_LT(crn), ID_R(ra), ID_R(rb));
	IR_EncodeCmp(IR_CONDITION_GTS,	ID_CR_GT(crn), ID_R(ra), ID_R(rb));
	IR_EncodeCmp(IR_CONDITION_EQ,	ID_CR_EQ(crn), ID_R(ra), ID_R(rb));
	IR_EncodeMove(					ID_CR_SO(crn), DFLOW_BIT_XER_SO);
}

static void EncodeCmpU (UINT crn, UINT ra, UINT rb)
{
	IR_EncodeCmp(IR_CONDITION_LTU,	ID_CR_LT(crn), ID_R(ra), ID_R(rb));
	IR_EncodeCmp(IR_CONDITION_GTU,	ID_CR_GT(crn), ID_R(ra), ID_R(rb));
	IR_EncodeCmp(IR_CONDITION_EQ,	ID_CR_EQ(crn), ID_R(ra), ID_R(rb));
	IR_EncodeMove(					ID_CR_SO(crn), DFLOW_BIT_XER_SO);
}

static void EncodeCmpiS (UINT crn, UINT reg, UINT32 imm)
{
	IR_EncodeCmpi(IR_CONDITION_LTS, ID_CR_LT(crn), ID_R(reg), imm);
	IR_EncodeCmpi(IR_CONDITION_GTS,	ID_CR_GT(crn), ID_R(reg), imm);
	IR_EncodeCmpi(IR_CONDITION_EQ,	ID_CR_EQ(crn), ID_R(reg), imm);
	IR_EncodeMove(					ID_CR_SO(crn), DFLOW_BIT_XER_SO);
}

static void EncodeCmpiU (UINT crn, UINT reg, UINT32 imm)
{
	IR_EncodeCmpi(IR_CONDITION_LTU, ID_CR_LT(crn), ID_R(reg), imm);
	IR_EncodeCmpi(IR_CONDITION_GTU,	ID_CR_GT(crn), ID_R(reg), imm);
	IR_EncodeCmpi(IR_CONDITION_EQ,	ID_CR_EQ(crn), ID_R(reg), imm);
	IR_EncodeMove(					ID_CR_SO(crn), DFLOW_BIT_XER_SO);
}

#define ENCODE_SET_CR0(reg) { if (op & _RC) EncodeCmpiS(0, reg, 0); }

/*******************************************************************************
 Decode Handlers
*******************************************************************************/

INT D_Addx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeAdd(ID_R(rt), ID_R(ra), ID_R(rb));

	// XER Ca
	ASSERT(!(op & _OVE));

	// CR0
	ENCODE_SET_CR0(rt);

	return DRPPC_OKAY;
}

INT D_Addcx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Addex(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Addi(UINT32 op)
{
	UINT32	imm	= SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
		IR_EncodeLoadi(ID_R(rt), imm);
	else
		IR_EncodeAddi(ID_R(rt), ID_R(ra), imm);

	return DRPPC_OKAY;
}

INT D_Addic(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Addic_(UINT32 op)
{
	UINT32	imm = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeMove(ID_TEMP(0), ID_R(ra));
	IR_EncodeAddi(ID_R(rt), ID_R(ra), imm);

	// XER Ca
	IR_EncodeCmp(IR_CONDITION_LTU, DFLOW_BIT_XER_CA, ID_R(rt), ID_TEMP(0));

	// CR0
	EncodeCmpiS(0, rt, 0);

	return DRPPC_OKAY;
}

INT D_Addis(UINT32 op)
{
	UINT32	imm = IMMS;
	UINT	a = RA;
	UINT	t = RT;

	if (a == 0)
		IR_EncodeLoadi(ID_R(t), imm);
	else
		IR_EncodeAddi(ID_R(t), ID_R(a), imm);

	return DRPPC_OKAY;
}

INT D_Addmex(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Addzex(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Cmp(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	crft = CRFT;

	EncodeCmpS(crft, ra, rb);

	return DRPPC_OKAY;
}

INT D_Cmpl(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	crft = CRFT;

	EncodeCmpU(crft, ra, rb);

	return DRPPC_OKAY;
}

INT D_Cmpi(UINT32 op)
{
	UINT32	imm = SIMM;
	UINT	ra = RA;
	UINT	crft = CRFT;

	EncodeCmpiS(crft, ra, imm);

	return DRPPC_OKAY;
}

INT D_Cmpli(UINT32 op)
{
	UINT32	imm = UIMM;
	UINT	ra = RA;
	UINT	crft = CRFT;

	EncodeCmpiU(crft, ra, imm);

	return DRPPC_OKAY;
}

INT D_Cntlzwx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Divwx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Divwux(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Eqvx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Extsbx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Extshx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Mulhwx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Mulhwux(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Mulli(UINT32 op)
{
	UINT32	imm = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeMului(ID_R(rt), ID_R(ra), imm);

	return DRPPC_OKAY;
}

INT D_Mullwx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	ASSERT(!(op & _OVE));

	IR_EncodeMulu(ID_R(rt), ID_R(ra), ID_R(rb));

	ENCODE_SET_CR0(rt);

	return DRPPC_OKAY;
}

INT D_Negx(UINT32 op)
{
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeNeg(ID_R(rt), ID_R(ra));

	ASSERT(!(op & _OVE));

	ENCODE_SET_CR0(rt);

	return DRPPC_OKAY;
}

INT D_Subfx (UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeSub(ID_R(rt), ID_R(rb), ID_R(ra));

	if (op & _OVE)
	{
		// SET_SUB_V(R(rt), R(ra), R(rb)) // old vals!
		ASSERT(0);
	}

	ENCODE_SET_CR0(rt);

	return DRPPC_OKAY;
}

INT D_Subfcx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Subfex(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Subfic(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Subfmex(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Subfzex(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Andx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_OKAY;
}

INT D_Andcx(UINT32 op)
{
	UINT	ra = RA;
	UINT	rb = RB;
	UINT	rt = RT;

	IR_EncodeNot(ID_TEMP(0), ID_R(rb));
	IR_EncodeAnd(ID_R(ra), ID_R(rt), ID_TEMP(0));

	ENCODE_SET_CR0(ra);

	return DRPPC_OKAY;
}

INT D_Andi_(UINT32 op)
{
	UINT32	imm = UIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeAndi(ID_R(ra), ID_R(rt), imm);

	EncodeCmpiS(0, ra, 0);

	return DRPPC_OKAY;
}

INT D_Andis_(UINT32 op)
{
	UINT32	imm = IMMS;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeAndi(ID_R(ra), ID_R(rt), imm);

	EncodeCmpiS(0, ra, 0);

	return DRPPC_OKAY;
}

INT D_Nandx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Norx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeOr(ID_R(ra), ID_R(rt), ID_R(rb));
	IR_EncodeNot(ID_R(ra), ID_R(ra));

	ENCODE_SET_CR0(ra);

	return DRPPC_OKAY;
}

INT D_Orx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeOr(ID_R(ra), ID_R(rt), ID_R(rb));

	ENCODE_SET_CR0(ra);

	return DRPPC_OKAY;
}

INT D_Orcx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Ori(UINT32 op)
{
	UINT32	imm = UIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeOri(ID_R(ra), ID_R(rt), imm);	

	return DRPPC_OKAY;
}

INT D_Oris(UINT32 op)
{
	UINT32	imm = IMMS;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeOri(ID_R(ra), ID_R(rt), imm);

	return DRPPC_OKAY;
}

INT D_Rlwimix(UINT32 op)
{
	UINT32	mask = mask_table[_MB_ME];
	UINT32	sh = _SH;
	UINT	ra = RA;
	UINT	rt = RT;

	ASSERT(mask);

	if (sh == 0)
		IR_EncodeAndi(ID_TEMP(0), ID_R(rt), mask);
	else if (mask == (0xFFFFFFFF << sh))
		IR_EncodeShli(ID_TEMP(0), ID_R(rt), sh);
	else if (mask == (0xFFFFFFFF >> (32 - sh)))
		IR_EncodeShri(ID_TEMP(0), ID_R(rt), 32 - sh);
	else
	{
		IR_EncodeRoli(ID_TEMP(0), ID_R(rt), sh);
		IR_EncodeAndi(ID_TEMP(0), ID_TEMP(0), mask);
	}

	IR_EncodeAndi(ID_R(ra), ID_R(ra), ~mask);
	IR_EncodeOr(ID_R(ra), ID_R(ra), ID_TEMP(0));

	if (op & _RC)
		EncodeCmpiS(0, ra, 0);

	return DRPPC_OKAY;
}

INT D_Rlwinmx(UINT32 op)
{
	UINT32	mask = mask_table[_MB_ME];
	UINT	sh = _SH;
	UINT	ra = RA;
	UINT	rt = RT;

	ASSERT(mask);

	if (sh == 0)								// And
		IR_EncodeAndi(ID_R(ra), ID_R(rt), mask);
	else if (mask == (0xFFFFFFFF << sh))		// Shl
		IR_EncodeShli(ID_R(ra), ID_R(rt), sh);
	else if (mask == (0xFFFFFFFF >> (32 - sh)))	// Shr
		IR_EncodeShri(ID_R(ra), ID_R(rt), 32 - sh);
	else										// Rol
	{
		IR_EncodeRoli(ID_R(ra), ID_R(rt), sh);
		IR_EncodeAndi(ID_R(ra), ID_R(ra), mask);
	}

	if (op & _RC)
		EncodeCmpiS (0, ra, 0);

	return DRPPC_OKAY;
}

INT D_Rlwnmx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Slwx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Srawx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Srawix(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Srwx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Xorx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeXor(ID_R(ra), ID_R(rt), ID_R(rb));

	if (op & _RC)
		EncodeCmpiS(0, ra, 0);

	return DRPPC_OKAY;
}

INT D_Xori(UINT32 op)
{
	UINT32	imm = UIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeXori(ID_R(ra), ID_R(rt), imm);

	return DRPPC_OKAY;
}

INT D_Xoris(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}
