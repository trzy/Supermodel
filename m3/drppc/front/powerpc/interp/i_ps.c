/*
 * front/powerpc/interp/i_ps.c
 *
 * *Very* primitive Paired Single emulation.
 */

#include "source.h"
#include "internal.h"

#if PPC_MODEL == PPC_MODEL_GEKKO

/*******************************************************************************
 Helper Functions
*******************************************************************************/

INLINE FLOAT32 ppc_load_dequantize(UINT lt, INT ls, UINT32 ea)
{
	if (lt == GQR_TYPE_F32)
	{
		UINT32 i = ppc_read_32(ea);
		return(*((FLOAT32 *)&i));
	}
	else
	{
		FLOAT32	scale;

		ls = ((INT32)(ls << 26)) >> 26;
		scale = pow(2.0f, (FLOAT32)-ls);

		switch (lt)
		{
		case GQR_TYPE_U8:
		case GQR_TYPE_U16:
		case GQR_TYPE_S8:
		case GQR_TYPE_S16:
		default:
			ppc_error("dequantize, invalid type (%u)\n", lt);
		}
	}

	return 0.0f;
}

INLINE INT I_store_quantize(FLOAT32 ps, UINT stt, INT sts, UINT32 ea)
{
	if (stt == GQR_TYPE_F32)
		ppc_write_32(ea, *((UINT32 *)&ps));
	else
		ppc_error("quantize, invalid type (%u)\n", stt);
}

/*******************************************************************************
 Load/Store Quantized
*******************************************************************************/

INT I_Psq_l(UINT32 op)
{
	UINT32	ea = (INT32)((op & 0xFFF) << 20) >> 20;
	UINT	i = (op >> 12) & 7;
	UINT	w = (op >> 15) & 1;
	UINT	ra = RA;
	UINT	rt = RT;
	UINT	lt = LD_TYPE(i);
	UINT	ls = LD_SCALE(i);
	UINT	c = 4;

	CHECK_PS_ENABLED();

	if (ra != 0)
		ea += R(ra);

	if (lt == 4 || lt == 6)
		c = 10;
	if (lt == 5 || lt == 7)
		c = 20;

	F(rt).ps[0] = ppc_load_dequantize(lt, ls, ea);
	F(rt).ps[1] = (w == 0) ? ppc_load_dequantize(lt, ls, ea+c) : 1.0f;

	return 1;
}

INT I_Psq_lx(UINT32 op)
{
	Error("%08X: psq_lx\n", PC);
	return 1;
}

INT I_Psq_lux(UINT32 op)
{
	Error("%08X: psq_lux\n", PC);
	return 1;
}

INT I_Psq_lu(UINT32 op)
{
	Error("%08X: psq_lu\n", PC);
	return 1;
}

INT I_Psq_st(UINT32 op)
{
	UINT32	ea = (INT32)((op & 0xFFF) << 20) >> 20;
	UINT	i = (op >> 12) & 7;
	UINT	w = (op >> 15) & 1;
	UINT	ra = RA;
	UINT	rt = RT;
	UINT	stt = ST_TYPE(i);
	UINT	sts = ST_SCALE(i);
	UINT	c = 4;

	CHECK_PS_ENABLED();

	if(ra != 0)
		ea += R(ra);

	if(stt == 4 || stt == 6)
		c = 10;
	if(stt == 5 || stt == 7)
		c = 20;

	ppc_store_quantize(F(rt).ps[0], stt, sts, ea);
	if(w == 0)
		ppc_store_quantize(F(rt).ps[1], stt, sts, ea);

	return 1;
}

INT I_Psq_stx(UINT32 op)
{
	Error("%08X: psq_stx\n", PC);
	return 1;
}

INT I_Psq_stux(UINT32 op)
{
	Error("%08X: psq_stux\n", PC);
	return 1;
}

INT I_Psq_stu(UINT32 op)
{
	Error("%08X: psq_stu\n", PC);
	return 1;
}

/*******************************************************************************
 Arithmetical
*******************************************************************************/

INT I_Ps_addx(UINT32 op)
{
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	CHECK_PS_ENABLED();

	F(t).ps[0] = F(a).ps[0] + F(b).ps[0];
	F(t).ps[1] = F(a).ps[1] + F(b).ps[1];

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_absx(UINT32 op)
{
	Error("%08X: ps_abs*\n", PC);
	return 1;
}

INT I_Ps_cmpo0(UINT32 op)
{
	UINT b = RB;
	UINT a = RA;
	UINT t = (RT >> 2);
	UINT c;

	CHECK_PS_ENABLED();

	if (is_nan_f32(F(a).ps[0]) || is_nan_f32(F(b).ps[0]))
		c = 1;
	else if (F(a).ps[0] < F(b).ps[0])
		c = 8;
	else if (F(a).ps[0] > F(b).ps[0])
		c = 4;
	else
		c = 2;

	CR(t) = c;

	FPSCR &= ~0x0001F000;
	FPSCR |= (c << 12);

	if (is_snan_f32(F(a).ps[0]) || is_snan_f32(F(b).ps[0]))
	{
		FPSCR |= FPSCR_FX | FPSCR_VXSNAN;
		if ((FPSCR & FPSCR_VE) == 0)
			FPSCR |= FPSCR_VXVC;
	}
	else if (is_qnan_f32(F(a).ps[0]) || is_qnan_f32(F(b).ps[0]))
		FPSCR |= FPSCR_VXVC;

	return 1;
}

INT I_Ps_cmpo1(UINT32 op)
{
	Error("%08X: ps_cmpo1*\n", PC);
	return 1;
}

INT I_Ps_cmpu0(UINT32 op)
{
	Error("%08X: ps_cmpu0*\n", PC);
	return 1;
}

INT I_Ps_cmpu1(UINT32 op)
{
	Error("%08X: ps_cmpu1*\n", PC);
	return 1;
}

INT I_Ps_divx(UINT32 op)
{
	Error("%08X: ps_div*\n", PC);
	return 1;
}

INT I_Ps_maddx(UINT32 op)
{
	UINT c = RC;
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = (F(a).ps[0] * F(c).ps[0]) + F(b).ps[0];
	FLOAT32 ps1 = (F(a).ps[1] * F(c).ps[1]) + F(b).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_madds0x(UINT32 op)
{
	UINT c = RC;
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = (F(a).ps[0] * F(c).ps[0]) + F(b).ps[0];
	FLOAT32 ps1 = (F(a).ps[1] * F(c).ps[0]) + F(b).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_madds1x(UINT32 op)
{
	UINT c = RC;
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = (F(a).ps[0] * F(c).ps[1]) + F(b).ps[0];
	FLOAT32 ps1 = (F(a).ps[1] * F(c).ps[1]) + F(b).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_merge00x(UINT32 op)
{
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = F(a).ps[0];
	FLOAT32 ps1 = F(b).ps[0];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	return 1;
}

INT I_Ps_merge01x(UINT32 op)
{
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = F(a).ps[0];
	FLOAT32 ps1 = F(b).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	return 1;
}

INT I_Ps_merge10x(UINT32 op)
{
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = F(a).ps[1];
	FLOAT32 ps1 = F(b).ps[0];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	SET_FCR1();

	return 1;
}

INT I_Ps_merge11x(UINT32 op)
{
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	CHECK_PS_ENABLED();

	FLOAT32 ps0 = F(a).ps[1];
	FLOAT32 ps1 = F(b).ps[1];

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	return 1;
}

INT I_Ps_mrx(UINT32 op)
{
	Error("%08X: ps_mr*\n", PC);
	return 1;
}

INT I_Ps_msubx(UINT32 op)
{
	UINT c = RC;
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = (F(a).ps[0] * F(c).ps[0]) - F(b).ps[0];
	FLOAT32 ps1 = (F(a).ps[1] * F(c).ps[1]) - F(b).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_mulx(UINT32 op)
{
	UINT c = RC;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = F(a).ps[0] * F(c).ps[0];
	FLOAT32 ps1 = F(a).ps[1] * F(c).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_muls0x(UINT32 op)
{
	UINT c = RC;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = F(a).ps[0] * F(c).ps[0];
	FLOAT32 ps1 = F(a).ps[1] * F(c).ps[0];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_muls1x(UINT32 op)
{
	UINT c = RC;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = F(a).ps[0] * F(c).ps[1];
	FLOAT32 ps1 = F(a).ps[1] * F(c).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_nabsx(UINT32 op)
{
	Error("%08X: ps_nabs*\n", PC);
	return 1;
}

INT I_Ps_negx(UINT32 op)
{
	UINT b = RB;
	UINT t = RT;

	CHECK_PS_ENABLED();

	F(t).ps[0] = -F(b).ps[0];
	F(t).ps[1] = -F(b).ps[1];

	return 1;
}

INT I_Ps_nmaddx(UINT32 op)
{
	UINT	c = RC;
	UINT	b = RB;
	UINT	a = RA;
	UINT	t = RT;

	FLOAT32 ps0 = -((F(a).ps[0] * F(c).ps[0]) + F(b).ps[0]);
	FLOAT32 ps1 = -((F(a).ps[1] * F(c).ps[1]) + F(b).ps[1]);

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	return 1;
}

INT I_Ps_nmsubx(UINT32 op)
{
	UINT	c = RC;
	UINT	b = RB;
	UINT	a = RA;
	UINT	t = RT;

	FLOAT32 ps0 = -((F(a).ps[0] * F(c).ps[0]) - F(b).ps[0]);
	FLOAT32 ps1 = -((F(a).ps[1] * F(c).ps[1]) - F(b).ps[1]);

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	return 1;
}

INT I_Ps_resx(UINT32 op)
{
	Error("%08X: ps_res*\n", PC);

	return 1;
}

INT I_Ps_rsqrtex(UINT32 op)
{
	Error("%08X: ps_rsqrte*\n", PC);

	return 1;
}

INT I_Ps_selx(UINT32 op)
{
	Error("%08X: ps_sel*\n", PC);

	return 1;
}

INT I_Ps_sum0x(UINT32 op)
{
	UINT c = RC;
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = F(a).ps[0] + F(b).ps[1];
	FLOAT32 ps1 = F(c).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_sum1x(UINT32 op)
{
	UINT c = RC;
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	FLOAT32 ps0 = F(c).ps[0];
	FLOAT32 ps1 = F(a).ps[0] + F(b).ps[1];

	CHECK_PS_ENABLED();

	F(t).ps[0] = ps0;
	F(t).ps[1] = ps1;

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

INT I_Ps_subx(UINT32 op)
{
	UINT b = RB;
	UINT a = RA;
	UINT t = RT;

	F(t).ps[0] = F(a).ps[0] - F(b).ps[0];
	F(t).ps[1] = F(a).ps[1] - F(b).ps[1];

	CHECK_PS_ENABLED();

	set_fprf(F(t).ps[0]);
	SET_FCR1();

	return 1;
}

#endif // PPC_MODEL_GEKKO
