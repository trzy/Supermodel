/*
 * ppc.c
 *
 * PowerPC Emulator.
 */

// TODO: build a jump table

#include "model3.h"

/*
 * Private Variables
 */

static PPC_CONTEXT	ppc;
static u32			pvr;    // default value initialized by ppc_init()

#ifndef LOG
#define LOG(...)
#endif

static UINT32	ppc_field_xlat[256];
static void		(* ppc_jump_table[131072])(UINT32 op);

static void ppc_update_pc(void);

/*
 * Shorthand Mnemonics
 */

#define SPR_XER			1		/* Fixed Point Exception Register				Read / Write */
#define SPR_LR			8		/* Link Register								Read / Write */
#define SPR_CTR			9		/* Count Register								Read / Write */
#define SPR_SRR0		26		/* Save/Restore Register 0						Read / Write */
#define SPR_SRR1		27		/* Save/Restore Register 1						Read / Write */
#define SPR_SPRG0		272		/* SPR General 0								Read / Write */
#define SPR_SPRG1		273		/* SPR General 1								Read / Write */
#define SPR_SPRG2		274		/* SPR General 2								Read / Write */
#define SPR_SPRG3		275		/* SPR General 3								Read / Write */
#define SPR_PVR			287		/* Processor Version Number						Read Only */

#define SPR_ICDBDR		0x3D3	/* 406GA Instruction Cache Debug Data Register	Rad Only */
#define SPR_ESR			0x3D4	/* 406GA Exception Syndrome Register			Read / Write */
#define SPR_DEAR		0x3D5	/* 406GA Data Exception Address Register		Read Only */
#define SPR_EVPR		0x3D6	/* 406GA Exception Vector Prefix Register		Read / Write */
#define SPR_CDBCR		0x3D7	/* 406GA Cache Debug Control Register			Read/Write */
#define SPR_TSR			0x3D8	/* 406GA Timer Status Register					Read / Clear */
#define SPR_TCR			0x3DA	/* 406GA Timer Control Register					Read / Write */
#define SPR_PIT			0x3DB	/* 406GA Programmable Interval Timer			Read / Write */
#define SPR_TBHI		988		/* 406GA Time Base High							Read / Write */
#define SPR_TBLO		989		/* 406GA Time Base Low							Read / Write */
#define SPR_SRR2		0x3DE	/* 406GA Save/Restore Register 2				Read / Write */
#define SPR_SRR3		0x3DF	/* 406GA Save/Restore Register 3				Read / Write */
#define SPR_DBSR		0x3F0	/* 406GA Debug Status Register					Read / Clear */
#define SPR_DBCR		0x3F2	/* 406GA Debug Control Register					Read / Write */
#define SPR_IAC1		0x3F4	/* 406GA Instruction Address Compare 1			Read / Write */
#define SPR_IAC2		0x3F5	/* 406GA Instruction Address Compare 2			Read / Write */
#define SPR_DAC1		0x3F6	/* 406GA Data Address Compare 1					Read / Write */
#define SPR_DAC2		0x3F7	/* 406GA Data Address Compare 2					Read / Write */
#define SPR_DCCR		0x3FA	/* 406GA Data Cache Cacheability Register		Read / Write */
#define SPR_ICCR		0x3FB	/* 406GA I Cache Cacheability Registe			Read / Write */
#define SPR_PBL1		0x3FC	/* 406GA Protection Bound Lower 1				Read / Write */
#define SPR_PBU1		0x3FD	/* 406GA Protection Bound Upper 1				Read / Write */
#define SPR_PBL2		0x3FE	/* 406GA Protection Bound Lower 2				Read / Write */
#define SPR_PBU2		0x3FF	/* 406GA Protection Bound Upper 2				Read / Write */

#define SPR_DSISR		18		/* 603E */
#define SPR_DAR			19		/* 603E */
#define SPR_DEC			22		/* 603E */
#define SPR_SDR1		25		/* 603E */
#define SPR_TBL_R		268		/* 603E Time Base Low (Read-only) */
#define SPR_TBU_R		269		/* 603E Time Base High (Read-only) */
#define SPR_TBL_W		284		/* 603E Time Base Low (Write-only) */
#define SPR_TBU_W		285		/* 603E Time Base Hight (Write-only) */
#define SPR_EAR			282		/* 603E */
#define SPR_IBAT0U		528		/* 603E */
#define SPR_IBAT0L		529		/* 603E */
#define SPR_IBAT1U		530		/* 603E */
#define SPR_IBAT1L		531		/* 603E */
#define SPR_IBAT2U		532		/* 603E */
#define SPR_IBAT2L		533		/* 603E */
#define SPR_IBAT3U		534		/* 603E */
#define SPR_IBAT3L		535		/* 603E */
#define SPR_DBAT0U		536		/* 603E */
#define SPR_DBAT0L		537		/* 603E */
#define SPR_DBAT1U		538		/* 603E */
#define SPR_DBAT1L		539		/* 603E */
#define SPR_DBAT2U		540		/* 603E */
#define SPR_DBAT2L		541		/* 603E */
#define SPR_DBAT3U		542		/* 603E */
#define SPR_DBAT3L		543		/* 603E */
#define SPR_DABR		1013	/* 603E */
#define SPR_DMISS		0x3d0	/* 603E */
#define SPR_DCMP		0x3d1	/* 603E */
#define SPR_HASH1		0x3d2	/* 603E */
#define SPR_HASH2		0x3d3	/* 603E */
#define SPR_IMISS		0x3d4	/* 603E */
#define SPR_ICMP		0x3d5	/* 603E */
#define SPR_RPA			0x3d6	/* 603E */
#define SPR_HID0		1008	/* 603E */
#define SPR_HID1		1009	/* 603E */
#define SPR_IABR		1010	/* 603E */

#define DCR_BEAR		0x90	/* bus error address */
#define DCR_BESR		0x91	/* bus error syndrome */
#define DCR_BR0			0x80	/* bank */
#define DCR_BR1			0x81	/* bank */
#define DCR_BR2			0x82	/* bank */
#define DCR_BR3			0x83	/* bank */
#define DCR_BR4			0x84	/* bank */
#define DCR_BR5			0x85	/* bank */
#define DCR_BR6			0x86	/* bank */
#define DCR_BR7			0x87	/* bank */
#define DCR_DMACC0		0xc4	/* dma chained count */
#define DCR_DMACC1		0xcc	/* dma chained count */
#define DCR_DMACC2		0xd4	/* dma chained count */
#define DCR_DMACC3		0xdc	/* dma chained count */
#define DCR_DMACR0		0xc0	/* dma channel control */
#define DCR_DMACR1		0xc8	/* dma channel control */
#define DCR_DMACR2		0xd0	/* dma channel control */
#define DCR_DMACR3		0xd8	/* dma channel control */
#define DCR_DMACT0		0xc1	/* dma destination address */
#define DCR_DMACT1		0xc9	/* dma destination address */
#define DCR_DMACT2		0xd1	/* dma destination address */
#define DCR_DMACT3		0xd9	/* dma destination address */
#define DCR_DMADA0		0xc2	/* dma destination address */
#define DCR_DMADA1		0xca	/* dma destination address */
#define DCR_DMADA2		0xd2	/* dma source address */
#define DCR_DMADA3		0xda	/* dma source address */
#define DCR_DMASA0		0xc3	/* dma source address */
#define DCR_DMASA1		0xcb	/* dma source address */
#define DCR_DMASA2		0xd3	/* dma source address */
#define DCR_DMASA3		0xdb	/* dma source address */
#define DCR_DMASR		0xe0	/* dma status */
#define DCR_EXIER		0x42	/* external interrupt enable */
#define DCR_EXISR		0x40	/* external interrupt status */
#define DCR_IOCR		0xa0	/* io configuration */

#define RD		((op >> 21) & 0x1F)
#define RS		((op >> 21) & 0x1F)
#define RT		((op >> 21) & 0x1F)
#define RA		((op >> 16) & 0x1F)
#define RB		((op >> 11) & 0x1F)
#define RC		((op >>  6) & 0x1F)

#define SIMM	((s32)(s16)op)
#define UIMM	((u32)(u16)op)

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
#define _BO		((op >> 21) & 0x1f)
#define _BI		((op >> 16) & 0x1f)
#define _BD		((op >> 2) & 0x3fff)

#define _SPRF	(((op >> 6) & 0x3E0) | ((op >> 16) & 0x1F))
#define _DCRF	(((op >> 6) & 0x3E0) | ((op >> 16) & 0x1F))

#define _FXM	((op >> 12) & 0xff)
#define _FM		((op >> 17) & 0xFF)

#define XER		ppc.spr[SPR_XER]
#define LR		ppc.spr[SPR_LR]
#define CTR		ppc.spr[SPR_CTR]
#define R(n)	ppc.gpr[n]
#define F(n)	ppc.fpr[n]
#define CR(n)	ppc.cr[n]
#define CRFD	ppc.cr[_CRFD]

#define _LT		8
#define _GT		4
#define _EQ		2
#define _SO		1

#define XER_SO	0x80000000
#define XER_OV	0x40000000
#define XER_CA	0x20000000

#define _NI		BIT(29)
#define _XE		BIT(28)
#define _ZE		BIT(27)
#define _UE		BIT(26)
#define _OE		BIT(25)
#define _VE		BIT(24)
#define _VXCVI	BIT(23)
#define _VXSQRT	BIT(22)
#define _VXSOFT	BIT(21)
#define _FI		BIT(14)
#define _FR		BIT(13)
#define _VXVC	BIT(12)
#define _VXIMZ	BIT(11)
#define _VXZDZ	BIT(10)
#define _VXIDI	BIT(9)
#define _VXISI	BIT(8)
#define _VXSNAN	BIT(7)
#define _XX		BIT(6)
#define _ZX		BIT(5)
#define _UX		BIT(4)
#define _OX		BIT(3)
#define _VX		BIT(2)
#define _FEX	BIT(1)
#define _FX		BIT(0)

/*
 * Helpers
 */

#define BIT(n)				((u32)0x80000000 >> n)

#define BITMASK_0(n)		((u32)((((u64)1 << n)) - 1))

#define ADD_C(r,a,b)		((u32)(r) < (u32)(a))
#define SUB_C(r,a,b)        (!((u32)(a) < (u32)(b)))

#define SET_ADD_C(r,a,b)	if(ADD_C(r,a,b)){ XER |= XER_CA; }else{ XER &= ~XER_CA; }
#define SET_SUB_C(r,a,b)	if(SUB_C(r,a,b)){ XER |= XER_CA; }else{ XER &= ~XER_CA; }

#define ADD_V(r,a,b)		((~((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)
#define SUB_V(r,a,b)		(( ((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)

#define SET_ADD_V(r,a,b)	if(op & _OVE){ if(ADD_V(r,a,b)){ XER |= XER_SO | XER_OV; }else{ XER &= ~XER_OV; } }
#define SET_SUB_V(r,a,b)	if(op & _OVE){ if(SUB_V(r,a,b)){ XER |= XER_SO | XER_OV; }else{ XER &= ~XER_OV; } }

#define CMPS(a,b)			(((s32)(a) < (s32)(b)) ? _LT : (((s32)(a) > (s32)(b)) ? _GT : _EQ))
#define CMPU(a,b)			(((u32)(a) < (u32)(b)) ? _LT : (((u32)(a) > (u32)(b)) ? _GT : _EQ))

#define _SET_ICR0(r)		{ CR(0) = CMPS(r,0); if(XER & XER_SO) CR(0) |= _SO; }
#define SET_ICR0(r)			if(op & _RC){ _SET_ICR0(r); }

#define _SET_FCR1()			{ CR(1) = (ppc.fpscr >> 28); }
#define SET_FCR1()			if(op & _RC){ _SET_FCR1(); }

#define SET_VXSNAN(a, b)    if (is_snan_f64(a) || is_snan_f64(b)) ppc.fpscr |= 0x80000000
#define SET_VXSNAN_1(c)     if (is_snan_f64(c)) ppc.fpscr |= 0x80000000

#define CHECK_SUPERVISOR()			\
	if((ppc.msr & 0x4000) != 0){	\
	}

#define CHECK_FPU_AVAILABLE()		\
	if((ppc.msr & 0x2000) == 0){	\
	}

/******************************************************************/
/* PowerPC 4xx Emulation                                          */
/******************************************************************/

#if PPC_MODEL == PPC_MODEL_4XX
#include "ppc_4xx.h"
#endif

/******************************************************************/
/* PowerPC 6xx Emulation                                          */
/******************************************************************/

#if (PPC_MODEL == PPC_MODEL_6XX)
#include "ppc_6xx.h"
#endif /* PPC_MODEL == PPC_MODEL_6XX */

/******************************************************************/
/* Instruction Handlers                                           */
/******************************************************************/

static void ppc_null(u32 op)
{
    char    string[256];
    char    mnem[16], oprs[48];

    DisassemblePowerPC(op, ppc.pc, mnem, oprs, 1);

    printf("ERROR: %08X: unhandled opcode %08X: %s\t%s\n", ppc.pc, op, mnem, oprs);
	exit(1);
}

/*
 * Arithmetical
 */

static void ppc_addx(u32 op){

	u32 rb = R(RB);
	u32 ra = R(RA);
	u32 t = RT;

	R(t) = ra + rb;

	SET_ADD_V(R(t), ra, rb);
	SET_ICR0(R(t));
}

static void ppc_addcx(u32 op){

	u32 rb = R(RB);
	u32 ra = R(RA);
	u32 t = RT;

	R(t) = ra + rb;

	SET_ADD_C(R(t), ra, rb);
	SET_ADD_V(R(t), ra, rb);
	SET_ICR0(R(t));
}

static void ppc_addex(u32 op){

	u32 rb = R(RB);
	u32 ra = R(RA);
	u32 t = RT;
	u32 c = (XER >> 29) & 1;
	u32 temp;

	temp = rb + c;
	R(t) = ra + temp;

	if(ADD_C(temp, rb, c) || ADD_C(R(t), ra, temp))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	if(op & _OVE)
	{
        if(ADD_V(R(t), ra, rb))
			XER |= XER_SO | XER_OV;
		else
			XER &= ~XER_OV;
	}

	SET_ICR0(R(t));
}

static void ppc_addi(u32 op){

	u32 i = SIMM;
	u32 a = RA;
	u32 d = RT;

	if(a) i += R(a);

	R(d) = i;
}

static void ppc_addic(u32 op){

	u32 i = SIMM;
	u32 ra = R(RA);
	u32 t = RT;

	R(t) = ra + i;

	if(R(t) < ra)
		XER |= XER_CA;
	else
		XER &= ~XER_CA;
}

static void ppc_addic_(u32 op){

	u32 i = SIMM;
	u32 ra = R(RA);
	u32 t = RT;

	R(t) = ra + i;

	if(R(t) < ra)
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	_SET_ICR0(R(t));
}

static void ppc_addis(u32 op){

	u32 i = (op << 16);
	u32 a = RA;
	u32 t = RT;

	if(a) i += R(a);

	R(t) = i;
}

static void ppc_addmex(u32 op){

	u32 ra = R(RA);
	u32 d = RT;
	u32 c = (XER >> 29) & 1;
    u32 temp;

    temp = ra + c;
    R(d) = temp + -1;

    if (ADD_C(temp, ra, c) || ADD_C(R(d), temp, -1))
        XER |= XER_CA;
    else
        XER &= ~XER_CA;

	SET_ADD_V(R(d), ra, (c - 1));
	SET_ICR0(R(d));
}

static void ppc_addzex(u32 op){

	u32 ra = R(RA);
	u32 d = RT;
	u32 c = (XER >> 29) & 1;

	R(d) = ra + c;

	SET_ADD_C(R(d), ra, c);
	SET_ADD_V(R(d), ra, c);
	SET_ICR0(R(d));
}

static void ppc_subfx(u32 op){

	u32 rb = R(RB);
	u32 ra = R(RA);
	u32 d = RT;

	R(d) = rb - ra;

	SET_SUB_V(R(d), rb, ra);
	SET_ICR0(R(d));
}

static void ppc_subfcx(u32 op){

	u32 rb = R(RB);
	u32 ra = R(RA);
	u32 t = RT;

	R(t) = rb - ra;

	SET_SUB_C(R(t), rb, ra);
	SET_SUB_V(R(t), rb, ra);
	SET_ICR0(R(t));
}

static void ppc_subfex(u32 op){

	u32 rb = R(RB);
	u32 ra = R(RA);
	u32 t = RT;
	u32 c = (XER >> 29) & 1;
    u32 a;

    a = ~ra + c;
    R(t) = rb + a;

    SET_ADD_C(a, ~ra, c);   // step 1 carry
    if (R(t) < a)           // step 2 carry
        XER |= XER_CA;
    SET_SUB_V(R(t), rb, ra);

	SET_ICR0(R(t));
}

static void ppc_subfic(u32 op){

	u32 i = SIMM;
	u32 ra = R(RA);
	u32 t = RT;

	R(t) = i - ra;

	SET_SUB_C(R(t), i, ra);
}

static void ppc_subfmex(u32 op){    
	u32 a = RA;
	u32 t = RT;
	u32 c = (XER >> 29) & 1;
    u32 a_c;

    a_c = ~R(a) + c;    // step 1
    R(t) = a_c - 1;     // step 2

    SET_ADD_C(a_c, ~R(a), c);   // step 1 carry
    if (R(t) < a_c)             // step 2 carry
        XER |= XER_CA;
    SET_SUB_V(R(t), -1, R(a));

	SET_ICR0(R(t));
}

static void ppc_subfzex(u32 op){
	u32 a = RA;
	u32 t = RT;
	u32 c = (XER >> 29) & 1;

	R(t) = ~R(a) + c;

    SET_ADD_C(R(t), ~R(a), c);
    SET_SUB_V(R(t), 0, R(a));

	SET_ICR0(R(t));
}

static void ppc_negx(u32 op){

	u32 a = RA;
	u32 t = RT;

	R(t) = -R(a);

	if(op & _OVE){
		if(R(t) == 0x80000000)
			XER |= (XER_SO | XER_OV);
		else
			XER &= ~XER_OV;
	}

	SET_ICR0(R(t));
}

static void ppc_cmp(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = _CRFD;

	CR(d) = CMPS(R(a), R(b));
	CR(d) |= (XER >> 31);
}

static void ppc_cmpl(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = _CRFD;

	CR(d) = CMPU(R(a), R(b));
	CR(d) |= (XER >> 31);
}

static void ppc_cmpi(u32 op){

	u32 i = SIMM;
	u32 a = RA;
	u32 d = _CRFD;

	CR(d) = CMPS(R(a), i);
	CR(d) |= (XER >> 31);
}

static void ppc_cmpli(u32 op){

	u32 i = UIMM;
	u32 a = RA;
	u32 d = _CRFD;

	CR(d) = CMPU(R(a), i);
	CR(d) |= (XER >> 31);
}

static void ppc_mulli(u32 op){

	u32 i = SIMM;
	u32 a = RA;
	u32 d = RD;

	R(d) = ((s32)R(a) * (s32)i);
}

static void ppc_mulhwx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = RD;

	R(d) = ((s64)(s32)R(a) * (s64)(s32)R(b)) >> 32;

	SET_ICR0(R(d));
}

static void ppc_mulhwux(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = RD;

	R(d) = ((u64)(u32)R(a) * (u64)(u32)R(b)) >> 32;

	SET_ICR0(R(d));
}

static void ppc_mullwx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = RD;
	u32 h;
	s64 r;

	r = (s64)(s32)R(a) * (s64)(s32)R(b);
	R(d) = (u32)r;

	h = (r >> 32) & 0xFFFFFFFF;

	if(op & _OVE){
		XER &= ~XER_OV;
		//if( h != 0 && h != 0xFFFFFFFF )
		if( r != (s64)(s32)(r & 0xFFFFFFFF) )
			XER |= (XER_OV | XER_SO);
	}

	SET_ICR0(R(d));
}

static void ppc_divwx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = RD;

	if(R(b) == 0 && (R(a) < 0x80000000))
	{
		R(d) = 0;

		if(op & _OVE) XER |= (XER_SO | XER_OV);
		SET_ICR0(R(d));
	}
	else if( R(b) == 0 ||
		(R(b) == 0xFFFFFFFF && R(a) == 0x80000000))
	{
		R(d) = 0xFFFFFFFF;

		if(op & _OVE) XER |= (XER_SO | XER_OV);
		SET_ICR0(R(d));
	}
	else
	{
		R(d) = (s32)R(a) / (s32)R(b);

		if(op & _OVE) XER &= ~XER_OV;

		SET_ICR0(R(d));
	}
}

static void ppc_divwux(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = RD;

	if(R(b) == 0)
	{
		R(d) = 0;

		if(op & _OVE) XER |= (XER_SO | XER_OV);

		SET_ICR0(R(d));
	}
	else
	{
		R(d) = (u32)R(a) / (u32)R(b);

		if(op & _OVE) XER &= ~XER_OV;

		SET_ICR0(R(d));
	}
}

static void ppc_extsbx(u32 op){

	u32 a = RA;
	u32 s = RS;

	R(a) = ((s32)R(s) << 24) >> 24;
	SET_ICR0(R(a));
}

static void ppc_extshx(u32 op){

	u32 a = RA;
	u32 s = RS;

	R(a) = ((s32)R(s) << 16) >> 16;
	SET_ICR0(R(a));
}

static void ppc_cntlzwx(u32 op){

	u32 a = RA;
	u32 t = RT;
	u32 m = 0x80000000;
	u32 n = 0;

	while(n < 32){
		if(R(t) & m) break;
		m >>= 1;
		n++;
	}

	R(a) = n;

	SET_ICR0(R(a));
}

/*
 * Logical
 */

static void ppc_andx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) & R(b);
	SET_ICR0(R(a));
}

static void ppc_andcx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) & ~R(b);
	SET_ICR0(R(a));
}

static void ppc_orx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) | R(b);
	SET_ICR0(R(a));
}

static void ppc_orcx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) | ~ R(b);
	SET_ICR0(R(a));
}

static void ppc_xorx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) ^ R(b);
	SET_ICR0(R(a));
}

static void ppc_nandx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 s = RS;

	R(a) = ~(R(s) & R(b));
	SET_ICR0(R(a));
}

static void ppc_norx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 s = RS;

	R(a) = ~(R(s) | R(b));
	SET_ICR0(R(a));
}

static void ppc_eqvx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 s = RS;

	R(a) = ~(R(s) ^ R(b));
	SET_ICR0(R(a));
}

static void ppc_andi_(u32 op){

	u32 i = (op & 0xFFFF);
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) & i;
	_SET_ICR0(R(a));
}

static void ppc_andis_(u32 op){

	u32 i = (op << 16);
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) & i;
	_SET_ICR0(R(a));
}

static void ppc_ori(u32 op){

	u32 i = (op & 0xFFFF);
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) | i;
}

static void ppc_oris(u32 op){

	u32 i = (op << 16);
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) | i;
}

static void ppc_xori(u32 op){

	u32 i = (op & 0xFFFF);
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) ^ i;
}

static void ppc_xoris(u32 op){

	u32 i = (op << 16);
	u32 a = RA;
	u32 s = RS;

	R(a) = R(s) ^ i;
}

INLINE u32 ppc_calc_mask(u32 mb, u32 me){

	u32 m ;
	m = ((u32)0xFFFFFFFF >> mb) ^ ((me >= 31) ? 0 : ((u32)0xFFFFFFFF >> (me + 1)));

	if(mb > me)
		return(~m);
	else
		return(m);
}

static void ppc_rlwimix(u32 op){

	u32 me = _ME;
	u32 mb = _MB;
	u32 sh = _SH;
	u32 a = RA;
	u32 s = RS;
	u32 m, r;

	r = (ppc.gpr[s] << sh) | (ppc.gpr[s] >> (32 - sh));
	m = ppc_calc_mask(mb, me);

	ppc.gpr[a] &= ~m;
	ppc.gpr[a] |= (r & m);

	SET_ICR0(ppc.gpr[a]);
}

static void ppc_rlwinmx(u32 op){

	u32 me = _ME;
	u32 mb = _MB;
	u32 sh = _SH;
	u32 a = RA;
	u32 s = RS;
	u32 m, r;

	r = (R(s) << sh) | (R(s) >> (32 - sh));
	m = ppc_calc_mask(mb, me);

	R(a) = r & m;

	SET_ICR0(R(a));
}

static void ppc_rlwnmx(u32 op){

	u32 me = _ME;
	u32 mb = _MB;
	u32 sh = (R(RB) & 0x1F);
	u32 a = RA;
	u32 s = RS;
	u32 m, r;

	r = (ppc.gpr[s] << sh) | (ppc.gpr[s] >> (32 - sh));
	m = ppc_calc_mask(mb, me);

	ppc.gpr[a] = (r & m);

	SET_ICR0(ppc.gpr[a]);
}

////////////////////////////////////////////////////////////////
// Shift

static void ppc_slwx(u32 op){

	u32 sa = R(RB) & 0x3F;
	u32 a = RA;
	u32 t = RT;

	if((sa & 0x20) == 0)
		R(a) = R(t) << sa;
	else
		R(a) = 0;
		
	SET_ICR0(R(a));
}

static void ppc_srwx(u32 op){

	u32 sa = R(RB) & 0x3F;
	u32 a = RA;
	u32 t = RT;

	if((sa & 0x20) == 0)
		R(a) = R(t) >> sa;
	else
		R(a) = 0;

	SET_ICR0(R(a));
}

static void ppc_srawx(u32 op){

	u32 sa = R(RB) & 0x3F;
	u32 a = RA;
	u32 t = RT;

	XER &= ~XER_CA;
	if(((s32)R(t) < 0) && R(t) & BITMASK_0(sa))
		XER |= XER_CA;

	if((sa & 0x20) == 0)
		R(a) = (s32)R(t) >> sa;
	else
		R(a) = (s32)R(t) >> 31;

	SET_ICR0(R(a));
}

static void ppc_srawix(u32 op){

	u32 sa = _SH;
	u32 a = RA;
	u32 t = RT;

	XER &= ~XER_CA;
	if(((s32)R(t) < 0) && R(t) & BITMASK_0(sa))
		XER |= XER_CA;

	R(a) = (s32)R(t) >> sa;

	SET_ICR0(R(a));
}

////////////////////////////////////////////////////////////////
// Load/Store

static void ppc_lbz(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)(u8)ppc_read_8(ea);
}

static void ppc_lha(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (s32)(s16)ppc_read_16(ea);
}

static void ppc_lhz(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)(u16)ppc_read_16(ea);
}

static void ppc_lwz(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)ppc_read_32(ea);
}

static void ppc_lbzu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)(u8)ppc_read_8(R(a));
}

static void ppc_lhau(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (s32)(s16)ppc_read_16(R(a));
}

static void ppc_lhzu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)(u16)ppc_read_16(R(a));
}

static void ppc_lwzu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)ppc_read_32(R(a));
}

static void ppc_lbzx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)(u8)ppc_read_8(ea);
}

static void ppc_lhax(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (s32)(s16)ppc_read_16(ea);
}

static void ppc_lhzx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)(u16)ppc_read_16(ea);
}

static void ppc_lwzx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)ppc_read_32(ea);
}

static void ppc_lbzux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)(u8)ppc_read_8(R(a));
}

static void ppc_lhaux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (s32)(s16)ppc_read_16(R(a));
}

static void ppc_lhzux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)(u16)ppc_read_16(R(a));
}

static void ppc_lwzux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)ppc_read_32(R(a));
}

////////////////////////////////////////////////////////////////

static void ppc_stb(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc_write_8(ea, R(t));
}

static void ppc_sth(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc_write_16(ea, R(t));
}

static void ppc_stw(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc_write_32(ea, R(t));
}

static void ppc_stbu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc_write_8(ea, R(t));
	R(a) = ea;
}

static void ppc_sthu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc_write_16(ea, R(t));
	R(a) = ea;
}

static void ppc_stwu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc_write_32(ea, R(t));
	R(a) = ea;
}

static void ppc_stbx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc_write_8(ea, R(t));
}

static void ppc_sthx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc_write_16(ea, R(t));
}

static void ppc_stwx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc_write_32(ea, R(t));
}

static void ppc_stbux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc_write_8(ea, R(t));
	R(a) = ea;
}

static void ppc_sthux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc_write_16(ea, R(t));
	R(a) = ea;
}

static void ppc_stwux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc_write_32(ea, R(t));
	R(a) = ea;
}

////////////////////////////////////////////////////////////////
// Load/Store Floating Point

static void ppc_lfs(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;
	u32 i;

	if(a)
		ea += R(a);

	i = ppc_read_32(ea);
	ppc.fpr[t].fd = (f64)(*((f32 *)&i));
}

static void ppc_lfsu(u32 op){

	u32 itemp;
	f32 ftemp;

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	itemp = ppc_read_32(ea);
	ftemp = *(f32 *)&itemp;

	ppc.fpr[t].fd = (f64)((f32)ftemp);

	R(a) = ea;
}

static void ppc_lfsux(u32 op){

	u32 itemp;
	f32 ftemp;

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	itemp = ppc_read_32(ea);
	ftemp = *(f32 *)&itemp;

	ppc.fpr[t].fd = (f64)((f32)ftemp);

	R(a) = ea;
}

static void ppc_lfsx(u32 op){

	u32 itemp;
	f32 ftemp;

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	itemp = ppc_read_32(ea);
	ftemp = *(f32 *)&itemp;

	ppc.fpr[t].fd = (f64)((f32)ftemp);
}

static void ppc_stfs(u32 op){

	f32 ftemp;

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ftemp = (f32)((f64)ppc.fpr[t].fd);

	ppc_write_32(ea, *(u32 *)&ftemp);
}

static void ppc_stfsu(u32 op){

	f32 ftemp;

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	ftemp = (f32)((f64)ppc.fpr[t].fd);

	ppc_write_32(ea, *(u32 *)&ftemp);

	R(a) = ea;
}

static void ppc_stfsux(u32 op){

	f32 ftemp;

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	ftemp = (f32)((f64)ppc.fpr[t].fd);

	ppc_write_32(ea, *(u32 *)&ftemp);

	R(a) = ea;
}

static void ppc_stfsx(u32 op){

	f32 ftemp;

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ftemp = (f32)((f64)ppc.fpr[t].fd);

	ppc_write_32(ea, *(u32 *)&ftemp);

}

////////////////////////////////////////////////////////////////

static void ppc_lfd(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ppc.fpr[t].id = ppc_read_64(ea);
}

static void ppc_lfdu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 d = RD;

	ea += R(a);

	ppc.fpr[d].id = ppc_read_64(ea);

	R(a) = ea;
}

static void ppc_lfdux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;

	ea += R(a);

	ppc.fpr[d].id = ppc_read_64(ea);

	R(a) = ea;
}

static void ppc_lfdx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;

	if(a)
		ea += R(a);

	ppc.fpr[d].id = ppc_read_64(ea);
}

static void ppc_stfd(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ppc_write_64(ea, ppc.fpr[t].id);
}

static void ppc_stfdu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	ppc_write_64(ea, ppc.fpr[t].id);

	R(a) = ea;
}

static void ppc_stfdux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	ppc_write_64(ea, ppc.fpr[t].id);

	R(a) = ea;
}

static void ppc_stfdx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ppc_write_64(ea, ppc.fpr[t].id);
}

////////////////////////////////////////////////////////////////

static void ppc_stfiwx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ppc_write_32(ea, ppc.fpr[t].iw);
}

////////////////////////////////////////////////////////////////
// Load/Store Reversed

static void ppc_lhbrx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;
	u32 t;

	if(a)
		ea += R(a);

	t = ppc_read_16(ea);
	R(d) =	((t >> 8) & 0x00FF) |
			((t << 8) & 0xFF00);
}

static void ppc_lwbrx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;
	u32 t;

	if(a)
		ea += R(a);

	t = ppc_read_32(ea);
	R(d) =	((t >> 24) & 0x000000FF) |
			((t >>  8) & 0x0000FF00) |
			((t <<  8) & 0x00FF0000) |
			((t << 24) & 0xFF000000);
}

static void ppc_sthbrx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;

	if(a)
		ea += R(a);

	ppc_write_16(ea, ((R(d) >> 8) & 0x00FF) |
				((R(d) << 8) & 0xFF00));
}

static void ppc_stwbrx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;

	if(a)
		ea += R(a);

	ppc_write_32(ea, ((R(d) >> 24) & 0x000000FF) |
				((R(d) >>  8) & 0x0000FF00) |
				((R(d) <<  8) & 0x00FF0000) |
				((R(d) << 24) & 0xFF000000));
}

////////////////////////////////////////////////////////////////
// Load/Store Multiple

static void ppc_lmw(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 d = RD;

	if(a >= d || a == 31){
		printf("ERROR: lmw\n");
		exit(1);
	}

	if(a) ea += R(a);

	while(d < 32){

		R(d) = ppc_read_32(ea);
		ea += 4;
		d++;
	}
}

static void ppc_stmw(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 d = RD;

	if(a) ea += R(a);

	while(d < 32){

		ppc_write_32(ea, R(d));
		ea += 4;
		d++;
	}
}

////////////////////////////////////////////////////////////////
// Load/Store String

static void ppc_lswi(u32 op){

	ppc_null(op);
}

static void ppc_lswx(u32 op){

	ppc_null(op);
}

static void ppc_stswi(u32 op){

	ppc_null(op);
}

static void ppc_stswx(u32 op){

	ppc_null(op);
}

////////////////////////////////////////////////////////////////
// System Control

static void ppc_lwarx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = ppc_read_32(ea);
	ppc.reserved = ea;
}

static void ppc_stwcx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);

	if(ea != ppc.reserved){

		/* not reserved anymore */

		CR(0) = (XER & XER_SO) ? 1 : 0; /* SO */

	}else{

		/* reserved */

		ppc_write_32(ea, R(t));
		CR(0) = 0x2 | (XER & XER_SO) ? 1 : 0; /* EQ | SO */
	}

	ppc.reserved = 0xFFFFFFFF;
}

////////////////////////////////////////////////////////////////
// Condition Register Manipulation

#define _BBM	((op >> 11) & 3)
#define _BBN	((op >> 13) & 7)
#define _BAM	((op >> 16) & 3)
#define _BAN	((op >> 18) & 7)
#define _BDM	((op >> 21) & 3)
#define _BDN	((op >> 23) & 7)

#define CROPD(op)	\
		{ \
		u32 bm = _BBM, bn = _BBN; \
		u32 am = _BAM, an = _BAN; \
		u32 dm = _BDM, dn = _BDN; \
		u32 i, j; \
		i = (CR(an) >> (3 - am)) & 1; \
		j = (CR(bn) >> (3 - bm)) & 1; \
		CR(dn) &= ~(8 >> dm); \
		CR(dn) |= (i op j) << (3 - dm); \
		}

#define CROPN(op) \
		{ \
		u32 bm = _BBM, bn = _BBN; \
		u32 am = _BAM, an = _BAN; \
		u32 dm = _BDM, dn = _BDN; \
		u32 i, j; \
		i = (CR(an) >> (3 - am)) & 1; \
		j = (CR(bn) >> (3 - bm)) & 1; \
		CR(dn) &= ~(8 >> dm); \
		CR(dn) |= (~(i op j) << (3 - dm)) & (8 >> dm); \
		}

static void ppc_crand(u32 op){	CROPD(&); }
static void ppc_crandc(u32 op){	CROPD(&~); }
static void ppc_cror(u32 op){	CROPD(|); }
static void ppc_crorc(u32 op){	CROPD(|~); }
static void ppc_crxor(u32 op){	CROPD(^); }
static void ppc_crnand(u32 op){	CROPN(&); }
static void ppc_crnor(u32 op){	CROPN(|); }
static void ppc_creqv(u32 op){	CROPN(^); }

static void ppc_mcrf(u32 op){	CR(RD>>2) = CR(RA>>2); }

////////////////////////////////////////////////////////////////

static void ppc_mcrxr(u32 op){

	ppc_null(op);
}

static void ppc_mfcr(u32 op){

	R(RD) =
		((u32)CR(0) << 28) |
		((u32)CR(1) << 24) |
		((u32)CR(2) << 20) |
		((u32)CR(3) << 16) |
		((u32)CR(4) << 12) |
		((u32)CR(5) <<  8) |
		((u32)CR(6) <<  4) |
		((u32)CR(7) <<  0);
}

static void ppc_mtcrf(u32 op){

	u32 f = _FXM;
	u32 d = RD;

	if(f & 0x80) CR(0) = (R(d) >> 28) & 15;
	if(f & 0x40) CR(1) = (R(d) >> 24) & 15;
	if(f & 0x20) CR(2) = (R(d) >> 20) & 15;
	if(f & 0x10) CR(3) = (R(d) >> 16) & 15;
	if(f & 0x08) CR(4) = (R(d) >> 12) & 15;
	if(f & 0x04) CR(5) = (R(d) >>  8) & 15;
	if(f & 0x02) CR(6) = (R(d) >>  4) & 15;
	if(f & 0x01) CR(7) = (R(d) >>  0) & 15;
}

static void ppc_mfmsr(u32 op){	R(RD) = ppc_get_msr(); }
static void ppc_mtmsr(u32 op){  ppc_set_msr(R(RD)); }

static void ppc_mfspr(u32 op){	R(RD) = ppc_get_spr(_SPRF); }
static void ppc_mtspr(u32 op){	ppc_set_spr(_SPRF, R(RD)); }

#if (PPC_MODEL == PPC_MODEL_4XX)

static void ppc_mfdcr(u32 op){	R(RD) = ppc_get_dcr(_DCRF); }
static void ppc_mtdcr(u32 op){	ppc_set_dcr(_DCRF, R(RD)); }

#endif

static void ppc_mftb(u32 op){

	u32 t = RT;
	u32 x = _SPRF;

	switch(x){

	case 268: R(t) = (u32)(ppc.tb); break;
	case 269: R(t) = (u32)(ppc.tb >> 32); break;

	default:
		printf("ERROR: invalid mftb\n");
		exit(1);
	}
}

static void ppc_mfsr(u32 op){

	u32 sr = (op >> 16) & 15;
	u32 t = RT;

	CHECK_SUPERVISOR();

	R(t) = ppc.sr[sr];
}

static void ppc_mfsrin(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_SUPERVISOR();

	R(t) = ppc.sr[R(b) >> 28];
}

static void ppc_mtsr(u32 op){

	u32 sr = (op >> 16) & 15;
	u32 t = RT;

	CHECK_SUPERVISOR();

	ppc.sr[sr] = R(t);
}

static void ppc_mtsrin(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_SUPERVISOR();

	ppc.sr[R(b) >> 28] = R(t);
}

static void ppc_wrtee(u32 op){

	ppc.msr &= ~0x00008000;
	ppc.msr |= R(RS) & 0x00008000;
}

static void ppc_wrteei(u32 op){

	ppc.msr &= ~0x00008000;
	ppc.msr |= op & 0x00008000;
}

////////////////////////////////////////////////////////////////
// Memory Management

static void ppc_sync(u32 op){

}

static void ppc_isync(u32 op){

}

static void ppc_eieio(u32 op){

}

static void ppc_icbi(u32 op){

	u32 ea = R(RB);
	u32 a = RA;

	if(a)
		ea += R(a);
}

static void ppc_dcbi(u32 op){

	u32 ea = R(RB);
	u32 a = RA;

	if(a)
		ea += R(a);
}

static void ppc_dcbf(u32 op){

	u32 ea = R(RB);
	u32 a = RA;

	if(a)
		ea += R(a);
}

static void ppc_dcba(u32 op){

	ppc_null(op);
}

static void ppc_dcbst(u32 op){

	ppc_null(op);
}

static void ppc_dcbt(u32 op){

	ppc_null(op);
}

static void ppc_dcbtst(u32 op){

	ppc_null(op);
}

static void ppc_dcbz(u32 op){

	ppc_null(op);
}

static void ppc_dccci(u32 op){

}

static void ppc_iccci(u32 op){

}

static void ppc_dcread(u32 op){

	ppc_null(op);
}

static void ppc_icread(u32 op){

	ppc_null(op);
}

static void ppc_icbt(u32 op){

	ppc_null(op);
}

////////////////////////////////////////////////////////////////

static void ppc_tlbie(u32 op){

}

static void ppc_tlbsync(u32 op){

}

static void ppc_tlbia(u32 op){

	ppc_null(op);
}

////////////////////////////////////////////////////////////////

static void ppc_eciwx(u32 op){

	ppc_null(op);
}

static void ppc_ecowx(u32 op){

	ppc_null(op);
}

/*
 * Floating-Point Instructions
 */

#define DOUBLE_SIGN		(0x8000000000000000)
#define DOUBLE_EXP		(0x7FF0000000000000)
#define DOUBLE_FRAC		(0x000FFFFFFFFFFFFF)
#define DOUBLE_ZERO		(0)

INLINE int is_nan_f64(f64 x){

	return(
		((*(u64 *)&(x) & DOUBLE_EXP) == DOUBLE_EXP) &&
		((*(u64 *)&(x) & DOUBLE_FRAC) != DOUBLE_ZERO)
	);
}

INLINE int is_qnan_f64(f64 x){

	return(
		((*(u64 *)&(x) & DOUBLE_EXP) == DOUBLE_EXP) &&
		((*(u64 *)&(x) & 0x0007FFFFFFFFFFF) == 0x000000000000000) &&
        ((*(u64 *)&(x) & 0x000800000000000) == 0x000800000000000)
    );
}

INLINE int is_snan_f64(f64 x){

	return(
		((*(u64 *)&(x) & DOUBLE_EXP) == DOUBLE_EXP) &&
        ((*(u64 *)&(x) & DOUBLE_FRAC) != DOUBLE_ZERO) &&
        ((*(u64 *)&(x) & 0x0008000000000000) == DOUBLE_ZERO)
	);
}

INLINE int is_infinity_f64(f64 x)
{
    return (
        ((*(u64 *)&(x) & DOUBLE_EXP) == DOUBLE_EXP) &&
        ((*(u64 *)&(x) & DOUBLE_FRAC) == DOUBLE_ZERO)
    );
}

INLINE int is_normalized_f64(f64 x)
{
    u64 exp;

    exp = ((*(u64 *) &(x)) & DOUBLE_EXP) >> 52;

    return (exp >= 1) && (exp <= 2046);
}

INLINE int is_denormalized_f64(f64 x)
{
    return (
        ((*(u64 *)&(x) & DOUBLE_EXP) == 0) &&
        ((*(u64 *)&(x) & DOUBLE_FRAC) != DOUBLE_ZERO)
    );
}

INLINE int sign_f64(f64 x)
{
    return ((*(u64 *)&(x) & DOUBLE_SIGN) != 0);
}

INLINE s32 round_f64_to_s32(f64 v){

	if(v >= 0)
		return((s32)(v + 0.5));
	else
		return(-(s32)(-v + 0.5));
}

INLINE s32 trunc_f64_to_s32(f64 v){

	if(v >= 0)
		return((s32)v);
	else
		return(-((s32)(-v)));
}

INLINE s32 ceil_f64_to_s32(f64 v){

	return((s32)ceil(v));
}

INLINE s32 floor_f64_to_s32(f64 v){

	return((s32)floor(v));
}

INLINE void set_fprf(f64 f)
{
    u32 fprf;

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

    ppc.fpscr &= ~0x0001F000;
    ppc.fpscr |= (fprf << 12);
}

static void ppc_fabsx(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

	ppc.fpr[t].id = ppc.fpr[b].id & ~DOUBLE_SIGN;

	SET_FCR1();
}

static void ppc_faddx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);

	ppc.fpr[t].fd = (f64)((f64)ppc.fpr[a].fd + (f64)ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_faddsx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);

    ppc.fpr[t].fd = (f32)((f64)ppc.fpr[a].fd + (f64)ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fcmpo(u32 op){

	CHECK_FPU_AVAILABLE();

    ppc_null(op);
}

static void ppc_fcmpu(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = (RT >> 2);
	u32 c;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);

    if(is_nan_f64(ppc.fpr[a].fd) ||
	   is_nan_f64(ppc.fpr[b].fd)){

		c = 1; /* OX */

		if(is_snan_f64(ppc.fpr[a].fd) ||
		   is_snan_f64(ppc.fpr[b].fd))
			ppc.fpscr |= 0x01000000; /* VXSNAN */

	}else if(ppc.fpr[a].fd < ppc.fpr[b].fd){

		c = 8; /* FX */

	}else if(ppc.fpr[a].fd > ppc.fpr[b].fd){

		c = 4; /* FEX */

	}else{

		c = 2; /* VX */
	}

	CR(t) = c;

	ppc.fpscr &= ~0x0001F000;
	ppc.fpscr |= (c << 12);
}

static void ppc_fctiwx(u32 op){

	u32 b = RB;
	u32 t = RT;

	// TODO: fix FPSCR flags FX,VXSNAN,VXCVI

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(ppc.fpr[b].fd);

    if(ppc.fpr[b].fd > (f64)((s32)0x7FFFFFFF)){

		ppc.fpr[t].iw = 0x7FFFFFFF;

		// FPSCR[FR] = 0
		// FPSCR[FI] = 1
		// FPSCR[XX] = 1

	}else
	if(ppc.fpr[b].fd < (f64)((s32)0x80000000)){

		ppc.fpr[t].iw = 0x80000000;

		// FPSCR[FR] = 1
		// FPSCR[FI] = 1
		// FPSCR[XX] = 1

	}else{

		switch(ppc.fpscr & 3){
		case 0: ppc.fpr[t].id = (s64)(s32)round_f64_to_s32(ppc.fpr[b].fd); break;
		case 1: ppc.fpr[t].id = (s64)(s32)trunc_f64_to_s32(ppc.fpr[b].fd); break;
		case 2: ppc.fpr[t].id = (s64)(s32)ceil_f64_to_s32(ppc.fpr[b].fd); break;
		case 3: ppc.fpr[t].id = (s64)(s32)floor_f64_to_s32(ppc.fpr[b].fd); break;
		}

		// FPSCR[FR] = t.iw > t.fd
		// FPSCR[FI] = t.iw == t.fd
		// FPSCR[XX] = ?
	}

	// FPSCR[FPRF] = undefined (leave it as is)

	SET_FCR1();
}

static void ppc_fctiwzx(u32 op){

	u32 b = RB;
	u32 t = RT;

	// TODO: fix FPSCR flags FX,VXSNAN,VXCVI

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(ppc.fpr[b].fd);

    if(ppc.fpr[b].fd > (f64)((s32)0x7FFFFFFF)){

		ppc.fpr[t].iw = 0x7FFFFFFF;

		// FPSCR[FR] = 0
		// FPSCR[FI] = 1
		// FPSCR[XX] = 1

	}else
	if(ppc.fpr[b].fd < (f64)((s32)0x80000000)){

		ppc.fpr[t].iw = 0x80000000;

		// FPSCR[FR] = 1
		// FPSCR[FI] = 1
		// FPSCR[XX] = 1

	}else{

		ppc.fpr[t].iw = trunc_f64_to_s32(ppc.fpr[b].fd);

		// FPSCR[FR] = t.iw > t.fd
		// FPSCR[FI] = t.iw == t.fd
		// FPSCR[XX] = ?
	}

	// FPSCR[FPRF] = undefined (leave it as is)

	SET_FCR1();
}

static void ppc_fdivx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);

    ppc.fpr[t].fd = (f64)((f64)ppc.fpr[a].fd / (f64)ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fdivsx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);

    ppc.fpr[t].fd = (f32)((f64)ppc.fpr[a].fd / (f64)ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fmaddx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);
    SET_VXSNAN_1(ppc.fpr[c].fd);

	ppc.fpr[t].fd = (f64)((ppc.fpr[a].fd * ppc.fpr[c].fd) + ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fmaddsx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);
    SET_VXSNAN_1(ppc.fpr[c].fd);

    ppc.fpr[t].fd = (f32)((ppc.fpr[a].fd * ppc.fpr[c].fd) + ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fmrx(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

	ppc.fpr[t].fd = ppc.fpr[b].fd;

	SET_FCR1();
}

static void ppc_fmsubx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);
    SET_VXSNAN_1(ppc.fpr[c].fd);

    ppc.fpr[t].fd = (f64)((ppc.fpr[a].fd * ppc.fpr[c].fd) - ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fmsubsx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);
    SET_VXSNAN_1(ppc.fpr[c].fd);

    ppc.fpr[t].fd = (f32)((ppc.fpr[a].fd * ppc.fpr[c].fd) - ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fnmaddx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);
    SET_VXSNAN_1(ppc.fpr[c].fd);

    ppc.fpr[t].fd = (f64)(-((ppc.fpr[a].fd * ppc.fpr[c].fd) + ppc.fpr[b].fd));

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fnmaddsx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);
    SET_VXSNAN_1(ppc.fpr[c].fd);

    ppc.fpr[t].fd = (f32)(-((ppc.fpr[a].fd * ppc.fpr[c].fd) + ppc.fpr[b].fd));

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fnmsubx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);
    SET_VXSNAN_1(ppc.fpr[c].fd);

    ppc.fpr[t].fd = (f64)(-((ppc.fpr[a].fd * ppc.fpr[c].fd) - ppc.fpr[b].fd));

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fnmsubsx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);
    SET_VXSNAN_1(ppc.fpr[c].fd);

    ppc.fpr[t].fd = (f32)(-((ppc.fpr[a].fd * ppc.fpr[c].fd) - ppc.fpr[b].fd));

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fmulx(u32 op){

	u32 c = RC;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[c].fd);

    ppc.fpr[t].fd = (f64)((f64)ppc.fpr[a].fd * (f64)ppc.fpr[c].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fmulsx(u32 op){

	u32 c = RC;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[c].fd);

	ppc.fpr[t].fd = (f32)((f64)ppc.fpr[a].fd * (f64)ppc.fpr[c].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fnabsx(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

	ppc.fpr[t].id = ppc.fpr[b].id | DOUBLE_SIGN;

	SET_FCR1();
}

static void ppc_fnegx(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

	ppc.fpr[t].id = ppc.fpr[b].id ^ DOUBLE_SIGN;

	SET_FCR1();
}

static void ppc_fresx(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(ppc.fpr[b].fd);

	ppc.fpr[t].fd = 1.0 / ppc.fpr[b].fd; /* ??? */

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_frspx(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(ppc.fpr[b].fd);

	ppc.fpr[t].fd = (f32)ppc.fpr[b].fd;

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_frsqrtex(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(ppc.fpr[b].fd);

	ppc.fpr[t].fd = 1.0 / sqrt(ppc.fpr[b].fd); /* ??? */

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fselx(u32 op){

	u32 c = RC;
	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

	ppc.fpr[t].fd = (ppc.fpr[a].fd >= 0.0) ? ppc.fpr[c].fd : ppc.fpr[b].fd;

	SET_FCR1();
}

static void ppc_fsqrtx(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(ppc.fpr[b].fd);

	ppc.fpr[t].fd = (f64)(sqrt(ppc.fpr[b].fd));

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fsqrtsx(u32 op){

	u32 b = RB;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN_1(ppc.fpr[b].fd);

	ppc.fpr[t].fd = (f32)(sqrt(ppc.fpr[b].fd));

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fsubx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);

	ppc.fpr[t].fd = (f64)((f64)ppc.fpr[a].fd - (f64)ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_fsubsx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	CHECK_FPU_AVAILABLE();

    SET_VXSNAN(ppc.fpr[a].fd, ppc.fpr[b].fd);

	ppc.fpr[t].fd = (f32)((f64)ppc.fpr[a].fd - (f64)ppc.fpr[b].fd);

    set_fprf(ppc.fpr[t].fd);
	SET_FCR1();
}

static void ppc_mcrfs(u32 op){
    u32 crfS, f;

    crfS = _CRFA;

    f = ppc.fpscr >> ((7 - crfS) * 4);  // get crfS field from FPSCR
    f &= 0xF;

//    LOG("model3.log", "MCRFS field %d\n", crfS);
//    message(0, "MCRFS field %d", crfS);

    switch (crfS)   // determine which exception bits to clear in FPSCR
    {
    case 0: // FX, OX
        ppc.fpscr &= ~0x90000000;
        break;
    case 1: // UX, ZX, XX, VXSNAN
        ppc.fpscr &= ~0x0F000000;
        break;
    case 2: // VXISI, VXIDI, VXZDZ, VXIMZ
        ppc.fpscr &= ~0x00F00000;
        break;
    case 3: // VXVC
        ppc.fpscr &= ~0x00080000;
        break;
    case 5: // VXSOFT, VXSQRT, VXCVI
        ppc.fpscr &= ~0x00000E00;
        break;
    default:
        break;
    }

    CR(_CRFD) = f;
}

static void ppc_mtfsb0x(u32 op){
    u32 crbD;

    crbD = (op >> 21) & 0x1F;

    if (crbD != 1 && crbD != 2) // these bits cannot be explicitly cleared
        ppc.fpscr &= ~(1 << (31 - crbD));

    SET_FCR1();
}

static void ppc_mtfsb1x(u32 op){
    u32 crbD;

    crbD = (op >> 21) & 0x1F;

    if (crbD != 1 && crbD != 2) // these bits cannot be explicitly cleared
        ppc.fpscr |= (1 << (31 - crbD));

    SET_FCR1();
}

static void ppc_mffsx(u32 op){

	u32 t = RT;

	ppc.fpr[t].iw = ppc.fpscr;

	SET_FCR1();
}

static void ppc_mtfsfx(u32 op){

	u32 b = RB;
	u32 f = _FM;

	f = ppc_field_xlat[_FM];

	ppc.fpscr &= (~f) | ~(_FEX | _VX);
	ppc.fpscr |= (ppc.fpr[b].iw) & ~(_FEX | _VX);

	// FEX, VX

	SET_FCR1();
}

static void ppc_mtfsfix(u32 op){

    u32 crfD = _CRFD;
    u32 imm = (op >> 12) & 0xF;

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
        ppc.fpscr &= 0x9FFFFFFF;
        ppc.fpscr |= (imm & 0x9FFFFFFF);
    }

    ppc.fpscr &= ~(0xF << crfD);    // clear field
    ppc.fpscr |= (imm << crfD);     // insert new data

	SET_FCR1();
}

/******************************************************************/
/* Branch, Syscall, Trap                                          */
/******************************************************************/

#define CHECK_BI(bi)		((CR(bi >> 2) & (8 >> (bi & 3))) != 0)

static u32 ppc_bo_0000(u32 bi){ return(--CTR != 0 && CHECK_BI(bi) == 0); }
static u32 ppc_bo_0001(u32 bi){ return(--CTR == 0 && CHECK_BI(bi) == 0); }
static u32 ppc_bo_001z(u32 bi){ return(CHECK_BI(bi) == 0); }
static u32 ppc_bo_0100(u32 bi){ return(--CTR != 0 && CHECK_BI(bi) != 0); }
static u32 ppc_bo_0101(u32 bi){ return(--CTR == 0 && CHECK_BI(bi) != 0); }
static u32 ppc_bo_011z(u32 bi){ return(CHECK_BI(bi) != 0); }
static u32 ppc_bo_1z00(u32 bi){ return(--CTR != 0); }
static u32 ppc_bo_1z01(u32 bi){ return(--CTR == 0); }
static u32 ppc_bo_1z1z(u32 bi){ return(1); }

static u32 (* ppc_bo[16])(u32 bi) =
{
	ppc_bo_0000, ppc_bo_0001, ppc_bo_001z, ppc_bo_001z,
	ppc_bo_0100, ppc_bo_0101, ppc_bo_011z, ppc_bo_011z,
	ppc_bo_1z00, ppc_bo_1z01, ppc_bo_1z1z, ppc_bo_1z1z,
	ppc_bo_1z00, ppc_bo_1z01, ppc_bo_1z1z, ppc_bo_1z1z,
};

static void ppc_update_pc(void)
{
	UINT i;

	if(ppc.cur_fetch.start <= ppc.pc && ppc.pc <= ppc.cur_fetch.end)
	{
		ppc._pc = (UINT32)ppc.cur_fetch.ptr + (UINT32)(ppc.pc - ppc.cur_fetch.start);
		return;
	}

	for(i = 0; ppc.fetch[i].ptr != NULL; i++)
	{
		if(ppc.fetch[i].start <= ppc.pc && ppc.pc <= ppc.fetch[i].end)
		{
			ppc.cur_fetch.start = ppc.fetch[i].start;
			ppc.cur_fetch.end = ppc.fetch[i].end;
			ppc.cur_fetch.ptr = ppc.fetch[i].ptr;

			ppc._pc = (UINT32)ppc.cur_fetch.ptr + (UINT32)(ppc.pc - ppc.cur_fetch.start);

			/*
			message(0, "region = [%08X,%08X],%08X --> pc = %08X,%08X -- %08X",
				ppc.cur_fetch.start, ppc.cur_fetch.end, ppc.cur_fetch.ptr,
				ppc.pc, ppc._pc, (ppc.pc - ppc.cur_fetch.start));
			*/

			return;
		}
	}

	error("ERROR: invalid PC %08X\n", ppc.pc);
}

static void ppc_bx(u32 op)
{
	UINT32 lr = ppc.pc;

	ppc.pc = (((s32)op << 6) >> 6) & ~3;

	if((op & _AA) == 0)
		ppc.pc += lr - 4;

	if(op & _LK)
		LR = lr;

	ppc_update_pc();
}

static void ppc_bcx(u32 op)
{
	UINT32 lr = ppc.pc;

	if(ppc_bo[_BO >> 1](_BI))
	{
		ppc.pc = SIMM & ~3;
		if((op & _AA) == 0)
			ppc.pc += lr - 4;
		ppc_update_pc();
	}

	if(op & _LK)
		LR = lr;
}

static void ppc_bcctrx(u32 op)
{
	UINT32 lr = ppc.pc;

	if(ppc_bo[_BO >> 1](_BI))
	{
		ppc.pc = CTR & ~3;
		ppc_update_pc();
	}

	if(op & _LK)
		LR = lr;
}

static void ppc_bclrx(u32 op)
{
	UINT32 lr = ppc.pc;

	if(ppc_bo[_BO >> 1](_BI))
	{
		ppc.pc = LR & ~3;
		ppc_update_pc();
	}

	if(op & _LK)
		LR = lr;
}

static void ppc_rfi(u32 op)
{
	ppc.pc = ppc.spr[SPR_SRR0];
	ppc_set_msr(ppc.spr[SPR_SRR1]);

	ppc_update_pc();
}

static void ppc_rfci(u32 op)
{
	ppc.pc = ppc.spr[SPR_SRR2];
	ppc_set_msr(ppc.spr[SPR_SRR3]);

	ppc_update_pc();
}

static void ppc_sc(u32 op)
{
	ppc_null(op);
}

static void ppc_tw(u32 op)
{
	ppc_null(op);
}

static void ppc_twi(u32 op)
{
	ppc_null(op);
}

/******************************************************************/
/* Opcode Dispatcher                                              */
/******************************************************************/

static void (* ppc_inst_19[0x400])(u32 op);
static void (* ppc_inst_31[0x400])(u32 op);
static void (* ppc_inst_59[0x400])(u32 op);
static void (* ppc_inst_63[0x400])(u32 op);

static void ppc_19(u32 op){ ppc_inst_19[_XO](op); }
static void ppc_31(u32 op){ ppc_inst_31[_XO](op); }
static void ppc_59(u32 op){ ppc_inst_59[_XO](op); }
static void ppc_63(u32 op){ ppc_inst_63[_XO](op); }

static void (* ppc_inst[64])(u32 op) =
{
	ppc_null,	ppc_null,	ppc_null,	ppc_twi,		// 00 ~ 03
	ppc_null,	ppc_null,	ppc_null,	ppc_mulli,		// 04 ~ 07
	ppc_subfic,	ppc_null,	ppc_cmpli,	ppc_cmpi,		// 08 ~ 0b
	ppc_addic,	ppc_addic_,	ppc_addi,	ppc_addis,		// 0c ~ 0f
	ppc_bcx,	ppc_sc,		ppc_bx,		ppc_19,			// 10 ~ 13
	ppc_rlwimix,ppc_rlwinmx,ppc_null,	ppc_rlwnmx,		// 14 ~ 17
	ppc_ori,	ppc_oris,	ppc_xori,	ppc_xoris,		// 18 ~ 1b
	ppc_andi_,	ppc_andis_,	ppc_null,	ppc_31,			// 1c ~ 1f
	ppc_lwz,	ppc_lwzu,	ppc_lbz,	ppc_lbzu,		// 20 ~ 23
	ppc_stw,	ppc_stwu,	ppc_stb,	ppc_stbu,		// 24 ~ 27
	ppc_lhz,	ppc_lhzu,	ppc_lha,	ppc_lhau,		// 28 ~ 2b
	ppc_sth,	ppc_sthu,	ppc_lmw,	ppc_stmw,		// 2c ~ 2f
	ppc_null,	ppc_null,	ppc_null,	ppc_null,		// 30 ~ 33
	ppc_null,	ppc_null,	ppc_null,	ppc_null,		// 34 ~ 37
	ppc_null,	ppc_null,	ppc_null,	ppc_null,		// 38 ~ 3b
	ppc_null,	ppc_null,	ppc_null,	ppc_null,		// 3c ~ 3f
};

#ifdef WATCH_PC
static void log_regs(void)
{
    LOG("model3.log", "R0 =0x%08X R7 =0x%08X R14=0x%08X R21=0x%08X R28=0x%08X\n", ppc_get_reg(PPC_REG_R0), ppc_get_reg(PPC_REG_R7), ppc_get_reg(PPC_REG_R14), ppc_get_reg(PPC_REG_R21), ppc_get_reg(PPC_REG_R28));
    LOG("model3.log", "R1 =0x%08X R8 =0x%08X R15=0x%08X R22=0x%08X R29=0x%08X\n", ppc_get_reg(PPC_REG_R1), ppc_get_reg(PPC_REG_R8), ppc_get_reg(PPC_REG_R15), ppc_get_reg(PPC_REG_R22), ppc_get_reg(PPC_REG_R29));
    LOG("model3.log", "R2 =0x%08X R9 =0x%08X R16=0x%08X R23=0x%08X R30=0x%08X\n", ppc_get_reg(PPC_REG_R2), ppc_get_reg(PPC_REG_R9), ppc_get_reg(PPC_REG_R16), ppc_get_reg(PPC_REG_R23), ppc_get_reg(PPC_REG_R30));
    LOG("model3.log", "R3 =0x%08X R10=0x%08X R17=0x%08X R24=0x%08X R31=0x%08X\n", ppc_get_reg(PPC_REG_R3), ppc_get_reg(PPC_REG_R10), ppc_get_reg(PPC_REG_R17), ppc_get_reg(PPC_REG_R24), ppc_get_reg(PPC_REG_R31));
    LOG("model3.log", "R4 =0x%08X R11=0x%08X R18=0x%08X R25=0x%08X LR =0x%08X\n", ppc_get_reg(PPC_REG_R4), ppc_get_reg(PPC_REG_R11), ppc_get_reg(PPC_REG_R18), ppc_get_reg(PPC_REG_R25), ppc_get_reg(PPC_REG_LR)); 
    LOG("model3.log", "R5 =0x%08X R12=0x%08X R19=0x%08X R26=0x%08X           \n", ppc_get_reg(PPC_REG_R5), ppc_get_reg(PPC_REG_R12), ppc_get_reg(PPC_REG_R19), ppc_get_reg(PPC_REG_R26));
    LOG("model3.log", "R6 =0x%08X R13=0x%08X R20=0x%08X R27=0x%08X           \n", ppc_get_reg(PPC_REG_R6), ppc_get_reg(PPC_REG_R13), ppc_get_reg(PPC_REG_R20), ppc_get_reg(PPC_REG_R27));
}
#endif

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

u32 ppc_run(u32 count)
{
	u32 op;

	ppc.count = count;

	while(ppc.count > 0)
	{
    	ppc.count--;

#ifdef WATCH_PC
        if (ppc.cia == WATCH_PC)
            log_regs();
#endif

		op = BSWAP32(*ppc._pc);

		ppc.pc += 4;
		ppc._pc++;

		ppc_inst[(op >> 26) & 0x3F](op);

		ppc_test_irq();

		if((ppc.count & 3) == 0)
		{
			ppc.tb++;
			if(ppc.tb >> 56)
				ppc.tb = 0;
            ppc.spr[SPR_DEC]--;
            if(ppc.spr[SPR_DEC] == 0xFFFFFFFF)
                ppc.dec_expired = 1;
		}
	}

	return(PPC_OKAY);
}

void ppc_set_irq_line(u32 state){

	ppc.irq_state = state;
}

u32 ppc_get_r(int n){ return(ppc.gpr[n]); }
void ppc_set_r(int n, u32 d){ ppc.gpr[n] = d; }

f64 ppc_get_f(int n){ return(ppc.fpr[n].fd); }
void ppc_set_f(int n, f64 d){ ppc.fpr[n].fd = d; }

u32 ppc_get_reg(int r){

	switch(r){

	case PPC_REG_R0: return(ppc.gpr[0]);
	case PPC_REG_R1: return(ppc.gpr[1]);
	case PPC_REG_R2: return(ppc.gpr[2]);
	case PPC_REG_R3: return(ppc.gpr[3]);
	case PPC_REG_R4: return(ppc.gpr[4]);
	case PPC_REG_R5: return(ppc.gpr[5]);
	case PPC_REG_R6: return(ppc.gpr[6]);
	case PPC_REG_R7: return(ppc.gpr[7]);
	case PPC_REG_R8: return(ppc.gpr[8]);
	case PPC_REG_R9: return(ppc.gpr[9]);
	case PPC_REG_R10: return(ppc.gpr[10]);
	case PPC_REG_R11: return(ppc.gpr[11]);
	case PPC_REG_R12: return(ppc.gpr[12]);
	case PPC_REG_R13: return(ppc.gpr[13]);
	case PPC_REG_R14: return(ppc.gpr[14]);
	case PPC_REG_R15: return(ppc.gpr[15]);
	case PPC_REG_R16: return(ppc.gpr[16]);
	case PPC_REG_R17: return(ppc.gpr[17]);
	case PPC_REG_R18: return(ppc.gpr[18]);
	case PPC_REG_R19: return(ppc.gpr[19]);
	case PPC_REG_R20: return(ppc.gpr[20]);
	case PPC_REG_R21: return(ppc.gpr[21]);
	case PPC_REG_R22: return(ppc.gpr[22]);
	case PPC_REG_R23: return(ppc.gpr[23]);
	case PPC_REG_R24: return(ppc.gpr[24]);
	case PPC_REG_R25: return(ppc.gpr[25]);
	case PPC_REG_R26: return(ppc.gpr[26]);
	case PPC_REG_R27: return(ppc.gpr[27]);
	case PPC_REG_R28: return(ppc.gpr[28]);
	case PPC_REG_R29: return(ppc.gpr[29]);
	case PPC_REG_R30: return(ppc.gpr[30]);
	case PPC_REG_R31: return(ppc.gpr[31]);

	case PPC_REG_PC: return(ppc.pc);

	case PPC_REG_CR:

		return((CR(0) << 28) |
			 (CR(1) << 24) |
			 (CR(2) << 20) |
			 (CR(3) << 16) |
			 (CR(4) << 12) |
			 (CR(5) <<  8) |
			 (CR(6) <<  4) |
			 (CR(7) <<  0));

	case PPC_REG_FPSCR: return(ppc.fpscr);
	case PPC_REG_XER: return(ppc.spr[SPR_XER]);
	case PPC_REG_LR: return(ppc.spr[SPR_LR]);
	case PPC_REG_CTR: return(ppc.spr[SPR_CTR]);
	case PPC_REG_MSR: return(ppc.msr);
	case PPC_REG_PVR: return(ppc.spr[SPR_PVR]);

	// 4XX

	// 6XX

	case PPC_REG_HID0: return(ppc.spr[SPR_HID0]);
	case PPC_REG_HID1: return(ppc.spr[SPR_HID1]);

	case PPC_REG_IBAT0L: return(ppc.spr[SPR_IBAT0L]);
	case PPC_REG_IBAT0H: return(ppc.spr[SPR_IBAT0U]);
	case PPC_REG_IBAT1L: return(ppc.spr[SPR_IBAT1L]);
	case PPC_REG_IBAT1H: return(ppc.spr[SPR_IBAT1U]);
	case PPC_REG_IBAT2L: return(ppc.spr[SPR_IBAT2L]);
	case PPC_REG_IBAT2H: return(ppc.spr[SPR_IBAT2U]);
	case PPC_REG_IBAT3L: return(ppc.spr[SPR_IBAT3L]);
	case PPC_REG_IBAT3H: return(ppc.spr[SPR_IBAT3U]);

	case PPC_REG_DBAT0L: return(ppc.spr[SPR_DBAT0L]);
	case PPC_REG_DBAT0H: return(ppc.spr[SPR_DBAT0U]);
	case PPC_REG_DBAT1L: return(ppc.spr[SPR_DBAT1L]);
	case PPC_REG_DBAT1H: return(ppc.spr[SPR_DBAT1U]);
	case PPC_REG_DBAT2L: return(ppc.spr[SPR_DBAT2L]);
	case PPC_REG_DBAT2H: return(ppc.spr[SPR_DBAT2U]);
	case PPC_REG_DBAT3L: return(ppc.spr[SPR_DBAT3L]);
	case PPC_REG_DBAT3H: return(ppc.spr[SPR_DBAT3U]);

	case PPC_REG_SPRG0: return(ppc.spr[SPR_SPRG0]);
	case PPC_REG_SPRG1: return(ppc.spr[SPR_SPRG1]);
	case PPC_REG_SPRG2: return(ppc.spr[SPR_SPRG2]);
	case PPC_REG_SPRG3: return(ppc.spr[SPR_SPRG3]);

	case PPC_REG_SRR0: return(ppc.spr[SPR_SRR0]);
	case PPC_REG_SRR1: return(ppc.spr[SPR_SRR1]);
	case PPC_REG_SRR2: return(ppc.spr[SPR_SRR2]);
	case PPC_REG_SRR3: return(ppc.spr[SPR_SRR3]);

	case PPC_REG_SR0: return(ppc.sr[0]);
	case PPC_REG_SR1: return(ppc.sr[1]);
	case PPC_REG_SR2: return(ppc.sr[2]);
	case PPC_REG_SR3: return(ppc.sr[3]);
	case PPC_REG_SR4: return(ppc.sr[4]);
	case PPC_REG_SR5: return(ppc.sr[5]);
	case PPC_REG_SR6: return(ppc.sr[6]);
	case PPC_REG_SR7: return(ppc.sr[7]);
	case PPC_REG_SR8: return(ppc.sr[8]);
	case PPC_REG_SR9: return(ppc.sr[9]);
	case PPC_REG_SR10: return(ppc.sr[10]);
	case PPC_REG_SR11: return(ppc.sr[11]);
	case PPC_REG_SR12: return(ppc.sr[12]);
	case PPC_REG_SR13: return(ppc.sr[13]);
	case PPC_REG_SR14: return(ppc.sr[14]);
	case PPC_REG_SR15: return(ppc.sr[15]);

	case PPC_REG_SDR1: return(ppc.spr[SPR_SDR1]);

	case PPC_REG_DSISR: return(0);
	case PPC_REG_DAR: return(0);

	default: return(0xFFFFFFFF);

	}
}

void ppc_set_reg(int r, u32 d){
  switch(r) {
  case PPC_REG_PC:
	ppc.pc = d;
	break;
  case PPC_REG_XER:
	ppc.spr[SPR_XER] = d;
	break;
  case PPC_REG_LR:
	ppc.spr[SPR_LR] = d;
	break;
  case PPC_REG_CR:
	CR(0) = (d >> 28) & 15;
	CR(1) = (d >> 24) & 15;
	CR(2) = (d >> 20) & 15;
	CR(3) = (d >> 16) & 15;
	CR(4) = (d >> 12) & 15;
	CR(5) = (d >>  8) & 15;
	CR(6) = (d >>  4) & 15;
	CR(7) = (d >>  0) & 15;
	break;
  default:
	printf("FIXME: ppc_set_reg(%d, %08x)\n", r, d);
	abort();
  }
}

int ppc_get_context(PPC_CONTEXT * dst)
{
	if(dst != NULL)
	{
		memcpy((void *)dst, (void *)&ppc, sizeof(PPC_CONTEXT));
		return(sizeof(PPC_CONTEXT));
	}
	return(-1);
}

int ppc_set_context(PPC_CONTEXT * src)
{
	if(src != NULL)
	{
		memcpy((void *)&ppc, (void *)src, sizeof(PPC_CONTEXT));
		return(sizeof(PPC_CONTEXT));
	}
	return(-1);
}

int ppc_reset(void)
{
	int i;

	if(ppc.fetch == NULL)
		error("ppc_reset() called without a previous call to ppc_set_fetch()!\n");

	for(i = 0; i < 32; i++)
		ppc.gpr[i] = 0;

	for(i = 0; i < 1024; i++)
		ppc.spr[i] = 0;

	for(i = 0; i < 8; i++)
		ppc.cr[i] = 0;

	ppc.reserved = 0xFFFFFFFF;
	ppc.irq_state = 0;
	ppc.tb = 0;

	#if (PPC_MODEL == PPC_MODEL_4XX)

	ppc.msr	= 0;

//	ppc.spr[SPR_PVR] = 0x00200011;	// 403GA
	ppc.spr[SPR_PVR] = 0x00201400;	// Konami Custom ?
	ppc.spr[SPR_DBSR] = 0x00000300;	// 0x100, 0x200

	for(i = 0; i < 256; i++)
		ppc.dcr[i] = 0;

	ppc.dcr[DCR_BR0]	= 0xFF193FFE;
	ppc.dcr[DCR_BR1]	= 0xFF013FFE;
	ppc.dcr[DCR_BR2]	= 0xFF013FFE;
	ppc.dcr[DCR_BR3]	= 0xFF013FFE;
	ppc.dcr[DCR_BR4]	= 0xFF013FFF;
	ppc.dcr[DCR_BR5]	= 0xFF013FFF;
	ppc.dcr[DCR_BR6]	= 0xFF013FFF;
	ppc.dcr[DCR_BR7]	= 0xFF013FFF;

	ppc.pc = 0xFFFFFFFC;

	#elif (PPC_MODEL == PPC_MODEL_6XX)

	ppc.dec_expired = 0;

	ppc.tgpr[0] = 0;
	ppc.tgpr[1] = 0;
	ppc.tgpr[2] = 0;
	ppc.tgpr[3] = 0;

	for(i = 0; i < 32; i++)
		ppc.fpr[i].fd = 1.0;

	for(i = 0; i < 16; i++)
		ppc.sr[i] = 0;

	ppc.msr = 0x40;
	ppc.fpscr = 0;

    ppc.spr[SPR_PVR] = pvr;
//    ppc.spr[SPR_PVR] = 0x00060104;      // 603e, Stretch, 1.4 - checked against by VS2V991
	ppc.spr[SPR_DEC] = 0xFFFFFFFF;

	ppc.pc = 0xFFF00100;

	#endif

	ppc.cur_fetch.start = 0;
	ppc.cur_fetch.end = 0;
	ppc.cur_fetch.ptr = NULL;

	ppc_update_pc();

    return PPC_OKAY;
}

void ppc_set_fetch(PPC_FETCH_REGION * fetch)
{
	ppc.fetch = fetch;
}

u32 ppc_read_byte(u32 a){ return(ppc_read_8(a)); }
u32 ppc_read_half(u32 a){ return(ppc_read_16(a)); }
u32 ppc_read_word(u32 a){ return(ppc_read_32(a)); }
u64 ppc_read_dword(u32 a){ return(ppc_read_64(a)); }

void ppc_write_byte(u32 a, u32 d){ ppc_write_8(a, d); }
void ppc_write_half(u32 a, u32 d){ ppc_write_16(a, d); }
void ppc_write_word(u32 a, u32 d){ ppc_write_32(a, d); }
void ppc_write_dword(u32 a, u64 d){ ppc_write_64(a, d); }

void ppc_set_irq_callback(u32 (* callback)(void)){

	ppc.irq_callback = callback;
}

void ppc_set_read_8_handler(void * handler){	ppc.read_8 = (u32 (*)(u32))handler; }
void ppc_set_read_16_handler(void * handler){	ppc.read_16 = (u32 (*)(u32))handler; }
void ppc_set_read_32_handler(void * handler){	ppc.read_32 = (u32 (*)(u32))handler; }
void ppc_set_read_64_handler(void * handler){	ppc.read_64 = (u64 (*)(u32))handler; }

#ifdef KHEPERIX_TEST
void ppc_set_read_op_handler(void * handler){	ppc.read_op = (u32 (*)(u32))handler; }
#endif

void ppc_set_write_8_handler(void * handler){	ppc.write_8 = (void (*)(u32,u32))handler; }
void ppc_set_write_16_handler(void * handler){	ppc.write_16 = (void (*)(u32,u32))handler; }
void ppc_set_write_32_handler(void * handler){	ppc.write_32 = (void (*)(u32,u32))handler; }
void ppc_set_write_64_handler(void * handler){	ppc.write_64 = (void (*)(u32,u64))handler; }

void ppc_set_pvr(u32 version)
{
    pvr = version;
}

int ppc_init(void * x){

	int i;

    pvr = 0x00060104;   // default to this version of the PPC603e

	x = NULL;

	// cleanup extended opcodes handlers

	for(i = 0; i < 1024; i++){

		ppc_inst_19[i] = ppc_null;
		ppc_inst_31[i] = ppc_null;
		ppc_inst_59[i] = ppc_null;
		ppc_inst_63[i] = ppc_null;
	}

	// install extended opcodes handlers

	ppc_inst_31[266]	= ppc_inst_31[266|(_OVE>>1)]	= ppc_addx;
	ppc_inst_31[10]		= ppc_inst_31[10|(_OVE>>1)]		= ppc_addcx;
	ppc_inst_31[138]	= ppc_inst_31[138|(_OVE>>1)]	= ppc_addex;
	ppc_inst_31[234]	= ppc_inst_31[234|(_OVE>>1)]	= ppc_addmex;
	ppc_inst_31[202]	= ppc_inst_31[202|(_OVE>>1)]	= ppc_addzex;
	ppc_inst_31[28]		= ppc_andx;
	ppc_inst_31[60]		= ppc_andcx;
	ppc_inst_19[528]	= ppc_bcctrx;
	ppc_inst_19[16]		= ppc_bclrx;
	ppc_inst_31[0]		= ppc_cmp;
	ppc_inst_31[32]		= ppc_cmpl;
	ppc_inst_31[26]		= ppc_cntlzwx;
	ppc_inst_19[257]	= ppc_crand;
	ppc_inst_19[129]	= ppc_crandc;
	ppc_inst_19[289]	= ppc_creqv;
	ppc_inst_19[225]	= ppc_crnand;
	ppc_inst_19[33]		= ppc_crnor;
	ppc_inst_19[449]	= ppc_cror;
	ppc_inst_19[417]	= ppc_crorc;
	ppc_inst_19[193]	= ppc_crxor;
	ppc_inst_31[86]		= ppc_dcbf;
	ppc_inst_31[470]	= ppc_dcbi;
	ppc_inst_31[54]		= ppc_dcbst;
	ppc_inst_31[278]	= ppc_dcbt;
	ppc_inst_31[246]	= ppc_dcbtst;
	ppc_inst_31[1014]	= ppc_dcbz;
	ppc_inst_31[491]	= ppc_inst_31[491|(_OVE>>1)]	= ppc_divwx;
	ppc_inst_31[459]	= ppc_inst_31[459|(_OVE>>1)]	= ppc_divwux;
	ppc_inst_31[854]	= ppc_eieio;
	ppc_inst_31[284]	= ppc_eqvx;
	ppc_inst_31[954]	= ppc_extsbx;
	ppc_inst_31[922]	= ppc_extshx;
	ppc_inst_31[982]	= ppc_icbi;
	ppc_inst_19[150]	= ppc_isync;
	ppc_inst_31[119]	= ppc_lbzux;
	ppc_inst_31[87]		= ppc_lbzx;
	ppc_inst_31[375]	= ppc_lhaux;
	ppc_inst_31[343]	= ppc_lhax;
	ppc_inst_31[790]	= ppc_lhbrx;
	ppc_inst_31[311]	= ppc_lhzux;
	ppc_inst_31[279]	= ppc_lhzx;
	ppc_inst_31[597]	= ppc_lswi;
	ppc_inst_31[533]	= ppc_lswx;
	ppc_inst_31[20]		= ppc_lwarx;
	ppc_inst_31[534]	= ppc_lwbrx;
	ppc_inst_31[55]		= ppc_lwzux;
	ppc_inst_31[23]		= ppc_lwzx;
	ppc_inst_19[0]		= ppc_mcrf;
	ppc_inst_31[512]	= ppc_mcrxr;
	ppc_inst_31[19]		= ppc_mfcr;
	ppc_inst_31[83]		= ppc_mfmsr;
	ppc_inst_31[339]	= ppc_mfspr;
	ppc_inst_31[144]	= ppc_mtcrf;
	ppc_inst_31[146]	= ppc_mtmsr;
	ppc_inst_31[467]	= ppc_mtspr;
	ppc_inst_31[75]		= ppc_mulhwx;
	ppc_inst_31[11]		= ppc_mulhwux;
    ppc_inst_31[235]    = ppc_inst_31[235|(_OVE>>1)]    = ppc_mullwx;
	ppc_inst_31[476]	= ppc_nandx;
    ppc_inst_31[104]    = ppc_inst_31[104|(_OVE>>1)]    = ppc_negx;
	ppc_inst_31[124]	= ppc_norx;
	ppc_inst_31[444]	= ppc_orx;
	ppc_inst_31[412]	= ppc_orcx;
	ppc_inst_19[51]		= ppc_rfci;
	ppc_inst_19[50]		= ppc_rfi;
	ppc_inst_31[24]		= ppc_slwx;
	ppc_inst_31[792]	= ppc_srawx;
    ppc_inst_31[824]    = ppc_srawix;                     
	ppc_inst_31[536]	= ppc_srwx;
	ppc_inst_31[247]	= ppc_stbux;
	ppc_inst_31[215]	= ppc_stbx;
	ppc_inst_31[918]	= ppc_sthbrx;
	ppc_inst_31[439]	= ppc_sthux;
	ppc_inst_31[407]	= ppc_sthx;
	ppc_inst_31[725]	= ppc_stswi;
	ppc_inst_31[661]	= ppc_stswx;
	ppc_inst_31[662]	= ppc_stwbrx;
	ppc_inst_31[150]	= ppc_stwcx;
	ppc_inst_31[183]	= ppc_stwux;
	ppc_inst_31[151]	= ppc_stwx;
    ppc_inst_31[40]     = ppc_inst_31[40|(_OVE>>1)]     = ppc_subfx;
    ppc_inst_31[8]      = ppc_inst_31[8|(_OVE>>1)]      = ppc_subfcx;
    ppc_inst_31[136]    = ppc_inst_31[136|(_OVE>>1)]    = ppc_subfex;
    ppc_inst_31[232]    = ppc_inst_31[232|(_OVE>>1)]    = ppc_subfmex;
    ppc_inst_31[200]    = ppc_inst_31[200|(_OVE>>1)]    = ppc_subfzex;
	ppc_inst_31[598]	= ppc_sync;
	ppc_inst_31[4]		= ppc_tw;
	ppc_inst_31[316]	= ppc_xorx;

	#if (PPC_MODEL == PPC_MODEL_4xx)

	ppc_inst_31[454] = ppc_dccci;
	ppc_inst_31[486] = ppc_dcread;
	ppc_inst_31[262] = ppc_icbt;
	ppc_inst_31[966] = ppc_iccci;
	ppc_inst_31[998] = ppc_icread;
	ppc_inst_31[323] = ppc_mfdcr;
	ppc_inst_31[451] = ppc_mtdcr;
	ppc_inst_31[131] = ppc_wrtee;
	ppc_inst_31[163] = ppc_wrteei;

	#elif (PPC_MODEL == PPC_MODEL_6XX)

	ppc_inst[48] = ppc_lfs;
	ppc_inst[49] = ppc_lfsu;
	ppc_inst[50] = ppc_lfd;
	ppc_inst[51] = ppc_lfdu;
	ppc_inst[52] = ppc_stfs;
	ppc_inst[53] = ppc_stfsu;
	ppc_inst[54] = ppc_stfd;
	ppc_inst[55] = ppc_stfdu;
	ppc_inst[59] = ppc_59;
	ppc_inst[63] = ppc_63;

	ppc_inst_31[631] = ppc_lfdux;
	ppc_inst_31[599] = ppc_lfdx;
	ppc_inst_31[567] = ppc_lfsux;
	ppc_inst_31[535] = ppc_lfsx;
	ppc_inst_31[595] = ppc_mfsr;
	ppc_inst_31[659] = ppc_mfsrin;
	ppc_inst_31[371] = ppc_mftb;
	ppc_inst_31[210] = ppc_mtsr;
	ppc_inst_31[242] = ppc_mtsrin;
	ppc_inst_31[758] = ppc_dcba;
	ppc_inst_31[759] = ppc_stfdux;
	ppc_inst_31[727] = ppc_stfdx;
	ppc_inst_31[983] = ppc_stfiwx;
	ppc_inst_31[695] = ppc_stfsux;
	ppc_inst_31[663] = ppc_stfsx;
	ppc_inst_31[370] = ppc_tlbia;
	ppc_inst_31[306] = ppc_tlbie;
	ppc_inst_31[566] = ppc_tlbsync;
	ppc_inst_31[310] = ppc_eciwx;
	ppc_inst_31[438] = ppc_ecowx;

	ppc_inst_63[264] = ppc_fabsx;
	ppc_inst_63[21] = ppc_faddx;
	ppc_inst_63[32] = ppc_fcmpo;
	ppc_inst_63[0] = ppc_fcmpu;
	ppc_inst_63[14] = ppc_fctiwx;
	ppc_inst_63[15] = ppc_fctiwzx;
	ppc_inst_63[18] = ppc_fdivx;
	ppc_inst_63[72] = ppc_fmrx;
	ppc_inst_63[136] = ppc_fnabsx;
	ppc_inst_63[40] = ppc_fnegx;
	ppc_inst_63[12] = ppc_frspx;
	ppc_inst_63[26] = ppc_frsqrtex;
	ppc_inst_63[22] = ppc_fsqrtx;
	ppc_inst_63[20] = ppc_fsubx;
	ppc_inst_63[583] = ppc_mffsx;
	ppc_inst_63[70] = ppc_mtfsb0x;
	ppc_inst_63[38] = ppc_mtfsb1x;
	ppc_inst_63[711] = ppc_mtfsfx;
	ppc_inst_63[134] = ppc_mtfsfix;
	ppc_inst_63[64] = ppc_mcrfs;

	ppc_inst_59[21] = ppc_faddsx;
	ppc_inst_59[18] = ppc_fdivsx;
	ppc_inst_59[24] = ppc_fresx;
	ppc_inst_59[22] = ppc_fsqrtsx;
	ppc_inst_59[20] = ppc_fsubsx;

	for(i = 0; i < 32; i += 1)
	{
		ppc_inst_63[i * 32 | 29] = ppc_fmaddx;
		ppc_inst_63[i * 32 | 28] = ppc_fmsubx;
		ppc_inst_63[i * 32 | 25] = ppc_fmulx;
		ppc_inst_63[i * 32 | 31] = ppc_fnmaddx;
		ppc_inst_63[i * 32 | 30] = ppc_fnmsubx;
		ppc_inst_63[i * 32 | 23] = ppc_fselx;

		ppc_inst_59[i * 32 | 29] = ppc_fmaddsx;
		ppc_inst_59[i * 32 | 28] = ppc_fmsubsx;
		ppc_inst_59[i * 32 | 25] = ppc_fmulsx;
		ppc_inst_59[i * 32 | 31] = ppc_fnmaddsx;
		ppc_inst_59[i * 32 | 30] = ppc_fnmsubsx;
	}

	#endif

	// create ppc_field_xlat

	for(i = 0; i <= 0xFF; i++){

		ppc_field_xlat[i] =
			((i & 0x80) ? 0xF0000000 : 0) |
			((i & 0x40) ? 0x0F000000 : 0) |
			((i & 0x20) ? 0x00F00000 : 0) |
			((i & 0x10) ? 0x000F0000 : 0) |
			((i & 0x08) ? 0x0000F000 : 0) |
			((i & 0x04) ? 0x00000F00 : 0) |
			((i & 0x02) ? 0x000000F0 : 0) |
			((i & 0x01) ? 0x0000000F : 0);
	}

	ppc.fetch = NULL;

    return PPC_OKAY;
}

void ppc_save_state(FILE *fp)
{
    fwrite(ppc.gpr, sizeof(u32), 32, fp);
    fwrite(ppc.fpr, sizeof(FPR), 32, fp);
    fwrite(&ppc.pc, sizeof(u32), 1, fp);
    fwrite(&ppc.msr, sizeof(u32), 1, fp);
    fwrite(&ppc.fpscr, sizeof(u32), 1, fp);
    fwrite(&ppc.count, sizeof(u32), 1, fp);
    fwrite(ppc.cr, sizeof(u8), 8, fp);
    fwrite(ppc.sr, sizeof(u32), 16, fp);
    fwrite(ppc.tgpr, sizeof(u32), 4, fp);
    fwrite(&ppc.dec_expired, sizeof(u32), 1, fp);
    fwrite(&ppc.reserved, sizeof(u32), 1, fp);
    fwrite(ppc.spr, sizeof(u32), 1024, fp);
    fwrite(&ppc.irq_state, sizeof(u32), 1, fp);
    fwrite(&ppc.tb, sizeof(u64), 1, fp);
}

void ppc_load_state(FILE *fp)
{
    fread(ppc.gpr, sizeof(u32), 32, fp);
    fread(ppc.fpr, sizeof(FPR), 32, fp);
    fread(&ppc.pc, sizeof(u32), 1, fp);
    fread(&ppc.msr, sizeof(u32), 1, fp);
    fread(&ppc.fpscr, sizeof(u32), 1, fp);
    fread(&ppc.count, sizeof(u32), 1, fp);
    fread(ppc.cr, sizeof(u8), 8, fp);
    fread(ppc.sr, sizeof(u32), 16, fp);
    fread(ppc.tgpr, sizeof(u32), 4, fp);
    fread(&ppc.dec_expired, sizeof(u32), 1, fp);
    fread(&ppc.reserved, sizeof(u32), 1, fp);
    fread(ppc.spr, sizeof(u32), 1024, fp);
    fread(&ppc.irq_state, sizeof(u32), 1, fp);
    fread(&ppc.tb, sizeof(u64), 1, fp);

	ppc.cur_fetch.start = 0;
	ppc.cur_fetch.end = 0;
	ppc.cur_fetch.ptr = NULL;

	ppc_update_pc();
}
