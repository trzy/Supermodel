/*
 * bbtools.c
 *
 * Utility functions common to all the BB tool programs.
 *
 * NOTE: Log file format is described in ppc/ppc.c. 
 */
 
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "bbtools.h"

static UINT	num_bbs;

/*
 * UINT GetNumberOfRegistersUsed(PPC_REGUSAGE *ru);
 *
 * Counts how many different registers were used (accessed at least once.)
 *
 * Parameters:
 *		ru = Register usage data.
 *
 * Returns:
 *		The number of different registers that were accessed.
 */
 
UINT GetNumberOfRegistersUsed(PPC_REGUSAGE *ru)
{
	UINT	num = 0;
	INT		i;
	
	for (i = 0; i < 32; i++)
	{
		if (ru->r[i] != 0)	++num;
		if (ru->f[i] != 0)	++num;
	}
	
	if (ru->lr != 0)	++num;
	if (ru->ctr != 0)	++num;
	if (ru->cr != 0)	++num;
	if (ru->xer != 0)	++num;
	if (ru->msr != 0)	++num;
	if (ru->fpscr != 0)	++num;
	if (ru->tbu != 0)	++num;
	if (ru->tbl != 0)	++num;
	if (ru->dec != 0)	++num;
	
	return num;
}

/*
 * UINT GetNumberOfBBs(void);
 *
 * Returns:
 *		The number of basic blocks last loaded.
 */

UINT GetNumberOfBBs(void)
{
	return num_bbs;
}
 
/*
 * SetRegUsage():
 *
 * Sets the field in ru corresponding to regname to the specified value.
 */
 
static void SetRegUsage(PPC_REGUSAGE *ru, const CHAR *regname, UINT value)
{
	INT	num;
		
	switch (regname[0])
	{
	case 'R':	// Rn
		sscanf(&regname[1], "%d", &num);
		ru->r[num] = value;
		break;
	case 'F':
		if (regname[1] == 'P')	// FPSCR
			ru->fpscr = value;
		else					// Fn
		{
			sscanf(&regname[1], "%d", &num);
			ru->f[num] = value;
		}
		break;
	case 'L':	// LR
		ru->lr = value;
		break;
	case 'C':
		if (regname[1] == 'T')	// CTR
			ru->ctr = value;
		else					// CR
			ru->cr = value;
		break;
	case 'X':	// XER
		ru->xer = value;
		break;
	case 'M':	// MSR
		ru->msr = value;
		break;
	case 'T':
		if (regname[2] == 'U')	// TBU
			ru->tbu = value;
		else					// TBL
			ru->tbl	= value;
		break;
	case 'D':	// DEC
		ru->dec = value;
		break;
	default:
		fprintf(stderr, "unknown register name encountered (%s), output will be incorrect\n", regname);
		exit(0);
		break;
	}
}
			
/* 
 * GetRegisterData():
 *
 * Reads register usage data from the BB log and sets the 3 PPC_REGUSAGE structures in BB appropriately.
 * Assumes that these structures have been initialized to 0's.
 */
 
static void GetRegisterData(BB *bb, FILE *fp)
{
	CHAR	regname[12];
	UINT	num_regs, written, read, i, tmp;
	
	fscanf(fp, "%u", &num_regs);	// number of registers that were accessed in any way
	fscanf(fp, "%u", &tmp);			// number of registers that were written (we ignore it)
	fscanf(fp, "%u", &tmp); 		// number of registers that were read (ignored)
	
	/*
	 * Each register entry looks like this:
	 *
	 *		regname times_written times_read
	 */
	 
	for (i = 0; i < num_regs; i++)
	{				
		fscanf(fp, "%s", regname);
		fscanf(fp, "%u", &tmp);		// total (reads + writes), not used
		fscanf(fp, "%u", &written);
		fscanf(fp, "%u", &read);
		
		SetRegUsage(&bb->used, regname, 1);
		SetRegUsage(&bb->written, regname, written);
		SetRegUsage(&bb->read, regname, read);
	}					
}

/*
 * BB *LoadBBs(const CHAR *fname);
 *
 * Loads all the basic blocks from the specified log file. The order of the BBs
 * is preserved (first BB in the file is the first one in the BB list.)
 *
 * Parameters:
 *		fname = Name of file to load.
 *
 * Returns:
 *		Pointer to the list of BBs or NULL if there was a file or memory error.
 */
 
BB *LoadBBs(const CHAR *fname)
{
	FILE	*fp;
	BB		bb_head, *bb;
	UINT	i;
	
	fp = fopen(fname, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "unable to open file: %s\n", fname);
		return NULL;
	}		
		
	bb = &bb_head;	// set current BB to dummy head node
	
	/*
	 * Read in all basic blocks
	 */
	 
	fscanf(fp, "%u", &num_bbs);	
		
	for (i = 0; i < num_bbs; i++)
	{	
		bb->next = calloc(1, sizeof(BB));
		bb = bb->next;	// set current BB to the new one
		if (bb == NULL)
		{
			fprintf(stderr, "out of memory on BB #%u\n", i);
			FreeBBs(bb_head.next);
			fclose(fp);
			return NULL;
		}
		
		bb->next = NULL;
		
		fscanf(fp, "%X", &bb->addr);
		fscanf(fp, "%u", &bb->inst_count);
		fscanf(fp, "%u", &bb->exec_count);
		
		fscanf(fp, "%d", &bb->is_indirect);
		fscanf(fp, "%d", &bb->is_subroutine);
		fscanf(fp, "%d", &bb->is_conditional);
		
		if (bb->is_conditional)
		{
			fscanf(fp, "%X", &bb->condition);
			fscanf(fp, "%X", &(bb->edge[1].target));
			fscanf(fp, "%u", &(bb->edge[1].exec_count));
			fscanf(fp, "%X", &(bb->edge[0].target));	
			fscanf(fp, "%u", &(bb->edge[0].exec_count));
		}
		else		
			fscanf(fp, "%X", &(bb->edge[0].target));
			
		GetRegisterData(bb, fp);
	}
			
	fclose(fp);
	return bb_head.next;
}

/*
 * void FreeBBs(BB *bb);
 *
 * Frees the memory occupied by the list of BBs.
 *
 * Parameters:
 *		bb = First BB in a list of BBs to free.
 */
 
void FreeBBs(BB *bb)
{
	BB	*next;
	
	while (bb != NULL)
	{
		next = bb->next;
		free(bb);
		bb = next;
	}
}
