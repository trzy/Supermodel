/*
 * front/powerpc/internal.h
 *
 * A bunch of definitions, macros, and functions, used internally to the PowerPC
 * front-end.
 */

#ifndef INCLUDED_INTERNAL_H
#define INCLUDED_INTERNAL_H

#include <math.h>	// ceil, floor

#include "drppc.h"
#include "toplevel.h"
#include "source.h"

/*******************************************************************************
 Global Variables and Routines

 These objects must be visible all through the front-end.
*******************************************************************************/

extern PPC_CONTEXT	ppc;
extern UINT32		mask_table[1024];


extern INT		InitModel (DRPPC_CFG *, UINT32);
extern INT		ResetModel (void);
extern void		ShutdownModel (void);

extern UINT32	GetMSR();
extern void		SetMSR(UINT32);
extern UINT32	GetSPR(UINT);
extern void		SetSPR(UINT, UINT32);
extern UINT32	GetDCR(UINT);
extern void		SetDCR(UINT, UINT32);

extern UINT8	Read8(UINT32);
extern UINT16	Read16(UINT32);
extern UINT32	Read32(UINT32);
extern UINT64	Read64(UINT32);
extern void		Write8(UINT32, UINT8);
extern void		Write16(UINT32, UINT16);
extern void		Write32(UINT32, UINT32);
extern void		Write64(UINT32, UINT64);

extern void		CheckIRQs();

extern INT		UpdatePC(void);
extern void		UpdateTimers(INT);
extern UINT32	ReadTimebaseLo(void);
extern UINT32	ReadTimebaseHi(void);
extern void		WriteTimebaseLo(UINT32);
extern void		WriteTimebaseHi(UINT32);

/*******************************************************************************
 Register Definitions

 Same as above. These macros encode register numbers for SPRs and DCRs.
*******************************************************************************/

#define SPR_XER			1
#define SPR_LR			8
#define SPR_CTR			9
#define SPR_SRR0		26
#define SPR_SRR1		27
#define SPR_SPRG0		272
#define SPR_SPRG1		273
#define SPR_SPRG2		274
#define SPR_SPRG3		275
#define SPR_PVR			287

/*
 * PowerPC 4xx SPRs
 */

#define SPR_ICDBDR		0x3D3
#define SPR_ESR			0x3D4
#define SPR_DEAR		0x3D5
#define SPR_EVPR		0x3D6
#define SPR_CDBCR		0x3D7
#define SPR_TSR			0x3D8
#define SPR_TCR			0x3DA
#define SPR_PIT			0x3DB
#define SPR_TBHI		0x3DC
#define SPR_TBLO		0x3DD
#define SPR_SRR2		0x3DE
#define SPR_SRR3		0x3DF
#define SPR_DBSR		0x3F0
#define SPR_DBCR		0x3F2
#define SPR_IAC1		0x3F4
#define SPR_IAC2		0x3F5
#define SPR_DAC1		0x3F6
#define SPR_DAC2		0x3F7
#define SPR_DCCR		0x3FA
#define SPR_ICCR		0x3FB
#define SPR_PBL1		0x3FC
#define SPR_PBU1		0x3FD
#define SPR_PBL2		0x3FE
#define SPR_PBU2		0x3FF

/*
 * PowerPC 4xx DCRs
 */

#define DCR_EXIER		0x42
#define DCR_EXISR		0x40
#define DCR_BR0			0x80
#define DCR_BR1			0x81
#define DCR_BR2			0x82
#define DCR_BR3			0x83
#define DCR_BR4			0x84
#define DCR_BR5			0x85
#define DCR_BR6			0x86
#define DCR_BR7			0x87
#define DCR_BEAR		0x90
#define DCR_BESR		0x91
#define DCR_IOCR		0xA0
#define DCR_DMACR0		0xC0
#define DCR_DMACT0		0xC1
#define DCR_DMADA0		0xC2
#define DCR_DMASA0		0xC3
#define DCR_DMACC0		0xC4
#define DCR_DMACR1		0xC8
#define DCR_DMACT1		0xC9
#define DCR_DMADA1		0xCA
#define DCR_DMASA1		0xCB
#define DCR_DMACC1		0xCC
#define DCR_DMACR2		0xD0
#define DCR_DMACT2		0xD1
#define DCR_DMADA2		0xD2
#define DCR_DMASA2		0xD3
#define DCR_DMACC2		0xD4
#define DCR_DMACR3		0xD8
#define DCR_DMACT3		0xD9
#define DCR_DMADA3		0xDA
#define DCR_DMASA3		0xDB
#define DCR_DMACC3		0xDC
#define DCR_DMASR		0xE0

/*
 * PowerPC 6xx SPRs
 */

#define SPR_DSISR		18
#define SPR_DAR			19
#define SPR_DEC			22
#define SPR_SDR1		25
#define SPR_TBL_R		268
#define SPR_TBU_R		269
#define SPR_TBL_W		284
#define SPR_TBU_W		285
#define SPR_EAR			282
#define SPR_IBAT0U		528
#define SPR_IBAT0L		529
#define SPR_IBAT1U		530
#define SPR_IBAT1L		531
#define SPR_IBAT2U		532
#define SPR_IBAT2L		533
#define SPR_IBAT3U		534
#define SPR_IBAT3L		535
#define SPR_DBAT0U		536
#define SPR_DBAT0L		537
#define SPR_DBAT1U		538
#define SPR_DBAT1L		539
#define SPR_DBAT2U		540
#define SPR_DBAT2L		541
#define SPR_DBAT3U		542
#define SPR_DBAT3L		543
#define SPR_DMISS		976
#define SPR_DCMP		977
#define SPR_HASH1		978
#define SPR_HASH2		979
#define SPR_IMISS		980
#define SPR_ICMP		981
#define SPR_RPA			982
#define SPR_DABR		1013
#define SPR_HID0		1008
#define SPR_HID1		1009
#define SPR_IABR		1010

/*
 * PowerPC 750 SPRs
 */

#define SPR_UPMC1		937
#define SPR_UPMC2		938
#define SPR_UPMC3		941
#define SPR_UPMC4		942
#define SPR_USIA		939
#define SPR_UMMCR0		936
#define SPR_UMMCR1		940
#define SPR_L2CR		1017
#define SPR_IABR		1010
#define SPR_PMC1		953
#define SPR_PMC2		954
#define SPR_PMC3		957
#define SPR_PMC4		958
#define SPR_SIA			955
#define SPR_MMCR0		952
#define SPR_MMCR1		956
#define SPR_THRM1		1020
#define SPR_THRM2		1021
#define SPR_THRM3		1022
#define SPR_ICTC		1019

/*
 * PowerPC Gekko SPRs
 */

#define SPR_GQR0		912
#define SPR_GQR1		913
#define SPR_GQR2		914
#define SPR_GQR3		915
#define SPR_GQR4		916
#define SPR_GQR5		917
#define SPR_GQR6		918
#define SPR_GQR7		919
#define SPR_HID2		920
#define SPR_WPAR		921
#define SPR_DMA_U		922
#define SPR_DMA_L		923

/*******************************************************************************
 Opcode Fields

 A bunch of macros that perform annoyingly frequent bitfield extractions.
*******************************************************************************/

#define RT		((op >> 21) & 0x1F)
#define RA		((op >> 16) & 0x1F)
#define RB		((op >> 11) & 0x1F)
#define RC		((op >>  6) & 0x1F)

#define SIMM	((INT32)(INT16)op)
#define UIMM	((UINT32)(UINT16)op)
#define IMMS	((UINT32)op << 16)

#define _OP		((op >> 26) & 0x3F)
#define _XO		((op >> 1) & 0x3FF)

#define _CRFD	((op >> 23) & 7)
#define _CRFA	((op >> 18) & 7)

#define _OVE	0x00000400
#define _AA		0x00000002
#define _RC		0x00000001
#define _LK		0x00000001

#define _SH		((op >> 11) & 31)
#define _MB		((op >> 6) & 31)
#define _ME		((op >> 1) & 31)
#define _MB_ME	((op >> 1) & 1023)
#define _BO		((op >> 21) & 0x1F)
#define _BI		((op >> 16) & 0x1F)
#define _BD		((op >> 2) & 0x3FFF)

#define _SPRF	(((op >> 6) & 0x3E0) | ((op >> 16) & 0x1F))
#define _DCRF	(((op >> 6) & 0x3E0) | ((op >> 16) & 0x1F))

#define _FXM	((op >> 12) & 0xFF)
#define _FM		((op >> 17) & 0xFF)

/*******************************************************************************
 Register Fields

 Heavily used constants.
*******************************************************************************/

#define CR_LT	8
#define CR_GT	4
#define CR_EQ	2
#define CR_SO	1

#define XER_SO	0x80000000
#define XER_OV	0x40000000
#define XER_CA	0x20000000

/*******************************************************************************
 Context Shortcuts

 Uhm ...
*******************************************************************************/

#define R(n)		ppc.r[n]
#define F(n)		ppc.f[n]
#define F_PS(n)		ppc.f_is_ps[n]
#define CR(n)		ppc.cr[n]
#define SR(n)		ppc.sr[n]
#define SPR(n)		ppc.spr[n]
#define DCR(n)		ppc.dcr[n]

#define PC			ppc.pc
#define MSR			ppc.msr
#define FPSCR		ppc.fpscr
#define CRFD		CR(_CRFD)
#define DEC			SPR(SPR_DEC)
#define XER			SPR(SPR_XER)
#define LR			SPR(SPR_LR)
#define CTR			SPR(SPR_CTR)

/*******************************************************************************
 Macros

 Generic helpers.
*******************************************************************************/

#define BIT(n)				((UINT32)0x80000000 >> (n))
#define BITMASK_0(n)		((UINT32)((((UINT64)1 << (n))) - 1))

#define ADD_C(r,a,b)		((UINT32)(r) < (UINT32)(a))
#define SUB_C(r,a,b)        (!((UINT32)(a) < (UINT32)(b)))

#define SET_ADD_C(r,a,b)	if (ADD_C(r,a,b)) { XER |= XER_CA; } else { XER &= ~XER_CA; }
#define SET_SUB_C(r,a,b)	if (SUB_C(r,a,b)) { XER |= XER_CA; } else { XER &= ~XER_CA; }

#define ADD_V(r,a,b)		((~((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)
#define SUB_V(r,a,b)		(( ((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)

#define SET_ADD_V(r,a,b)	if(op & _OVE) { if(ADD_V(r,a,b)) { XER |= XER_SO | XER_OV; } else { XER &= ~XER_OV; } }
#define SET_SUB_V(r,a,b)	if(op & _OVE) { if(SUB_V(r,a,b)) { XER |= XER_SO | XER_OV; } else { XER &= ~XER_OV; } }

#define _SET_ICR0(r)		{ CR(0) = CMPS(r,0); if (XER & XER_SO) CR(0) |= CR_SO; }
#define SET_ICR0(r)			if (op & _RC){ _SET_ICR0(r); }

static UINT CMPS(INT32 a, INT32 b)
{
	if(a < b)
		return CR_LT;
	else if(a > b)
		return CR_GT;
	else
		return CR_EQ;
}

static UINT CMPU(UINT32 a, UINT32 b)
{
	if(a < b)
		return CR_LT;
	else if(a > b)
		return CR_GT;
	else
		return CR_EQ;
}

/*******************************************************************************
 Floating-Point Code

 Helpful macros for FPU emulation.
*******************************************************************************/

#if PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO

#define CHECK_FPU_AVAILABLE()			\
			if ((MSR & 0x2000) == 0)	\
			{ /* Do nothing. :) */ }

/*
 * FPSCR fields.
 */

#define FPSCR_NI		BIT(29)
#define FPSCR_XE		BIT(28)
#define FPSCR_ZE		BIT(27)
#define FPSCR_UE		BIT(26)
#define FPSCR_OE		BIT(25)
#define FPSCR_VE		BIT(24)
#define FPSCR_VXCVI		BIT(23)
#define FPSCR_VXSQRT	BIT(22)
#define FPSCR_VXSOFT	BIT(21)
#define FPSCR_FI		BIT(14)
#define FPSCR_FR		BIT(13)
#define FPSCR_VXVC		BIT(12)
#define FPSCR_VXIMZ		BIT(11)
#define FPSCR_VXZDZ		BIT(10)
#define FPSCR_VXIDI		BIT(9)
#define FPSCR_VXISI		BIT(8)
#define FPSCR_VXSNAN	BIT(7)
#define FPSCR_XX		BIT(6)
#define FPSCR_ZX		BIT(5)
#define FPSCR_UX		BIT(4)
#define FPSCR_OX		BIT(3)
#define FPSCR_VX		BIT(2)
#define FPSCR_FEX		BIT(1)
#define FPSCR_FX		BIT(0)

/*
 * Single (32-bit floating-point) fields.
 */

#define SINGLE_SIGN		0x80000000
#define SINGLE_EXP		0x7F800000
#define SINGLE_FRAC		0x007FFFFF
#define SINGLE_QUIET	0x00400000

/*
 * Double (64-bit floating-point) fields.
 */

#define DOUBLE_SIGN		0x8000000000000000LL
#define DOUBLE_EXP		0x7FF0000000000000LL
#define DOUBLE_FRAC		0x000FFFFFFFFFFFFFLL
#define DOUBLE_QUIET	0x0008000000000000LL

/*
 * FP value checks.
 */

#define INLINE static

INLINE BOOL is_nan_f32(FLOAT32 x)
{
	return	((*(UINT32 *)&x & SINGLE_EXP) == SINGLE_EXP) && 
			((*(UINT32 *)&x & SINGLE_FRAC) != 0);
}

INLINE BOOL is_qnan_f32(FLOAT32 x)
{
	return	is_nan_f32(x) && ((*(UINT32 *)&x & SINGLE_QUIET) != 0);
}

INLINE BOOL is_snan_f32(FLOAT32 x)
{
	return	is_nan_f32(x) && ((*(UINT32 *)&x & SINGLE_QUIET) == 0);
}

INLINE BOOL is_infinity_f32(FLOAT32 x)
{
	return	((*(UINT32 *)&x & SINGLE_EXP) == SINGLE_EXP) &&
			((*(UINT32 *)&x & SINGLE_FRAC) == 0);
}

INLINE BOOL is_denormal_f32(FLOAT32 x)
{
	return	((*(UINT32 *)&x & SINGLE_EXP) == 0) &&
			((*(UINT32 *)&x & SINGLE_FRAC) != 0);
}

INLINE BOOL is_nan_f64(FLOAT64 x)
{
	return	((*(UINT64 *)&x & DOUBLE_EXP) == DOUBLE_EXP) &&
			((*(UINT64 *)&x & DOUBLE_FRAC) != 0);
}

INLINE BOOL is_qnan_f64(FLOAT64 x)
{
	return	is_nan_f64(x) && ((*(UINT64 *)&x & DOUBLE_QUIET) != 0);
}

INLINE BOOL is_snan_f64(FLOAT64 x)
{
	return	is_nan_f64(x) && ((*(UINT64 *)&x & DOUBLE_QUIET) == 0);
}

INLINE BOOL is_infinity_f64(FLOAT64 x)
{
    return	((*(UINT64 *)&(x) & DOUBLE_EXP) == DOUBLE_EXP) &&
			((*(UINT64 *)&(x) & DOUBLE_FRAC) == 0);
}

INLINE BOOL is_normalized_f64(FLOAT64 x)
{
    UINT64 exp;

    exp = ((*(UINT64 *) &(x)) & DOUBLE_EXP) >> 52;

    return (exp >= 1) && (exp <= 2046);
}

INLINE BOOL is_denormalized_f64(FLOAT64 x)
{
	return	((*(UINT64 *)&x & DOUBLE_EXP) == 0) &&
			((*(UINT64 *)&x & DOUBLE_FRAC) != 0);
}

INLINE BOOL sign_f64(FLOAT64 x)
{
    return ((*(UINT64 *)&(x) & DOUBLE_SIGN) != 0);
}

/*
 * FP conversion routines.
 */

INLINE INT32 round_f64_to_s32(FLOAT64 v)
{
	if(v >= 0)
		return((INT32)(v + 0.5));
	else
		return(-(INT32)(-v + 0.5));
}

INLINE INT32 trunc_f64_to_s32(FLOAT64 v)
{
	if(v >= 0)
		return((INT32)v);
	else
		return(-((INT32)(-v)));
}

INLINE INT32 ceil_f64_to_s32(FLOAT64 v)
{
	return((INT32)ceil(v));
}

INLINE INT32 floor_f64_to_s32(FLOAT64 v)
{
	return((INT32)floor(v));
}

/*
 * Flag computations.
 */

#define _SET_FCR1()			{ CR(1) = (FPSCR >> 28); }
#define SET_FCR1()			if (op & _RC) { _SET_FCR1(); }

#define SET_VXSNAN(a, b)	if (is_snan_f64(a) || is_snan_f64(b)){	FPSCR |= FPSCR_FX; }
#define SET_VXSNAN_1(c)		if (is_snan_f64(c)){					FPSCR |= FPSCR_FX; }

INLINE void set_fprf(FLOAT64 f)
{
    UINT32 fprf;

    // see page 3-30, 3-31

    if (is_qnan_f64(f))
        fprf = 0x11;
    else if (is_infinity_f64(f))
    {
        if (sign_f64(f))    // -INF
            fprf = 0x09;
        else                // +INF
            fprf = 0x05;
    }
    else if (is_normalized_f64(f))
    {
        if (sign_f64(f))    // -Normalized
            fprf = 0x08;
        else                // +Normalized
            fprf = 0x04;
    }
    else if (is_denormalized_f64(f))
    {
        if (sign_f64(f))    // -Denormalized
            fprf = 0x18;
        else                // +Denormalized
            fprf = 0x14;
    }
    else    // Zero
    {
        if (sign_f64(f))    // -Zero
            fprf = 0x12;
        else                // +Zero
            fprf = 0x02;
    }

    FPSCR &= ~0x0001F000;
    FPSCR |= (fprf << 12);
}

#endif // PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO

/*******************************************************************************
 Gekko-specific Macros.

 Additional register definitions, helpful macros and paired-single stuff.
*******************************************************************************/

#if PPC_MODEL == PPC_MODEL_GEKKO

#define HID2			(SPR(SPR_HID2))

#define HID2_LSQE		BIT(0)
#define HID2_WPE		BIT(1)
#define HID2_PSE		BIT(2)
#define HID2_LCE		BIT(3)
#define HID2_DMAQL		((HID2 >> 24) & 15)
#define HID2_DCHERR		BIT(8)
#define HID2_DNCERR		BIT(9)
#define HID2_DCMERR		BIT(10)
#define HID2_DQOERR		BIT(11)
#define HID2_DCHEE		BIT(12)
#define HID2_DNCEE		BIT(31)
#define HID2_DCMEE		BIT(14)
#define HID2_DQOEE		BIT(15)

#define CHECK_PS_ENABLED()								\
			if (!(HID2 & HID2_PSE))						\
			{ Error("%08X: PS instruction hit, PSE=0\n", PC); }

#define GQR(n)			(SPR(SPR_GQR0 + (n & 7)))

#define LD_SCALE(n)		((GQR(n) >> 24) & 0x3F)
#define LD_TYPE(n)		((GQR(n) >> 16) & 7)
#define ST_SCALE(n)		((GQR(n) >> 8) & 0x3F)
#define ST_TYPE(n)		(GQR(n) & 7)

#define GQR_TYPE_F32	0
#define GQR_TYPE_U8		4
#define GQR_TYPE_U16	5
#define GQR_TYPE_S8		6
#define GQR_TYPE_S16	7

#endif // PPC_MODEL == PPC_MODEL_GEKKO

#endif // INCLUDED_INTERNAL_H
