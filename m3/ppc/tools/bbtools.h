/*
 * bbtools.h
 *
 * Header file common to all BB analysis tools.
 */
 
#ifndef INCLUDED_BBTOOLS_H
#define INCLUDED_BBTOOLS_H

#include "types.h"
#include "analys.h"	// only for PPC_REGUSAGE. analys.c is not linked in to any BB tool

/*
 * BB
 *
 * A basic block.
 */
 
typedef struct bb
{
	UINT32		addr;			// address
	UINT		inst_count;		// instruction count
	UINT		exec_count;		// number of times it has been executed	
	
	BOOL		is_indirect;	// whether the terminating branch is indirect
	BOOL		is_subroutine;	// whether it appears to be a subroutine entry point or not
	BOOL		is_conditional;	// whether the terminating branch is conditional
	
	UINT		condition;		// if conditional, the 5-bit BO field
	
	struct
	{
		UINT32		target;
		UINT		exec_count;
		struct bb	*target_bb;
	} edge[2];					// 0 is false edge, 1 is true
	
	PPC_REGUSAGE	used, written, read;	// registers used, number of times each is written, number of times each is read
	
	struct bb	*next;
} BB;

/*
 * Functions
 */
 
extern UINT	GetNumberOfRegistersUsed(PPC_REGUSAGE *);
extern UINT	GetNumberOfBBs(void);
extern BB	*LoadBBs(const CHAR *); 
extern void	FreeBBs(BB *);

#endif	// INCLUDED_BBTOOLS_H
