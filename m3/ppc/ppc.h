/*
 * ppc.h
 *
 * PowerPC Interface
 */

#ifndef _PPC_H_
#define _PPC_H_

/*
 * Includes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * Configuration
 */

#define PPC_MODEL_4XX	4
#define PPC_MODEL_6XX	6

#define PPC_MODEL		PPC_MODEL_6XX

/*
 * Custom Types
 */

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
#define INLINE inline
#endif

/*
 * Return Codes
 */

#define PPC_OKAY    0

/*
 * Context
 */

typedef union FPR
{
	UINT32	iw;
	UINT64	id;
	FLOAT64	fd;

} FPR;

typedef struct
{
	UINT32	start;
	UINT32	end;
	UINT32	* ptr;

} PPC_FETCH_REGION;

typedef struct PPC_CONTEXT
{
	UINT32	gpr[32];
	FPR		fpr[32];

	UINT32	* _pc;
	UINT32	pc;

	PPC_FETCH_REGION	cur_fetch;
	PPC_FETCH_REGION	* fetch;

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

	u32		(* read_8)(u32);
	u32		(* read_16)(u32);
	u32		(* read_32)(u32);
	u64		(* read_64)(u32);
#ifdef KHEPERIX_TEST
	u32		(* read_op)(u32);
#endif
	void	(* write_8)(u32, u32);
	void	(* write_16)(u32, u32);
	void	(* write_32)(u32, u32);
	void	(* write_64)(u32, u64);

} PPC_CONTEXT;

/*
 * Macros
 */

enum
{
	PPC_REG_PC,

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
};

/*
 * Interface
 */

extern int      ppc_init(void);
extern void		ppc_shutdown(void);

extern void		ppc_set_fetch(PPC_FETCH_REGION *);

#ifdef KHEPERIX_TEST
extern void     ppc_set_read_op_handler(void *);
#endif
extern void		ppc_set_irq_callback(u32 (*)(void));
extern void     ppc_set_read_8_handler(void *);
extern void     ppc_set_read_16_handler(void *);
extern void     ppc_set_read_32_handler(void *);
extern void     ppc_set_read_64_handler(void *);
extern void     ppc_set_write_8_handler(void *);
extern void     ppc_set_write_16_handler(void *);
extern void     ppc_set_write_32_handler(void *);
extern void     ppc_set_write_64_handler(void *);

extern void     ppc_save_state(FILE *);
extern void     ppc_load_state(FILE *);

extern int		ppc_get_context(PPC_CONTEXT *);
extern int		ppc_set_context(PPC_CONTEXT *);

extern int      ppc_reset(void);
extern u32		ppc_run(u32);
extern void		ppc_set_irq_line(u32);

extern u64      ppc_get_timebase(void);
extern u32		ppc_get_reg(int);
extern void		ppc_set_reg(int, u32);

extern u32		ppc_get_r(int);
extern void		ppc_set_r(int, u32);

extern f64		ppc_get_f(int);
extern void		ppc_set_f(int, f64);

extern u32		ppc_read_byte(u32);
extern u32		ppc_read_half(u32);
extern u32		ppc_read_word(u32);
extern void		ppc_write_byte(u32, u32);
extern void		ppc_write_half(u32, u32);
extern void		ppc_write_word(u32, u32);

#endif /* _PPC_H_ */
