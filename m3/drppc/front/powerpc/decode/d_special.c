/*
 * front/powerpc/decode/d_special.c
 *
 * Decode handlers for special instructions (special register management and
 * such.)
 */

#include "drppc.h"
#include "../powerpc.h"
#include "../internal.h"
#include "middle/ir.h"

/*******************************************************************************
 Local Macros
*******************************************************************************/

#define CROP_GET_REGS \
		UINT bm = _BBM, bn = _BBN, crfb = DFLOW_BIT_CR0_SO + bn * 4 + bm; \
		UINT am = _BAM, an = _BAN, crfa = DFLOW_BIT_CR0_SO + an * 4 + am; \
		UINT dm = _BDM, dn = _BDN, crfd = DFLOW_BIT_CR0_SO + dn * 4 + dm;

/*******************************************************************************
 Decode Handlers
*******************************************************************************/

INT D_Crand(UINT32 op)
{
	CROP_GET_REGS

	IR_EncodeAnd(crfd, crfa, crfb);

	return DRPPC_OKAY;
}

INT D_Crandc(UINT32 op)
{
	CROP_GET_REGS

	IR_EncodeMove(ID_TEMP(0), crfb);
	IR_EncodeNot(ID_TEMP(0), ID_TEMP(0));
	IR_EncodeAnd(crfd, crfa, ID_TEMP(0));

	return DRPPC_OKAY;
}

INT D_Crnand(UINT32 op)
{
	CROP_GET_REGS

	IR_EncodeAnd(crfd, crfa, crfb);
	IR_EncodeNot(crfd, crfd);

	return DRPPC_OKAY;
}

INT D_Cror(UINT32 op)
{
	CROP_GET_REGS

	IR_EncodeOr(crfd, crfa, crfb);

	return DRPPC_OKAY;
}

INT D_Crorc(UINT32 op)
{
	CROP_GET_REGS

	IR_EncodeMove(ID_TEMP(0), crfb);
	IR_EncodeNot(ID_TEMP(0), ID_TEMP(0));
	IR_EncodeOr(crfd, crfa, ID_TEMP(0));

	return DRPPC_OKAY;
}

INT D_Crnor(UINT32 op)
{
	CROP_GET_REGS

	IR_EncodeOr(crfd, crfa, crfb);
	IR_EncodeNot(crfd, crfd);

	return DRPPC_OKAY;
}

INT D_Crxor(UINT32 op)
{
	CROP_GET_REGS

	IR_EncodeXor(crfd, crfa, crfb);

	return DRPPC_OKAY;
}

INT D_Creqv(UINT32 op)
{
	CROP_GET_REGS

	IR_EncodeXor(crfd, crfa, crfb);
	IR_EncodeNot(crfd, crfd);

	return DRPPC_OKAY;
}

/*******************************************************************************

*******************************************************************************/

INT D_Dcba(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Dcbf(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Dcbi(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Dcbst(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Dcbt(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Dcbtst(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Dcbz(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Eieio(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Icbi(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Isync(UINT32 op)
{
	/* ISYNC! Possible self-modifying code! */

	return DRPPC_OKAY;
}

INT D_Mcrf(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mcrxr(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mfcr(UINT32 op)
{
	UINT	rt = RT;
	UINT	i;

	IR_EncodeLoadi(ID_R(rt), 0);

	for (i = 0; i < 8; i++)
	{
		IR_EncodeOr(ID_R(rt), ID_R(rt), DFLOW_BIT_CR0_LT+i*4);
		IR_EncodeShli(ID_R(rt), ID_R(rt), 1);
		IR_EncodeOr(ID_R(rt), ID_R(rt), DFLOW_BIT_CR0_GT+i*4);
		IR_EncodeShli(ID_R(rt), ID_R(rt), 1);
		IR_EncodeOr(ID_R(rt), ID_R(rt), DFLOW_BIT_CR0_EQ+i*4);
		IR_EncodeShli(ID_R(rt), ID_R(rt), 1);
		IR_EncodeOr(ID_R(rt), ID_R(rt), DFLOW_BIT_CR0_SO+i*4);
		if (i != 7)
			IR_EncodeShli(ID_R(rt), ID_R(rt), 1);
	}

	return DRPPC_OKAY;
}

INT D_Mfspr(UINT32 op)
{
	UINT	sprf = _SPRF;
	UINT	rt = RT;

	// NOTE: PowerPC 6xx only!

	switch (sprf)
	{

	// Special Handling
	case SPR_XER:
		IR_EncodeMove(ID_R(rt),   DFLOW_BIT_XER_COUNT);	// rt <- xer.count
		IR_EncodeMove(ID_TEMP(0), DFLOW_BIT_XER_SO);	// t0 <- xer.so
		IR_EncodeMove(ID_TEMP(1), DFLOW_BIT_XER_OV);	// t1 <- xer.ov
		IR_EncodeMove(ID_TEMP(2), DFLOW_BIT_XER_CA);	// t2 <- xer.ca

		IR_EncodeAndi(ID_R(rt),   ID_TEMP(3), 0x7F);	// rt <- rt & 0x7F
		IR_EncodeShli(ID_TEMP(0), ID_TEMP(0), 31);		// t0 <- t0 << 31
		IR_EncodeShli(ID_TEMP(1), ID_TEMP(1), 30);		// t1 <- t1 << 30
		IR_EncodeShli(ID_TEMP(2), ID_TEMP(2), 29);		// t2 <- t2 << 29

		IR_EncodeOr(ID_TEMP(0), ID_TEMP(0), ID_TEMP(1));// t0 <- t0 | t1
		IR_EncodeOr(ID_R(rt),   ID_R(rt),   ID_TEMP(2));// rt <- rt | t2
		IR_EncodeOr(ID_R(rt),   ID_R(rt),   ID_TEMP(0));// rt <- rt | t0
		break;
	case SPR_LR:
		IR_EncodeMove(ID_R(rt), DFLOW_BIT_LR);
		break;
	case SPR_CTR:
		IR_EncodeMove(ID_R(rt), DFLOW_BIT_CTR);
		break;
	case SPR_PVR:
		IR_EncodeLoadi(ID_R(rt), SPR(SPR_PVR));
		break;

	// Default Handling
	case SPR_SRR0:
	case SPR_SRR1:
	case SPR_HID0:
		IR_EncodeLoadPtr32(ID_R(rt), (UINT32)&SPR(sprf));
		break;

	// Unhandled Cases
	default:
		printf("Mfspr: sprf = %u\n", sprf);
		ASSERT(0);
		break;
	}

	return DRPPC_OKAY;
}

INT D_Mtspr(UINT32 op)
{
	UINT	sprf = _SPRF;
	UINT	rt = RT;

	// NOTE: PowerPC 6xx only!

	switch (sprf)
	{
	// Special Cases
	case SPR_XER:
		IR_EncodeMove(DFLOW_BIT_XER_SO, ID_R(rt));
		IR_EncodeMove(DFLOW_BIT_XER_OV, ID_R(rt));
		IR_EncodeMove(DFLOW_BIT_XER_CA, ID_R(rt));
		IR_EncodeMove(DFLOW_BIT_XER_COUNT, ID_R(rt));
		IR_EncodeShri(DFLOW_BIT_XER_SO, DFLOW_BIT_XER_SO, 31);
		IR_EncodeShri(DFLOW_BIT_XER_OV, DFLOW_BIT_XER_OV, 30);
		IR_EncodeShri(DFLOW_BIT_XER_CA, DFLOW_BIT_XER_CA, 29);
		IR_EncodeAndi(DFLOW_BIT_XER_COUNT, DFLOW_BIT_XER_COUNT, 0x7F);
		IR_EncodeAndi(DFLOW_BIT_XER_SO, DFLOW_BIT_XER_SO, 1);
		IR_EncodeAndi(DFLOW_BIT_XER_OV, DFLOW_BIT_XER_OV, 1);
		IR_EncodeAndi(DFLOW_BIT_XER_CA, DFLOW_BIT_XER_CA, 1);
		break;
	case SPR_LR:
		IR_EncodeMove(DFLOW_BIT_LR, ID_R(rt));
		break;
	case SPR_CTR:
		IR_EncodeMove(DFLOW_BIT_CTR, ID_R(rt));
		break;
	case SPR_DEC:
	case SPR_PVR:
	case SPR_TBL_W:
	case SPR_TBL_R:	// special 603e case
	case SPR_TBU_R:
	case SPR_TBU_W:	// special 603e case
	case SPR_DMA_U:
	case SPR_DMA_L:
		printf("Mtsprf sprf = %u\n", sprf);
		ASSERT(0);
	default:
		IR_EncodeStorePtr32(ID_R(rt), (UINT32)&SPR(sprf));
		break;
	}
	
	return DRPPC_OKAY;
}

INT D_Mfmsr (UINT32 op)
{
	IR_EncodeLoadPtr32(ID_R(RT), (UINT32)&MSR);		// FIXME

	return DRPPC_OKAY;
}

INT D_Mtmsr(UINT32 op)
{
	IR_EncodeStorePtr32(ID_R(RT), (UINT32)&MSR);	// FIXME

	return DRPPC_OKAY;
}

INT D_Mtcrf(UINT32 op)
{
	UINT	fxm = _FXM;
	UINT	rt = RT;
	UINT	i;

	for (i = 0; i < 8; i++)
	{
		if (fxm & (1 << i))
		{
			IR_EncodeMove(ID_TEMP(0), ID_R(rt));
			IR_EncodeAndi(ID_TEMP(0), ID_TEMP(0), (1 << ((i*4)+0)));
			IR_EncodeCmpi(IR_CONDITION_NE, ((7-i)*4)+DFLOW_BIT_CR0_SO, ID_TEMP(0), 0);
			IR_EncodeMove(ID_TEMP(0), ID_R(rt));
			IR_EncodeAndi(ID_TEMP(0), ID_TEMP(0), (1 << ((i*4)+1)));
			IR_EncodeCmpi(IR_CONDITION_NE, ((7-i)*4)+DFLOW_BIT_CR0_EQ, ID_TEMP(0), 0);
			IR_EncodeMove(ID_TEMP(0), ID_R(rt));
			IR_EncodeAndi(ID_TEMP(0), ID_TEMP(0), (1 << ((i*4)+2)));
			IR_EncodeCmpi(IR_CONDITION_NE, ((7-i)*4)+DFLOW_BIT_CR0_GT, ID_TEMP(0), 0);
			IR_EncodeMove(ID_TEMP(0), ID_R(rt));
			IR_EncodeAndi(ID_TEMP(0), ID_TEMP(0), (1 << ((i*4)+3)));
			IR_EncodeCmpi(IR_CONDITION_NE, ((7-i)*4)+DFLOW_BIT_CR0_LT, ID_TEMP(0), 0);
		}
	}

	return DRPPC_OKAY;
}

INT D_Sync(UINT32 op)
{
	/* SYNC! */

	return DRPPC_OKAY;
}

/*
 * 4xx
 */

INT D_Dccci(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Dcread(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Icbt(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Iccci(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Icread(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mfdcr(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mtdcr(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Wrtee(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Wrteei(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

/*
 * 6xx
 */

INT D_Eciwx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Ecowx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mcrfs(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mffsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mfsr(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mfsrin(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mftb (UINT32 op)
{
	UINT32	ptr;

	IR_EncodeSync(GetDecodeCycles());

	ResetDecodeCycles();

	switch (_SPRF)
	{
	case 268:	ptr = (UINT32)&ReadTimebaseLo; break;
	case 269:	ptr = (UINT32)&ReadTimebaseHi; break;
	default:	ASSERT(0); break;
	}

	IR_EncodeCallRead(ptr, ID_R(RT));

	return DRPPC_OKAY;
}

INT D_Mtfsb0x(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mtfsb1x(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mtfsfx(UINT32 op)
{
	UINT	rb = RB;
	UINT	fm = _FM;

	/*
	UINT	fprm[4] =
	{
		IR_FPRM_ROUND,	IR_FPRM_TRUNC,
		IR_FPRM_CEIL,	IR_FPRM_FLOOR,
	};

	// 
	IR_EncodeStorePtr32(ID_TEMP(0), (UINT32)&FPSCR);

	// Set rounding mode
	IR_EncodeAndi(ID_TEMP(0), ID_TEMP(0), 3);
	IR_EncodeSetFPRoundingMode(ID_TEMP(0));
	*/

	return DRPPC_OKAY;
}
/*
INT I_Mtfsfx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 f = _FM;

	f =	((f & 0x80) ? 0xF0000000 : 0) |
		((f & 0x40) ? 0x0F000000 : 0) |
		((f & 0x20) ? 0x00F00000 : 0) |
		((f & 0x10) ? 0x000F0000 : 0) |
		((f & 0x08) ? 0x0000F000 : 0) |
		((f & 0x04) ? 0x00000F00 : 0) |
		((f & 0x02) ? 0x000000F0 : 0) |
		((f & 0x01) ? 0x0000000F : 0);

	FPSCR &= (~f) | ~(FPSCR_FEX | FPSCR_VX);
	FPSCR |= (F(b).iw) & ~(FPSCR_FEX | FPSCR_VX);

	// FEX, VX

	SET_FCR1();

	return 1;
}
*/
INT D_Mtfsfix(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mtsr(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Mtsrin(UINT32 op)
{
	UINT	rb = RB;
	UINT	rt = RT;

	// SR(R(b) >> 28) = R(t)

	/* TODO */

	return DRPPC_OKAY;
}

INT D_Tlbia(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Tlbie(UINT32 op)
{
	/* TLBIE! */

	return DRPPC_OKAY;
}

INT D_Tlbld(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Tlbli(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Tlbsync(UINT32 op)
{
	/* TLBSYNC! */

	return DRPPC_OKAY;
}
