/*
 * front/powerpc/decode/d_fp.c
 *
 * Decode handlers for FPU instructions.
 */

#include "drppc.h"
#include "../powerpc.h"
#include "../internal.h"
#include "middle/ir.h"

/*******************************************************************************
 Local Stuff
*******************************************************************************/

#define ENCODE_FPU_CHECK() \
		{ /* ASSERT(MSR & 0x2000); */ }

/*******************************************************************************
 Load/Store
*******************************************************************************/

INT D_Lfd(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeLoad64(ID_F(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeLoad64(ID_F(rt), ID_R(ra), ea);


	return DRPPC_OKAY;
}

INT D_Lfdu(UINT32 op)
{
	ASSERT(0);
	return DRPPC_OKAY;
}

INT D_Lfdux(UINT32 op)
{
	ASSERT(0);
	return DRPPC_OKAY;
}

INT D_Lfdx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_OKAY;
}

INT D_Lfs(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeLoad32(ID_TEMP(2), ID_TEMP(0), 0);
	}
	else
		IR_EncodeLoad32(ID_TEMP(2), ID_R(ra), ea);

	IR_EncodeConvert(IR_S_TO_D, ID_F(rt), ID_TEMP(2));

	return DRPPC_OKAY;
}

INT D_Lfsu(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Lfsux(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Lfsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Stfd(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeStore64(ID_F(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeStore64(ID_F(rt), ID_R(ra), ea);

	return DRPPC_OKAY;
}

INT D_Stfdu(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Stfdux(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Stfdx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Stfiwx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	ASSERT(0);

	if (ra == 0)
	{

	}
	else
		IR_EncodeStore32(

	return DRPPC_OKAY;
}

/*
INT I_Stfiwx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if(a) ea += R(a);

	Write32(ea, F(t).iw);

	return 1;
}
*/

INT D_Stfs(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeConvert(IR_D_TO_S, ID_TEMP(2), ID_F(rt));
		IR_EncodeStore32(ID_TEMP(2), ID_TEMP(0), 0);
	}
	else
	{
		IR_EncodeConvert(IR_D_TO_S, ID_TEMP(2), ID_F(rt));
		IR_EncodeStore32(ID_TEMP(2), ID_R(ra), ea);
	}

	return DRPPC_OKAY;
}

INT D_Stfsu(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Stfsux(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Stfsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

/*******************************************************************************
 Arithmetical
*******************************************************************************/

INT D_Fabsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Faddx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Faddsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fcmpo(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fcmpu(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

void drc_append_set_temp_fp_rounding(struct drccore *drc, UINT8 rounding)
{
	_fldcw_m16abs(&fp_control[rounding]);							// fldcw [fp_control]
}

void drc_append_restore_fp_rounding(struct drccore *drc)
{
	_fldcw_m16abs(&drc->fpcw_curr);									// fldcw [fpcw_curr]
}

INT D_Fctiwx(UINT32 op)
{
	UINT	rb = RB;
	UINT	rt = RT;

	ENCODE_FPU_CHECK();

	// VXSNAN from rb!
	// various flags

	// F(t).id = (INT64)(INT32)(round/trunc/ceil/floor_f64_to_s32(F(b).fd);

	return DRPPC_OKAY;
}

INT D_Fctiwzx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fdivx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	ENCODE_FPU_CHECK();

	IR_EncodeFDiv(ID_F(rt), ID_F(ra), ID_F(rb));

	// VXSNAN from ra, rb!
	// FPRF from rt!
	// FCR1!

	return DRPPC_OKAY;
}

INT D_Fdivsx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	ENCODE_FPU_CHECK();

	IR_EncodeFDiv(ID_F(rt), ID_F(ra), ID_F(rb));
	IR_EncodeConvert(IR_D_TO_S, ID_TEMP(0), ID_TEMP(0));
	IR_EncodeConvert(IR_S_TO_D, ID_F(rt), ID_TEMP(0));

	// VXSNAN from ra, rb!
	// FPRF from rt!
	// FCR1!

	return DRPPC_OKAY;
}

INT D_Fmaddx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fmaddsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fmrx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fmsubx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fmsubsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fmulx(UINT32 op)
{
	UINT	rc = RC;
	UINT	ra = RA;
	UINT	rt = RT;

	ENCODE_FPU_CHECK();

	IR_EncodeFMul(ID_F(rt), ID_F(ra), ID_F(rc));

	// VXSNAN from ra, rb!
	// FPRF from rt!
	// FCR1!

	return DRPPC_OKAY;
}

INT D_Fmulsx(UINT32 op)
{
	UINT	rc = RC;
	UINT	ra = RA;
	UINT	rt = RT;

	ENCODE_FPU_CHECK();

	IR_EncodeFMul(ID_TEMP(0), ID_F(ra), ID_F(rc));
	IR_EncodeConvert(IR_D_TO_S, ID_TEMP(0), ID_TEMP(0));
	IR_EncodeConvert(IR_S_TO_D, ID_F(rt), ID_TEMP(0));

	// VXSNAN from ra, rb!
	// FPRF from rt!
	// FCR1!

	return DRPPC_OKAY;
}

INT D_Fnabsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fnegx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fnmaddx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fnmaddsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fnmsubx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fnmsubsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fresx(UINT32 op)
{
	UINT	rb = RB;
	UINT	rt = RT;

	ENCODE_FPU_CHECK();

//	IR_EncodeFRes_64(ID_F(rt), ID_F(rb));

	// VXSNAN on rb!
	// FPRF on rt!
	// FCR1!

	ASSERT(0);

	return DRPPC_OKAY;
}

INT D_Frspx(UINT32 op)
{
	UINT	rb = RB;
	UINT	rt = RT;

	ENCODE_FPU_CHECK();

	IR_EncodeConvert(IR_D_TO_S, ID_TEMP(2), ID_F(rb));
	IR_EncodeConvert(IR_S_TO_D, ID_F(rt), ID_TEMP(2));

	// VXSNAN on rb!
	// FPRF on rt!
	// FCR1!

	return DRPPC_OKAY;
}

INT D_Frsqrtex(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fselx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fsqrtx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fsqrtsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fsubx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	ENCODE_FPU_CHECK();

	IR_EncodeFSub(ID_F(rt), ID_F(rb), ID_F(ra));

	// VXSNAN on ra, rb!
	// FPRF on rt!
	// FCR1!

	return DRPPC_OKAY;
}

INT D_Fsubsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}
