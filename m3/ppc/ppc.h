////////////////////////////////////////////////////////////////
// Portable PowerPC Emulator

#ifndef _PPC_H_
#define _PPC_H_

#define PPC_OKAY    0

////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

////////////////////////////////////////////////////////////////

#ifndef s8

#define s8	signed char
#define s16	signed short
#define s32	signed long

#define u8	unsigned char
#define u16	unsigned short
#define u32	unsigned long

#ifdef __GNUC__

#define s64	signed long long
#define u64	unsigned long long

#else

#define u64	signed __int64
#define s64	unsigned __int64

#endif

#define f32	float
#define f64	double

#endif

#ifndef INLINE
#define INLINE
#endif

////////////////////////////////////////////////////////////////
// Configuration

#define PPC_USE_TRACE_CALLBACK

////////////////////////////////////////////////////////////////
// Context

typedef union fpr_t {

	u32 iw;	/* integer word */
	u64 id;	/* integer double word */
	f64 fd; /* floating-point double */

} fpr_t;

typedef struct ppc_t {

	// General Purpose

	u32	gpr[32];
	fpr_t fpr[32];

	u32	cia;
	u32	nia;
	u32	ir;
	u32	msr;
	u32	fpscr;
	u32	count;
	u8	cr[8];
	u32	sr[16];

	u32	tgpr[4];
	u32 dec_expired;

	u32	reserved;

	// Special Purpose

	u32	spr[1024];

	// Device Control

	u32	dcr[256];

	// Interrupts

	u32 irq_state;
	u32 (* irq_callback)(void);

	// Memory Management

	struct {

		u32 dunno;

	} itlb[64], dtlb[64];

	// Timers

	u64	tb;

	/* u64 ts; -- timestamp, sum of all cycles ran so far */

	u32	pit_reload;
	u32	pit_enabled;

	// Serial Controller

	struct {

		u8	spls;
		u8	sphs;
		u8	brdh;
		u8	brdl;
		u8	spctl;
		u8	sprc;
		u8	sptc;
		u8	sprb;
		u8	sptb;

	} spu;

	// Internal Handlers

	u32		(* fetch)(u32);

	u32		(* irb)(u32);
	u32		(* irw)(u32);
	u32		(* ird)(u32);
	u64		(* irq)(u32);
	void	(* iwb)(u32, u32);
	void	(* iww)(u32, u32);
	void	(* iwd)(u32, u32);
	void	(* iwq)(u32, u64);

	u32		(* get_spr)(u32);
	void	(* set_spr)(u32, u32);

	u32		(* get_msr)(void);
	void	(* set_msr)(u32);

	void	(* test_irq_event)(void);

	// Handlers

	u32		(* rb)(u32);
	u32		(* rw)(u32);
	u32		(* rd)(u32);
	u64		(* rq)(u32);
	void	(* wb)(u32, u32);
	void	(* ww)(u32, u32);
	void	(* wd)(u32, u32);
	void	(* wq)(u32, u64);

	// Callbacks

	void	(* trace)(u32, u32);
	u32		(* interrupt_ack)(u32);

	// Config

	u32		type;

} ppc_t;

extern ppc_t ppc; // the internal context

////////////////////////////////////////////////////////////////
// Defines

typedef enum {

	PPC_TYPE_4XX,
	PPC_TYPE_6XX,
	PPC_TYPE_7XX,

} PPC_TYPE;

typedef enum {

	PPC_REG_PC,
	PPC_REG_IR,

	PPC_REG_R0, PPC_REG_R1, PPC_REG_R2, PPC_REG_R3,
	PPC_REG_R4, PPC_REG_R5, PPC_REG_R6, PPC_REG_R7,
	PPC_REG_R8, PPC_REG_R9, PPC_REG_R10, PPC_REG_R11,
	PPC_REG_R12, PPC_REG_R13, PPC_REG_R14, PPC_REG_R15,
	PPC_REG_R16, PPC_REG_R17, PPC_REG_R18, PPC_REG_R19,
	PPC_REG_R20, PPC_REG_R21, PPC_REG_R22, PPC_REG_R23,
	PPC_REG_R24, PPC_REG_R25, PPC_REG_R26, PPC_REG_R27,
	PPC_REG_R28, PPC_REG_R29, PPC_REG_R30, PPC_REG_R31,

	PPC_REG_CR,
	PPC_REG_FPSCR,
	PPC_REG_MSR,

	PPC_REG_SR0, PPC_REG_SR1, PPC_REG_SR2, PPC_REG_SR3,
	PPC_REG_SR4, PPC_REG_SR5, PPC_REG_SR6, PPC_REG_SR7,
	PPC_REG_SR8, PPC_REG_SR9, PPC_REG_SR10, PPC_REG_SR11,
	PPC_REG_SR12, PPC_REG_SR13, PPC_REG_SR14, PPC_REG_SR15,

	// Special Purpose Registers

	PPC_REG_XER,
	PPC_REG_LR,
	PPC_REG_CTR,
	PPC_REG_HID0,
	PPC_REG_HID1,
	PPC_REG_PVR,

	PPC_REG_DAR,
	PPC_REG_DSISR,

	PPC_REG_IBAT0H, PPC_REG_IBAT0L,
	PPC_REG_IBAT1H, PPC_REG_IBAT1L,
	PPC_REG_IBAT2H, PPC_REG_IBAT2L,
	PPC_REG_IBAT3H, PPC_REG_IBAT3L,
	PPC_REG_DBAT0H, PPC_REG_DBAT0L,
	PPC_REG_DBAT1H, PPC_REG_DBAT1L,
	PPC_REG_DBAT2H, PPC_REG_DBAT2L,
	PPC_REG_DBAT3H, PPC_REG_DBAT3L,

	PPC_REG_SPRG0, PPC_REG_SPRG1,
	PPC_REG_SPRG2, PPC_REG_SPRG3,

	PPC_REG_SRR0, PPC_REG_SRR1,
	PPC_REG_SRR2, PPC_REG_SRR3,

	PPC_REG_SDR1,

	// Device Control Registers

	PPC_REG_BEAR,
	PPC_REG_BESR,

	PPC_REG_BR0, PPC_REG_BR1, PPC_REG_BR2, PPC_REG_BR3,
	PPC_REG_BR4, PPC_REG_BR5, PPC_REG_BR6, PPC_REG_BR7,

	PPC_REG_DMACC0, PPC_REG_DMACC1, PPC_REG_DMACC2, PPC_REG_DMACC3,
	PPC_REG_DMACR0, PPC_REG_DMACR1, PPC_REG_DMACR2, PPC_REG_DMACR3,
	PPC_REG_DMACT0, PPC_REG_DMACT1, PPC_REG_DMACT2, PPC_REG_DMACT3,
	PPC_REG_DMADA0, PPC_REG_DMADA1, PPC_REG_DMADA2, PPC_REG_DMADA3,
	PPC_REG_DMASA0, PPC_REG_DMASA1, PPC_REG_DMASA2, PPC_REG_DMASA3,
	PPC_REG_DMASR,

	PPC_REG_EXIER,
	PPC_REG_EXISR,
	PPC_REG_IOCR,

	// Serial Port Unit

	PPC_REG_SPLS,
	PPC_REG_SPHS,
	PPC_REG_BRDH,
	PPC_REG_BRDL,
	PPC_REG_SPCTL,
	PPC_REG_SPRC,
	PPC_REG_SPTC,
	PPC_REG_SPRB,
	PPC_REG_SPTB,

} PPC_REG;

////////////////////////////////////////////////////////////////

extern char * m3_stat_string(u32 stat);

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

#define _OE		0x00000400
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

////////////////////////////////////////////////////////////////
// Special Purpose Registers

/* Common SPRs */

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

/* PPC403GA SPRs */

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

/* PPC603e SPRs */

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

////////////////////////////////////////////////////////////////
// Device Control Registers (406GA only)

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

////////////////////////////////////////////////////////////////
// Interface

extern int		ppc_icount;

extern int      ppc_init(void * x);
extern int      ppc_reset(void);
extern void		ppc_exit(void);
extern u32		ppc_run(u32);
extern int		ppc_get_context(ppc_t *);
extern int		ppc_set_context(ppc_t *);
extern u32		ppc_get_reg(PPC_REG);
extern u32		ppc_get_r(int);
extern f64		ppc_get_f(int);
extern void		ppc_set_reg(PPC_REG, u32);
extern void		ppc_set_irq_line(u32);
extern void		ppc_set_irq_callback(u32 (*)(void));
extern void		ppc_state_save(void * file);
extern void		ppc_state_load(void * file);
extern const char	* ppc_info(void * context, int regnum);
extern char		* ppc_disasm(int, int, char *);

extern void		ppc_set_trace_callback(void *);

extern void     ppc_set_read_8_handler(void *);
extern void     ppc_set_read_16_handler(void *);
extern void     ppc_set_read_32_handler(void *);
extern void     ppc_set_read_64_handler(void *);

extern void     ppc_set_write_8_handler(void *);
extern void     ppc_set_write_16_handler(void *);
extern void     ppc_set_write_32_handler(void *);
extern void     ppc_set_write_64_handler(void *);

extern u32		ppc_read_byte(u32);
extern u32		ppc_read_half(u32);
extern u32		ppc_read_word(u32);
extern void		ppc_write_byte(u32, u32);
extern void		ppc_write_half(u32, u32);
extern void		ppc_write_word(u32, u32);

extern void     ppc_save_state(FILE *);
extern void     ppc_load_state(FILE *);

////////////////////////////////////////////////////////////////

#endif // _PPC_H_
