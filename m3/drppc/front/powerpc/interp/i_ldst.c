/*
 * front/powerpc/interp/i_ldst.c
 *
 * Load/Store instruction handlers.
 */

#include "../powerpc.h"
#include "../internal.h"

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

	Write8(ea, (UINT8)R(t));

	return 1;
}

INT I_Sth(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	Write16(ea, (UINT16)R(t));

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
	Write8(ea, (UINT8)R(t));
	R(a) = ea;

	return 1;
}

INT I_Sthu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);
	Write16(ea, (UINT16)R(t));
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

	Write8(ea, (UINT8)R(t));

	return 1;
}

INT I_Sthx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	Write16(ea, (UINT16)R(t));

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
	Write8(ea, (UINT8)R(t));
	R(a) = ea;

	return 1;
}

INT I_Sthux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);
	Write16(ea, (UINT16)R(t));
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
	UINT	ea = R(RB);
	UINT	a = RA;
	UINT	t = RT;

	if (a)
		ea += R(a);

	R(t) = Byteswap16(Read16(ea));

	return 1;
}

INT I_Lwbrx(UINT32 op)
{
	UINT	ea = R(RB);
	UINT	a = RA;
	UINT	t = RT;

	if (a)
		ea += R(a);

	R(t) = Byteswap32(Read32(ea));

	return 1;
}

INT I_Sthbrx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a)
		ea += R(a);

	Write16(ea, Byteswap16((UINT16)R(t)));

	return 1;
}

INT I_Stwbrx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a)
		ea += R(a);

	Write32(ea, Byteswap32(R(t)));

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
		Print("%08X: invalid lmw (ra = %i)", PC, a);
#endif

	if (a) ea += R(a);

	while (t < 32)
	{
		R(t) = Read32(ea);
		ea += 4;
		t++;
	}

	return 1; // 2 + (32 - RT);
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

	return 1; // 1 + (32 - RT);
}

/*******************************************************************************
 Load/Store Strings
*******************************************************************************/

INT I_Lswi(UINT32 op)
{
	Print("%08X: unhandled instruction lswi\n", PC);
	return 1;
}

INT I_Lswx(UINT32 op)
{
	Print("%08X: unhandled instruction lswx\n", PC);
	return 1;
}

INT I_Stswi(UINT32 op)
{
	Print("%08X: unhandled instruction stswi\n", PC);
	return 1;
}

INT I_Stswx(UINT32 op)
{
	Print("%08X: unhandled instruction stswx\n", PC);
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

	return 1; // 10;
}

INT I_Stwcx_(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) ea += R(a);

	if (ea == ppc.reserved_bit)
	{
		// Still reserved
		Write32(ea, R(t));
		SET_CR_EQ(0);
	}

	if (GET_XER_SO())
		SET_CR_SO(0);

	ppc.reserved_bit = 0xFFFFFFFF;

	return 1;
}
