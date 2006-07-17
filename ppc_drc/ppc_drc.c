/* Simple PowerPC recompiler */

#include "model3.h"
#include "ppc_drc.h"
#include "genx86.h"

#define COMPILE_OPS					1
#define DISABLE_UNTESTED_OPS		0
#define ENABLE_FASTRAM_PATH			1
#define ENABLE_FASTRAM_PATH_FPU		1
#define COMPILE_FPU_OPS				1

#define DONT_COMPILE_ADD			0
#define DONT_COMPILE_ADDC			0
#define DONT_COMPILE_ADDE			0
#define DONT_COMPILE_ADDI			0
#define DONT_COMPILE_ADDIC			0
#define DONT_COMPILE_ADDIC_RC		0
#define DONT_COMPILE_ADDIS			0
#define DONT_COMPILE_ADDME			0
#define DONT_COMPILE_ADDZE			0
#define DONT_COMPILE_AND			0
#define DONT_COMPILE_ANDC			0
#define DONT_COMPILE_ANDI_RC		0
#define DONT_COMPILE_ANDIS_RC		0
#define DONT_COMPILE_CMP			0
#define DONT_COMPILE_CMPI			0
#define DONT_COMPILE_CMPL			0
#define DONT_COMPILE_CMPLI			0
#define DONT_COMPILE_EXTSB			0
#define DONT_COMPILE_EXTSH			0
#define DONT_COMPILE_LBZ			0
#define DONT_COMPILE_LBZU			0
#define DONT_COMPILE_LBZUX			0
#define DONT_COMPILE_LBZX			0
#define DONT_COMPILE_LHA			0
#define DONT_COMPILE_LHAU			0
#define DONT_COMPILE_LHAUX			0
#define DONT_COMPILE_LHAX			0
#define DONT_COMPILE_LHBRX			1
#define DONT_COMPILE_LHZ			0
#define DONT_COMPILE_LHZU			0
#define DONT_COMPILE_LHZUX			0
#define DONT_COMPILE_LHZX			0
#define DONT_COMPILE_LWBRX			1
#define DONT_COMPILE_LWZ			0
#define DONT_COMPILE_LWZU			0
#define DONT_COMPILE_LWZUX			0
#define DONT_COMPILE_LWZX			0
#define DONT_COMPILE_MFSPR			0		// slow?
#define DONT_COMPILE_MTSPR			0		// slow?
#define DONT_COMPILE_MULHW			0
#define DONT_COMPILE_MULHWU			0
#define DONT_COMPILE_MULLI			0
#define DONT_COMPILE_MULLW			0
#define DONT_COMPILE_NAND			0
#define DONT_COMPILE_NEG			0
#define DONT_COMPILE_NOR			0
#define DONT_COMPILE_OR				0
#define DONT_COMPILE_ORC			0
#define DONT_COMPILE_ORI			0
#define DONT_COMPILE_ORIS			0
#define DONT_COMPILE_RLWIMI			1		// slow?
#define DONT_COMPILE_RLWINM			1		// slow?
#define DONT_COMPILE_RLWNM			1		// slow?
#define DONT_COMPILE_SLW			1		// slow?
#define DONT_COMPILE_SRW			1		// slow?
#define DONT_COMPILE_STB			0
#define DONT_COMPILE_STBU			0
#define DONT_COMPILE_STBUX			0
#define DONT_COMPILE_STBX			0
#define DONT_COMPILE_STH			1
#define DONT_COMPILE_STW			0
#define DONT_COMPILE_STWU			0
#define DONT_COMPILE_STWUX			0
#define DONT_COMPILE_STWX			0
#define DONT_COMPILE_SUBF			0
#define DONT_COMPILE_SUBFC			0
#define DONT_COMPILE_SUBFIC			0
#define DONT_COMPILE_XOR			0
#define DONT_COMPILE_XORI			0
#define DONT_COMPILE_XORIS			0
#define DONT_COMPILE_LFS			0
#define DONT_COMPILE_LFSU			0
#define DONT_COMPILE_LFSUX			0
#define DONT_COMPILE_LFSX			0
#define DONT_COMPILE_LFD			0
#define DONT_COMPILE_LFDU			0
#define DONT_COMPILE_LFDUX			0
#define DONT_COMPILE_LFDX			0
#define DONT_COMPILE_STFS			0
#define DONT_COMPILE_STFSU			0
#define DONT_COMPILE_STFSUX			0
#define DONT_COMPILE_STFSX			0
#define DONT_COMPILE_STFD			0
#define DONT_COMPILE_STFDU			0
#define DONT_COMPILE_STFDUX			0
#define DONT_COMPILE_STFDX			0


void **drccore_get_lookup_ptr(UINT32 pc);
void drccore_insert_dispatcher(void);
void drccore_insert_cycle_check(int cycles, UINT32 pc);
void drccore_insert_sanity_check(UINT32 value);
void drccore_init(void);
void drccore_shutdown(void);
void drccore_reset(void);
void drccore_execute(void);


// for fastram path
extern UINT8 *ram;


#define DRC_END_BLOCK		0x1


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

	BATENT ibat[4];
	BATENT dbat[4];

	int reserved;
	UINT32 reserved_address;

	int interrupt_pending;
	int external_int;

	UINT64 tb;			/* 56-bit timebase register */

	PPC_FETCH_REGION	cur_fetch;
	PPC_FETCH_REGION	* fetch;

	UINT32 dec, dec_frac;
	UINT32 fpscr;

	FPR	fpr[32];
	UINT32 sr[16];
} PPC_REGS;

static int ppc_icount;
static int ppc_stolen_cycles;
static int ppc_decrementer_exception_scheduled;
static int ppc_tb_base_icount;
static int ppc_dec_base_icount;
static int ppc_dec_trigger_cycle;
static int ppc_dec_divider;
static int bus_freq_multiplier = 1;
static PPC_REGS ppc;
static UINT32 ppc_rotate_mask[32][32];


#define TB_DIVIDER	4

INLINE int CYCLES_TO_DEC(int cycles)
{
	return cycles / ppc_dec_divider;
}

INLINE int DEC_TO_CYCLES(int dec)
{
	return dec * ppc_dec_divider;
}



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
	return ppc.tb + (cycles / TB_DIVIDER);
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
	int dec_cycles = CYCLES_TO_DEC(cycles);

	// decrementer is decremented once every four bus clock cycles, so adjust the cycles accordingly
	return DEC - dec_cycles;
}

INLINE void write_decrementer(UINT32 value)
{
	int dec_cycles;
	//ppc_dec_base_icount = ppc_icount + (ppc_dec_base_icount - ppc_icount) % (bus_freq_multiplier * 2);
	ppc_dec_base_icount = ppc_icount;

	DEC = value;

	dec_cycles = CYCLES_TO_DEC(ppc_icount);

	// check if decrementer exception occurs during execution
	if ((UINT32)(DEC - dec_cycles) > (UINT32)(DEC))
	{
		ppc_dec_trigger_cycle = DEC_TO_CYCLES((dec_cycles - DEC));
		
		ppc_stolen_cycles += ppc_dec_trigger_cycle;
		ppc_icount = ppc_icount - ppc_dec_trigger_cycle;

		ppc_decrementer_exception_scheduled = 1;
	}
	else
	{
		ppc_dec_trigger_cycle = 0x7fffffff;

		ppc_decrementer_exception_scheduled = 0;
	}
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
				//ppc.interrupt_pending |= 0x2;
				ppc.interrupt_pending |= 0x2;
				if (ppc.msr & MSR_EE)
				{
					ppc_stolen_cycles += ppc_icount;
					ppc_icount = 0;
				}

				DEC = value;
				return;
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

	if (value & MSR_EE)
	{
		if (ppc.interrupt_pending != 0)
		{
			ppc_stolen_cycles = ppc_icount;
			ppc_icount = 0;
		}
	}

	MSR = value;
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

static UINT32 (* drc_op_table19[1024])(UINT32);
static UINT32 (* drc_op_table31[1024])(UINT32);
static UINT32 (* drc_op_table59[1024])(UINT32);
static UINT32 (* drc_op_table63[1024])(UINT32);
static UINT32 (* drc_op_table[64])(UINT32);

#include "ppc_itp/ppc_ops.c"
#include "ppc_drc_ops.c"

static void init_ppc_drc(void)
{
	int i;
	for (i=0; i < 64; i++)
	{
		drc_op_table[i] = drc_invalid;
	}
	for (i=0; i < 1024; i++)
	{
		drc_op_table19[i] = drc_invalid;
		drc_op_table31[i] = drc_invalid;
		drc_op_table59[i] = drc_invalid;
		drc_op_table63[i] = drc_invalid;
	}

	drc_op_table[ 3] = drc_twi;
	drc_op_table[ 7] = drc_mulli;
	drc_op_table[ 8] = drc_subfic;
	drc_op_table[10] = drc_cmpli;
	drc_op_table[11] = drc_cmpi;
	drc_op_table[12] = drc_addic;
	drc_op_table[13] = drc_addic_rc;
	drc_op_table[14] = drc_addi;
	drc_op_table[15] = drc_addis;
	drc_op_table[16] = drc_bc;
	drc_op_table[17] = drc_sc;
	drc_op_table[18] = drc_b;
	drc_op_table[20] = drc_rlwimi;
	drc_op_table[21] = drc_rlwinm;
	drc_op_table[23] = drc_rlwnm;
	drc_op_table[24] = drc_ori;
	drc_op_table[25] = drc_oris;
	drc_op_table[26] = drc_xori;
	drc_op_table[27] = drc_xoris;
	drc_op_table[28] = drc_andi_rc;
	drc_op_table[29] = drc_andis_rc;
	drc_op_table[32] = drc_lwz;
	drc_op_table[33] = drc_lwzu;
	drc_op_table[34] = drc_lbz;
	drc_op_table[35] = drc_lbzu;
	drc_op_table[36] = drc_stw;
	drc_op_table[37] = drc_stwu;
	drc_op_table[38] = drc_stb;
	drc_op_table[39] = drc_stbu;
	drc_op_table[40] = drc_lhz;
	drc_op_table[41] = drc_lhzu;
	drc_op_table[42] = drc_lha;
	drc_op_table[43] = drc_lhau;
	drc_op_table[44] = drc_sth;
	drc_op_table[45] = drc_sthu;
	drc_op_table[46] = drc_lmw;
	drc_op_table[47] = drc_stmw;
	drc_op_table[48] = drc_lfs;
	drc_op_table[49] = drc_lfsu;
	drc_op_table[50] = drc_lfd;
	drc_op_table[51] = drc_lfdu;
	drc_op_table[52] = drc_stfs;
	drc_op_table[53] = drc_stfsu;
	drc_op_table[54] = drc_stfd;
	drc_op_table[55] = drc_stfdu;

	drc_op_table19[   0] = drc_mcrf;
	drc_op_table19[  16] = drc_bclr;
	drc_op_table19[  33] = drc_crnor;
	drc_op_table19[  50] = drc_rfi;
	drc_op_table19[ 129] = drc_crandc;
	drc_op_table19[ 150] = drc_isync;
	drc_op_table19[ 193] = drc_crxor;
	drc_op_table19[ 225] = drc_crnand;
	drc_op_table19[ 257] = drc_crand;
	drc_op_table19[ 289] = drc_creqv;
	drc_op_table19[ 417] = drc_crorc;
	drc_op_table19[ 449] = drc_cror;
	drc_op_table19[ 528] = drc_bcctr;

	drc_op_table31[0x000] = drc_cmp;
	drc_op_table31[0x004] = drc_tw;
	drc_op_table31[0x008] = drc_subfc;
	drc_op_table31[0x208] = drc_subfc;
	drc_op_table31[0x00a] = drc_addc;
	drc_op_table31[0x20a] = drc_addc;
	drc_op_table31[0x00b] = drc_mulhwu;
	drc_op_table31[0x013] = drc_mfcr;
	drc_op_table31[0x014] = drc_lwarx;
	drc_op_table31[0x017] = drc_lwzx;
	drc_op_table31[0x018] = drc_slw;
	drc_op_table31[0x01a] = drc_cntlzw;
	drc_op_table31[0x01c] = drc_and;
	drc_op_table31[0x020] = drc_cmpl;
	drc_op_table31[0x028] = drc_subf;
	drc_op_table31[0x228] = drc_subf;
	drc_op_table31[0x036] = drc_dcbst;
	drc_op_table31[0x037] = drc_lwzux;
	drc_op_table31[0x03c] = drc_andc;
	drc_op_table31[0x04b] = drc_mulhw;
	drc_op_table31[0x053] = drc_mfmsr;
	drc_op_table31[0x056] = drc_dcbf;
	drc_op_table31[0x057] = drc_lbzx;
	drc_op_table31[0x068] = drc_neg;
	drc_op_table31[0x268] = drc_neg;
	drc_op_table31[0x077] = drc_lbzux;
	drc_op_table31[0x07c] = drc_nor;
	drc_op_table31[0x088] = drc_subfe;
	drc_op_table31[0x288] = drc_subfe;
	drc_op_table31[0x08a] = drc_adde;
	drc_op_table31[0x28a] = drc_adde;
	drc_op_table31[0x090] = drc_mtcrf;
	drc_op_table31[0x092] = drc_mtmsr;
	drc_op_table31[0x096] = drc_stwcx_rc;
	drc_op_table31[0x097] = drc_stwx;
	drc_op_table31[0x0b7] = drc_stwux;
	drc_op_table31[0x0c8] = drc_subfze;
	drc_op_table31[0x2c8] = drc_subfze;
	drc_op_table31[0x0ca] = drc_addze;
	drc_op_table31[0x2ca] = drc_addze;
	drc_op_table31[0x0d2] = drc_mtsr;
	drc_op_table31[0x0d7] = drc_stbx;
	drc_op_table31[0x0e8] = drc_subfme;
	drc_op_table31[0x2e8] = drc_subfme;
	drc_op_table31[0x0ea] = drc_addme;
	drc_op_table31[0x2ea] = drc_addme;
	drc_op_table31[0x0eb] = drc_mullw;
	drc_op_table31[0x2eb] = drc_mullw;
	drc_op_table31[0x0f2] = drc_mtsrin;
	drc_op_table31[0x0f6] = drc_dcbtst;
	drc_op_table31[0x0f7] = drc_stbux;
	drc_op_table31[0x10a] = drc_add;
	drc_op_table31[0x30a] = drc_add;
	drc_op_table31[0x116] = drc_dcbt;
	drc_op_table31[0x117] = drc_lhzx;
	drc_op_table31[0x11c] = drc_eqv;
	drc_op_table31[0x132] = drc_tlbie;
	drc_op_table31[0x136] = drc_eciwx;
	drc_op_table31[0x137] = drc_lhzux;
	drc_op_table31[0x13c] = drc_xor;
	drc_op_table31[0x153] = drc_mfspr;
	drc_op_table31[0x157] = drc_lhax;
	drc_op_table31[0x172] = drc_tlbia;
	drc_op_table31[0x173] = drc_mftb;
	drc_op_table31[0x177] = drc_lhaux;
	drc_op_table31[0x197] = drc_sthx;
	drc_op_table31[0x19c] = drc_orc;
	drc_op_table31[0x1b6] = drc_ecowx;
	drc_op_table31[0x1b7] = drc_sthux;
	drc_op_table31[0x1bc] = drc_or;
	drc_op_table31[0x1cb] = drc_divwu;
	drc_op_table31[0x3cb] = drc_divwu;
	drc_op_table31[0x1d3] = drc_mtspr;
	drc_op_table31[0x1d6] = drc_dcbi;
	drc_op_table31[0x1dc] = drc_nand;
	drc_op_table31[0x1eb] = drc_divw;
	drc_op_table31[0x3eb] = drc_divw;
	drc_op_table31[0x200] = drc_mcrxr;
	drc_op_table31[0x215] = drc_lswx;
	drc_op_table31[0x216] = drc_lwbrx;
	drc_op_table31[0x217] = drc_lfsx;
	drc_op_table31[0x218] = drc_srw;
	drc_op_table31[0x236] = drc_tlbsync;
	drc_op_table31[0x237] = drc_lfsux;
	drc_op_table31[0x253] = drc_mfsr;
	drc_op_table31[0x255] = drc_lswi;
	drc_op_table31[0x256] = drc_sync;
	drc_op_table31[0x257] = drc_lfdx;
	drc_op_table31[0x277] = drc_lfdux;
	drc_op_table31[0x293] = drc_mfsrin;
	drc_op_table31[0x295] = drc_stswx;
	drc_op_table31[0x296] = drc_stwbrx;
	drc_op_table31[0x297] = drc_stfsx;
	drc_op_table31[0x2b7] = drc_stfsux;
	drc_op_table31[0x2d5] = drc_stswi;
	drc_op_table31[0x2d7] = drc_stfdx;
	drc_op_table31[0x2f6] = drc_dcba;
	drc_op_table31[0x2f7] = drc_stfdux;
	drc_op_table31[0x316] = drc_lhbrx;
	drc_op_table31[0x318] = drc_sraw;
	drc_op_table31[0x338] = drc_srawi;
	drc_op_table31[0x356] = drc_eieio;
	drc_op_table31[0x396] = drc_sthbrx;
	drc_op_table31[0x39a] = drc_extsh;
	drc_op_table31[0x3ba] = drc_extsb;
	drc_op_table31[0x3d6] = drc_icbi;
	drc_op_table31[0x3d7] = drc_stfiwx;
	drc_op_table31[0x3f6] = drc_dcbz;

	drc_op_table59[0x012] = drc_fdivs;
	drc_op_table59[0x014] = drc_fsubs;
	drc_op_table59[0x015] = drc_fadds;
	drc_op_table59[0x016] = drc_fsqrts;
	drc_op_table59[0x018] = drc_fres;
	for (i=0; i < 32; i++)
	{
		drc_op_table59[(i << 5) | 0x019] = drc_fmuls;
		drc_op_table59[(i << 5) | 0x01c] = drc_fmsubs;
		drc_op_table59[(i << 5) | 0x01d] = drc_fmadds;
		drc_op_table59[(i << 5) | 0x01e] = drc_fnmsubs;
		drc_op_table59[(i << 5) | 0x01f] = drc_fnmadds;
	}

	drc_op_table63[0x000] = drc_fcmpu;
	drc_op_table63[0x00c] = drc_frsp;
	drc_op_table63[0x00e] = drc_fctiw;
	drc_op_table63[0x00f] = drc_fctiwz;
	drc_op_table63[0x012] = drc_fdiv;
	drc_op_table63[0x014] = drc_fsub;
	drc_op_table63[0x015] = drc_fadd;
	drc_op_table63[0x016] = drc_fsqrt;
	drc_op_table63[0x01a] = drc_frsqrte;
	drc_op_table63[0x020] = drc_fcmpo;
	drc_op_table63[0x026] = drc_mtfsb1;
	drc_op_table63[0x028] = drc_fneg;
	drc_op_table63[0x040] = drc_mcrfs;
	drc_op_table63[0x046] = drc_mtfsb0;
	drc_op_table63[0x048] = drc_fmr;
	drc_op_table63[0x086] = drc_mtfsfi;
	drc_op_table63[0x088] = drc_fnabs;
	drc_op_table63[0x108] = drc_fabs;
	drc_op_table63[0x247] = drc_mffs;
	drc_op_table63[0x2c7] = drc_mtfsf;
	for (i=0; i < 32; i++)
	{
		drc_op_table63[(i << 5) | 0x017] = drc_fsel;
		drc_op_table63[(i << 5) | 0x019] = drc_fmul;
		drc_op_table63[(i << 5) | 0x01c] = drc_fmsub;
		drc_op_table63[(i << 5) | 0x01d] = drc_fmadd;
		drc_op_table63[(i << 5) | 0x01e] = drc_fnmsub;
		drc_op_table63[(i << 5) | 0x01f] = drc_fnmadd;
	}

	drccore_init();
}

void ppc_init(const PPC_CONFIG *config)
{
	int pll_config = 0;
	float multiplier;
	int i, j;

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

	ppc_dec_divider = (int)(multiplier * 4);

	if (pll_config == -1)
	{
		error("PPC: Invalid bus/multiplier combination (bus frequency = %d, multiplier = %1.1f)", config->bus_frequency, multiplier);
	}

	ppc.hid1 = pll_config << 28;

	init_ppc_drc();
}

void ppc_shutdown(void)
{
	drccore_shutdown();
}

void ppc_exception(int exception)
{
	switch (exception)
	{
		case EXCEPTION_IRQ:		/* External Interrupt */
		{
			if( ppc_get_msr() & MSR_EE )
			{
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.pc;
				SRR1 = msr & 0xff73;

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.pc = 0xfff00000 | 0x0500;
				else
					ppc.pc = 0x00000000 | 0x0500;

				ppc.interrupt_pending &= ~0x1;
				ppc_change_pc(ppc.pc);
			}
			break;
		}

		case EXCEPTION_DECREMENTER:		/* Decrementer overflow exception */
		{
			if (ppc_get_msr() & MSR_EE)
			{
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.pc;
				SRR1 = msr & 0xff73;

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.pc = 0xfff00000 | 0x0900;
				else
					ppc.pc = 0x00000000 | 0x0900;

				ppc.interrupt_pending &= ~0x2;
				ppc_change_pc(ppc.pc);
			}
			break;
		}

		case EXCEPTION_TRAP:			/* Program exception / Trap */
		{
			UINT32 msr = ppc_get_msr();

			SRR0 = ppc.pc;
			SRR1 = (msr & 0xff73) | 0x20000;	/* 0x20000 = TRAP bit */

			msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
			if( msr & MSR_ILE )
				msr |= MSR_LE;
			else
				msr &= ~MSR_LE;
			ppc_set_msr(msr);

			if( msr & MSR_IP )
				ppc.pc = 0xfff00000 | 0x0700;
			else
				ppc.pc = 0x00000000 | 0x0700;
			ppc_change_pc(ppc.pc);
		}
		break;

		case EXCEPTION_SYSTEM_CALL:		/* System call */
		{
			UINT32 msr = ppc_get_msr();

			SRR0 = ppc.pc;
			SRR1 = (msr & 0xff73);

			msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
			if( msr & MSR_ILE )
				msr |= MSR_LE;
			else
				msr &= ~MSR_LE;
			ppc_set_msr(msr);

			if( msr & MSR_IP )
				ppc.pc = 0xfff00000 | 0x0c00;
			else
				ppc.pc = 0x00000000 | 0x0c00;
			ppc_change_pc(ppc.pc);
		}
		break;

		default:
			error("ppc: Unhandled exception %d", exception);
			break;
	}
}

void ppc_set_irq_line(int irqline)
{
	ppc.interrupt_pending |= 0x1;

	if (ppc.msr & MSR_EE)
	{
		ppc_stolen_cycles += ppc_icount;
		ppc_icount = 0;
	}
}

UINT32 ppc_get_pc(void)
{
	return ppc.pc;
}

void ppc_set_fetch(PPC_FETCH_REGION * fetch)
{
	ppc.fetch = fetch;
}

void ppc_reset(void)
{
	ppc.pc = ppc.npc = 0xfff00100;

	ppc_set_msr(0x40);
	ppc_change_pc(ppc.pc);

	ppc.hid0 = 1;

	ppc.interrupt_pending = 0;

	drccore_reset();
}

void ppc_check_interrupts(void)
{
	if (ppc.msr & MSR_EE)
	{
		if (ppc.interrupt_pending & 0x1)
		{
			ppc_exception(EXCEPTION_IRQ);
		}
		else if (ppc.interrupt_pending & 0x2)
		{
			ppc_exception(EXCEPTION_DECREMENTER);
		}
	}
}

int ppc_execute(int cycles)
{
	int run_cycles;
	int cumulative_cycles;
	int cycles_left;
	UINT32 opcode;
	
	/*
	ppc_icount = cycles;
	ppc_tb_base_icount = cycles;
	ppc_dec_base_icount = cycles + ppc.dec_frac;

	// check if decrementer exception occurs during execution
	if ((UINT32)(DEC - (cycles / (bus_freq_multiplier * 2))) > (UINT32)(DEC))
	{
		ppc_dec_trigger_cycle = ((cycles / (bus_freq_multiplier * 2)) - DEC) * 4;
	}
	else
	{
		ppc_dec_trigger_cycle = 0x7fffffff;
	}

	run_cycles = ppc_icount;
	ppc_icount = run_cycles / 2;

	ppc_change_pc(ppc.pc);
	ppc_check_interrupts();
	drccore_execute();

	ppc_icount = run_cycles / 2;
	ppc.interrupt_pending |= 2;
	ppc_change_pc(ppc.pc);
	ppc_check_interrupts();
	drccore_execute();
	*/

	cumulative_cycles = 0;
	cycles_left = cycles;
	while (cumulative_cycles < cycles)
	{
		int dec_count;
		int real_cycles;
		int target_cycles;
		ppc_stolen_cycles = 0;

		ppc_decrementer_exception_scheduled = 0;

		//ppc_change_pc(ppc.pc);
		ppc_check_interrupts();

		//ppc_tb_base_icount = cycles_left;
		//ppc_dec_base_icount = cycles_left + ppc.dec_frac;

		// check if decrementer exception occurs during execution
		dec_count = CYCLES_TO_DEC(cycles_left);
		if ((UINT32)(DEC - dec_count) > (UINT32)(DEC))
		{
			ppc_dec_trigger_cycle = DEC_TO_CYCLES((dec_count - DEC));
		}
		else
		{
			ppc_dec_trigger_cycle = 0x7fffffff;
		}

		if (ppc_dec_trigger_cycle == cycles_left)
		{
			ppc_dec_trigger_cycle = 0x7fffffff;
		}

		if (ppc_dec_trigger_cycle != 0x7fffffff)
		{
			target_cycles = ppc_icount = cycles_left - ppc_dec_trigger_cycle;
			ppc_decrementer_exception_scheduled = 1;
		}
		else
		{
			target_cycles = ppc_icount = cycles_left;
			ppc_decrementer_exception_scheduled = 0;
		}

		ppc_tb_base_icount = ppc_icount;
		ppc_dec_base_icount = ppc_icount + ppc.dec_frac;
		
		drccore_execute();
		if (ppc_decrementer_exception_scheduled)
		{
			ppc.interrupt_pending |= 2;
			//ppc_change_pc(ppc.pc);
			//ppc_check_interrupts();
		}

		real_cycles = (target_cycles - ppc_stolen_cycles) - ppc_icount;
		cycles_left -= real_cycles;
		cumulative_cycles += real_cycles;

//		printf("run for %d cycles, stolen cycles %d, trigger cycle %d\n", real_cycles, ppc_stolen_cycles, ppc_dec_trigger_cycle);

		{
			int tb_cycles = (ppc_tb_base_icount) - ppc_stolen_cycles;
			int dec_cycles = (ppc_dec_base_icount) - ppc_stolen_cycles;

			// update timebase
			// timebase is incremented once every four core clock cycles, so adjust the cycles accordingly
			ppc.tb += ((tb_cycles) / TB_DIVIDER);

			// update decrementer
			ppc.dec_frac = ((dec_cycles) % (bus_freq_multiplier * 2));
			DEC -= CYCLES_TO_DEC(dec_cycles);
		}
	}

	/*
	// update timebase
	// timebase is incremented once every four core clock cycles, so adjust the cycles accordingly
	ppc.tb += ((ppc_tb_base_icount - ppc_icount) / 4);

	// update decrementer
	ppc.dec_frac = ((ppc_dec_base_icount - ppc_icount) % (bus_freq_multiplier * 2));
	DEC -= ((ppc_dec_base_icount - ppc_icount) / (bus_freq_multiplier * 2));
	*/

	/*
	{
		char string1[200];
		char string2[200];
		opcode = BSWAP32(*ppc.op);
		DisassemblePowerPC(opcode, ppc.pc, string1, string2, TRUE);
		printf("%08X: %s %s\n", ppc.pc, string1, string2);
	}
	*/

	return cycles - ppc_icount;
}




static void **pc_lookup;
static void **pc_ram_lookup;
static void **pc_rom_lookup;
static void **pc_dummy_lookup;

#define PC_LOOKUP_SIZE		0x200		// 9 top bits of address
#define PC_LOOKUP_SIZE_SUB	0x200000	// 23 bits - 2 unused LSB bits

#define PC_LOOKUP_SHIFT		23			// how much to shift address to index main lookup

static void (* drc_entry_point)(void);
static void (* drc_exit_point)(void);
static void (* drc_compiler_point)(void);
static void (* drc_invalid_area_point)(void);
static void ( *drc_check_interrupts)(void);
static void (* drc_irq_exception)(void);
static void (* drc_decrementer_exception)(void);

void **drccore_get_lookup_ptr(UINT32 pc)
{
	switch (pc >> PC_LOOKUP_SHIFT)
	{
		case 0x000:		return &pc_ram_lookup[(pc & 0x7fffff) / 4];
		case 0x1ff:		return &pc_rom_lookup[(pc & 0x7fffff) / 4];
		default:		return &pc_dummy_lookup[(pc & 0x7fffff) / 4];
	}
}

void drccore_insert_dispatcher(void)
{
	gen(MOVMR, REG_ECX, (UINT32)&ppc.pc);
	gen(MOV, REG_EAX, REG_ECX);
	gen(SHRI, REG_EAX, PC_LOOKUP_SHIFT);
	gen_mov_dprs_to_reg(REG_EAX, (UINT32)pc_lookup, REG_EAX, 4);
	gen(ANDI, REG_ECX, (PC_LOOKUP_SIZE_SUB - 1) << 2);
	gen_jmp_rpr(REG_EAX, REG_ECX);
}

static void out_of_cycles_sanity_check(void)
{
	printf("out of cycles sanity check\n");
}

void drccore_insert_cycle_check(int cycles, UINT32 pc)
{
	JUMP_TARGET timeslice_not_expired;
	JUMP_TARGET no_decrementer_check;
	JUMP_TARGET no_decrementer_exception;
	JUMP_TARGET no_interrupts;
	JUMP_TARGET interrupts_not_enabled;
	init_jmp_target(&timeslice_not_expired);
	init_jmp_target(&no_decrementer_check);
	init_jmp_target(&no_decrementer_exception);
	init_jmp_target(&no_interrupts);
	init_jmp_target(&interrupts_not_enabled);


	//gen(SUBIM, (UINT32)&ppc_icount, cycles);

	// AAAAAARGH this is complex :-/
	
	/*gen(MOVMR, REG_EAX, (UINT32)&ppc_icount);
	gen(CMPIM, (UINT32)&ppc_dec_trigger_cycle, 0x7fffffff);
	gen_jmp(JNZ, &no_decrementer_check);

	gen(MOVMR, REG_EDX, (UINT32)&ppc_dec_trigger_cycle);
	gen(CMP, REG_EDX, REG_EAX);
	gen_jmp(JNS, &no_decrementer_exception);
	
	gen(ORIM, (UINT32)&ppc.interrupt_pending, 0x2);
	gen_jmp_target(&no_decrementer_exception);

	gen_jmp_target(&no_decrementer_check);



	gen(CMPIM, (UINT32)&ppc.interrupt_pending, 0);
	gen_jmp(JZ, &no_interrupts);

	gen(MOVMR, REG_EDX, (UINT32)&ppc.msr);
	gen(TESTI, REG_EDX, MSR_EE);
	gen_jmp(JZ, &interrupts_not_enabled);
	gen(MOVIM, (UINT32)&ppc.pc, pc);
	gen(JMPI, (UINT32)drc_check_interrupts, 0);
	
	gen_jmp_target(&interrupts_not_enabled);

	gen_jmp_target(&no_interrupts);

	gen(CMPI, REG_EAX, 0);
	*/

	gen(CMPIM, (UINT32)&ppc_icount, 0);
	
	gen_jmp(JNS, &timeslice_not_expired);		// continue if cycles >= 0
	gen(MOVIM, (UINT32)&ppc.pc, pc);
	gen(JMPI, (UINT32)drc_exit_point, 0);

	gen_jmp_target(&timeslice_not_expired);
}

static void sanity_check(UINT32 value)
{
	printf("ppc_sanity_check: %08X, cycles = %d\n", value, ppc_icount);
	//printf("CTR = %08X, CR0 = %01X, CR1 = %01X\n", ppc.ctr, ppc.cr[0], ppc.cr[1]);
}

void drccore_insert_sanity_check(UINT32 value)
{
	//gen(PUSHI, value, 0);
	//gen(CALLI, (UINT32)&sanity_check, 0);
	//gen(ADDI, REG_ESP, 4);
}

static UINT32 entry_point_stackptr;
static void exit_point_sanity_check(UINT32 stackptr)
{
	printf("exit point sanity check: current ESP %08X, start %08X\n", stackptr, entry_point_stackptr);
}

static void check_irqs_sanity_check(void)
{
	printf("check IRQs sanity check\n");
}

static void exception_sanity_check(void)
{
	printf("exception sanity check\n");
}

static void drccore_insert_check_interrupts(void)
{
	JUMP_TARGET interrupt_not_irq;
	init_jmp_target(&interrupt_not_irq);

	gen(MOVMR, REG_EAX, (UINT32)&ppc.interrupt_pending);
	gen(TESTI, REG_EAX, 0x1);
	gen_jmp(JZ, &interrupt_not_irq);
	// IRQ
	gen(MOVMR, REG_EDX, (UINT32)&ppc.pc);
	gen(MOVRM, (UINT32)&ppc.srr0, REG_EDX);
	gen(JMPM, (UINT32)&drc_irq_exception, 0);

	gen_jmp_target(&interrupt_not_irq);
	// decrementer
	gen(MOVMR, REG_EDX, (UINT32)&ppc.pc);
	gen(MOVRM, (UINT32)&ppc.srr0, REG_EDX);
	gen(JMPM, (UINT32)&drc_decrementer_exception, 0);
}

static void drccore_insert_exception_generator(int exception)
{
	JUMP_TARGET clear_le, exception_base_0;
	init_jmp_target(&clear_le);
	init_jmp_target(&exception_base_0);

	gen(MOVMR, REG_EAX, (UINT32)&ppc.msr);
	gen(ANDI, REG_EAX, 0xff73);
	gen(MOVRM, (UINT32)&ppc.srr1, REG_EAX);

	// Clear POW, EE, PR, FP, FE0, SE, BE, FE1, IR, DR, RI
	gen(ANDI, REG_EAX, ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI));

	// Set LE to ILE
	gen(ANDI, REG_EAX, ~MSR_LE);		// clear LE first
	gen(TESTI, REG_EAX, MSR_ILE);
	gen_jmp(JZ, &clear_le);				// if Z == 0, bit = 1
	gen(ORI, REG_EAX, MSR_LE);			// set LE
	gen_jmp_target(&clear_le);

	gen(MOVRM, (UINT32)&ppc.msr, REG_EAX);

	switch (exception)
	{
		case EXCEPTION_IRQ:
		{
			gen(MOVI, REG_EDI, 0x00000500);			// exception vector
			gen(TESTI, REG_EAX, MSR_IP);
			gen_jmp(JZ, &exception_base_0);			// if Z == 1, bit = 0 means base == 0x00000000
			gen(ORI, REG_EDI, 0xfff00000);
			gen_jmp_target(&exception_base_0);

			gen(ANDIM, (UINT32)&ppc.interrupt_pending, ~0x1);	// clear decrementer exception

			gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
			drccore_insert_dispatcher();
			break;
		}
		case EXCEPTION_DECREMENTER:
		{
			gen(MOVI, REG_EDI, 0x00000900);			// exception vector
			gen(TESTI, REG_EAX, MSR_IP);
			gen_jmp(JZ, &exception_base_0);			// if Z == 1, bit = 0 means base == 0x00000000
			gen(ORI, REG_EDI, 0xfff00000);
			gen_jmp_target(&exception_base_0);

			gen(ANDIM, (UINT32)&ppc.interrupt_pending, ~0x2);	// clear decrementer exception

			gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
			drccore_insert_dispatcher();
			break;
		}
	}
}

void drccore_init(void)
{
	int i;

	init_genx86();

	pc_lookup = malloc(sizeof(void*) * PC_LOOKUP_SIZE);

	// these lookups are the only ones we need runtime for Model 3
	pc_ram_lookup = malloc(sizeof(void*) * PC_LOOKUP_SIZE_SUB);
	pc_rom_lookup = malloc(sizeof(void*) * PC_LOOKUP_SIZE_SUB);

	// this table will catch all the possible jumps outside ram/rom
	pc_dummy_lookup = malloc(sizeof(void*) * PC_LOOKUP_SIZE_SUB);

	// initialize main lookup
	for (i=0; i < PC_LOOKUP_SIZE; i++)
	{
		pc_lookup[i] = pc_dummy_lookup;
	}

	// we know that rom is at location 0xff800000 - 0xffffffff
	pc_lookup[0x1ff] = pc_rom_lookup;
	// we know that ram is at location 0x00000000 - 0x007fffff
	pc_lookup[0x000] = pc_ram_lookup;
}

void drccore_shutdown(void)
{
	shutdown_genx86();
}

void drccore_reset(void)
{
	int i;

	gen_reset_cache();

	// insert dynarec entry point
	drc_entry_point = (void*)gen_get_cache_pos();
	gen(PUSHAD, 0, 0);
	drccore_insert_dispatcher();

	// insert dynarec exit point
	drc_exit_point = (void*)gen_get_cache_pos();
	gen(POPAD, 0, 0);
	gen(RET, 0, 0);

	// insert dynarec compiler point
	drc_compiler_point = (void*)gen_get_cache_pos();
	gen(CALLI, (UINT32)drc_recompile_block, 0);
	drccore_insert_dispatcher();

	// insert dynarec invalid area point
	drc_invalid_area_point = (void*)gen_get_cache_pos();
	gen(CALLI, (UINT32)drc_invalid_area, 0);

	// insert check interrupts
	drc_check_interrupts = (void*)gen_get_cache_pos();
	drccore_insert_check_interrupts();

	// insert irq exception generator
	drc_irq_exception = (void*)gen_get_cache_pos();
	drccore_insert_exception_generator(EXCEPTION_IRQ);

	// insert decrementer exception generator
	drc_decrementer_exception = (void*)gen_get_cache_pos();
	drccore_insert_exception_generator(EXCEPTION_DECREMENTER);

	// initialize sub lookups
	for (i=0; i < PC_LOOKUP_SIZE_SUB; i++)
	{
		// point valid areas to recompile handler
		pc_ram_lookup[i] = (void*)drc_compiler_point;
		pc_rom_lookup[i] = (void*)drc_compiler_point;

		// point invalid areas to invalid area handler
		pc_dummy_lookup[i] = (void*)drc_invalid_area;
	}
}

void drccore_execute(void)
{
	drc_entry_point();
}