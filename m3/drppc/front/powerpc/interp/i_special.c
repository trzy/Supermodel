/*
 * front/powerpc/interp/i_special.c
 *
 * Special instruction handlers
 */

#include "../powerpc.h"
#include "../internal.h"

/*******************************************************************************
 Helper Macros
*******************************************************************************/

#define CROPD(op)										\
		{												\
		UINT32 bm = _BBM, bn = _BBN;					\
		UINT32 am = _BAM, an = _BAN;					\
		UINT32 dm = _BDM, dn = _BDN;					\
		UINT32 i, j, k;									\
		i = (GET_CR(an) >> (3 - am)) & 1;				\
		j = (GET_CR(bn) >> (3 - bm)) & 1;				\
		k = GET_CR(dn) & ~(8 >> dm);					\
		k |= (i op j) << (3 - dm);						\
		SET_CR(dn, k);									\
		}

#define CROPN(op)										\
		{												\
		UINT32 bm = _BBM, bn = _BBN;					\
		UINT32 am = _BAM, an = _BAN;					\
		UINT32 dm = _BDM, dn = _BDN;					\
		UINT32 i, j, k;									\
		i = (GET_CR(an) >> (3 - am)) & 1;				\
		j = (GET_CR(bn) >> (3 - bm)) & 1;				\
		k = GET_CR(dn) & ~(8 >> dm);					\
		k |= (~(i op j) << (3 - dm)) & (8 >> dm);		\
		SET_CR(dn, k);									\
		}

/*******************************************************************************
 Instruction Handlers
*******************************************************************************/

INT I_Crand(UINT32 op)
{
	CROPD(&);
	return 1;
}

INT I_Crandc(UINT32 op)
{
	CROPD(&~);
	return 1;
}

INT I_Cror(UINT32 op)
{
	CROPD(|);
	return 1;
}

INT I_Crorc(UINT32 op)
{
	CROPD(|~);
	return 1;
}

INT I_Crxor(UINT32 op)
{
	CROPD(^);
	return 1;
}

INT I_Crnand(UINT32 op)
{
	CROPN(&);
	return 1;
}

INT I_Crnor(UINT32 op)
{
	CROPN(|);
	return 1;
}

INT I_Creqv(UINT32 op)
{
	CROPN(^);
	return 1;
}

INT I_Mcrf(UINT32 op)
{
	SET_CR(RT>>2, GET_CR(RA>>2));
	return 1;
}

INT I_Mcrxr(UINT32 op)
{
	Print("%08X: mcrxr\n", PC);
	return 1;
}

INT I_Mfcr(UINT32 op)
{
	R(RT) = GET_CR(0) << 28 |
			GET_CR(1) << 24 |
			GET_CR(2) << 20 |
			GET_CR(3) << 16 |
			GET_CR(4) << 12 |
			GET_CR(5) << 8 |
			GET_CR(6) << 4 |
			GET_CR(7) << 0;

	return 1;
}

INT I_Mtcrf(UINT32 op)
{
	UINT32 f = _FXM;
	UINT32 t = RT;

	if(f & 0x80) SET_CR(0, (R(t) >> 28) & 15);
	if(f & 0x40) SET_CR(1, (R(t) >> 24) & 15);
	if(f & 0x20) SET_CR(2, (R(t) >> 20) & 15);
	if(f & 0x10) SET_CR(3, (R(t) >> 16) & 15);
	if(f & 0x08) SET_CR(4, (R(t) >> 12) & 15);
	if(f & 0x04) SET_CR(5, (R(t) >>  8) & 15);
	if(f & 0x02) SET_CR(6, (R(t) >>  4) & 15);
	if(f & 0x01) SET_CR(7, (R(t) >>  0) & 15);

	return 1;
}

INT I_Mfmsr(UINT32 op)
{
	R(RT) = GetMSR();

	return 1;
}

INT I_Mtmsr(UINT32 op)
{
	SetMSR(R(RT));

	return 1;
}

INT I_Mfspr(UINT32 op)
{
	R(RT) = GetSPR(_SPRF);

	return 1;
}

INT I_Mtspr(UINT32 op)
{
	SetSPR(_SPRF, R(RT));

	return 1;
}

#if PPC_MODEL == PPC_MODEL_4XX

INT I_Mfdcr(UINT32 op)
{
	R(RT) = GetDCR(_DCRF);

	return 1;
}

INT I_Mtdcr(UINT32 op)
{
	SetDCR(_DCRF, R(RT));

	return 1;
}

#endif

INT I_Mftb(UINT32 op)
{
	UINT32 t = RT;
	UINT32 x = _SPRF;

	switch (x)
	{
	case 268: R(t) = ReadTimebaseLo(); break;
	case 269: R(t) = ReadTimebaseHi(); break;
	default:
		Print("invalid mftb\n");
	}

	return 1;
}

INT I_Mfsr(UINT32 op)
{
	UINT32 sr = (op >> 16) & 15;
	UINT32 t = RT;

	R(t) = SR(sr);

	return 1;
}

INT I_Mfsrin(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	R(t) = SR(R(b) >> 28);

	return 1;
}

INT I_Mtsr(UINT32 op)
{
	UINT32 sr = (op >> 16) & 15;
	UINT32 t = RT;

	SR(sr) = R(t);

	return 1;
}

INT I_Mtsrin(UINT32 op)
{
	UINT32 b = RB;
	UINT32 t = RT;

	SR(R(b) >> 28) = R(t);

	return 1;
}

#if PPC_MODEL == PPC_MODEL_4XX

INT I_Wrtee(UINT32 op)
{
	SetMSR((GetMSR() & ~0x00008000) | (R(RT) & 0x00008000));

	return 1;
}

INT I_Wrteei(UINT32 op)
{
	SetMSR((GetMSR() & ~0x00008000) | (op & 0x00008000));

	return 1;
}

#endif

INT I_Eieio(UINT32 op)
{
	return 1;
}

INT I_Isync(UINT32 op)
{
	return 1;
}

INT I_Sync(UINT32 op)
{
	return 1;
}

INT I_Icbi(UINT32 op)
{
	return 1;
}

INT I_Dcbi(UINT32 op)
{
	return 1;
}

INT I_Dcbf(UINT32 op)
{
	return 1;
}

INT I_Dcba(UINT32 op)
{
	return 1;
}

INT I_Dcbst(UINT32 op)
{
	return 1;
}

INT I_Dcbt(UINT32 op)
{
	return 1;
}

INT I_Dcbtst(UINT32 op)
{
	return 1;
}

INT I_Dcbz(UINT32 op)
{
	return 1;
}

INT I_Dccci(UINT32 op)
{
	return 1;
}

INT I_Iccci(UINT32 op)
{
	return 1;
}

INT I_Dcread(UINT32 op)
{
	return 1;
}

INT I_Icread(UINT32 op)
{
	return 1;
}

INT I_Icbt(UINT32 op)
{
	return 1;
}

INT I_Tlbie(UINT32 op)
{
	return 1;
}

INT I_Tlbld(UINT32 op)
{
	return 1;
}

INT I_Tlbli(UINT32 op)
{
	return 1;
}

INT I_Tlbsync(UINT32 op)
{
	return 1;
}

INT I_Tlbia(UINT32 op)
{
	return 1;
}

INT I_Eciwx(UINT32 op)
{
	return 1;
}

INT I_Ecowx(UINT32 op)
{
	return 1;
}

#if PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO

INT I_Mcrfs(UINT32 op)
{
    UINT32 crfS, f;

    crfS = CRFA;

    f = FPSCR >> ((7 - crfS) * 4);  // get crfS field from FPSCR
    f &= 0xF;

    switch (crfS)   // determine which exception bits to clear in FPSCR
    {
    case 0: // FX, OX
        FPSCR &= ~0x90000000;
        break;
    case 1: // UX, ZX, XX, VXSNAN
        FPSCR &= ~0x0F000000;
        break;
    case 2: // VXISI, VXIDI, VXZDZ, VXIMZ
        FPSCR &= ~0x00F00000;
        break;
    case 3: // VXVC
        FPSCR &= ~0x00080000;
        break;
    case 5: // VXSOFT, VXSQRT, VXCVI
        FPSCR &= ~0x00000E00;
        break;
    default:
        break;
    }

    SET_CR(CRFT, f);

	return 1;
}

INT I_Mtfsb0x(UINT32 op)
{
    UINT32 crbD;

    crbD = (op >> 21) & 0x1F;

    if (crbD != 1 && crbD != 2) // these bits cannot be explicitly cleared
        FPSCR &= ~(1 << (31 - crbD));

    SET_FCR1();

	return 1;
}

INT I_Mtfsb1x(UINT32 op)
{
    UINT32 crbD;

    crbD = (op >> 21) & 0x1F;

    if (crbD != 1 && crbD != 2) // these bits cannot be explicitly cleared
        FPSCR |= (1 << (31 - crbD));

    SET_FCR1();

	return 1;
}

INT I_Mffsx(UINT32 op)
{
	UINT32 t = RT;

	F(t).iw = FPSCR;

	SET_FCR1();

	return 1;
}

INT I_Mtfsfx(UINT32 op)
{
	UINT32 b = RB;
	UINT32 f = _FM;

	f =	((f & 0x80) ? 0xF0000000 : 0) |
		((f & 0x40) ? 0x0F000000 : 0) |
		((f & 0x20) ? 0x00F00000 : 0) |
		((f & 0x10) ? 0x000F0000 : 0) |
		((f & 0x08) ? 0x0000F000 : 0) |
		((f & 0x04) ? 0x00000F00 : 0) |
		((f & 0x02) ? 0x000000F0 : 0) |
		((f & 0x01) ? 0x0000000F : 0);

	FPSCR &= (~f) | ~(FPSCR_FEX | FPSCR_VX);
	FPSCR |= (F(b).iw) & ~(FPSCR_FEX | FPSCR_VX);

	// FEX, VX

	SET_FCR1();

	return 1;
}

INT I_Mtfsfix(UINT32 op)
{
    UINT32 crfD = CRFT;
    UINT32 imm = (op >> 12) & 0xF;

    /*
     * According to the manual:
     *
     * If bits 0 and 3 of FPSCR are to be modified, they take the immediate
     * value specified. Bits 1 and 2 (FEX and VX) are set according to the
     * "usual rule" and not from IMM[1-2].
     *
     * The "usual rule" is not emulated, so these bits simply aren't modified
     * at all here.
     */

    crfD = (7 - crfD) * 4;  // calculate LSB position of field

    if (crfD == 28)         // field containing FEX and VX is special...
    {                       // bits 1 and 2 of FPSCR must not be altered
        FPSCR &= 0x9FFFFFFF;
        FPSCR |= (imm & 0x9FFFFFFF);
    }

    FPSCR &= ~(0xF << crfD);    // clear field
    FPSCR |= (imm << crfD);     // insert new data

	SET_FCR1();

	return 1;
}

#endif
