/*
 * front/powerpc/interp/i_arith.c
 *
 * Interpreter handlers for arithmetic instructions.
 */

#include "../powerpc.h"
#include "../internal.h"

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
	UINT32 c = GET_XER_CA();
	UINT32 temp;

	temp = rb + c;
	R(t) = ra + temp;

	if (ADD_C(temp, rb, c) || ADD_C(R(t), ra, temp))
		SET_XER_CA();
	else
		CLEAR_XER_CA();

	if (op & _OVE)
	{
        if (ADD_V(R(t), ra, rb))
		{
			SET_XER_SO();
			SET_XER_OV();
		}
		else
			CLEAR_XER_OV();
	}

	SET_ICR0(R(t));

	return 1;
}

INT I_Addi(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) i += R(a);

	R(t) = i;

	return 1;
}

INT I_Addic(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = ra + i;

	if (R(t) < ra)
		SET_XER_CA();
	else
		CLEAR_XER_CA();

	return 1;
}

INT I_Addic_(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = ra + i;

	if (R(t) < ra)
		SET_XER_CA();
	else
		CLEAR_XER_CA();

	_SET_ICR0(R(t));

	return 1;
}

INT I_Addis(UINT32 op)
{
	UINT32 i = IMMS;
	UINT32 a = RA;
	UINT32 t = RT;

	if (a) i += R(a);

	R(t) = i;

	return 1;
}

INT I_Addmex(UINT32 op)
{
	UINT32 ra = R(RA);
	UINT32 t = RT;
	UINT32 c = GET_XER_CA();
    UINT32 temp;

    temp = ra + c;
    R(t) = temp + -1;

    if (ADD_C(temp, ra, c) || ADD_C(R(t), temp, -1))
        SET_XER_CA();
    else
        CLEAR_XER_CA();

	SET_ADD_V(R(t), ra, (c - 1));
	SET_ICR0(R(t));

	return 1;
}

INT I_Addzex(UINT32 op)
{
	UINT32 ra = R(RA);
	UINT32 t = RT;
	UINT32 c = GET_XER_CA();

	R(t) = ra + c;

	SET_ADD_C(R(t), ra, c);
	SET_ADD_V(R(t), ra, c);
	SET_ICR0(R(t));

	return 1;
}

INT I_Subfx(UINT32 op)
{
	UINT32 rb = R(RB);
	UINT32 ra = R(RA);
	UINT32 t = RT;

	R(t) = rb - ra;

	SET_SUB_V(R(t), rb, ra);
	SET_ICR0(R(t));

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
	UINT32 c = GET_XER_CA();
    UINT32 a;

    a = ~ra + c;
    R(t) = rb + a;

    SET_ADD_C(a, ~ra, c);   // step 1 carry
    if (R(t) < a)           // step 2 carry
        SET_XER_CA();
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
	UINT32 c = GET_XER_CA();
    UINT32 a_c;

    a_c = ~R(a) + c;    // step 1
    R(t) = a_c - 1;     // step 2

    SET_ADD_C(a_c, ~R(a), c);   // step 1 carry
    if (R(t) < a_c)             // step 2 carry
        SET_XER_CA();
    SET_SUB_V(R(t), -1, R(a));

	SET_ICR0(R(t));

	return 1;
}

INT I_Subfzex(UINT32 op)
{
	UINT32 a = RA;
	UINT32 t = RT;
	UINT32 c = GET_XER_CA();

	R(t) = ~R(a) + c;

    SET_ADD_C(R(t), ~R(a), c);
    SET_SUB_V(R(t), 0, R(a));

	SET_ICR0(R(t));

	return 1;
}

INT I_Negx(UINT32 op)
{
	UINT32 a = RA;
	UINT32 t = RT;

	R(t) = -R(a);

	if (op & _OVE)
	{
		if (R(t) == 0x80000000)
		{
			SET_XER_SO();
			SET_XER_OV();
		}
		else
			CLEAR_XER_OV();
	}

	SET_ICR0(R(t));

	return 1;
}

INT I_Cmp(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = CRFT;

	CMPS(t, R(a), R(b));

	return 1;
}

INT I_Cmpl(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = CRFT;

	CMPU(t, R(a), R(b));

	return 1;
}

INT I_Cmpi(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 a = RA;
	UINT32 t = CRFT;

	CMPS(t, R(a), i);

	return 1;
}

INT I_Cmpli(UINT32 op)
{
	UINT32 i = UIMM;
	UINT32 a = RA;
	UINT32 t = CRFT;

	CMPU(t, R(a), i);

	return 1;
}

INT I_Mulli(UINT32 op)
{
	UINT32 i = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	R(t) = ((INT32)R(a) * (INT32)i);

	return 1; // 2; // 2-3
}

INT I_Mulhwx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(t) = (UINT32)(((INT64)(INT32)R(a) * (INT64)(INT32)R(b)) >> 32);

	SET_ICR0(R(t));

	return 1; // 3; // 2-5
}

INT I_Mulhwux(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(t) = (UINT32)(((UINT64)(UINT32)R(a) * (UINT64)(UINT32)R(b)) >> 32);

	SET_ICR0(R(t));

	return 1; // 4; // 2-6
}

INT I_Mullwx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;
	INT64 r;

	r = (INT64)(INT32)R(a) * (INT64)(INT32)R(b);
	R(t) = (UINT32)r;

	if (op & _OVE)
	{
		if (r != (INT64)(INT32)(r & 0xFFFFFFFF))
		{
			SET_XER_SO();
			SET_XER_OV();
		}
		else
			CLEAR_XER_OV();
	}

	SET_ICR0(R(t));

	return 1; // 3; // 2-5
}

INT I_Divwx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	if (R(b) == 0 && (R(a) < 0x80000000))
	{
		R(t) = 0;

		if (op & _OVE)
		{
			SET_XER_SO();
			SET_XER_OV();
		}
		SET_ICR0(R(t));
	}
	else if (R(b) == 0 || (R(b) == 0xFFFFFFFF && R(a) == 0x80000000))
	{
		R(t) = 0xFFFFFFFF;

		if (op & _OVE)
		{
			SET_XER_SO();
			SET_XER_OV();
		}
		SET_ICR0(R(t));
	}
	else
	{
		R(t) = (INT32)R(a) / (INT32)R(b);

		if (op & _OVE)
			CLEAR_XER_OV();

		SET_ICR0(R(t));
	}

	return 1; // 37;
}

INT I_Divwux(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	if (R(b) == 0)
	{
		R(t) = 0;

		if (op & _OVE)
		{
			SET_XER_OV();
			SET_XER_SO();
		}

		SET_ICR0(R(t));
	}
	else
	{
		R(t) = (UINT32)R(a) / (UINT32)R(b);

		if (op & _OVE)
			CLEAR_XER_OV();

		SET_ICR0(R(t));
	}

	return 1; // 37;
}

INT I_Extsbx(UINT32 op)
{
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = ((INT32)R(t) << 24) >> 24;
	SET_ICR0(R(a));

	return 1;
}

INT I_Extshx(UINT32 op)
{
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = ((INT32)R(t) << 16) >> 16;
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

INT I_Andx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) & R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Andcx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) & ~R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Orx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) | R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Orcx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) | ~ R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Xorx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) ^ R(b);
	SET_ICR0(R(a));

	return 1;
}

INT I_Nandx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = ~(R(t) & R(b));
	SET_ICR0(R(a));

	return 1;
}

INT I_Norx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = ~(R(t) | R(b));
	SET_ICR0(R(a));

	return 1;
}

INT I_Eqvx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = ~(R(t) ^ R(b));
	SET_ICR0(R(a));

	return 1;
}

INT I_Andi_(UINT32 op)
{
	UINT32 i = UIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) & i;
	_SET_ICR0(R(a));

	return 1;
}

INT I_Andis_(UINT32 op)
{
	UINT32 i = IMMS;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) & i;
	_SET_ICR0(R(a));

	return 1;
}

INT I_Ori(UINT32 op)
{
	UINT32 i = UIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) | i;

	return 1;
}

INT I_Oris(UINT32 op)
{
	UINT32 i = IMMS;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) | i;

	return 1;
}

INT I_Xori(UINT32 op)
{
	UINT32 i = UIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) ^ i;

	return 1;
}

INT I_Xoris(UINT32 op)
{
	UINT32 i = IMMS;
	UINT32 a = RA;
	UINT32 t = RT;

	R(a) = R(t) ^ i;

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

	if (((INT32)R(t) < 0) && R(t) & BITMASK_0(sa))
		SET_XER_CA();
	else
		CLEAR_XER_CA();

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

	if (((INT32)R(t) < 0) && R(t) & BITMASK_0(sa))
		SET_XER_CA();
	else
		CLEAR_XER_CA();

	R(a) = (INT32)R(t) >> sa;

	SET_ICR0(R(a));

	return 1;
}

INT I_Rlwimix(UINT32 op)
{
	UINT32 mb_me = _MB_ME;
	UINT32 sh = _SH;
	UINT32 a = RA;
	UINT32 t = RT;
	UINT32 m, r;

	r = (R(t) << sh) | (R(t) >> (32 - sh));
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
	UINT32 t = RT;
	UINT32 m, r;

	r = (R(t) << sh) | (R(t) >> (32 - sh));
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
	UINT32 t = RT;
	UINT32 m, r;

	r = (R(t) << sh) | (R(t) >> (32 - sh));
	m = mask_table[mb_me];

	R(a) = (r & m);

	SET_ICR0(R(a));

	return 1;
}
