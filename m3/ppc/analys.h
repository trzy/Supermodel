/*
 * ppc/analys.h
 *
 * PowerPC instruction and basic block analysis header.
 */
 
#ifndef INCLUDED_PPC_ANALYS_H
#define INCLUDED_PPC_ANALYS_H

/*
 * Register Usage Structure
 *
 * Records information on register usage.
 */
 
typedef struct
{
	UINT	r[32];	// 32 GPRs
	UINT	f[32];	// 32 FPRs

	/*
	 * Misc. Regs
	 *
	 * Only commonly used registers are kept track of. There's no point to
	 * logging accesses to IBAT, DBAT, and other rarely-used regs.
	 */
	 
	UINT	lr;			// LR (SPR)
	UINT	ctr;		// CTR (SPR)
	UINT	cr;			// CR
	UINT	xer;		// XER (SPR)
	UINT	msr;		// MSR
	UINT	fpscr;		// FPSCR
	UINT	tbu, tbl;	// TBR (2 32-bit parts) (SPR)
	UINT	dec;		// DEC (SPR)
} PPC_REGUSAGE;

/*
 * Functions
 */
 
extern void	ppc_analyze_regusage(PPC_REGUSAGE *, PPC_REGUSAGE *, UINT32);
extern UINT	ppc_get_num_regs_used(PPC_REGUSAGE *);
extern void ppc_add_regusage_data(PPC_REGUSAGE *, PPC_REGUSAGE *, PPC_REGUSAGE *);
 

#endif	// INCLUDED_PPC_ANALYS_H

