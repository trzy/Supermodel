/* IBM/Motorola PowerPC 4xx/6xx Emulator */

#include "model3.h"
#include "ppc.h"

void ppc603_exception(int exception);
void ppc603_check_interrupts(void);

#define RD				((op >> 21) & 0x1F)
#define RT				((op >> 21) & 0x1f)
#define RS				((op >> 21) & 0x1f)
#define RA				((op >> 16) & 0x1f)
#define RB				((op >> 11) & 0x1f)
#define RC				((op >> 6) & 0x1f)

#define MB				((op >> 6) & 0x1f)
#define ME				((op >> 1) & 0x1f)
#define SH				((op >> 11) & 0x1f)
#define BO				((op >> 21) & 0x1f)
#define BI				((op >> 16) & 0x1f)
#define CRFD			((op >> 23) & 0x7)
#define CRFA			((op >> 18) & 0x7)
#define FXM				((op >> 12) & 0xff)
#define SPR				(((op >> 16) & 0x1f) | ((op >> 6) & 0x3e0))

#define SIMM16			(INT32)(INT16)(op & 0xffff)
#define UIMM16			(UINT32)(op & 0xffff)

#define RCBIT			(op & 0x1)
#define OEBIT			(op & 0x400)
#define AABIT			(op & 0x2)
#define LKBIT			(op & 0x1)

#define REG(x)			(ppc.r[x])
#define LR				(ppc.lr)
#define CTR				(ppc.ctr)
#define XER				(ppc.xer)
#define CR(x)			(ppc.cr[x])
#define MSR				(ppc.msr)
#define SRR0			(ppc.srr0)
#define SRR1			(ppc.srr1)
#define SRR2			(ppc.srr2)
#define SRR3			(ppc.srr3)
#define EVPR			(ppc.evpr)
#define EXIER			(ppc.exier)
#define EXISR			(ppc.exisr)
#define DEC				(ppc.dec)


// Stuff added for the 6xx
#define FPR(x)			(ppc.fpr[x])
#define FM				((op >> 17) & 0xFF)
#define SPRF			(((op >> 6) & 0x3E0) | ((op >> 16) & 0x1F))


#define CHECK_SUPERVISOR()			\
	if((ppc.msr & 0x4000) != 0){	\
	}

#define CHECK_FPU_AVAILABLE()		\
	if((ppc.msr & 0x2000) == 0){	\
	}

static UINT32		ppc_field_xlat[256];



#define FPSCR_FX		0x80000000
#define FPSCR_FEX		0x40000000
#define FPSCR_VX		0x20000000
#define FPSCR_OX		0x10000000
#define FPSCR_UX		0x08000000
#define FPSCR_ZX		0x04000000
#define FPSCR_XX		0x02000000



#define BITMASK_0(n)	(UINT32)(((UINT64)1 << n) - 1)
#define CRBIT(x)		((ppc.cr[x / 4] & (1 << (3 - (x % 4)))) ? 1 : 0)
#define _BIT(n)			(1 << (n))
#define GET_ROTATE_MASK(mb,me)		(ppc_rotate_mask[mb][me])
#define ADD_CA(r,a,b)		((UINT32)r < (UINT32)a)
#define SUB_CA(r,a,b)		(!((UINT32)a < (UINT32)b))
#define ADD_OV(r,a,b)		((~((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)
#define SUB_OV(r,a,b)		(( ((a) ^ (b)) & ((a) ^ (r))) & 0x80000000)

#define XER_SO			0x80000000
#define XER_OV			0x40000000
#define XER_CA			0x20000000

#define MSR_POW			0x00040000	/* Power Management Enable */
#define MSR_WE			0x00040000
#define MSR_CE			0x00020000
#define MSR_ILE			0x00010000	/* Interrupt Little Endian Mode */
#define MSR_EE			0x00008000	/* External Interrupt Enable */
#define MSR_PR			0x00004000	/* Problem State */
#define MSR_FP			0x00002000	/* Floating Point Available */
#define MSR_ME			0x00001000	/* Machine Check Enable */
#define MSR_FE0			0x00000800
#define MSR_SE			0x00000400	/* Single Step Trace Enable */
#define MSR_BE			0x00000200	/* Branch Trace Enable */
#define MSR_DE			0x00000200
#define MSR_FE1			0x00000100
#define MSR_IP			0x00000040	/* Interrupt Prefix */
#define MSR_IR			0x00000020	/* Instruction Relocate */
#define MSR_DR			0x00000010	/* Data Relocate */
#define MSR_PE			0x00000008
#define MSR_PX			0x00000004
#define MSR_RI			0x00000002	/* Recoverable Interrupt Enable */
#define MSR_LE			0x00000001

#define TSR_ENW			0x80000000
#define TSR_WIS			0x40000000

#define BYTE_REVERSE16(x)	((((x) >> 8) & 0xff) | (((x) << 8) & 0xff00))
#define BYTE_REVERSE32(x)	((((x) >> 24) & 0xff) | (((x) >> 8) & 0xff00) | (((x) << 8) & 0xff0000) | (((x) << 24) & 0xff000000))

typedef union {
	UINT64	id;
	double	fd;
} FPR;

typedef union {
	UINT32 i;
	float f;
} FPR32;

typedef struct {
	UINT32 u;
	UINT32 l;
} BATENT;


typedef struct {
	UINT32 r[32];
	UINT32 pc;
	UINT32 npc;

	UINT32 *op;

	UINT32 lr;
	UINT32 ctr;
	UINT32 xer;
	UINT32 msr;
	UINT8 cr[8];
	UINT32 pvr;
	UINT32 srr0;
	UINT32 srr1;
	UINT32 srr2;
	UINT32 srr3;
	UINT32 hid0;
	UINT32 hid1;
	UINT32 hid2;
	UINT32 sdr1;
	UINT32 sprg[4];

	UINT32 dsisr;
	UINT32 dar;
	UINT32 ear;
	UINT32 dmiss;
	UINT32 dcmp;
	UINT32 hash1;
	UINT32 hash2;
	UINT32 imiss;
	UINT32 icmp;
	UINT32 rpa;


	BATENT ibat[4];
	BATENT dbat[4];

	UINT32 evpr;
	UINT32 exier;
	UINT32 exisr;
	UINT32 bear;
	UINT32 besr;
	UINT32 iocr;
	UINT32 br[8];
	UINT32 iabr;
	UINT32 esr;
	UINT32 iccr;
	UINT32 dccr;
	UINT32 pit;
	UINT32 pit_counter;
 	UINT32 pit_int_enable;
	UINT32 tsr;
	UINT32 dbsr;
	UINT32 sgr;
	UINT32 pid;

	int reserved;
	UINT32 reserved_address;

	int interrupt_pending;
	int external_int;

	UINT64 tb;			/* 56-bit timebase register */

	int (*irq_callback)(int irqline);

	PPC_FETCH_REGION	cur_fetch;
	PPC_FETCH_REGION	* fetch;

	// STUFF added for the 6xx series
	UINT32 dec, dec_frac;
	UINT32 fpscr;

	FPR	fpr[32];
	UINT32 sr[16];

#if HAS_PPC603
	int is603;
#endif
#if HAS_PPC602
	int is602;
#endif
} PPC_REGS;



typedef struct {
	int code;
	int subcode;
	void (* handler)(UINT32);
} PPC_OPCODE;



static int ppc_icount;
static int ppc_tb_base_icount;
static int ppc_dec_base_icount;
static int ppc_dec_trigger_cycle;
static int bus_freq_multiplier = 1;
static PPC_REGS ppc;
static UINT32 ppc_rotate_mask[32][32];



static void ppc_change_pc(UINT32 newpc)
{
	UINT i;

	if (ppc.cur_fetch.start <= newpc && newpc <= ppc.cur_fetch.end)
	{
		ppc.op = (UINT32 *)((UINT32)ppc.cur_fetch.ptr + (UINT32)(newpc - ppc.cur_fetch.start));
		return;
	}

	for(i = 0; ppc.fetch[i].ptr != NULL; i++)
	{
		if (ppc.fetch[i].start <= newpc && newpc <= ppc.fetch[i].end)
		{
			ppc.cur_fetch.start = ppc.fetch[i].start;
			ppc.cur_fetch.end = ppc.fetch[i].end;
			ppc.cur_fetch.ptr = ppc.fetch[i].ptr;

			ppc.op = (UINT32 *)((UINT32)ppc.cur_fetch.ptr + (UINT32)(newpc - ppc.cur_fetch.start));

			return;
		}
	}

	error("Invalid PC %08X, previous PC %08X\n", newpc, ppc.pc);
}

INLINE UINT8 READ8(UINT32 address)
{
	return m3_ppc_read_8(address);
}

INLINE UINT16 READ16(UINT32 address)
{
	return m3_ppc_read_16(address);
}

INLINE UINT32 READ32(UINT32 address)
{
	return m3_ppc_read_32(address);
}

INLINE UINT64 READ64(UINT32 address)
{
	return m3_ppc_read_64(address);
}

INLINE void WRITE8(UINT32 address, UINT8 data)
{
	m3_ppc_write_8(address, data);
}

INLINE void WRITE16(UINT32 address, UINT16 data)
{
	m3_ppc_write_16(address, data);
}

INLINE void WRITE32(UINT32 address, UINT32 data)
{
	m3_ppc_write_32(address, data);
}

INLINE void WRITE64(UINT32 address, UINT64 data)
{
	m3_ppc_write_64(address, data);
}


/*********************************************************************/


INLINE void SET_CR0(INT32 rd)
{
	if( rd < 0 ) {
		CR(0) = 0x8;
	} else if( rd > 0 ) {
		CR(0) = 0x4;
	} else {
		CR(0) = 0x2;
	}

	if( XER & XER_SO )
		CR(0) |= 0x1;
}

INLINE void SET_CR1(void)
{
	CR(1) = (ppc.fpscr >> 28) & 0xf;
}

INLINE void SET_ADD_OV(UINT32 rd, UINT32 ra, UINT32 rb)
{
	if( ADD_OV(rd, ra, rb) )
		XER |= XER_SO | XER_OV;
	else
		XER &= ~XER_OV;
}

INLINE void SET_SUB_OV(UINT32 rd, UINT32 ra, UINT32 rb)
{
	if( SUB_OV(rd, ra, rb) )
		XER |= XER_SO | XER_OV;
	else
		XER &= ~XER_OV;
}

INLINE void SET_ADD_CA(UINT32 rd, UINT32 ra, UINT32 rb)
{
	if( ADD_CA(rd, ra, rb) )
		XER |= XER_CA;
	else
		XER &= ~XER_CA;
}

INLINE void SET_SUB_CA(UINT32 rd, UINT32 ra, UINT32 rb)
{
	if( SUB_CA(rd, ra, rb) )
		XER |= XER_CA;
	else
		XER &= ~XER_CA;
}

INLINE UINT32 check_condition_code(UINT32 bo, UINT32 bi)
{
	UINT32 ctr_ok;
	UINT32 condition_ok;
	UINT32 bo0 = (bo & 0x10) ? 1 : 0;
	UINT32 bo1 = (bo & 0x08) ? 1 : 0;
	UINT32 bo2 = (bo & 0x04) ? 1 : 0;
	UINT32 bo3 = (bo & 0x02) ? 1 : 0;

	if( bo2 == 0 )
		--CTR;

	ctr_ok = bo2 | ((CTR != 0) ^ bo3);
	condition_ok = bo0 | (CRBIT(bi) ^ (~bo1 & 0x1));

	return ctr_ok && condition_ok;
}

INLINE UINT64 ppc_read_timebase(void)
{
	int cycles = ppc_tb_base_icount - ppc_icount;

	// timebase is incremented once every four core clock cycles, so adjust the cycles accordingly
	return ppc.tb + (cycles / 4);
}

INLINE void ppc_write_timebase_l(UINT32 tbl)
{
	ppc_tb_base_icount = ppc_icount;

	ppc.tb &= ~0xffffffff;
	ppc.tb |= tbl;
}

INLINE void ppc_write_timebase_h(UINT32 tbh)
{
	ppc_tb_base_icount = ppc_icount;

	ppc.tb &= 0xffffffff;
	ppc.tb |= (UINT64)(tbh) << 32;
}

INLINE UINT32 read_decrementer(void)
{
	int cycles = ppc_dec_base_icount - ppc_icount;

	// decrementer is decremented once every four bus clock cycles, so adjust the cycles accordingly
	return DEC - (cycles / (bus_freq_multiplier * 2));
}

INLINE void write_decrementer(UINT32 value)
{
	ppc_dec_base_icount = ppc_icount + (ppc_dec_base_icount - ppc_icount) % (bus_freq_multiplier * 2);

	DEC = value;

	// check if decrementer exception occurs during execution
	if ((UINT32)(DEC - (ppc_icount / (bus_freq_multiplier * 2))) > (UINT32)(DEC))
	{
		ppc_dec_trigger_cycle = ((ppc_icount / (bus_freq_multiplier * 2)) - DEC) * 4;
	}
	else
	{
		ppc_dec_trigger_cycle = 0x7fffffff;
	}

	printf("DEC = %08X at %08X\n", value, ppc.pc);
}

/*********************************************************************/

INLINE void ppc_set_spr(int spr, UINT32 value)
{
	switch (spr)
	{
		case SPR_LR:		LR = value; return;
		case SPR_CTR:		CTR = value; return;
		case SPR_XER:		XER = value; return;
		case SPR_SRR0:		ppc.srr0 = value; return;
		case SPR_SRR1:		ppc.srr1 = value; return;
		case SPR_SPRG0:		ppc.sprg[0] = value; return;
		case SPR_SPRG1:		ppc.sprg[1] = value; return;
		case SPR_SPRG2:		ppc.sprg[2] = value; return;
		case SPR_SPRG3:		ppc.sprg[3] = value; return;
		case SPR_PVR:		return;
			
		case SPR603E_DEC:
			if(((value & 0x80000000) && !(DEC & 0x80000000)) || value == 0)
			{
				/* trigger interrupt */
				ppc.interrupt_pending |= 0x2;
				ppc603_check_interrupts();
			}
			write_decrementer(value);
			return;

		case SPR603E_TBL_W:
		case SPR603E_TBL_R: // special 603e case
			ppc_write_timebase_l(value);
			return;

		case SPR603E_TBU_R:
		case SPR603E_TBU_W: // special 603e case
			ppc_write_timebase_h(value);
			return;

		case SPR603E_HID0:			ppc.hid0 = value; return;
		case SPR603E_HID1:			ppc.hid1 = value; return;
		case SPR603E_HID2:			ppc.hid2 = value; return;

		case SPR603E_DSISR:			ppc.dsisr = value; return;
		case SPR603E_DAR:			ppc.dar = value; return;
		case SPR603E_EAR:			ppc.ear = value; return;
		case SPR603E_DMISS:			ppc.dmiss = value; return;
		case SPR603E_DCMP:			ppc.dcmp = value; return;
		case SPR603E_HASH1:			ppc.hash1 = value; return;
		case SPR603E_HASH2:			ppc.hash2 = value; return;
		case SPR603E_IMISS:			ppc.imiss = value; return;
		case SPR603E_ICMP:			ppc.icmp = value; return;
		case SPR603E_RPA:			ppc.rpa = value; return;

		case SPR603E_IBAT0L:		ppc.ibat[0].l = value; return;
		case SPR603E_IBAT0U:		ppc.ibat[0].u = value; return;
		case SPR603E_IBAT1L:		ppc.ibat[1].l = value; return;
		case SPR603E_IBAT1U:		ppc.ibat[1].u = value; return;
		case SPR603E_IBAT2L:		ppc.ibat[2].l = value; return;
		case SPR603E_IBAT2U:		ppc.ibat[2].u = value; return;
		case SPR603E_IBAT3L:		ppc.ibat[3].l = value; return;
		case SPR603E_IBAT3U:		ppc.ibat[3].u = value; return;
		case SPR603E_DBAT0L:		ppc.dbat[0].l = value; return;
		case SPR603E_DBAT0U:		ppc.dbat[0].u = value; return;
		case SPR603E_DBAT1L:		ppc.dbat[1].l = value; return;
		case SPR603E_DBAT1U:		ppc.dbat[1].u = value; return;
		case SPR603E_DBAT2L:		ppc.dbat[2].l = value; return;
		case SPR603E_DBAT2U:		ppc.dbat[2].u = value; return;
		case SPR603E_DBAT3L:		ppc.dbat[3].l = value; return;
		case SPR603E_DBAT3U:		ppc.dbat[3].u = value; return;

		case SPR603E_SDR1:
			ppc.sdr1 = value;
			return;

		case SPR603E_IABR:			ppc.iabr = value; return;
	}

	error("ppc: set_spr: unknown spr %d (%03X) !", spr, spr);
}

INLINE UINT32 ppc_get_spr(int spr)
{
	switch(spr)
	{
		case SPR_LR:		return LR;
		case SPR_CTR:		return CTR;
		case SPR_XER:		return XER;
		case SPR_SRR0:		return ppc.srr0;
		case SPR_SRR1:		return ppc.srr1;
		case SPR_SPRG0:		return ppc.sprg[0];
		case SPR_SPRG1:		return ppc.sprg[1];
		case SPR_SPRG2:		return ppc.sprg[2];
		case SPR_SPRG3:		return ppc.sprg[3];
		case SPR_PVR:		return ppc.pvr;
		case SPR603E_TBL_R:
			error("ppc: get_spr: TBL_R ");
			break;

		case SPR603E_TBU_R:
			error("ppc: get_spr: TBU_R ");
			break;

		case SPR603E_TBL_W:		return (UINT32)(ppc_read_timebase());
		case SPR603E_TBU_W:		return (UINT32)(ppc_read_timebase() >> 32);
		case SPR603E_HID0:		return ppc.hid0;
		case SPR603E_HID1:		return ppc.hid1;
		case SPR603E_HID2:		return ppc.hid2;
		case SPR603E_DEC:		return read_decrementer();
		case SPR603E_SDR1:		return ppc.sdr1;
		case SPR603E_DSISR:		return ppc.dsisr;
		case SPR603E_DAR:		return ppc.dar;
		case SPR603E_EAR:		return ppc.ear;
		case SPR603E_DMISS:		return ppc.dmiss;
		case SPR603E_DCMP:		return ppc.dcmp;
		case SPR603E_HASH1:		return ppc.hash1;
		case SPR603E_HASH2:		return ppc.hash2;
		case SPR603E_IMISS:		return ppc.imiss;
		case SPR603E_ICMP:		return ppc.icmp;
		case SPR603E_RPA:		return ppc.rpa;
		case SPR603E_IBAT0L:	return ppc.ibat[0].l;
		case SPR603E_IBAT0U:	return ppc.ibat[0].u;
		case SPR603E_IBAT1L:	return ppc.ibat[1].l;
		case SPR603E_IBAT1U:	return ppc.ibat[1].u;
		case SPR603E_IBAT2L:	return ppc.ibat[2].l;
		case SPR603E_IBAT2U:	return ppc.ibat[2].u;
		case SPR603E_IBAT3L:	return ppc.ibat[3].l;
		case SPR603E_IBAT3U:	return ppc.ibat[3].u;
		case SPR603E_DBAT0L:	return ppc.dbat[0].l;
		case SPR603E_DBAT0U:	return ppc.dbat[0].u;
		case SPR603E_DBAT1L:	return ppc.dbat[1].l;
		case SPR603E_DBAT1U:	return ppc.dbat[1].u;
		case SPR603E_DBAT2L:	return ppc.dbat[2].l;
		case SPR603E_DBAT2U:	return ppc.dbat[2].u;
		case SPR603E_DBAT3L:	return ppc.dbat[3].l;
		case SPR603E_DBAT3U:	return ppc.dbat[3].u;
	}

	error("ppc: get_spr: unknown spr %d (%03X) !", spr, spr);
	return 0;
}

INLINE void ppc_set_msr(UINT32 value)
{
	if( value & (MSR_ILE | MSR_LE) )
		error("ppc: set_msr: little_endian mode not supported !");

	MSR = value;

	ppc603_check_interrupts();
}

INLINE UINT32 ppc_get_msr(void)
{
	return MSR;
}

INLINE void ppc_set_cr(UINT32 value)
{
	CR(0) = (value >> 28) & 0xf;
	CR(1) = (value >> 24) & 0xf;
	CR(2) = (value >> 20) & 0xf;
	CR(3) = (value >> 16) & 0xf;
	CR(4) = (value >> 12) & 0xf;
	CR(5) = (value >> 8) & 0xf;
	CR(6) = (value >> 4) & 0xf;
	CR(7) = (value >> 0) & 0xf;
}

INLINE UINT32 ppc_get_cr(void)
{
	return CR(0) << 28 | CR(1) << 24 | CR(2) << 20 | CR(3) << 16 | CR(4) << 12 | CR(5) << 8 | CR(6) << 4 | CR(7);
}

/***********************************************************************/

static void (* optable19[1024])(UINT32);
static void (* optable31[1024])(UINT32);
static void (* optable59[1024])(UINT32);
static void (* optable63[1024])(UINT32);
static void (* optable[64])(UINT32);

#include "ppc603.c"

/********************************************************************/

#include "ppc_ops.c"
#include "ppc_ops.h"

/* Initialization and shutdown */

void ppc_base_init(void)
{
	int i,j;

	memset(&ppc, 0, sizeof(ppc));

	for( i=0; i < 64; i++ ) {
		optable[i] = ppc_invalid;
	}
	for( i=0; i < 1024; i++ ) {
		optable19[i] = ppc_invalid;
		optable31[i] = ppc_invalid;
		optable59[i] = ppc_invalid;
		optable63[i] = ppc_invalid;
	}

	/* Fill the opcode tables */
	for( i=0; i < (sizeof(ppc_opcode_common) / sizeof(PPC_OPCODE)); i++ ) {

		switch(ppc_opcode_common[i].code)
		{
			case 19:
				optable19[ppc_opcode_common[i].subcode] = ppc_opcode_common[i].handler;
				break;

			case 31:
				optable31[ppc_opcode_common[i].subcode] = ppc_opcode_common[i].handler;
				break;

			case 59:
			case 63:
				break;

			default:
				optable[ppc_opcode_common[i].code] = ppc_opcode_common[i].handler;
		}

	}

	/* Calculate rotate mask table */
	for( i=0; i < 32; i++ ) {
		for( j=0; j < 32; j++ ) {
			UINT32 mask;
			int mb = i;
			int me = j;
			mask = ((UINT32)0xFFFFFFFF >> mb) ^ ((me >= 31) ? 0 : ((UINT32)0xFFFFFFFF >> (me + 1)));
			if( mb > me )
				mask = ~mask;

			ppc_rotate_mask[i][j] = mask;
		}
	}
}

void ppc_init(const PPC_CONFIG *config)
{
	int pll_config = 0;
	float multiplier;
	int i ;

	ppc_base_init() ;

	optable[48] = ppc_lfs;
	optable[49] = ppc_lfsu;
	optable[50] = ppc_lfd;
	optable[51] = ppc_lfdu;
	optable[52] = ppc_stfs;
	optable[53] = ppc_stfsu;
	optable[54] = ppc_stfd;
	optable[55] = ppc_stfdu;
	optable31[631] = ppc_lfdux;
	optable31[599] = ppc_lfdx;
	optable31[567] = ppc_lfsux;
	optable31[535] = ppc_lfsx;
	optable31[595] = ppc_mfsr;
	optable31[659] = ppc_mfsrin;
	optable31[371] = ppc_mftb;
	optable31[210] = ppc_mtsr;
	optable31[242] = ppc_mtsrin;
	optable31[758] = ppc_dcba;
	optable31[759] = ppc_stfdux;
	optable31[727] = ppc_stfdx;
	optable31[983] = ppc_stfiwx;
	optable31[695] = ppc_stfsux;
	optable31[663] = ppc_stfsx;
	optable31[370] = ppc_tlbia;
	optable31[306] = ppc_tlbie;
	optable31[566] = ppc_tlbsync;
	optable31[310] = ppc_eciwx;
	optable31[438] = ppc_ecowx;

	optable63[264] = ppc_fabsx;
	optable63[21] = ppc_faddx;
	optable63[32] = ppc_fcmpo;
	optable63[0] = ppc_fcmpu;
	optable63[14] = ppc_fctiwx;
	optable63[15] = ppc_fctiwzx;
	optable63[18] = ppc_fdivx;
	optable63[72] = ppc_fmrx;
	optable63[136] = ppc_fnabsx;
	optable63[40] = ppc_fnegx;
	optable63[12] = ppc_frspx;
	optable63[26] = ppc_frsqrtex;
	optable63[22] = ppc_fsqrtx;
	optable63[20] = ppc_fsubx;
	optable63[583] = ppc_mffsx;
	optable63[70] = ppc_mtfsb0x;
	optable63[38] = ppc_mtfsb1x;
	optable63[711] = ppc_mtfsfx;
	optable63[134] = ppc_mtfsfix;
	optable63[64] = ppc_mcrfs;

	optable59[21] = ppc_faddsx;
	optable59[18] = ppc_fdivsx;
	optable59[24] = ppc_fresx;
	optable59[22] = ppc_fsqrtsx;
	optable59[20] = ppc_fsubsx;

	for(i = 0; i < 32; i++)
	{
		optable63[i * 32 | 29] = ppc_fmaddx;
		optable63[i * 32 | 28] = ppc_fmsubx;
		optable63[i * 32 | 25] = ppc_fmulx;
		optable63[i * 32 | 31] = ppc_fnmaddx;
		optable63[i * 32 | 30] = ppc_fnmsubx;
		optable63[i * 32 | 23] = ppc_fselx;

		optable59[i * 32 | 29] = ppc_fmaddsx;
		optable59[i * 32 | 28] = ppc_fmsubsx;
		optable59[i * 32 | 25] = ppc_fmulsx;
		optable59[i * 32 | 31] = ppc_fnmaddsx;
		optable59[i * 32 | 30] = ppc_fnmsubsx;
	}

	for(i = 0; i < 256; i++)
	{
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

	ppc.pvr = config->pvr;

	multiplier = (float)((config->bus_frequency_multiplier >> 4) & 0xf) +
				 (float)(config->bus_frequency_multiplier & 0xf) / 10.0f;
	bus_freq_multiplier = (int)(multiplier * 2);

	switch(config->pvr)
	{
		case PPC_MODEL_603E:	pll_config = mpc603e_pll_config[bus_freq_multiplier-1][config->bus_frequency]; break;
		case PPC_MODEL_603EV:	pll_config = mpc603ev_pll_config[bus_freq_multiplier-1][config->bus_frequency]; break;
		case PPC_MODEL_603R:	pll_config = mpc603r_pll_config[bus_freq_multiplier-1][config->bus_frequency]; break;
		default: break;
	}

	if (pll_config == -1)
	{
		error("PPC: Invalid bus/multiplier combination (bus frequency = %d, multiplier = %1.1f)", config->bus_frequency, multiplier);
	}

	ppc.hid1 = pll_config << 28;
}

void ppc_shutdown(void)
{

}

void ppc_set_irq_line(int irqline)
{
	ppc.interrupt_pending |= 0x1;
	
	ppc603_check_interrupts();
}

UINT32 ppc_get_pc(void)
{
	return ppc.pc;
}

void ppc_set_fetch(PPC_FETCH_REGION * fetch)
{
	ppc.fetch = fetch;
}