#include "model3.h"
///////////////////////////////////////////////////////////////
// Portable PowerPC Emulator

// TODO:
//
// - fix 6xx set_spr and get_spr to handle read-only and write-only registers correctly
// - change tb base to use a timestamp and arithmetic in get_spr/set_spr TBL,TBU
// - add simple cycle count & change time base / decrementer method
// - fix shift amount and carry in shift instructions
// - fix carry and overflow in add, subtract instructions (especially extended)
//
// - increase opcode tables size to handle separate OE,RC,A,L instruction flavours
// - fix lwarx and stwcx (lw/sw on same location reset "reserved"?)
// - fix FP flags in FP instructions
// - add FP result informations to FPSCR
// - check BITMASK_0
// - add missing FP instructions
// - check FP load/store instructions
// - add string load/store instructions
// - major cleanup

#include "ppc.h"

////////////////////////////////////////////////////////////////
// Context

ppc_t ppc;

static u32 ppc_field_xlat[256];

///////////////////////////////////////////////////////////////
// Short Mnemonics

#define XER_SO	0x80000000
#define XER_OV	0x40000000
#define XER_CA	0x20000000

#define CIA		ppc.cia
#define NIA		ppc.nia
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

////////////////////////////////////////////////////////////////

#define BIT(n)				((u32)0x80000000 >> n)

#define BITMASK_0(n)		((u32)(((u64)1 << (n + 1)) - 1))
#define BITMASK(n,m)		(BITMASK(n) & ~BITMASK(m))

/* SET_XXX_Y : */

#define SET_ADD_C(a,b)		if(op & _OE){ _SET_ADD_C(a,b); }

#define SET_SUB_C(a,b)		if(op & _OE){ _SET_SUB_C(a,b); }

/* CMPS, CMPU : signed and unsigned integer comparisons */

#define CMPS(a,b)			(((s32)(a) < (s32)(b)) ? _LT : (((s32)(a) > (s32)(b)) ? _GT : _EQ))
#define CMPU(a,b)			(((u32)(a) < (u32)(b)) ? _LT : (((u32)(a) > (u32)(b)) ? _GT : _EQ))

/* SET_ICR0() : set result flags into CR#0 */

#define _SET_ICR0(r)		{ CR(0) = CMPS(r,0); if(XER & XER_SO) CR(r) |= _SO; }
#define SET_ICR0(r)			if(op & _RC){ _SET_ICR0(r); }

/* SET_FCR1() : copies FPSCR MSBs into CR#1 */

#define _SET_FCR1()			{ CR(1) = (ppc.fpscr >> 28) & 15; }
#define SET_FCR1()			if(op & _RC){ _SET_FCR1(); }

/* SET_VXSNAN() : sets VXSNAN and updates FX */

#define SET_VXSNAN(a, b)    if (is_snan_f64(a) || is_snan_f64(b)) ppc.fpscr |= 0x80000000

/* SET_VXSNAN_1() : sets VXSNAN and updates FX (for use with 1 number only) */

#define SET_VXSNAN_1(c)     if (is_snan_f64(c)) ppc.fpscr |= 0x80000000

/* */

#define CHECK_SUPERVISOR()	\
	if((ppc.msr & 0x4000) != 0){	\
		printf("ERROR: %08X : supervisor instruction executed in user mode\n"); \
		exit(1); \
	}

/* */

#define CHECK_FPU_AVAILABLE()	\
	if((ppc.msr & 0x2000) == 0){ \
		printf("ERROR: %08X : fp unavailable\n"); \
		exit(1); \
	}

////////////////////////////////////////////////////////////////
// Serial Port Unit

// 0x40000000	SPLS:		RFEPLTt.	RBR,FE,OE,PE,LB,TBR,TSR	(10000110 = 0x86)
// 0x40000002	SPHS:		DC......	DIS,CS
// 0x40000004	BRDH:		....DDDD
// 0x40000005	BRDL:		DDDDDDDD
// 0x40000006	SPCTL:		LLDRdPpS	LM,DTRT,RTS,DB,PE,PTY,SB
// 0x40000008	SPTC:		EDDTeStP	ET,DME,TIE,EIE,SPE,TB,PGM
// 0x40000007	SPRC:		EDDeP...	ER,DME,EIE,PME
// 0x40000009/R	SPRB:		xxxxxxxx	data
// 0x40000009/W	SPTB:		xxxxxxxx	data

static const char * spu_id[16] = {
	"SPLS",		"-",		"SPHS",		"-",
	"BRDH",		"BRDL",		"SPCTL",	"SPRC",
	"SPTC",		"SPXB",		"-",		"-",
	"-",		"-",		"-",		"-",
};

INLINE void spu_wb(u32 a, u8 d){

	switch(a & 15){

	case 0: ppc.spu.spls &= ~(d & 0xF6); return;
	case 2: ppc.spu.sphs &= ~(d & 0xC0); return;
	case 4: ppc.spu.brdh = d; return;
	case 5: ppc.spu.brdl = d; return;
	case 6: ppc.spu.spctl = d; return;

	case 7:
	ppc.spu.sprc = d;
		return;

	case 8: ppc.spu.sptc = d; return;
	case 9: ppc.spu.sptb = d; return;

	}

	printf("%08X : wb %08X = %02X\n", CIA, a, d);
	exit(1);
}

INLINE u8 spu_rb(u32 a){

	switch(a & 15){

	case 0: return(ppc.spu.spls);
	case 2: return(ppc.spu.sphs);
	case 4: return(ppc.spu.brdh);
	case 5: return(ppc.spu.brdl);
	case 6: return(ppc.spu.spctl);
	case 7: return(ppc.spu.sprc);
	case 8: return(ppc.spu.sptc);
	case 9: return(ppc.spu.sprb);

	}

	printf("%08X : rb %08X\n", CIA, a);
	exit(1);
}

////////////////////////////////////////////////////////////////
// DMA Unit

#define DMA_CR(ch)	ppc.dcr[0xC0 + (ch << 3) + 0]
#define DMA_CT(ch)	ppc.dcr[0xC0 + (ch << 3) + 1]
#define DMA_DA(ch)	ppc.dcr[0xC0 + (ch << 3) + 2]
#define DMA_SA(ch)	ppc.dcr[0xC0 + (ch << 3) + 3]
#define DMA_CC(ch)	ppc.dcr[0xC0 + (ch << 3) + 4]
#define DMA_SR		ppc.dcr[0xE0]

static void ppc_dma(u32 ch){

	if(DMA_CR(ch) & 0x000000F0){

		printf("ERROR: PPC chained DMA %u\n", ch);
		exit(1);
	}

	switch((DMA_CR(ch) >> 26) & 3){

	case 0: // 1 byte

		// internal transfer (prolly *SPU*)

		while(DMA_CT(ch)){

			//d = ppc_r_b(DMA_SA(ch));
			if(DMA_CR(ch) & 0x01000000) DMA_SA(ch)++;
			//ppc_w_b(DMA_DA(ch), d);
			if(DMA_CR(ch) & 0x02000000) DMA_DA(ch)++;
			DMA_CT(ch)--;
		}

		break;

	case 1: // 2 bytes
		printf("%08X : DMA%u %08X -> %08X * %08X (2-bytes)\n", CIA, ch, DMA_SA(ch), DMA_DA(ch), DMA_CT(ch));
		exit(1);

	case 2: // 4 bytes
		printf("%08X : DMA%u %08X -> %08X * %08X (4-bytes)\n", CIA, ch, DMA_SA(ch), DMA_DA(ch), DMA_CT(ch));
		exit(1);

	default: // 16 bytes
		printf("%08X : DMA%u %08X -> %08X * %08X (16-bytes)\n", CIA, ch, DMA_SA(ch), DMA_DA(ch), DMA_CT(ch));
		exit(1);
	}

	if(DMA_CR(ch) & 0x40000000){ // DIE

		ppc.dcr[DCR_EXISR] |= (0x00800000 >> ch);
		ppc.irq_state = 1;
	}
}

////////////////////////////////////////////////////////////////
// Memory Access : 4XX

u32 ppc_4xx_fetch(u32 a){

	return(ppc.rd(a & 0x0FFFFFFF));
}

u32 ppc_4xx_irb(u32 a){

	if((a >> 28) == 4){

		return(spu_rb(a));
	}

	return(ppc.rb(a & 0x0FFFFFFF));
}

u32 ppc_4xx_irw(u32 a){

	if(a & 1){

		printf("PPC : %08X rw %08X\n", CIA, a);
		exit(1);
	}

	return(ppc.rw(a & 0x0FFFFFFE));
}

u32 ppc_4xx_ird(u32 a){

	if(a & 3){

		printf("PPC : %08X rd %08X\n", CIA, a);
		exit(1);
	}

	return(ppc.rd(a & 0x0FFFFFFC));
}

void ppc_4xx_iwb(u32 a, u32 d){

	if((a >> 28) == 4){

		spu_wb(a,d);
		return;
	}

	ppc.wb(a & 0x0FFFFFFF, d);
}

void ppc_4xx_iww(u32 a, u32 d){

	if(a & 1){

		printf("PPC : %08X ww %08X = %08X\n", CIA, a, d);
		exit(1);
	}

	ppc.ww(a & 0x0FFFFFFE, d);
}

void ppc_4xx_iwd(u32 a, u32 d){

	if(a & 3){

		printf("PPC : %08X wd %08X = %08X\n", CIA, a, d);
		exit(1);
	}

	ppc.wd(a & 0x0FFFFFFC, d);
}

////////////////////////////////////////////////////////////////
// Memory Access : 6XX

u32 ppc_6xx_fetch(u32 a){

	if(ppc.msr & 0x20){

	}

	if(a & 3){

		printf("ERROR: unaligned fetch %08X\n", a);
		exit(1);
	}

	return(ppc.rd(a));
}

u32 ppc_6xx_irb(u32 a){

	if(ppc.msr & 0x10){

		/* ... */
	}

	return(ppc.rb(a));
}

u32 ppc_6xx_irw(u32 a){

	if(ppc.msr & 0x10){

		/* ... */
	}

	return(ppc.rw(a));
}

u32 ppc_6xx_ird(u32 a){

	if(ppc.msr & 0x10){

		/* ... */
	}

	return(ppc.rd(a));
}

u64 ppc_6xx_irq(u32 a){

	if(ppc.msr & 0x10){

		/* ... */
	}

	return(ppc.rq(a));
}

void ppc_6xx_iwb(u32 a, u32 d){

	if(ppc.msr & 0x10){

		/* ... */
	}

	ppc.wb(a, d);
}

void ppc_6xx_iww(u32 a, u32 d){

	if(ppc.msr & 0x10){

		/* ... */
	}

	ppc.ww(a, d);
}

void ppc_6xx_iwd(u32 a, u32 d){

	if(ppc.msr & 0x10){

		/* ... */
	}

	ppc.wd(a, d);
}

void ppc_6xx_iwq(u32 a, u64 d){

	if(ppc.msr & 0x10){

		/* ... */
	}

	ppc.wq(a, d);
}

////////////////////////////////////////////////////////////////
// SPR Access : 4XX

static void ppc_4xx_set_spr(u32 n, u32 d){

	switch(n){

	case SPR_PVR:
		return;

	case SPR_PIT:
		if(d){
			printf("PIT = %i\n", d);
			exit(1);
		}
		ppc.spr[n] = d;
		ppc.pit_reload = d;
		break;

	case SPR_TSR:
		ppc.spr[n] &= ~d; // 1 clears, 0 does nothing
		return;
	}

	ppc.spr[n] = d;
}

static u32 ppc_4xx_get_spr(u32 n){

	if(n == SPR_TBLO)
		return((u32)ppc.tb);
	else
	if(n == SPR_TBHI)
		return((u32)(ppc.tb >> 32));

	return(ppc.spr[n]);
}

////////////////////////////////////////////////////////////////
// SPR Access : 6XX

static void ppc_6xx_set_spr(u32 n, u32 d){

	switch(n){

	case SPR_DEC:

		if((d & 0x80000000) && !(ppc.spr[SPR_DEC] & 0x80000000)){

			/* trigger interrupt */

			printf("ERROR: set_spr to DEC triggers IRQ\n");
			exit(1);
		}

		ppc.spr[SPR_DEC] = d;
		break;

	case SPR_PVR:
		return;

	case SPR_PIT:
		if(d){
			printf("PIT = %i\n", d);
			exit(1);
		}
		ppc.spr[n] = d;
		ppc.pit_reload = d;
		return;

	case SPR_TSR:
		ppc.spr[n] &= ~d; // 1 clears, 0 does nothing
		return;

	case SPR_TBL_W:
	case SPR_TBL_R: // special 603e case
		ppc.tb &= 0xFFFFFFFF00000000;
		ppc.tb |= (u64)(d);
		return;

	case SPR_TBU_R:
	case SPR_TBU_W: // special 603e case
		printf("ERROR: set_spr - TBU_W = %08X\n", d);
		exit(1);

	}

	ppc.spr[n] = d;
}

static u32 ppc_6xx_get_spr(u32 n){

	switch(n){

	case SPR_TBL_R:
		printf("ERROR: get_spr - TBL_R\n");
		exit(1);

	case SPR_TBU_R:
		printf("ERROR: get_spr - TBU_R\n");
		exit(1);

	case SPR_TBL_W:
		printf("ERROR: invalid get_spr - TBL_W\n");
		exit(1);

	case SPR_TBU_W:
		printf("ERROR: invalid get_spr - TBU_W\n");
		exit(1);
	}

	return(ppc.spr[n]);
}

////////////////////////////////////////////////////////////////
// Machine Status Register (4xx)

void ppc_4xx_set_msr(u32 d){

	ppc.msr = d;

	if(d & 0x00080000){ // WE

		printf("PPC : waiting ...\n");
		exit(1);
	}
}

u32 ppc_4xx_get_msr(void){

	return(ppc.msr);
}

////////////////////////////////////////////////////////////////
// Machine Status Register (6xx)

void ppc_6xx_set_msr(u32 d){

	if((ppc.msr ^ d) & 0x00020000){

		// TGPR

		u32 temp[4];

		temp[0] = ppc.gpr[0];
		temp[1] = ppc.gpr[1];
		temp[2] = ppc.gpr[2];
		temp[3] = ppc.gpr[3];

		ppc.gpr[0] = ppc.tgpr[0];
		ppc.gpr[1] = ppc.tgpr[1];
		ppc.gpr[2] = ppc.tgpr[2];
		ppc.gpr[3] = ppc.tgpr[3];

		ppc.tgpr[0] = temp[0];
		ppc.tgpr[1] = temp[1];
		ppc.tgpr[2] = temp[2];
		ppc.tgpr[3] = temp[3];
	}

	ppc.msr = d;

	if(d & 0x00080000){ // WE

		printf("PPC : waiting ...\n");
		exit(1);
	}
}

u32 ppc_6xx_get_msr(void){

	return(ppc.msr);
}

////////////////////////////////////////////////////////////////
// DCR Access : 4XX Only

static void ppc_set_dcr(u32 n, u32 d){

	switch(n){

	case DCR_BEAR: ppc.dcr[n] = d; break;
	case DCR_BESR: ppc.dcr[n] = d; break;

	case DCR_DMACC0:
	case DCR_DMACC1:
	case DCR_DMACC2:
	case DCR_DMACC3:
		if(d){
			printf("chained dma\n");
			exit(1);
		}
		break;

	case DCR_DMACR0:
		ppc.dcr[n] = d;
		if(d & 0x80000000)
			ppc_dma(0);
		return;

	case DCR_DMACR1:
		ppc.dcr[n] = d;
		if(d & 0x80000000)
			ppc_dma(1);
		return;

	case DCR_DMACR2:
		ppc.dcr[n] = d;
		if(d & 0x80000000)
			ppc_dma(2);
		return;

	case DCR_DMACR3:
		ppc.dcr[n] = d;
		if(d & 0x80000000)
			ppc_dma(3);
		return;

	case DCR_DMASR:
		ppc.dcr[n] &= ~d;
		return;

	case DCR_EXIER:
		ppc.dcr[n] = d;
		break;

	case DCR_EXISR:
		ppc.dcr[n] = d;
		break;

	case DCR_IOCR:
		ppc.dcr[n] = d;
		break;
	}

	ppc.dcr[n] = d;
}

static u32 ppc_get_dcr(u32 n){

	return(ppc.dcr[n]);
}

////////////////////////////////////////////////////////////////
// Unhandled

static void ppc_null(u32 op){
    extern int  DisassemblePowerPC(unsigned, unsigned, char *, char *, int);
    char    string[256];
    char    mnem[16], oprs[48];

    DisassemblePowerPC(op, CIA, mnem, oprs, 1);

    printf("ERROR: %08X: unhandled opcode %08X: %s\t%s\n", CIA, op, mnem, oprs);
//    printf("ERROR: %08X: unhandled opcode %08X (OP=%X XO=%X OE_XO=%X FP_XO=%X)\n%s\n",
//    CIA, op, _OP, _XO&0x3FF, _XO&0x1FF, _XO&0x1F, ppc_disasm(CIA, op, string));
	exit(1);
}

////////////////////////////////////////////////////////////////
// Arithmetical

// Carry

#define ADD_C(r,a,b)		((u32)(r) < (u32)(a))
#define SUB_C(r,a,b)		(!((u32)(a) < (u32)(b)))

// Overflow 

#define ADD_V(r,a,b)		((~((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)
#define SUB_V(r,a,b)		(( ((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)

#define SET_ADD_V(r,a,b)	if(op & _OE){ if(ADD_V(r,a,b)){ XER |= XER_SO | XER_OV; }else{ XER &= ~XER_OV; } }
#define SET_SUB_V(r,a,b)	if(op & _OE){ if(SUB_V(r,a,b)){ XER |= XER_SO | XER_OV; }else{ XER &= ~XER_OV; } }

static void ppc_addx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	R(t) = R(a) + R(b);

	SET_ADD_V(R(t), R(a), R(b));
	SET_ICR0(R(t));
}

static void ppc_addcx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	R(t) = R(a) + R(b);

	if(R(t) < R(a))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	SET_ADD_V(R(t), R(a), R(b));
	SET_ICR0(R(t));
}

static void ppc_addex(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;
	u32 c = (XER >> 29) & 1;

	R(t) = R(a) + (R(b) + c);

	if(R(t) < R(a))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	SET_ADD_V(R(t), R(a), R(b));
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
	u32 a = RA;
	u32 t = RT;

	R(t) = R(a) + i;

	if(R(t) < R(a))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;
}

static void ppc_addic_(u32 op){

	u32 i = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(t) = R(a) + i;

	if(R(t) < R(a))
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

	u32 a = RA;
	u32 d = RT;
	u32 c = (XER >> 29) & 1;

	R(d) = R(a) + (c - 1);

	if(R(d) < R(a))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	SET_ADD_V(R(d), R(a), (c - 1));
	SET_ICR0(R(d));
}

static void ppc_addzex(u32 op){

	u32 a = RA;
	u32 d = RT;
	u32 c = (XER >> 29) & 1;

	R(d) = R(a) + c;

	if(R(d) < R(a))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	SET_ADD_V(R(d), R(a), c);
	SET_ICR0(R(d));
}

static void ppc_subfx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = RT;

	R(d) = R(b) - R(a);

	SET_SUB_V(R(d), R(b), R(a));
	SET_ICR0(R(d));
}

static void ppc_subfcx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	R(t) = R(b) - R(a);

	if(SUB_C(R(t), R(b), R(a)))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	SET_SUB_V(R(t), R(b), R(a));
	SET_ICR0(R(t));
}

static void ppc_subfex(u32 op){

	// fixme

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;
	u32 c = (XER >> 29) & 1;

//	R(t) = R(b) + ~R(a) + c; // R(b) - R(a) + (c - 1)
	R(t) = R(b) - (R(a) + (c ^ 1));

	if(SUB_C(R(t), R(b), (R(a) + (c - 1))))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	SET_SUB_V(R(t), R(b), (R(a) + (c - 1)));
	SET_ICR0(R(t));
}

static void ppc_subfic(u32 op){

	u32 i = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(t) = i - R(a);

	if(SUB_C(R(t), i, R(a)))
		XER |= XER_CA;
	else
		XER &= ~XER_CA;
}

static void ppc_subfmex(u32 op){

	// fixme

	u32 a = RA;
	u32 t = RT;
	u32 c = (XER >> 29) & 1;

	R(t) = ~R(a) + c - 1;

	if(R(t) == 0 && c == 0)
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	if(op & _OE){
		if(c == 0 && (R(t) == 0x7FFFFFFF || R(t) == 0xFFFFFFFF))
			XER |= (XER_OV | XER_SO);
		else
			XER &= ~XER_OV;
	}

	SET_ICR0(R(t));
}

static void ppc_subfzex(u32 op){

	// fixme

	u32 a = RA;
	u32 t = RT;
	u32 c = (XER >> 29) & 1;

	R(t) = ~R(a) + c;

	if(R(t) == 0 && c)
		XER |= XER_CA;
	else
		XER &= ~XER_CA;

	if(op & _OE){
		if(c != 0 && (R(t) == 0 || R(t) == 0x80000000))
			XER |= (XER_OV | XER_SO);
		else
			XER &= ~XER_OV;
	}

	SET_ICR0(R(t));
}

char * ppc_stat_string(u32 x)
{
	return(NULL);
}

static void ppc_negx(u32 op){

	u32 a = RA;
	u32 t = RT;

	R(t) = -R(a);

	if(op & _OE){
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
	s64 r;

	r = (s64)(s32)R(a) * (s64)(s32)R(b);
	R(d) = (u32)r;

	if(op & _OE){
		XER &= ~XER_OV;
		if( (u64)r > 0xFFFFFFFF)
			XER |= (XER_OV | XER_SO);
	}

	SET_ICR0(R(d));
}

static void ppc_divwx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = RD;

	if((R(b) == 0) ||
	   (R(b) == 0xFFFFFFFF && R(a) == 0x80000000)){

		R(d) = 0;

		if(op & _OE) XER |= (XER_SO | XER_OV);
		/*if(op & _RC) CR(0) = 0x0F;*/

		return;
	}

	R(d) = (s32)R(a) / (s32)R(b);

	if(op & _OE) XER &= ~XER_OV;
	SET_ICR0(R(d));
}

static void ppc_divwux(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 d = RD;

	if(R(b) == 0){

		R(d) = 0;

		if(op & _OE) XER |= (XER_SO | XER_OV);
		/*if(op & _RC) CR(0) = 0x0F;*/

		return;
	}

	R(d) = (u32)R(a) / (u32)R(b);

	if(op & _OE) XER &= ~XER_OV;
	SET_ICR0(R(d));
}

////////////////////////////////////////////////////////////////
// Logical

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

	#if 1

	u32 m = 0x80000000;
	u32 n = 0;

	while(n < 32){
		if(R(t) & m) break;
		m >>= 1;
		n++;
	}

	R(a) = n;

	SET_ICR0(R(a));

	#else

	u32 n[3];

	// just an idea, bad implementation though

	static const u32 ppc_onecnt_table[256] = {
		0,	1,	2,	2,	3,	3,	3,	3,
		4,	4,	4,	4,	4,	4,	4,	4,
		5,	5,	5,	5,	5,	5,	5,	5,
		5,	5,	5,	5,	5,	5,	5,	5,
		6,	6,	6,	6,	6,	6,	6,	6,
		6,	6,	6,	6,	6,	6,	6,	6,
		6,	6,	6,	6,	6,	6,	6,	6,
		6,	6,	6,	6,	6,	6,	6,	6,
		7,	7,	7,	7,	7,	7,	7,	7,
		7,	7,	7,	7,	7,	7,	7,	7,
		7,	7,	7,	7,	7,	7,	7,	7,
		7,	7,	7,	7,	7,	7,	7,	7,
		7,	7,	7,	7,	7,	7,	7,	7,
		7,	7,	7,	7,	7,	7,	7,	7,
		7,	7,	7,	7,	7,	7,	7,	7,
		7,	7,	7,	7,	7,	7,	7,	7,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
		8,	8,	8,	8,	8,	8,	8,	8,
	};

	n[0] = ppc_onecnt_table[R(t) >> 24];
	if(n[0] == 0){
		n[1] = ppc_onecnt_table[(R(t) >> 16) & 0xFF];
		if(n[1] == 0){
			n[2] = ppc_onecnt_table[(R(t) >> 8) & 0xFF];
			if(n[2] == 0){
				n[2] = 8 + (8 - ppc_onecnt_table[R(t) & 0xFF]);
			}
			n[1] = (8 - n[1]) + n[2];
		}
		n[0] = (8 - n[0]) + n[1];
	}

	R(a) = n[0];
	SET_ICR0(R(a));

	#endif
}

////////////////////////////////////////////////////////////////
// Rotate

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

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;
	
	R(a) = R(t) << (R(b) & 0x3F);
		
	SET_ICR0(R(a));
}

static void ppc_srwx(u32 op){

	u32 b = RB;
	u32 a = RA;
	u32 t = RT;

	R(a) = R(t) >> (R(b) & 0x3F);
	SET_ICR0(R(a));
}

static void ppc_srawx(u32 op){

	u32 sa = R(RB) & 0x3F;
	u32 a = RA;
	u32 t = RT;

	XER &= ~XER_CA;
	if(R(t) < 0 && R(t) & BITMASK_0(sa))
		XER |= XER_CA;

	R(a) = (s32)R(t) >> sa;

	SET_ICR0(R(a));
}

static void ppc_srawix(u32 op){

	u32 sa = _SH;
	u32 a = RA;
	u32 t = RT;

	XER &= ~XER_CA;
	if(R(t) < 0 && R(t) & BITMASK_0(sa))
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
	R(t) = (u32)(u8)ppc.irb(ea);
}

static void ppc_lha(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (s32)(s16)ppc.irw(ea);
}

static void ppc_lhz(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)(u16)ppc.irw(ea);
}

static void ppc_lwz(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)ppc.ird(ea);

}

static void ppc_lbzu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)(u8)ppc.irb(R(a));
}

static void ppc_lhau(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (s32)(s16)ppc.irw(R(a));
}

static void ppc_lhzu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)(u16)ppc.irw(R(a));
}

static void ppc_lwzu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)ppc.ird(R(a));
}

static void ppc_lbzx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)(u8)ppc.irb(ea);
}

static void ppc_lhax(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (s32)(s16)ppc.irw(ea);
}

static void ppc_lhzx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)(u16)ppc.irw(ea);
}

static void ppc_lwzx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	R(t) = (u32)ppc.ird(ea);
}

static void ppc_lbzux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)(u8)ppc.irb(R(a));
}

static void ppc_lhaux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (s32)(s16)ppc.irw(R(a));
}

static void ppc_lhzux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)(u16)ppc.irw(R(a));
}

static void ppc_lwzux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	R(a) += ea;
	R(t) = (u32)ppc.ird(R(a));
}

////////////////////////////////////////////////////////////////

static void ppc_stb(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc.iwb(ea, R(t));
}

static void ppc_sth(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc.iww(ea, R(t));
}

static void ppc_stw(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc.iwd(ea, R(t));
}

static void ppc_stbu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc.iwb(ea, R(t));
	R(a) = ea;
}

static void ppc_sthu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc.iww(ea, R(t));
	R(a) = ea;
}

static void ppc_stwu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc.iwd(ea, R(t));
	R(a) = ea;
}

static void ppc_stbx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc.iwb(ea, R(t));
}

static void ppc_sthx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc.iww(ea, R(t));
}

static void ppc_stwx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a) ea += R(a);
	ppc.iwd(ea, R(t));
}

static void ppc_stbux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc.iwb(ea, R(t));
	R(a) = ea;
}

static void ppc_sthux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc.iww(ea, R(t));
	R(a) = ea;
}

static void ppc_stwux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);
	ppc.iwd(ea, R(t));
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

	i = ppc.ird(ea);
	ppc.fpr[t].fd = (f64)(*((f32 *)&i));
}

static void ppc_lfsu(u32 op){

	u32 itemp;
	f32 ftemp;

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	itemp = ppc.ird(ea);
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

	itemp = ppc.ird(ea);
	ftemp = *(f32 *)&itemp;

	ppc.fpr[t].fd = (f64)((f32)ftemp);

	R(a) = ea;

	//ppc_null(op);
}

static void ppc_lfsx(u32 op){

	u32 itemp;
	f32 ftemp;

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	itemp = ppc.ird(ea);
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

	ppc.iwd(ea, *(u32 *)&ftemp);
}

static void ppc_stfsu(u32 op){

	f32 ftemp;

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	ftemp = (f32)((f64)ppc.fpr[t].fd);

	ppc.iwd(ea, *(u32 *)&ftemp);

	R(a) = ea;

	//ppc_null(op);
}

static void ppc_stfsux(u32 op){

	f32 ftemp;

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	ftemp = (f32)((f64)ppc.fpr[t].fd);

	ppc.iwd(ea, *(u32 *)&ftemp);

	R(a) = ea;

	//ppc_null(op);
}

static void ppc_stfsx(u32 op){

	f32 ftemp;

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ftemp = (f32)((f64)ppc.fpr[t].fd);

	ppc.iwd(ea, *(u32 *)&ftemp);

	//ppc_null(op);
}

////////////////////////////////////////////////////////////////

static void ppc_lfd(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ppc.fpr[t].id = ppc.irq(ea);
}

static void ppc_lfdu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 d = RD;

	ea += R(a);

	ppc.fpr[d].id = ppc.irq(ea);

	R(a) = ea;

	//ppc_null(op);
}

static void ppc_lfdux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;

	ea += R(a);

	ppc.fpr[d].id = ppc.irq(ea);

	R(a) = ea;

	//ppc_null(op);
}

static void ppc_lfdx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;

	if(a)
		ea += R(a);

	ppc.fpr[d].id = ppc.irq(ea);

	//ppc_null(op);
}

static void ppc_stfd(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ppc.iwq(ea, ppc.fpr[t].id);
}

static void ppc_stfdu(u32 op){

	u32 ea = SIMM;
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	ppc.iwq(ea, ppc.fpr[t].id);

	R(a) = ea;
}

static void ppc_stfdux(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	ea += R(a);

	ppc.iwq(ea, ppc.fpr[t].id);

	R(a) = ea;

	//ppc_null(op);
}

static void ppc_stfdx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ppc.iwq(ea, ppc.fpr[t].id);

	//ppc_null(op);
}

////////////////////////////////////////////////////////////////

static void ppc_stfiwx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 t = RT;

	if(a)
		ea += R(a);

	ppc.iwd(ea, ppc.fpr[t].iw);
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

	t = ppc.irw(ea);
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

	t = ppc.ird(ea);
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

	ppc.iww(ea, ((R(d) >> 8) & 0x00FF) |
				((R(d) << 8) & 0xFF00));
}

static void ppc_stwbrx(u32 op){

	u32 ea = R(RB);
	u32 a = RA;
	u32 d = RD;

	if(a)
		ea += R(a);

	ppc.iwd(ea, ((R(d) >> 24) & 0x000000FF) |
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

		R(d) = ppc.ird(ea);
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

		ppc.iwd(ea, R(d));
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
	R(t) = ppc.ird(ea);
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

		ppc.iwd(ea, R(t));
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
// Branch

INLINE u32 CHECK_BI(u32 bi){

	switch(bi & 3){
	case 0:		return((CR(bi >> 2) & _LT) != 0);
	case 1:		return((CR(bi >> 2) & _GT) != 0);
	case 2:		return((CR(bi >> 2) & _EQ) != 0);
	default:	return((CR(bi >> 2) & _SO) != 0);
	}
}

static u32 ppc_bo_0000(u32 bi){ return(--CTR != 0 && CHECK_BI(bi) == 0); }
static u32 ppc_bo_0001(u32 bi){ return(--CTR == 0 && CHECK_BI(bi) == 0); }
static u32 ppc_bo_001z(u32 bi){ return(CHECK_BI(bi) == 0); }
static u32 ppc_bo_0100(u32 bi){ return(--CTR != 0 && CHECK_BI(bi) != 0); }
static u32 ppc_bo_0101(u32 bi){ return(--CTR == 0 && CHECK_BI(bi) != 0); }
static u32 ppc_bo_011z(u32 bi){ return(CHECK_BI(bi) != 0); }
static u32 ppc_bo_1z00(u32 bi){ return(--CTR != 0); }
static u32 ppc_bo_1z01(u32 bi){ return(--CTR == 0); }
static u32 ppc_bo_1z1z(u32 bi){ return(1); }

static u32 (* ppc_bo[16])(u32 bi) = {
	ppc_bo_0000,	ppc_bo_0001,	ppc_bo_001z,	ppc_bo_001z,
	ppc_bo_0100,	ppc_bo_0101,	ppc_bo_011z,	ppc_bo_011z,
	ppc_bo_1z00,	ppc_bo_1z01,	ppc_bo_1z1z,	ppc_bo_1z1z,
	ppc_bo_1z00,	ppc_bo_1z01,	ppc_bo_1z1z,	ppc_bo_1z1z,
};

static void ppc_bx(u32 op){

	NIA = (((s32)op << 6) >> 6) & ~3;

	if((op & _AA) == 0)
		NIA += CIA;

	if(op & _LK)
		LR = CIA + 4;
}

static void ppc_bcx(u32 op){

	if(ppc_bo[_BO >> 1](_BI)){
		NIA = SIMM & ~3;
		if((op & _AA) == 0)
			NIA += CIA;
	}

	if(op & _LK)
		LR = CIA + 4;
}

static void ppc_bcctrx(u32 op){

	if(ppc_bo[_BO >> 1](_BI)){
		NIA = CTR & ~3;
	}

	if(op & _LK)
		LR = CIA + 4;
}

static void ppc_bclrx(u32 op){

	if(ppc_bo[_BO >> 1](_BI))
		NIA = LR & ~3;

	if(op & _LK)
		LR = CIA + 4;
}

////////////////////////////////////////////////////////////////
// Exception Control

static void ppc_rfi(u32 op){

	ppc.nia = ppc.spr[SPR_SRR0];
	ppc.msr = ppc.spr[SPR_SRR1];

	// recalc ppc.test_irq
}

static void ppc_rfci(u32 op){

	ppc.nia = ppc.spr[SPR_SRR2];
	ppc.msr = ppc.spr[SPR_SRR3];

	// reacalc ppc.test_irq
}

static void ppc_sc(u32 op){

	ppc_null(op);
}

static void ppc_tw(u32 op){

	ppc_null(op);
}

static void ppc_twi(u32 op){

	ppc_null(op);
}

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

static void ppc_mfmsr(u32 op){	R(RD) = ppc.get_msr(); }
static void ppc_mtmsr(u32 op){	ppc.set_msr(R(RD));
                                LOG("model3.log", "FE0-1=%d%d\n", !!(R(RD) & 0x800), !!(R(RD) & 0x100));
                                                        }

static void ppc_mfspr(u32 op){	R(RD) = ppc.get_spr(_SPRF); }
static void ppc_mtspr(u32 op){	ppc.set_spr(_SPRF, R(RD)); }

static void ppc_mfdcr(u32 op){	R(RD) = ppc_get_dcr(_DCRF); }
static void ppc_mtdcr(u32 op){	ppc_set_dcr(_DCRF, R(RD)); }

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

////////////////////////////////////////////////////////////////
// Floating-Point Computation

// everything is IEEE754, except "madd" and "estimate" instructions

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
		((*(u64 *)&(x) & DOUBLE_FRAC) != DOUBLE_ZERO) &&
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
    return (*(u64 *)&(x) & DOUBLE_SIGN);
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

static void set_fprf(f64 f)
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

////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////
// Floating-Point Status / Control

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

	if(op & _RC)
		CR(1) = (ppc.fpscr >> 28);
}

static void ppc_mtfsfx(u32 op){

	u32 b = RB;
	u32 f = _FM;

	f = ppc_field_xlat[_FM];

	ppc.fpscr &= ~f;
	ppc.fpscr |= (ppc.fpr[b].iw);

	if(op & _RC)
		CR(1) = (ppc.fpscr >> 28);
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
}

////////////////////////////////////////////////////////////////

static void (* ppc_inst_19[0x400])(u32 op);
static void (* ppc_inst_31[0x400])(u32 op);
static void (* ppc_inst_59[0x400])(u32 op);
static void (* ppc_inst_63[0x400])(u32 op);

static void ppc_19(u32 op){ ppc_inst_19[_XO](op); }
static void ppc_31(u32 op){ ppc_inst_31[_XO](op); }
static void ppc_59(u32 op){ ppc_inst_59[_XO](op); }
static void ppc_63(u32 op){ ppc_inst_63[_XO](op); }

////////////////////////////////////////////////////////////////

static void (* ppc_inst[64])(u32 op) = {

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
	ppc_lfs,	ppc_lfsu,	ppc_lfd,	ppc_lfdu,		// 30 ~ 33
	ppc_stfs,	ppc_stfsu,	ppc_stfd,	ppc_stfdu,		// 34 ~ 37
	ppc_null,	ppc_null,	ppc_null,	ppc_59,			// 38 ~ 3b
	ppc_null,	ppc_null,	ppc_null,	ppc_63,			// 3c ~ 3f
};

////////////////////////////////////////////////////////////////

u64 ppc_read_time_stamp(void){

	// for timer implementation and such

	return(0);
}

////////////////////////////////////////////////////////////////

void ppc_4xx_test_irq(void){

	/*

	//if(ppc.test_irq){

		if(ppc.msr & 0x80000000){

			// Critical Enabled

			if(ppc.dcr[DCR_EXIER] & ppc.dcr[DCR_EXISR] & 0x80000000){

				// Critical Interrupt

				ppc.spr[SPR_SRR2] = ppc.nia;
				ppc.nia = (ppc.spr[SPR_EVPR] & 0xFFFF0000) | 0x0100;
				ppc.spr[SPR_SRR3] = ppc.msr;
				ppc.msr &= ~0x8008C008; // WE=PR=CE=EE=DE=PE=0
			}

			// Watch-Dog Interrupt

		}else
		if(ppc.msr & 0x00008000){

			// External Enabled

			if(ppc.dcr[DCR_EXIER] & ppc.dcr[DCR_EXISR] & 0x0FF0001F){

				// External Interrupt

				ppc.spr[SPR_SRR0] = ppc.nia;
				ppc.nia = (ppc.spr[SPR_EVPR] & 0xFFFF0000) | 0x0500;
				ppc.spr[SPR_SRR1] = ppc.msr;
				ppc.msr &= ~0x0008C008; // WE=EE=PR=PE=0
			}
		}

	//	if(!(ppc.dcr[DCR_EXIER] & ppc.dcr[DCR_EXISR] & 0x8FF0001F))
	//		ppc.test_irq = 0;
	//	}

	//}

	*/
}

void ppc_machine_check(void){

	if(ppc.msr & 0x1000){

		/* machine check enabled */

		ppc.spr[SPR_SRR0] = ppc.nia;
		ppc.spr[SPR_SRR1] = ppc.msr;

		ppc.nia = (ppc.msr & 0x40) ? 0xFFF00200 : 0x00000200;
		ppc.msr = (ppc.msr & 0x11040) | ((ppc.msr >> 16) & 1);
	}

	/* enter checkstop condition */
}

void ppc_6xx_test_irq(void){

	if(ppc.msr & 0x8000){

		/* external irq and decrementer enabled */

		if(ppc.irq_state){

			/* accept external interrupt */

			ppc.spr[SPR_SRR0] = ppc.nia;
			ppc.spr[SPR_SRR1] = ppc.msr;

			ppc.nia = (ppc.msr & 0x40) ? 0xFFF00500 : 0x00000500;
			ppc.msr = (ppc.msr & 0x11040) | ((ppc.msr >> 16) & 1);

			/* notify to the interrupt manager */

			if(ppc.irq_callback != NULL)
				ppc.irq_state = ppc.irq_callback();
            else
				ppc.irq_state = 0;

		}else
		if(ppc.dec_expired){

			/* accept decrementer exception */

			ppc.spr[SPR_SRR0] = ppc.nia;
			ppc.spr[SPR_SRR1] = ppc.msr;

			ppc.nia = (ppc.msr & 0x40) ? 0xFFF00900 : 0x00000900;
			ppc.msr = (ppc.msr & 0x11040) | ((ppc.msr >> 16) & 1);

			/* clear pending decrementer exception */

			ppc.dec_expired = 0;
		}
	}
}

void ppc_set_irq_line(u32 state){

	ppc.irq_state = state;
}

void ppc_set_irq_callback(u32 (* callback)(void)){

	ppc.irq_callback = callback;
}

////////////////////////////////////////////////////////////////

char * m3_stat_string(u32 stat){

	return(NULL);
}

u32 ppc_run(u32 count){

	ppc.count = count;

	while(ppc.count > 0){

		ppc.count--;

#if 0
        if (ppc.cia == 0x3C7C)  // VF3
        {
            printf("BREAKPOINT!\n");
            exit(0);
        }
#endif

        // fetch and execute

		ppc.ir = ppc.fetch(ppc.cia);

		ppc_inst[(ppc.ir >> 26) & 0x3f](ppc.ir);

		// check for interrupts

		ppc.test_irq_event();

		/* tb & dec are modified each 4 cycles! */

		// increment time base

		if((ppc.count & 3) == 0){

			/* won't work with real instruction timings (non-1 cycle) */

			ppc.tb++;
			if(ppc.tb >> 56)
				ppc.tb = 0;

            ppc.spr[SPR_DEC]--;
            if(ppc.spr[SPR_DEC] == 0xFFFFFFFF)
                ppc.dec_expired = 1;
		}


		// go to next instructions

		ppc.cia = ppc.nia;
		ppc.nia += 4;

	}

	return(0);
}

////////////////////////////////////////////////////////////////

void ppc_set_trace_callback(void * callback){

	ppc.trace = (void (*)(u32, u32))callback;
}

////////////////////////////////////////////////////////////////

void ppc_set_read_8_handler(void * handler){     ppc.rb = (u32 (*)(u32))handler; }
void ppc_set_read_16_handler(void * handler){     ppc.rw = (u32 (*)(u32))handler; }
void ppc_set_read_32_handler(void * handler){     ppc.rd = (u32 (*)(u32))handler; }
void ppc_set_read_64_handler(void * handler){    ppc.rq = (u64 (*)(u32))handler; }

void ppc_set_write_8_handler(void * handler){    ppc.wb = (void (*)(u32,u32))handler; }
void ppc_set_write_16_handler(void * handler){    ppc.ww = (void (*)(u32,u32))handler; }
void ppc_set_write_32_handler(void * handler){    ppc.wd = (void (*)(u32,u32))handler; }
void ppc_set_write_64_handler(void * handler){   ppc.wq = (void (*)(u32,u64))handler; }

////////////////////////////////////////////////////////////////

static u32 ppc_rb_null(u32 a){ return(0); }
static u32 ppc_rw_null(u32 a){ return(0); }
static u32 ppc_rd_null(u32 a){ return(0); }
static u64 ppc_rq_null(u32 a){ return(0); }

static void ppc_wb_null(u32 a, u32 d){ }
static void ppc_ww_null(u32 a, u32 d){ }
static void ppc_wd_null(u32 a, u32 d){ }
static void ppc_wq_null(u32 a, u64 d){ }

////////////////////////////////////////////////////////////////

u32 ppc_read_byte(u32 a){ return(ppc.irb(a)); }
u32 ppc_read_half(u32 a){ return(ppc.irw(a)); }
u32 ppc_read_word(u32 a){ return(ppc.ird(a)); }
u64 ppc_read_dword(u32 a){ return(ppc.irq(a)); }

void ppc_write_byte(u32 a, u32 d){ ppc.iwb(a, d); }
void ppc_write_half(u32 a, u32 d){ ppc.iww(a, d); }
void ppc_write_word(u32 a, u32 d){ ppc.iwd(a, d); }
void ppc_write_dword(u32 a, u64 d){ ppc.iwq(a, d); }

////////////////////////////////////////////////////////////////

u32 ppc_get_r(int n){ return(ppc.gpr[n]); }
void ppc_set_r(int n, u32 d){ ppc.gpr[n] = d; }

f64 ppc_get_f(int n){ return(ppc.fpr[n].fd); }
void ppc_set_f(int n, f64 d){ ppc.fpr[n].fd = d; }

u32 ppc_get_reg(PPC_REG r){

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

	case PPC_REG_PC: return(ppc.cia);
	case PPC_REG_IR: return(ppc.ir);

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

void ppc_set_reg(PPC_REG r, u32 d){

	if(r == PPC_REG_PC){
		ppc.nia = d;
	}
}

////////////////////////////////////////////////////////////////

int ppc_get_context(ppc_t * dst){

	if(dst != NULL){

		memcpy((void *)dst, (void *)&ppc, sizeof(ppc_t));

		return(sizeof(ppc_t));
	}

	return(-1);
}

int ppc_set_context(ppc_t * src){

	if(src != NULL){

		memcpy((void *)&ppc, (void *)src, sizeof(ppc_t));

		return(sizeof(ppc_t));
	}

	return(-1);
}

////////////////////////////////////////////////////////////////

int ppc_reset(void){

	int i;

	for(i = 0; i < 32; i++)
		ppc.gpr[i] = 0;

	for(i = 0; i < 1024; i++)
		ppc.spr[i] = 0;

	for(i = 0; i < 8; i++)
		ppc.cr[i] = 0;

	ppc.reserved = 0xFFFFFFFF;
	ppc.irq_state = 0;
	ppc.tb = 0;

	if(ppc.type == PPC_TYPE_4XX){

		// PPC 4XX

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

		ppc.cia = 0xFFFFFFFC;
		ppc.nia = 0xFFFFFFFF;

		// Serial Controller

	}else
	if(ppc.type == PPC_TYPE_6XX){

		// PPC 6XX

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

		ppc.spr[SPR_PVR] = 0x00060104;		// 603e, Stretch, 1.4 - checked against by VS2V991
		ppc.spr[SPR_DEC] = 0xFFFFFFFF;

		ppc.cia = 0xFFF00100;
		ppc.nia = 0xFFF00104;
	}

    return PPC_OKAY;
}

////////////////////////////////////////////////////////////////

int ppc_init(void * x){

	int i;

	x = NULL;

	// setup cpu type

	ppc.type = PPC_TYPE_6XX;

	// install internal memory handlers

	ppc.irq_callback = (u32 (*)(void))NULL;

	switch(ppc.type){

	case PPC_TYPE_4XX:

		ppc.fetch = ppc_4xx_fetch;

		ppc.irb = ppc_4xx_irb;
		ppc.irw = ppc_4xx_irw;
		ppc.ird = ppc_4xx_ird;
		ppc.irq = NULL;

		ppc.iwb = ppc_4xx_iwb;
		ppc.iww = ppc_4xx_iww;
		ppc.iwd = ppc_4xx_iwd;
		ppc.iwq = NULL;

		ppc.set_spr = ppc_4xx_set_spr;
		ppc.get_spr = ppc_4xx_get_spr;

		ppc.set_msr = ppc_4xx_set_msr;
		ppc.get_msr = ppc_4xx_get_msr;

		ppc.test_irq_event = ppc_4xx_test_irq;

		break;

	case PPC_TYPE_6XX:

		ppc.fetch = ppc_6xx_fetch;

		ppc.irb = ppc_6xx_irb;
		ppc.irw = ppc_6xx_irw;
		ppc.ird = ppc_6xx_ird;
		ppc.irq = ppc_6xx_irq;

		ppc.iwb = ppc_6xx_iwb;
		ppc.iww = ppc_6xx_iww;
		ppc.iwd = ppc_6xx_iwd;
		ppc.iwq = ppc_6xx_iwq;

		ppc.set_spr = ppc_6xx_set_spr;
		ppc.get_spr = ppc_6xx_get_spr;

		ppc.set_msr = ppc_6xx_set_msr;
		ppc.get_msr = ppc_6xx_get_msr;

		ppc.test_irq_event = ppc_6xx_test_irq;

		break;

	default:

		printf("ERROR: unhandled cpu type in ppc_init\n");
		exit(1);
	}

	// install default (noop) memory handlers

	ppc.rb = ppc_rb_null;
	ppc.rw = ppc_rw_null;
	ppc.rd = ppc_rd_null;
	ppc.rq = ppc_rq_null;

	ppc.wb = ppc_wb_null;
	ppc.ww = ppc_ww_null;
	ppc.wd = ppc_wd_null;
	ppc.wq = ppc_wq_null;

	// install default callbacks

	ppc.trace = NULL;

	// cleanup extended opcodes handlers

	for(i = 0; i < 1024; i++){

		ppc_inst_19[i] = ppc_null;
		ppc_inst_31[i] = ppc_null;
		ppc_inst_59[i] = ppc_null;
		ppc_inst_63[i] = ppc_null;
	}

	// install extended opcodes handlers

	ppc_inst_31[266]	= ppc_inst_31[266|(_OE>>1)]	= ppc_addx;
	ppc_inst_31[10]		= ppc_inst_31[10|(_OE>>1)]	= ppc_addcx;
	ppc_inst_31[138]	= ppc_inst_31[138|(_OE>>1)]	= ppc_addex;
	ppc_inst_31[234]	= ppc_inst_31[234|(_OE>>1)]	= ppc_addmex;
	ppc_inst_31[202]	= ppc_inst_31[202|(_OE>>1)]	= ppc_addzex;
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
	ppc_inst_31[491]	= ppc_inst_31[491|(_OE>>1)]	= ppc_divwx;
	ppc_inst_31[459]	= ppc_inst_31[459|(_OE>>1)]	= ppc_divwux;
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
    ppc_inst_31[235]    = ppc_inst_31[235|(_OE>>1)]    = ppc_mullwx;
	ppc_inst_31[476]	= ppc_nandx;
    ppc_inst_31[104]    = ppc_inst_31[104|(_OE>>1)]    = ppc_negx;
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
    ppc_inst_31[40]     = ppc_inst_31[40|(_OE>>1)]     = ppc_subfx;
    ppc_inst_31[8]      = ppc_inst_31[8|(_OE>>1)]      = ppc_subfcx;
    ppc_inst_31[136]    = ppc_inst_31[136|(_OE>>1)]    = ppc_subfex;
    ppc_inst_31[232]    = ppc_inst_31[232|(_OE>>1)]    = ppc_subfmex;
    ppc_inst_31[200]    = ppc_inst_31[200|(_OE>>1)]    = ppc_subfzex;
	ppc_inst_31[598]	= ppc_sync;
	ppc_inst_31[4]		= ppc_tw;
	ppc_inst_31[316]	= ppc_xorx;

	switch(ppc.type){

	case PPC_TYPE_4XX:

		ppc_inst_31[454] = ppc_dccci;
		ppc_inst_31[486] = ppc_dcread;
		ppc_inst_31[262] = ppc_icbt;
		ppc_inst_31[966] = ppc_iccci;
		ppc_inst_31[998] = ppc_icread;
		ppc_inst_31[323] = ppc_mfdcr;
		ppc_inst_31[451] = ppc_mtdcr;
		ppc_inst_31[131] = ppc_wrtee;
		ppc_inst_31[163] = ppc_wrteei;

		break;

	case PPC_TYPE_6XX:

		ppc_inst[48] = ppc_lfs;
		ppc_inst[49] = ppc_lfsu;
		ppc_inst[50] = ppc_lfd;
		ppc_inst[51] = ppc_lfdu;
		ppc_inst[52] = ppc_stfs;
		ppc_inst[53] = ppc_stfsu;
		ppc_inst[54] = ppc_stfd;
		ppc_inst[55] = ppc_stfdu;

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

		// special FP opcodes with C field must be
		// installed another way. (5-bit field)

		for(i = 0; i < 32; i += 1){

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

		break;
	}

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

    return PPC_OKAY;
}

void ppc_save_state(FILE *fp)
{
    fwrite(ppc.gpr, sizeof(u32), 32, fp);
    fwrite(ppc.fpr, sizeof(fpr_t), 32, fp);
    fwrite(&ppc.cia, sizeof(u32), 1, fp);
    fwrite(&ppc.nia, sizeof(u32), 1, fp);
    fwrite(&ppc.ir, sizeof(u32), 1, fp);
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
    fread(ppc.fpr, sizeof(fpr_t), 32, fp);
    fread(&ppc.cia, sizeof(u32), 1, fp);
    fread(&ppc.nia, sizeof(u32), 1, fp);
    fread(&ppc.ir, sizeof(u32), 1, fp);
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
}
