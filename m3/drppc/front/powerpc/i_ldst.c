/*
 * i_ldst.c
 *
 * Load/Store instruction handlers.
 */

#include "source.h"
#include "internal.h"

/*******************************************************************************
 Load/Store Byte, HalfWord and Word
*******************************************************************************/

INT I_Lbz(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = (UINT32)(UINT8)Read8(ea);

	return 1;
}

INT I_Lha(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = (INT32)(INT16)Read16(ea);

	return 1;
}

INT I_Lhz(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = (UINT32)(UINT16)Read16(ea);

	return 1;
}

INT I_Lwz(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = (UINT32)Read32(ea);

	return 1;
}

INT I_Lbzu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) += ea;
	R(t) = (UINT32)(UINT8)Read8(R(a));

	return 1;
}

INT I_Lhau(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) += ea;
	R(t) = (INT32)(INT16)Read16(R(a));

	return 1;
}

INT I_Lhzu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) += ea;
	R(t) = (UINT32)(UINT16)Read16(R(a));

	return 1;
}

INT I_Lwzu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) += ea;
	R(t) = (UINT32)Read32(R(a));

	return 1;
}

INT I_Lbzx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = (UINT32)(UINT8)Read8(ea);

	return 1;
}

INT I_Lhax(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = (INT32)(INT16)Read16(ea);

	return 1;
}

INT I_Lhzx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = (UINT32)(UINT16)Read16(ea);

	return 1;
}

INT I_Lwzx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = (UINT32)Read32(ea);

	return 1;
}

INT I_Lbzux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) += ea;
	R(t) = (UINT32)(UINT8)Read8(R(a));

	return 1;
}

INT I_Lhaux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) += ea;
	R(t) = (INT32)(INT16)Read16(R(a));

	return 1;
}

INT I_Lhzux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) += ea;
	R(t) = (UINT32)(UINT16)Read16(R(a));

	return 1;
}

INT I_Lwzux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) += ea;
	R(t) = (UINT32)Read32(R(a));

	return 1;
}

INT I_Stb(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	Write8(ea, R(t));

	return 1;
}

INT I_Sth(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	Write16(ea, R(t));

	return 1;
}

INT I_Stw(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	Write32(ea, R(t));

	return 1;
}

INT I_Stbu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);
	Write8(ea, R(t));
	R(a) = ea;

	return 1;
}

INT I_Sthu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);
	Write16(ea, R(t));
	R(a) = ea;

	return 1;
}

INT I_Stwu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);
	Write32(ea, R(t));
	R(a) = ea;

	return 1;
}

INT I_Stbx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	Write8(ea, R(t));

	return 1;
}

INT I_Sthx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	Write16(ea, R(t));

	return 1;
}

INT I_Stwx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	Write32(ea, R(t));

	return 1;
}

INT I_Stbux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);
	Write8(ea, R(t));
	R(a) = ea;

	return 1;
}

INT I_Sthux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);
	Write16(ea, R(t));
	R(a) = ea;

	return 1;
}

INT I_Stwux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);
	Write32(ea, R(t));
	R(a) = ea;

	return 1;
}

/*******************************************************************************
 Load/Store Reversed
*******************************************************************************/

INT I_Lhbrx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 d = RT;
	UINT32 t;

	if (a) ea += R(a);

	t = Read16(ea);
	R(d) =	((t >> 8) & 0x00FF) |
			((t << 8) & 0xFF00);

	return 1;
}

INT I_Lwbrx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 d = RT;
	UINT32 t;

	if (a) ea += R(a);

	t = Read32(ea);
	R(d) =	((t >> 24) & 0x000000FF) |
			((t >>  8) & 0x0000FF00) |
			((t <<  8) & 0x00FF0000) |
			((t << 24) & 0xFF000000);

	return 1;
}

INT I_Sthbrx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 d = RT;

	if (a) ea += R(a);

	Write16(ea, ((R(d) >> 8) & 0x00FF) |
				 ((R(d) << 8) & 0xFF00));

	return 1;
}

INT I_Stwbrx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 d = RT;

	if (a) ea += R(a);

	Write32(ea, ((R(d) >> 24) & 0x000000FF) |
				 ((R(d) >>  8) & 0x0000FF00) |
				 ((R(d) <<  8) & 0x00FF0000) |
				 ((R(d) << 24) & 0xFF000000));

	return 1;
}

/*******************************************************************************
 Load/Store Multiple
*******************************************************************************/

INT I_Lmw(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

#ifdef DRPPC_DEBUG
	if (a >= t || a == 31)
		Error("%08X: invalid lmw (ra = %i)", PC, a);
#endif

	if (a) ea += R(a);

	while (t < 32)
	{
		R(t) = Read32(ea);
		ea += 4;
		t++;
	}

	return 2 + (32 - RT);
}

INT I_Stmw(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	while (t < 32)
	{
		Write32(ea, R(t));
		ea += 4;
		t++;
	}

	return 1 + (32 - RT);
}

/*******************************************************************************
 Load/Store Strings
*******************************************************************************/

INT I_Lswi(UINT32 op)
{
	Error("%08X: unhandled instruction lswi\n", PC);
	return 1;
}

INT I_Lswx(UINT32 op)
{
	Error("%08X: unhandled instruction lswx\n", PC);
	return 1;
}

INT I_Stswi(UINT32 op)
{
	Error("%08X: unhandled instruction stswi\n", PC);
	return 1;
}

INT I_Stswx(UINT32 op)
{
	Error("%08X: unhandled instruction stswx\n", PC);
	return 1;
}

/*******************************************************************************
 Load/Store Reserve
*******************************************************************************/

/*
 * TODO: lwarx/stwcx. emulation may be seriously broken!
 */

INT I_Lwarx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	R(t) = Read32(ea);
	ppc.reserved_bit = ea;

	return 10;
}

INT I_Stwcx_(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	if (ea != ppc.reserved_bit)
	{
		// Not reserved anymore
		CR(0) = (XER & XER_SO) ? 1 : 0; // SO
	}
	else
	{
		// Reserved
		Write32(ea, R(t));
		CR(0) = 0x2 | (XER & XER_SO) ? 1 : 0; // EQ | SO
	}

	ppc.reserved_bit = 0xFFFFFFFF;

	return 1;
}
