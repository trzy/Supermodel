/*
 * front/powerpc/powerpc.h
 *
 * PowerPC Front-end interface. This file is included by drppc.h, and so it's
 * visible to the main application.
 */

#ifndef INCLUDED_SOURCE_H
#define INCLUDED_SOURCE_H

#include <stdio.h>
#include "drppc.h"
#include "toplevel.h"
#include "mempool.h"

/*******************************************************************************
 Configuration
*******************************************************************************/

#define PPC_MODEL_4XX	400	// PowerPC 403GA
#define PPC_MODEL_6XX	600	// PowerPC 603e
#define PPC_MODEL_GEKKO	750	// PowerPC Gekko (A quite modified 750)

#define PPC_MODEL		PPC_MODEL_6XX

/*******************************************************************************
 Context
*******************************************************************************/

/*
 * union FPR
 *
 * This is a typical PowerPC floating-point register (FPR). The union thing is
 * due to the fact that an FPR can be acceeded in various modes (single fp,
 * double fp, integer, and paired-singel on Gekko), so it is convenient to code
 * it this way.
 */

typedef union
{
	UINT32	iw;		// 32-bit Integer
	UINT64	id;		// 64-bit Integer
	FLOAT64	fd;		// 64-bit Floating Point Double
	FLOAT32	ps[2];	// 2 * 32-bit Floating Point Singles

} FPR;

/*
 * struct CR
 *
 * This is a PowerPC condition register (CR). It owns four fields, each holding
 * a single bit of information.
 */

typedef struct
{
	UINT32	lt, gt, eq, so;

} CR;

/*
 * struct XER
 *
 * This is the PowerPC integer exception register (XER). It owns four fields, of
 * which three are flags (1 bit) and one is the word count reg (7 bits).
 */

typedef struct
{
	UINT32	so, ov, ca, count;

} XER;

/*
 * struct SPU
 *
 * This is the register file used by the 4xx's serial port unit (SPU). It's
 * currently unused (almost).
 */

#if PPC_MODEL == PPC_MODEL_4XX

typedef struct
{
	UINT8 spls;
	UINT8 sphs;
	UINT8 brdh;
	UINT8 brdl;
	UINT8 spctl;
	UINT8 sprc;
	UINT8 sptc;
	UINT8 sprb;
	UINT8 sptb;

} SPU;

#endif // PPC_MODEL_4XX

/*
 * enum DFLOW_BITS;
 *
 * This big enum delines the register layout in memory, and attaches to each of
 * the enumered registers a corresponding bit in the dflow_in[] and dflow_out[]
 * vectors.
 */

typedef enum
{
	// Flags

	DFLOW_FLAG_BASE = 0,

	DFLOW_BIT_CR0_LT = DFLOW_FLAG_BASE,
						DFLOW_BIT_CR0_GT,	DFLOW_BIT_CR0_EQ,	DFLOW_BIT_CR0_SO,
	DFLOW_BIT_CR1_LT,	DFLOW_BIT_CR1_GT,	DFLOW_BIT_CR1_EQ,	DFLOW_BIT_CR1_SO,
	DFLOW_BIT_CR2_LT,	DFLOW_BIT_CR2_GT,	DFLOW_BIT_CR2_EQ,	DFLOW_BIT_CR2_SO,
	DFLOW_BIT_CR3_LT,	DFLOW_BIT_CR3_GT,	DFLOW_BIT_CR3_EQ,	DFLOW_BIT_CR3_SO,
	DFLOW_BIT_CR4_LT,	DFLOW_BIT_CR4_GT,	DFLOW_BIT_CR4_EQ,	DFLOW_BIT_CR4_SO,
	DFLOW_BIT_CR5_LT,	DFLOW_BIT_CR5_GT,	DFLOW_BIT_CR5_EQ,	DFLOW_BIT_CR5_SO,
	DFLOW_BIT_CR6_LT,	DFLOW_BIT_CR6_GT,	DFLOW_BIT_CR6_EQ,	DFLOW_BIT_CR6_SO,
	DFLOW_BIT_CR7_LT,	DFLOW_BIT_CR7_GT,	DFLOW_BIT_CR7_EQ,	DFLOW_BIT_CR7_SO,

	DFLOW_BIT_XER_SO,
	DFLOW_BIT_XER_OV,
	DFLOW_BIT_XER_CA,
	DFLOW_BIT_XER_COUNT,

	DFLOW_FLAG_END	= DFLOW_BIT_XER_COUNT,

	// Integer Registers

	DFLOW_INTEGER_BASE,

	DFLOW_BIT_R0 = DFLOW_INTEGER_BASE,
					DFLOW_BIT_R1,	DFLOW_BIT_R2,	DFLOW_BIT_R3,
	DFLOW_BIT_R4,	DFLOW_BIT_R5,	DFLOW_BIT_R6,	DFLOW_BIT_R7,
	DFLOW_BIT_R8,	DFLOW_BIT_R9,	DFLOW_BIT_R10,	DFLOW_BIT_R11,
	DFLOW_BIT_R12,	DFLOW_BIT_R13,	DFLOW_BIT_R14,	DFLOW_BIT_R15,
	DFLOW_BIT_R16,	DFLOW_BIT_R17,	DFLOW_BIT_R18,	DFLOW_BIT_R19,
	DFLOW_BIT_R20,	DFLOW_BIT_R21,	DFLOW_BIT_R22,	DFLOW_BIT_R23,
	DFLOW_BIT_R24,	DFLOW_BIT_R25,	DFLOW_BIT_R26,	DFLOW_BIT_R27,
	DFLOW_BIT_R28,	DFLOW_BIT_R29,	DFLOW_BIT_R30,	DFLOW_BIT_R31,

	DFLOW_BIT_CTR,
	DFLOW_BIT_LR,

	DFLOW_INTEGER_END = DFLOW_BIT_LR,

	// Floating-Point Registers

	DFLOW_FP_BASE,

	DFLOW_BIT_F0 = DFLOW_FP_BASE,
					DFLOW_BIT_F1,	DFLOW_BIT_F2,	DFLOW_BIT_F3,
	DFLOW_BIT_F4,	DFLOW_BIT_F5,	DFLOW_BIT_F6,	DFLOW_BIT_F7,
	DFLOW_BIT_F8,	DFLOW_BIT_F9,	DFLOW_BIT_F10,	DFLOW_BIT_F11,
	DFLOW_BIT_F12,	DFLOW_BIT_F13,	DFLOW_BIT_F14,	DFLOW_BIT_F15,
	DFLOW_BIT_F16,	DFLOW_BIT_F17,	DFLOW_BIT_F18,	DFLOW_BIT_F19,
	DFLOW_BIT_F20,	DFLOW_BIT_F21,	DFLOW_BIT_F22,	DFLOW_BIT_F23,
	DFLOW_BIT_F24,	DFLOW_BIT_F25,	DFLOW_BIT_F26,	DFLOW_BIT_F27,
	DFLOW_BIT_F28,	DFLOW_BIT_F29,	DFLOW_BIT_F30,	DFLOW_BIT_F31,

	DFLOW_FP_END	= DFLOW_BIT_F31,

	// Temporary Registers

	DFLOW_TEMP_BASE,

	DFLOW_BIT_TEMP0	= DFLOW_TEMP_BASE,
	DFLOW_BIT_TEMP1,
	DFLOW_BIT_TEMP2,
	DFLOW_BIT_TEMP3,

	DFLOW_TEMP_END	= DFLOW_BIT_TEMP3,

	// Native Registers

	DFLOW_NATIVE_BASE	= DFLOW_TEMP_END,
	DFLOW_NATIVE_END	= DFLOW_NATIVE_BASE + NUM_TARGET_NATIVE_REGS,

	NUM_DFLOW_BITS,

} DFLOW_BITS;

/*
 * Source CPU Flag Definitions (Carry, Overflow, etc.)
 */

#define CARRY_FLAG_BIT		DFLOW_BIT_XER_CA
#define OVERFLOW_FLAG_BIT	DFLOW_BIT_XER_OV

/*
 * struct PPC_CONTEXT
 *
 * Here's the main thing. It holds all the data needed for a drppc instance to
 * 'be itself'. (This comment deserves a prize for its originality. ;) )
 */

typedef struct
{
	UINT32			r[32];
	UINT32			temp[4];
	CR				cr[8];
	XER				xer;
	UINT32			pc;
	UINT32			* pc_ptr;
	UINT			irq_state;
	INT				requested;
	INT				remaining;
	UINT64			timebase;
	UINT32			msr;
	UINT32			fpscr;

	FPR				f[32];
	UINT32			is_ps;

	UINT32			spr[1024];
	UINT32			sr[16];

	DRPPC_REGION	* cur_fetch_region;

	// PowerPC 4xx Only

#if PPC_MODEL == PPC_MODEL_4XX

	UINT32			dcr[256];
	UINT32			pit_reload;
	UINT32			pit_enabled;
	SPU				spu;

#endif

	// PowerPC 6xx+ Only

#if PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO

	UINT			dec_expired;
	UINT32			reserved_bit;	// Used by LWARX and STWCX.

#endif

	// Configuration

	BOOL			interpret_only;


	// MMap configuration

	DRPPC_MMAP		mmap;

	// IRQ callback

	UINT			(* IRQCallback)(void);

	// BB LUT variables and handlers

	void			* bblookup_info;

	INT				(* SetupBBLookup)(DRPPC_CFG *, void **);
	void			(* CleanBBLookup)(void);
	DRPPC_BB *		(* LookupBB)(UINT32, INT *);
	void			(* InvalidateBBLookup)(void);
	void			(* SetBBLookupInfo)(void *);

	// BB hot threshold

	UINT			hot_threshold;

	// Native code cache

	MEMPOOL			code_cache;

	// Breakpoint

	UINT32			breakpoint;

} PPC_CONTEXT;

/*******************************************************************************
 Interface
*******************************************************************************/

/*
 * extern INT PowerPC_Init(DRPPC_CFG *);
 *
 * Initializes drppc. Only needs to be called once per program session and must
 * be called before anything else.
 */
 
extern INT		PowerPC_Init(DRPPC_CFG *);

/*
 * extern INT PowerPC_SetupContext(DRPPC_CFG *, UINT32, UINT (*)(void));
 *
 * Sets up the current context with the given informations.
 */

extern INT		PowerPC_SetupContext(DRPPC_CFG *, UINT32, UINT (*)(void));

/*
 * extern void PowerPC_Shutdown(void);
 *
 * Shuts down drppc and frees all resources occupied by it.
 */
 
extern void		PowerPC_Shutdown(void);

/*
 * extern INT PowerPC_Reset(void);
 *
 * Resets the PowerPC.
 */
 
extern INT		PowerPC_Reset(void);

/*
 * extern INT PowerPC_Run(INT, INT *);
 *
 * Runs the PowerPC for a specified number of cycles.
 */
 
extern INT		PowerPC_Run(INT, INT *);

/*
 * extern void PowerPC_AddCycles(INT);
 *
 * Adds cycles to the timeslice being run.
 */
 
extern void		PowerPC_AddCycles(INT);

/*
 * extern void PowerPC_ResetCycles(void);
 *
 * Sets the current timeslice to 0 cycles remaining so drppc exits early.
 */
 
extern void		PowerPC_ResetCycles(void);

/*
 * extern INT PowerPC_GetCyclesLeft(void);
 *
 * Returns the number of cycles left in the timeslice.
 */

extern INT		PowerPC_GetCyclesLeft(void);

/*
 * extern void PowerPC_SetIRQLine(UINT);
 *
 * Used to raise or lower the IRQ line (to trigger or clear an IRQ.)
 */

extern void		PowerPC_SetIRQLine(UINT);

/*
 * extern void PowerPC_GetContext(PPC_CONTEXT *);
 *
 * Copies the current PowerPC context to the location specified.
 */
 
extern void		PowerPC_GetContext(PPC_CONTEXT *);

/*
 * extern void PowerPC_SetContext(PPC_CONTEXT *);
 *
 * Copies the context passed to the internal PowerPC context.
 */
 
extern void		PowerPC_SetContext(PPC_CONTEXT *);

/*
 * extern void PowerPC_SetBreakpoint(UINT32);
 *
 * 
 */

extern void		PowerPC_SetBreakpoint(UINT32);

#endif // INCLUDED_SOURCE_H
