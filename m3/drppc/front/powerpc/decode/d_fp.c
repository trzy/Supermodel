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
 Load/Store
*******************************************************************************/

INT D_Lfd(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
		IR_EncodeLoadi(ID_TEMP(0), ea);
	else
		IR_EncodeAddi(ID_TEMP(0), ID_R(ra), ea);

	IR_EncodeLoad64(ID_F(rt), ID_TEMP(0), 0);

	return DRPPC_OKAY;
}

INT D_Lfdu(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Lfdux(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Lfdx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Lfs(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
		IR_EncodeLoadi(ID_TEMP(0), ea);
	else
		IR_EncodeAddi(ID_TEMP(0), ID_R(ra), ea);

	IR_EncodeLoad32(ID_TEMP(1), ID_TEMP(0), 0);			// temp1 = temp0 + 0
	IR_EncodeConvert(IR_S_TO_D, ID_F(rt), ID_TEMP(1));	// ft = cvt.s.d temp0

	return DRPPC_OKAY;
}

INT D_Lfsu(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Lfsux(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Lfsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stfd(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
		IR_EncodeLoadi(ID_TEMP(0), ea);
	else
		IR_EncodeAddi(ID_TEMP(0), ID_R(ra), ea);

	IR_EncodeStore64(ID_F(rt), ID_TEMP(0), 0);

	return DRPPC_OKAY;
}

INT D_Stfdu(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stfdux(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stfdx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stfiwx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stfs(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
		IR_EncodeLoadi(ID_TEMP(0), ea);
	else
		IR_EncodeAddi(ID_TEMP(0), ID_R(ra), ea);

	IR_EncodeConvert(IR_D_TO_S, ID_TEMP(1), ID_F(rt));
	IR_EncodeStore32(ID_TEMP(1), ID_TEMP(0), 0);

	return DRPPC_OKAY;
}

INT D_Stfsu(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stfsux(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stfsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

/*******************************************************************************
 Arithmetical
*******************************************************************************/

INT D_Fabsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Faddx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Faddsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fcmpo(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fcmpu(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fctiwx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fctiwzx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fdivx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fdivsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fmaddx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fmaddsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fmrx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fmsubx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fmsubsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fmulx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fmulsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fnabsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fnegx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fnmaddx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fnmaddsx(UINT32 op)
{
	ASSERT(0); return DRPPC_OKAY;
}

INT D_Fnmsubx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fnmsubsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fresx(UINT32 op)
{
	UINT	rb = RB;
	UINT	rt = RT;

	// CHECK_FPU_AVAILABLE();

	// SET_VXSNAN_1(F(rb).fd);

	// F(rt).fd = 1.0 / F(rb).fd;

	// set_fprf(F(rt).fd);

	// SET_FCR1();

	return DRPPC_OKAY;
}

INT D_Frspx(UINT32 op)
{
	UINT	rb = RB;
	UINT	rt = RT;

	// CHECK_FPU_AVAILABLE();

	// SET_VXSNAN_1(F(b).fd);

	// F(t).fd = (FLOAT32)F(b).fd;

	// set_fprf(F(t).fd);

	// SET_FCR1();

	return DRPPC_OKAY;
}

INT D_Frsqrtex(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fselx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fsqrtx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fsqrtsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Fsubx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	// CHECK_FPU_AVAILABLE();

    // SET_VXSNAN(F(a).fd, F(b).fd);

	// F(t).fd = F(a).fd - F(b).fd;

    // set_fprf(F(t).fd);

	// SET_FCR1();

	return DRPPC_OKAY;
}

INT D_Fsubsx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}
