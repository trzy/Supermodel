/*
 * front/powerpc/i_arith.c
 *
 * Interpreter handlers for arithmetic instructions.
 */

#include "source.h"
#include "internal.h"

INT I_Addx(UINT32 op)
{
	UINT32 rb = R(RB);
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = ra + rb;

	SET_ADD_V(R(t), ra, rb);
	SET_ICR0(R(t));

	return 1;
}

INT I_Addcx(UINT32 op)
{
	UINT32 rb = R(RB);
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = ra + rb;

	SET_ADD_C(R(t), ra, rb);
	SET_ADD_V(R(t), ra, rb);
	SET_ICR0(R(t));

	return 1;
}

INT I_Addex(UINT32 op)
{
	UINT32 rb = R(RB);
	UINT32 ra = R(RA);
	UINT32 t = RT;
	UINT32 c = (XER >> 29) & 1;
	UINT32 temp;

	temp = rb + c;
	R(t) = ra + temp;

	if (ADD_C(temp, rb, c) || ADD_C(R(t), ra, temp))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	if (op & _OVE)
	{
        if (ADD_V(R(t), ra, rb))
			XER |= XER_SO | XER_OV;
		else
			XER &= ~XER_OV;
	}

	SET_ICR0(R(t));

	return 1;
}

INT I_Addi(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 a = RA;
	UINT32 d = RT;

	if (a) i += R(a);

	R(d) = i;

	return 1;
}

INT I_Addic(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = ra + i;

	if (R(t) < ra)
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	return 1;
}

INT I_Addic_(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = ra + i;

	if (R(t) < ra)
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	_SET_ICR0(R(t));

	return 1;
}

INT I_Addis(UINT32 op)
{
	UINT32 i = (op << 16);
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) i += R(a);

	R(t) = i;

	return 1;
}

INT I_Addmex(UINT32 op)
{
	UINT32 ra = R(RA);
	UINT32 d = RT;
	UINT32 c = (XER >> 29) & 1;
    UINT32 temp;

    temp = ra + c;
    R(d) = temp + -1;

    if (ADD_C(temp, ra, c) || ADD_C(R(d), temp, -1))
        XER |= XER_CA;
    else
        XER &= ~XER_CA;

	SET_ADD_V(R(d), ra, (c - 1));
	SET_ICR0(R(d));

	return 1;
}

INT I_Addzex(UINT32 op)
{
	UINT32 ra = R(RA);
	UINT32 d = RT;
	UINT32 c = (XER >> 29) & 1;

	R(d) = ra + c;

	SET_ADD_C(R(d), ra, c);
	SET_ADD_V(R(d), ra, c);
	SET_ICR0(R(d));

	return 1;
}

INT I_Subfx(UINT32 op)
{
	UINT32 rb = R(RB);
	UINT32 ra = R(RA);
	UINT32 d = RT;

	R(d) = rb - ra;

	SET_SUB_V(R(d), rb, ra);
	SET_ICR0(R(d));

	return 1;
}

INT I_Subfcx(UINT32 op)
{
	UINT32 rb = R(RB);
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = rb - ra;

	SET_SUB_C(R(t), rb, ra);
	SET_SUB_V(R(t), rb, ra);
	SET_ICR0(R(t));

	return 1;
}

INT I_Subfex(UINT32 op)
{
	UINT32 rb = R(RB);
	UINT32 ra = R(RA);
	UINT32 t = RT;
	UINT32 c = (XER >> 29) & 1;
    UINT32 a;

    a = ~ra + c;
    R(t) = rb + a;

    SET_ADD_C(a, ~ra, c);   // step 1 carry
    if (R(t) < a)           // step 2 carry
        XER |= XER_CA;
    SET_SUB_V(R(t), rb, ra);

	SET_ICR0(R(t));

	return 1;
}

INT I_Subfic(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = i - ra;

	SET_SUB_C(R(t), i, ra);

	return 1;
}

INT I_Subfmex(UINT32 op)
{
	UINT32 a = RA;
	UINT32 t = RT;
	UINT32 c = (XER >> 29) & 1;
    UINT32 a_c;

    a_c = ~R(a) + c;    // step 1
    R(t) = a_c - 1;     // step 2

    SET_ADD_C(a_c, ~R(a), c);   // step 1 carry
    if (R(t) < a_c)             // step 2 carry
        XER |= XER_CA;
    SET_SUB_V(R(t), -1, R(a));

	SET_ICR0(R(t));

	return 1;
}

INT I_Subfzex(UINT32 op)
{
	UINT32 a = RA;
	UINT32 t = RT;
	UINT32 c = (XER >> 29) & 1;

	R(t) = ~R(a) + c;

    SET_ADD_C(R(t), ~R(a), c);
    SET_SUB_V(R(t), 0, R(a));

	return 1;

	SET_ICR0(R(t));
}

INT I_Negx(UINT32 op)
{
	UINT32 a = RA;
	UINT32 t = RT;

	R(t) = -R(a);

	if (op & _OVE)
	{
		if (R(t) == 0x80000000)
			XER |= (XER_SO | XER_OV);
		else
			XER &= ~XER_OV;
	}

	SET_ICR0(R(t));

	return 1;
}

INT I_Cmp(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 d = _CRFD;

	CR(d) = CMPS(R(a), R(b));
	CR(d) |= (XER >> 31);

	return 1;
}

INT I_Cmpl(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 d = _CRFD;

	CR(d) = CMPU(R(a), R(b));
	CR(d) |= (XER >> 31);

	return 1;
}

INT I_Cmpi(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 a = RA;
	UINT32 d = _CRFD;

	CR(d) = CMPS(R(a), i);
	CR(d) |= (XER >> 31);

	return 1;
}

INT I_Cmpli(UINT32 op)
{
	UINT32 i = UIMM;
	UINT32 a = RA;
	UINT32 d = _CRFD;

	CR(d) = CMPU(R(a), i);
	CR(d) |= (XER >> 31);

	return 1;
}

INT I_Mulli(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 a = RA;
	UINT32 d = RT;

	R(d) = ((INT32)R(a) * (INT32)i);

	return 2; // 2-3
}

INT I_Mulhwx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 d = RT;

	R(d) = ((INT64)(INT32)R(a) * (INT64)(INT32)R(b)) >> 32;

	SET_ICR0(R(d));

	return 3; // 2-5
}

INT I_Mulhwux(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 d = RT;

	R(d) = ((UINT64)(UINT32)R(a) * (UINT64)(UINT32)R(b)) >> 32;

	SET_ICR0(R(d));

	return 4; // 2-6
}

INT I_Mullwx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 d = RT;
	INT64 r;

	r = (INT64)(INT32)R(a) * (INT64)(INT32)R(b);
	R(d) = (UINT32)r;

	if (op & _OVE)
	{
		XER &= ~XER_OV;
		if (r != (INT64)(INT32)(r & 0xFFFFFFFF))
			XER |= (XER_OV | XER_SO);
	}

	SET_ICR0(R(d));

	return 3; // 2-5
}

INT I_Divwx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 d = RT;

	if (R(b) == 0 && (R(a) < 0x80000000))
	{
		R(d) = 0;

		if (op & _OVE) XER |= (XER_SO | XER_OV);
		SET_ICR0(R(d));
	}
	else if (R(b) == 0 || (R(b) == 0xFFFFFFFF && R(a) == 0x80000000))
	{
		R(d) = 0xFFFFFFFF;

		if (op & _OVE) XER |= (XER_SO | XER_OV);
		SET_ICR0(R(d));
	}
	else
	{
		R(d) = (INT32)R(a) / (INT32)R(b);

		if (op & _OVE) XER &= ~XER_OV;

		SET_ICR0(R(d));
	}

	return 37;
}

INT I_Divwux(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 d = RT;

	if (R(b) == 0)
	{
		R(d) = 0;

		if (op & _OVE) XER |= (XER_SO | XER_OV);

		SET_ICR0(R(d));
	}
	else
	{
		R(d) = (UINT32)R(a) / (UINT32)R(b);

		if (op & _OVE) XER &= ~XER_OV;

		SET_ICR0(R(d));
	}

	return 37;
}

INT I_Extsbx(UINT32 op)
{
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = ((INT32)R(s) << 24) >> 24;
	SET_ICR0(R(a));

	return 1;
}

INT I_Extshx(UINT32 op)
{
	UINT32 a = RA;
	UINT32 s = RT;

	R(a) = ((INT32)R(s) << 16) >> 16;
	SET_ICR0(R(a));

	return 1;
}

INT I_Cntlzwx(UINT32 op)
{
	UINT32 a = RA;
	UINT32 t = RT;
	UINT32 m = 0x80000000;
	UINT32 n = 0;

	while (n < 32)
	{
		if (R(t) & m)
			break;
		m >>= 1;
		n++;
	}

	R(a) = n;

	SET_ICR0(R(a));

	return 1;
}
