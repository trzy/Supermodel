/*
 * front/powerpc/decode/d_ldst.c
 *
 * Decode handlers for load/store instructions.
 */

#include "drppc.h"
#include "../powerpc.h"
#include "../internal.h"
#include "middle/ir.h"

/*******************************************************************************
 Decode handlers
*******************************************************************************/

INT D_Lbz(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeLoad8(ID_R(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeLoad8(ID_R(rt), ID_R(ra), ea);

	return DRPPC_OKAY;
}

INT D_Lha(UINT32 op)
{
	ASSERT(0);
/*
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeLoad16(ID_R(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeLoad16(ID_R(rt), ID_R(ra), ea);
*/
	return DRPPC_OKAY;
}

INT D_Lhz(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeLoad16(ID_R(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeLoad16(ID_R(rt), ID_R(ra), ea);

	return DRPPC_OKAY;
}

INT D_Lwz(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeLoad32(ID_R(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeLoad32(ID_R(rt), ID_R(ra), ea);

	return DRPPC_OKAY;
}

INT D_Lbzu(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeAddi(ID_R(ra), ID_R(ra), ea);
	IR_EncodeLoad8(ID_R(rt), ID_R(ra), 0);
	IR_EncodeAndi(ID_R(rt), ID_R(rt), 0xFF);

	return DRPPC_OKAY;
}

INT D_Lhau(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lhzu(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lwzu(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeAddi(ID_R(ra), ID_R(ra), ea);
	IR_EncodeLoad32(ID_R(rt), ID_R(ra), 0);

	return DRPPC_OKAY;
}

INT D_Lbzux(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lhaux(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lhzux(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lwzux(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lbzx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
		IR_EncodeMove(ID_TEMP(0), ID_R(rb));
	else
		IR_EncodeAdd(ID_TEMP(0), ID_R(rb), ID_R(ra));

	IR_EncodeLoad8(ID_R(rt), ID_TEMP(0), 0);

	return DRPPC_OKAY;
}

INT D_Lhax(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lhzx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
		IR_EncodeMove(ID_TEMP(0), ID_R(rb));
	else
		IR_EncodeAdd(ID_TEMP(0), ID_R(rb), ID_R(ra));

	IR_EncodeLoad16(ID_R(rt), ID_TEMP(0), 0);

	return DRPPC_OKAY;
}

INT D_Lwzx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
		IR_EncodeMove(ID_TEMP(0), ID_R(rb));
	else
		IR_EncodeAdd(ID_TEMP(0), ID_R(rb), ID_R(ra));

	IR_EncodeLoad32(ID_R(rt), ID_TEMP(0), 0);

	return DRPPC_OKAY;
}

INT D_Lhbrx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lswi(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lswx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lwarx(UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Lwbrx(UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoad32(ID_TEMP(1), ID_R(rb), 0);
		IR_EncodeBrev(ID_R(rt), ID_TEMP(1), IR_REV_8_IN_32);
	}
	else
	{
		IR_EncodeMove(ID_TEMP(0), ID_R(rb));
		IR_EncodeAdd(ID_TEMP(0), ID_TEMP(0), ID_R(ra));
		IR_EncodeLoad32(ID_TEMP(1), ID_TEMP(0), 0);
		IR_EncodeBrev(ID_R(rt), ID_TEMP(1), IR_REV_8_IN_32);
	}

	return DRPPC_OKAY;
}

INT D_Stb(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeStore8(ID_R(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeStore8(ID_R(rt), ID_R(ra), ea);

	return DRPPC_OKAY;
}

INT D_Sth(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeStore16(ID_R(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeStore16(ID_R(rt), ID_R(ra), ea);

	return DRPPC_OKAY;
}

INT D_Stw(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT32	ra = RA;
	UINT32	rt = RT;

	if (ra == 0)
	{
		IR_EncodeLoadi(ID_TEMP(0), ea);
		IR_EncodeStore32(ID_R(rt), ID_TEMP(0), 0);
	}
	else
		IR_EncodeStore32(ID_R(rt), ID_R(ra), ea);

	return DRPPC_OKAY;
}

INT D_Stbu(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	// ea += r(a)
	// [ea].8 <- r(t)
	// r(a) = ea

	IR_EncodeStore32(ID_R(rt), ID_R(ra), ea);
	IR_EncodeAddi(ID_R(ra), ID_R(ra), ea);

	return DRPPC_OKAY;
}

INT D_Stbux(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stbx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Sthu(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Sthux(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Sthx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stswi(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Stswx(UINT32 op)
{
	ASSERT(0); return DRPPC_ERROR;
}

INT D_Sthbrx (UINT32 op)
{
	UINT	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeBrev(ID_TEMP(0), ID_R(rt), IR_REV_8_IN_16);

	if (ra != 0)
	{
		IR_EncodeAdd(ID_TEMP(1), ID_R(ra), ID_R(rb));
		IR_EncodeStore16(ID_TEMP(0), ID_TEMP(1), 0);
	}
	else
		IR_EncodeStore16(ID_TEMP(0), ID_R(rb), 0);

	return DRPPC_OKAY;
}

INT D_Stwbrx (UINT32 op)
{
	UINT32	rb = RB;
	UINT	ra = RA;
	UINT	rt = RT;

	IR_EncodeBrev(ID_TEMP(0), ID_R(rt), IR_REV_8_IN_32);

	if (ra != 0)
	{
		IR_EncodeAdd(ID_TEMP(1), ID_R(ra), ID_R(rb));
		IR_EncodeStore32(ID_TEMP(0), ID_TEMP(1), 0);
	}
	else
		IR_EncodeStore32(ID_TEMP(0), ID_R(rb), 0);

	return DRPPC_OKAY;
}

INT D_Stwcx_ (UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Stwu (UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;

	if (rt != ra)
	{
		IR_EncodeAddi(ID_R(ra), ID_R(ra), ea);
		IR_EncodeStore32(ID_R(rt), ID_R(ra), 0);
	}
	else
	{
		IR_EncodeStore32(ID_R(rt), ID_R(ra), ea);
		IR_EncodeAddi(ID_R(ra), ID_R(ra), ea);
	}

	return DRPPC_OKAY;
}

INT D_Stwux (UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

INT D_Stwx (UINT32 op)
{
	ASSERT(0);
	return DRPPC_ERROR;
}

/*******************************************************************************
 Load/Store Multiple
*******************************************************************************/

INT D_Lmw (UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;
	UINT	i;

	ASSERT(ra < rt);
	ASSERT(ra != 31);

	if (ra == 0)
		IR_EncodeLoadi(ID_TEMP(0), ea);
	else
		IR_EncodeAddi(ID_TEMP(0), ID_R(ra), ea);

	for (i = rt; i < 32; i++)
	{
		IR_EncodeLoad32(ID_R(i), ID_TEMP(0), 0);
		IR_EncodeAddi(ID_TEMP(0), ID_TEMP(0), 4);
	}

	// AddDecodeCycles(2 + (32 - rt));

	return DRPPC_OKAY;
}

INT D_Stmw (UINT32 op)
{
	UINT32	ea = SIMM;
	UINT	ra = RA;
	UINT	rt = RT;
	UINT	i;

	if (ra == 0)
		IR_EncodeLoadi(ID_TEMP(0), ea);
	else
		IR_EncodeAddi(ID_TEMP(0), ID_R(ra), ea);

	for (i = rt; i < 32; i++)
	{
		IR_EncodeStore32(ID_R(i), ID_TEMP(0), 0);
		IR_EncodeAddi(ID_TEMP(0), ID_TEMP(0), 4);
	}

	// AddDecodeCycles(2 + (32 - rt));

	return DRPPC_OKAY;
}
