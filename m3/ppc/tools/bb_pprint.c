/*
 * bb_pprint.c 
 *
 * BB log pretty printer. Prints out the BB log in a human-readable form.
 *
 * NOTES: 
 *
 * - If a conditional path is never taken, the target address will appear to be
 *   0. 
 * - For conditional indirect branches, the true/false target addresses are 
 *   most likely wrong and should be ignored.
 */
 
#include <stdio.h>
#include "types.h"
#include "bbtools.h"


static const char * cond_name[] =
{
	"--ctr != 0 && cc == 0",	"--ctr == 0 && cc == 0",
	"cc == 0",					"cc == 0",
	"--ctr != 0 && cc != 0",	"--ctr == 0 && cc != 0",
	"cc != 0",					"cc != 0",
	"--ctr != 0",				"--ctr == 0"
	"always",					"always",
	"--ctr != 0",				"--ctr == 0"
	"always",					"always",
};

static void PrintRegisterUsage(BB *bb)
{	
	UINT			num_written, num_read, num_used;
	INT				i;
		
	/*
	 * Count number of different registers used (these functions are in analys.c)
	 */
	 
	num_written = GetNumberOfRegistersUsed(&bb->written);
	num_read = GetNumberOfRegistersUsed(&bb->read);
	num_used = GetNumberOfRegistersUsed(&bb->used);
	
	printf("  Register Usage: %d registers accessed (Written=%d, Read=%d)\n", num_used, num_written, num_read);
	
	/*
	 * Print the registers used and how many times they were accessed
	 */
	 
	for (i = 0; i < 32; i++)
	{
		if (bb->used.r[i] != 0)
			printf("\tR%d: %d accesses (%d writes, %d reads)\n", i, bb->used.r[i], bb->written.r[i], bb->read.r[i]);
	}
	
	for (i = 0; i < 32; i++)
	{
		if (bb->used.f[i] != 0)
			printf("\tF%d: %d accesses (%d writes, %d reads)\n", i, bb->used.f[i], bb->written.f[i], bb->read.f[i]);
	}
	
	if (bb->used.lr != 0)	printf("\tLR: %d accesses (%d writes, %d reads)\n", bb->used.lr, bb->written.lr, bb->read.lr);
	if (bb->used.ctr != 0)	printf("\tCTR: %d accesses (%d writes, %d reads)\n", bb->used.ctr, bb->written.ctr, bb->read.ctr);	
	if (bb->used.cr != 0)	printf("\tCR: %d accesses (%d writes, %d reads)\n", bb->used.cr, bb->written.cr, bb->read.cr);
	if (bb->used.xer != 0)	printf("\tXER: %d accesses (%d writes, %d reads)\n", bb->used.xer, bb->written.xer, bb->read.xer);
	if (bb->used.msr != 0)	printf("\tMSR: %d accesses (%d writes, %d reads)\n", bb->used.msr, bb->written.msr, bb->read.msr);
	if (bb->used.fpscr != 0)	printf("\tFPSCR: %d accesses (%d writes, %d reads)\n", bb->used.fpscr, bb->written.fpscr, bb->read.fpscr);
	if (bb->used.tbu != 0)	printf("\tTBU: %d accesses (%d writes, %d reads)\n", bb->used.tbu, bb->written.tbu, bb->read.tbu);
	if (bb->used.tbl != 0)	printf("\tTBL: %d accesses (%d writes, %d reads)\n", bb->used.tbl, bb->written.tbl, bb->read.tbl);
	if (bb->used.dec != 0)	printf("\tDEC: %d accesses (%d writes, %d reads)\n", bb->used.dec, bb->written.dec, bb->read.dec);
}

int main(int argc, char **argv)
{
	BB		*bb_head, *bb;
	UINT	num_cond_branches, num_uncond_branches, total_bb_size;
	
	
	if (argc <= 1)
	{
		puts("missing file argument");
		exit(0);
	}
	
	bb_head = LoadBBs(argv[1]);
	if (bb_head == NULL)
		exit(1);
		
	/*
	 * Print out the BBs
	 */

	num_cond_branches = 0;
	num_uncond_branches = 0;
	total_bb_size = 0;
	
	printf("\n%u basic blocks\n\n", GetNumberOfBBs());
	printf("================================================================================\n");
	
	for (bb = bb_head; bb != NULL; bb = bb->next)
	{
		printf(" BB %08X, %u instructions, executed %u time(s) [%s]", 
			bb->addr, bb->inst_count, bb->exec_count, bb->is_indirect ? "indirect" : "direct");
		if (bb->is_subroutine)
			printf(" [subroutine]");
		printf("\n");

		if(bb->is_conditional)	// conditional
		{
			printf(	"  cond = [%s]\n"
					"   false  -> %08X, executed %u times (%f%%)\n"
					"   true   -> %08X, executed %u times (%f%%)\n",
					cond_name[bb->condition >> 1],
					bb->edge[0].target, bb->edge[0].exec_count,
					(bb->edge[0].exec_count * 100.0f) / (float)bb->exec_count,
					bb->edge[1].target, bb->edge[1].exec_count,
					(bb->edge[1].exec_count * 100.0f) / (float)bb->exec_count
			);
			
			++num_cond_branches;
		}
		else					// unconditional
		{
			printf("  target -> %08X\n", bb->edge[0].target);		
			++num_uncond_branches;
		}
		
		PrintRegisterUsage(bb);
		printf("\n");
		
		total_bb_size += bb->inst_count * 4;
	}		
				
	printf("================================================================================\n");
	printf(" average block size = %f bytes (%f instructions)\n",
			(float) total_bb_size / (float) GetNumberOfBBs(),
			((float) total_bb_size / (float) GetNumberOfBBs()) / 4.0f);
	printf(	" %7u conditional branches   (%f%%)\n"
			" %7u unconditional branches (%f%%)\n",
			num_cond_branches,   (num_cond_branches * 100.0f) /   (float) (num_cond_branches + num_uncond_branches),
			num_uncond_branches, (num_uncond_branches * 100.0f) / (float) (num_cond_branches + num_uncond_branches));
						
	FreeBBs(bb_head);	
	return 0;
}
