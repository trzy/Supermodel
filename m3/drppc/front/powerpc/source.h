/*
 * front/powerpc/source.h
 *
 * PowerPC Front-end interface. This file is included by drppc.h, and so it's
 * visible to the main application.
 */

#ifndef INCLUDED_SOURCE_H
#define INCLUDED_SOURCE_H

#include <stdio.h>
#include "drppc.h"
#include "toplevel.h"

/*******************************************************************************
 Configuration
*******************************************************************************/

#define PPC_MODEL_4XX	400	// PowerPC 403GA
#define PPC_MODEL_6XX	600	// PowerPC 603e
#define PPC_MODEL_GEKKO	750	// PowerPC Gekko (A quite modified 750)

#define PPC_MODEL PPC_MODEL_6XX

/*******************************************************************************
 Register IDs
*******************************************************************************/

typedef enum
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

	PPC_REG_XER,
	PPC_REG_LR,
	PPC_REG_CTR,
	PPC_REG_HID0,
	PPC_REG_HID1,
	PPC_REG_PVR,

	PPC_REG_DAR,
	PPC_REG_DSISR,


	PPC_REG_SPRG0, PPC_REG_SPRG1,
	PPC_REG_SPRG2, PPC_REG_SPRG3,

	PPC_REG_SRR0, PPC_REG_SRR1,
	PPC_REG_SRR2, PPC_REG_SRR3,

	PPC_REG_SDR1,

#if PPC_MODEL == PPC_MODEL_4XX

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

	// SPR

	// DCR

	// SPU

	PPC_REG_SPLS,
	PPC_REG_SPHS,
	PPC_REG_BRDH,
	PPC_REG_BRDL,
	PPC_REG_SPCTL,
	PPC_REG_SPRC,
	PPC_REG_SPTC,
	PPC_REG_SPRB,
	PPC_REG_SPTB,

#endif

#if PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO

	PPC_REG_IBAT0H, PPC_REG_IBAT0L,
	PPC_REG_IBAT1H, PPC_REG_IBAT1L,
	PPC_REG_IBAT2H, PPC_REG_IBAT2L,
	PPC_REG_IBAT3H, PPC_REG_IBAT3L,
	PPC_REG_DBAT0H, PPC_REG_DBAT0L,
	PPC_REG_DBAT1H, PPC_REG_DBAT1L,
	PPC_REG_DBAT2H, PPC_REG_DBAT2L,
	PPC_REG_DBAT3H, PPC_REG_DBAT3L,

#endif

#if PPC_MODEL == PPC_MODEL_GEKKO

	PPC_REG_GEKKO /* DUMMY */

#endif

} PPC_REGID;

/*******************************************************************************
 Context
*******************************************************************************/

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

typedef union
{
	UINT32	iw;		// 32-bit Integer
	UINT64	id;		// 64-bit Integer
	FLOAT64	fd;		// 64-bit Floating Point Double
	FLOAT32	ps[2];	// 2 * 32-bit Floating Point Single

} FPR;

typedef struct
{
	UINT8	lt, gt, eq, so;

} CR;

typedef struct
{
	UINT32			r[32];

#if PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO

	FPR				f[32];
	UINT32			is_ps;

#endif

	UINT32			pc;
	UINT32			* pc_ptr;

	UINT64			timebase;		// 56.2 fixed point
	UINT			requested;
	UINT			remaining;

	UINT			irq_state;
	UINT32			msr;
	UINT32			fpscr;
	UINT8			cr[8];
	CR				cr0, cr1;
	UINT32			spr[1024];
	UINT32			sr[16];

	DRPPC_REGION	* cur_fetch_region;

#if PPC_MODEL == PPC_MODEL_4XX

	UINT32			dcr[256];
	UINT32			pit_reload;
	UINT32			pit_enabled;
	SPU				spu;

#endif

#if PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO

	UINT			dec_expired;
	UINT32			reserved_bit;	// Used by LWARX and STWCX.

#endif

	// MMap configuration

	DRPPC_MMAP		mmap;

	// IRQ callback

	UINT			(* IRQCallback)(void);

	// BB LUT handlers

	INT				(* SetupBBLookup)(DRPPC_CFG *);
	void			(* CleanBBLookup)(void);
	DRPPC_BB *		(* LookupBB)(UINT32, INT *);
	void			(* InvalidateBBLookup)(void);

	// BB hot threshold

	UINT			hot_threshold;

	// Native code cache

	CODE_CACHE		code_cache;

} PPC_CONTEXT;

/*******************************************************************************
 Interface
*******************************************************************************/

extern INT		PowerPC_Init(DRPPC_CFG *, UINT32, UINT (*)(void));
extern INT		PowerPC_Reset(void);
extern void		PowerPC_Shutdown(void);
extern INT		PowerPC_Run(INT, INT *);
extern void		PowerPC_AddCycles(INT);
extern void		PowerPC_ResetCycles(void);
extern INT		PowerPC_GetCyclesLeft(void);
extern void		PowerPC_SetIRQLine(UINT);
extern INT		PowerPC_GetContext(PPC_CONTEXT *);
extern INT		PowerPC_SetContext(PPC_CONTEXT *);

extern INT		PowerPC_LoadState(FILE *);
extern INT		PowerPC_SaveState(FILE *);
extern UINT32	PowerPC_GetReg(UINT);
extern void		PowerPC_SetReg(UINT, UINT32);

#endif // INCLUDED_SOURCE_H
