/*
 * front/powerpc/i_logic.c
 *
 * ALU logic instruction handlers.
 */

#include "source.h"
#include "internal.h"

INT I_Andx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) & R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Andcx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) & ~R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Orx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) | R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Orcx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) | ~ R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Xorx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) ^ R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Nandx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = ~(R(s) & R(b));
	SET_ICR0(R(a));

	return 1;
}

INT I_Norx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = ~(R(s) | R(b));
	SET_ICR0(R(a));

	return 1;
}

INT I_Eqvx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = ~(R(s) ^ R(b));
	SET_ICR0(R(a));

	return 1;
}

INT I_Andi_(UINT32 op)
{
	UINT32 i = UIMM;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) & i;
	_SET_ICR0(R(a));

	return 1;
}

INT I_Andis_(UINT32 op)
{
	UINT32 i = IMMS;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) & i;
	_SET_ICR0(R(a));

	return 1;
}

INT I_Ori(UINT32 op)
{
	UINT32 i = UIMM;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) | i;

	return 1;
}

INT I_Oris(UINT32 op)
{
	UINT32 i = IMMS;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) | i;

	return 1;
}

INT I_Xori(UINT32 op)
{
	UINT32 i = UIMM;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) ^ i;

	return 1;
}

INT I_Xoris(UINT32 op)
{
	UINT32 i = IMMS;
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = R(s) ^ i;

	return 1;
}

INT I_Slwx(UINT32 op)
{
	UINT32 sa = R(RB) & 0x3F;
	UINT32 a = RA;
	UINT32 t = RT;

	if ((sa & 0x20) == 0)
		R(a) = R(t) << sa;
	else
		R(a) = 0;
		
	SET_ICR0(R(a));

	return 1;
}

INT I_Srwx(UINT32 op)
{
	UINT32 sa = R(RB) & 0x3F;
	UINT32 a = RA;
	UINT32 t = RT;

	if ((sa & 0x20) == 0)
		R(a) = R(t) >> sa;
	else
		R(a) = 0;

	SET_ICR0(R(a));

	return 1;
}

INT I_Srawx(UINT32 op)
{
	UINT32 sa = R(RB) & 0x3F;
	UINT32 a = RA;
	UINT32 t = RT;

	XER &= ~XER_CA;
	if (((INT32)R(t) < 0) && R(t) & BITMASK_0(sa))
		XER |= XER_CA;

	if ((sa & 0x20) == 0)
		R(a) = (INT32)R(t) >> sa;
	else
		R(a) = (INT32)R(t) >> 31;

	SET_ICR0(R(a));

	return 1;
}

INT I_Srawix(UINT32 op)
{
	UINT32 sa = _SH;
	UINT32 a = RA;
	UINT32 t = RT;

	XER &= ~XER_CA;
	if (((INT32)R(t) < 0) && R(t) & BITMASK_0(sa))
		XER |= XER_CA;

	R(a) = (INT32)R(t) >> sa;

	SET_ICR0(R(a));

	return 1;
}

INT I_Rlwimix(UINT32 op)
{
	UINT32 mb_me = _MB_ME;
	UINT32 sh = _SH;
	UINT32 a = RA;
	UINT32 s = RT;
	UINT32 m, r;

	r = (R(s) << sh) | (R(s) >> (32 - sh));
	m = mask_table[mb_me];

	R(a) &= ~m;
	R(a) |= (r & m);

	SET_ICR0(R(a));

	return 1;
}

INT I_Rlwinmx(UINT32 op)
{
	UINT32 mb_me = _MB_ME;
	UINT32 sh = _SH;
	UINT32 a = RA;
	UINT32 s = RT;
	UINT32 m, r;

	r = (R(s) << sh) | (R(s) >> (32 - sh));
	m = mask_table[mb_me];

	R(a) = r & m;

	SET_ICR0(R(a));

	return 1;
}

INT I_Rlwnmx(UINT32 op)
{
	UINT32 mb_me = _MB_ME;
	UINT32 sh = (R(RB) & 0x1F);
	UINT32 a = RA;
	UINT32 s = RT;
	UINT32 m, r;

	r = (R(s) << sh) | (R(s) >> (32 - sh));
	m = mask_table[mb_me];

	R(a) = (r & m);

	SET_ICR0(R(a));

	return 1;
}
