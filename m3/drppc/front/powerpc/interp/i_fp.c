/*
 * front/powerpc/interp/i_fp.c
 *
 * Floating-Point instruction handlers.
 */

#include "source.h"
#include "internal.h"

/*******************************************************************************
 Load/Store Floating-Point
*******************************************************************************/

INT I_Lfs(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT32	a = RA;
	UINT32	t = RT;
	UINT32	i;

	if(a) ea += R(a);

	i = Read32(ea);

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
		F(t).ps[0] = F(t).ps[1] = *((FLOAT32 *)&i);
	else
#endif	/* PPC_MODEL_GEKKO */
		F(t).fd = (FLOAT64)(*((FLOAT32 *)&i));

	return 1;
}

INT I_Lfsu(UINT32 op)
{
	UINT32	ea = SIMM;
	UINT32	ra = RA;
	UINT32	rt = RT;
	UINT32	i;

	ea += R(ra);

	i = Read32(ea);

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
		F(rt).ps[0] = F(rt).ps[1] = *((FLOAT32 *)&i);
	else
#endif	/* PPC_MODEL_GEKKO */
		F(rt).fd = (FLOAT64)(*((FLOAT32 *)&i));

	R(ra) = ea;

	return 1;
}

INT I_Lfsux(UINT32 op)
{
	UINT32	ea = R(RB);
	UINT32	ra = RA;
	UINT32	rt = RT;
	UINT32	i;

	ea += R(ra);

	i = Read32(ea);

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
		F(rt).ps[0] = F(rt).ps[1] = *((FLOAT32 *)&i);
	else
#endif	/* PPC_MODEL_GEKKO */
		F(rt).fd = (FLOAT64)(*((FLOAT32 *)&i));

	R(ra) = ea;

	return 1;
}

INT I_Lfsx(UINT32 op)
{
	UINT32	ea = R(RB);
	UINT32	ra = RA;
	UINT32	rt = RT;
	UINT32	i;

	if(ra)
		ea += R(ra);

	i = Read32(ea);

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
		F(rt).ps[0] = F(rt).ps[1] = *((FLOAT32 *)&i);
	else
#endif	/* PPC_MODEL_GEKKO */
		F(rt).fd = (FLOAT64)(*((FLOAT32 *)&i));

	return 1;
}

INT I_Stfs(UINT32 op)
{
	FLOAT32 ftemp;

	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if(a)
		ea += R(a);

	ftemp = (FLOAT32)((FLOAT64)F(t).fd);

	Write32(ea, *(UINT32 *)&ftemp);

	return 1;
}

INT I_Stfsu(UINT32 op)
{
	FLOAT32 ftemp;

	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);

	ftemp = (FLOAT32)((FLOAT64)F(t).fd);

	Write32(ea, *(UINT32 *)&ftemp);

	R(a) = ea;

	return 1;
}

INT I_Stfsux(UINT32 op)
{
	FLOAT32 ftemp;

	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);

	ftemp = (FLOAT32)((FLOAT64)F(t).fd);

	Write32(ea, *(UINT32 *)&ftemp);

	R(a) = ea;

	return 1;
}

INT I_Stfsx(UINT32 op)
{
	FLOAT32 ftemp;

	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if(a)
		ea += R(a);

	ftemp = (FLOAT32)((FLOAT64)F(t).fd);

	Write32(ea, *(UINT32 *)&ftemp);

	return 1;
}

INT I_Lfd(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if(a) ea += R(a);

	F(t).id = Read64(ea);

	return 1;
}

INT I_Lfdu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);

	F(t).id = Read64(ea);

	R(a) = ea;

	return 1;
}

INT I_Lfdux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);

	F(t).id = Read64(ea);

	R(a) = ea;

	return 1;
}

INT I_Lfdx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if(a) ea += R(a);

	F(t).id = Read64(ea);

	return 1;
}

INT I_Stfd(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	if(a) ea += R(a);

	Write64(ea, F(t).id);

	return 1;
}

INT I_Stfdu(UINT32 op)
{
	UINT32 ea = SIMM;
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);

	Write64(ea, F(t).id);

	R(a) = ea;

	return 1;
}

INT I_Stfdux(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	ea += R(a);

	Write64(ea, F(t).id);

	R(a) = ea;

	return 1;
}

INT I_Stfdx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if(a) ea += R(a);

	Write64(ea, F(t).id);

	return 1;
}

INT I_Stfiwx(UINT32 op)
{
	UINT32 ea = R(RB);
	UINT32 a = RA;
	UINT32 t = RT;

	if(a) ea += R(a);

	Write32(ea, F(t).iw);

	return 1;
}

/*******************************************************************************
 Floating-Point Arithmetic
*******************************************************************************/

INT I_Fabsx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

	F(t).id = F(b).id & ~DOUBLE_SIGN;

	SET_FCR1();

	return 1;
}

INT I_Faddx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(b).fd);

	F(t).fd = F(a).fd + F(b).fd;

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Faddsx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = F(a).ps[0] + F(b).ps[0];
	}
	else
#endif	/* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN(F(a).fd, F(b).fd);

		F(t).fd = (FLOAT32)(F(a).fd + F(b).fd);

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Fcmpo(UINT32 op)
{
	UINT32	b = RB;
	UINT32	a = RA;
	UINT32	t = (RT >> 2);
	UINT	c;

	CHECK_FPU_AVAILABLE();

	if (is_nan_f64(F(a).fd) || is_nan_f64(F(b).fd))
		c = 1;	// OX
	else if (F(a).fd < F(b).fd)
		c = 8;	// FX
	else if (F(a).fd > F(b).fd)
		c = 4;	// FEX
	else
		c = 2;	// VX

	CR(t) = c;

	FPSCR &= ~0x0001F000;
	FPSCR |= (c << 12);

	if (is_snan_f64(F(a).fd) || is_snan_f64(F(b).fd))
	{
		FPSCR |= 0x80000000/* | FPSCR_VXSNAN*/;
		if((FPSCR & FPSCR_VE) == 0)
			FPSCR |= FPSCR_VXVC;
	}
	else if (is_qnan_f64(F(a).fd) || is_qnan_f64(F(b).fd))
	{
		FPSCR |= FPSCR_VXVC;
	}

	return 1;
}

INT I_Fcmpu(UINT32 op)
{
	UINT32	b = RB;
	UINT32	a = RA;
	UINT32	t = (RT >> 2);
	UINT	c;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(b).fd);

    if (is_nan_f64(F(a).fd) || is_nan_f64(F(b).fd))
	{
		c = 1;	// OX

		if (is_snan_f64(F(a).fd) || is_snan_f64(F(b).fd))
			FPSCR |= 0x01000000;	// VXSNAN
	}
	else if (F(a).fd < F(b).fd)
		c = 8;	// FX
	else if (F(a).fd > F(b).fd)
		c = 4;	// FEX
	else
		c = 2;	// VX

	CR(t) = c;

	FPSCR &= ~0x0001F000;
	FPSCR |= (c << 12);

	return 1;
}

INT I_Fctiwx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(F(b).fd);

    if (F(b).fd > (FLOAT64)((INT32)0x7FFFFFFF))
	{
		F(t).iw = 0x7FFFFFFF;
		// FPSCR[FR] = 0
		// FPSCR[FI] = 1
		// FPSCR[XX] = 1
	}
	else if (F(b).fd < (FLOAT64)((INT32)0x80000000))
	{
		F(t).iw = 0x80000000;
		// FPSCR[FR] = 1
		// FPSCR[FI] = 1
		// FPSCR[XX] = 1
	}
	else
	{
		switch (FPSCR & 3)
		{
		case 0:  F(t).id = (INT64)(INT32)round_f64_to_s32(F(b).fd); break;
		case 1:  F(t).id = (INT64)(INT32)trunc_f64_to_s32(F(b).fd); break;
		case 2:  F(t).id = (INT64)(INT32)ceil_f64_to_s32(F(b).fd); break;
		default: F(t).id = (INT64)(INT32)floor_f64_to_s32(F(b).fd); break;
		}

		// FPSCR[FR] = t.iw > t.fd
		// FPSCR[FI] = t.iw == t.fd
		// FPSCR[XX] = ?
	}

	SET_FCR1();

	return 1;
}

INT I_Fctiwzx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(F(b).fd);

    if(F(b).fd > (FLOAT64)((INT32)0x7FFFFFFF))
	{
		F(t).iw = 0x7FFFFFFF;
		// FPSCR[FR] = 0
		// FPSCR[FI] = 1
		// FPSCR[XX] = 1
	}
	else if(F(b).fd < (FLOAT64)((INT32)0x80000000))
	{
		F(t).iw = 0x80000000;
		// FPSCR[FR] = 1
		// FPSCR[FI] = 1
		// FPSCR[XX] = 1
	}
	else
	{
		F(t).iw = trunc_f64_to_s32(F(b).fd);
		// FPSCR[FR] = t.iw > t.fd
		// FPSCR[FI] = t.iw == t.fd
		// FPSCR[XX] = ?
	}

	SET_FCR1();

	return 1;
}

INT I_Fdivx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(b).fd);

    F(t).fd = F(a).fd / F(b).fd;

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fdivsx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = F(a).ps[0] / F(b).ps[0];
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
	    SET_VXSNAN(F(a).fd, F(b).fd);

	    F(t).fd = (FLOAT32)(F(a).fd / F(b).fd);

	    set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Fmaddx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(b).fd);
    SET_VXSNAN_1(F(c).fd);

	F(t).fd = (F(a).fd * F(c).fd) + F(b).fd;

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fmaddsx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = F(a).ps[0] * F(c).ps[0] + F(b).ps[0];
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN(F(a).fd, F(b).fd);
		SET_VXSNAN_1(F(c).fd);

		F(t).fd = (FLOAT32)((F(a).fd * F(c).fd) + F(b).fd);

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Fmrx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		ppc_error("%08X: fmr* with PSE=1\n", PC);
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		F(t).fd = F(b).fd;

		SET_FCR1();
	}

	return 1;
}

INT I_Fmsubx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = F(a).ps[0] * F(c).ps[0] - F(b).ps[0];
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN(F(a).fd, F(b).fd);
		SET_VXSNAN_1(F(c).fd);

		F(t).fd = (F(a).fd * F(c).fd) - F(b).fd;

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Fmsubsx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(b).fd);
    SET_VXSNAN_1(F(c).fd);

    F(t).fd = (FLOAT32)((F(a).fd * F(c).fd) - F(b).fd);

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fnmaddx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(b).fd);
    SET_VXSNAN_1(F(c).fd);

    F(t).fd = -((F(a).fd * F(c).fd) + F(b).fd);

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fnmaddsx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = -((F(a).ps[0] * F(c).ps[0]) + F(b).ps[0]);
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN(F(a).fd, F(b).fd);
		SET_VXSNAN_1(F(c).fd);

		F(t).fd = (FLOAT32)(-((F(a).fd * F(c).fd) + F(b).fd));

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Fnmsubx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(b).fd);
    SET_VXSNAN_1(F(c).fd);

    F(t).fd = -((F(a).fd * F(c).fd) - F(b).fd);

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fnmsubsx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = -((F(a).ps[0] * F(c).ps[0]) - F(b).ps[0]);
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN(F(a).fd, F(b).fd);
		SET_VXSNAN_1(F(c).fd);

		F(t).fd = (FLOAT32)(-((F(a).fd * F(c).fd) - F(b).fd));

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Fmulx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(c).fd);

    F(t).fd = F(a).fd * F(c).fd;

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fmulsx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = F(a).ps[0] * F(c).ps[0];
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN(F(a).fd, F(c).fd);

		F(t).fd = (FLOAT32)(F(a).fd * F(c).fd);

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Fnabsx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

	F(t).id = F(b).id | DOUBLE_SIGN;

	SET_FCR1();

	return 1;
}

INT I_Fnegx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

	F(t).id = F(b).id ^ DOUBLE_SIGN;

	SET_FCR1();

	return 1;
}

INT I_Fresx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = 1.0f / F(b).ps[0];
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN_1(F(b).fd);

		F(t).fd = 1.0 / F(b).fd;

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Frspx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		printf("%08X: frsp* with PSE=1\n", PC);
		exit(1);
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN_1(F(b).fd);

		F(t).fd = (FLOAT32)F(b).fd;

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Frsqrtex(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

	SET_VXSNAN_1(F(b).fd);

	F(t).fd = 1.0 / ((FLOAT64)sqrt(F(b).fd));

	set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fselx(UINT32 op)
{
	UINT32 c = RC;
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		printf("%08X: fsel* with PSE=1\n", PC);
		exit(1);
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		F(t).fd = (F(a).fd >= 0.0) ? F(c).fd : F(b).fd;

		SET_FCR1();
	}

	return 1;
}

INT I_Fsqrtx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(F(b).fd);

	F(t).fd = (FLOAT64)(sqrt(F(b).fd));

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fsqrtsx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = sqrt(F(b).ps[0]);
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN_1(F(b).fd);

		F(t).fd = (FLOAT32)(sqrt(F(b).fd));

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}

INT I_Fsubx(UINT32 op){

	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(F(a).fd, F(b).fd);

	F(t).fd = F(a).fd - F(b).fd;

    set_fprf(F(t).fd);
	SET_FCR1();

	return 1;
}

INT I_Fsubsx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 a = RA;
	UINT32 t = RT;

	CHECK_FPU_AVAILABLE();

#if (PPC_MODEL == PPC_MODEL_GEKKO)
	if (IS_PS_ENABLED)
	{
		F(t).ps[1] = F(t).ps[0] = F(a).ps[0] - F(b).ps[0];
	}
	else
#endif /* PPC_MODEL_GEKKO */
	{
		SET_VXSNAN(F(a).fd, F(b).fd);

		F(t).fd = (FLOAT32)(F(a).fd - F(b).fd);

		set_fprf(F(t).fd);
		SET_FCR1();
	}

	return 1;
}
