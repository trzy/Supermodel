/*
 * bb_subs.c
 *
 * Subroutine analysis. Determines size of subroutines (number of BBs) and detects leaf
 * routines.
 *
 * BBs are counted in a fairly strange way: Nested subroutine calls are not followed,
 * but such calls do count as BB terminators. So this sequence is 2 BBs:
 *
 *		nop			; first BB
 *		bl	foo
 *		nop			; second BB
 *		bl 	bar
 */
 
#include <stdio.h>
#include "types.h"
#include "bbtools.h"

/*
 * bb_head:
 *
 * A list of all BBs recorded for the program.
 */
 
static BB	*bb_head;

/*
 * bblist:
 *
 * Keeps track of BBs we've traversed for the current subroutine.
 */
 
#define MAX_BB	256
static BB		*bblist[MAX_BB];
static INT		bbindex;

/*
 * is_leaf:
 *
 * Keeps track of whether a subroutine is a leaf or not.
 */
 
static BOOL		is_leaf;

/*
 * FindBB():
 *
 * Finds a BB with the matching address or returns NULL.
 */
 
static BB *FindBB(UINT32 addr)
{
	BB	*bb;
	
	for (bb = bb_head; bb != NULL; bb = bb->next)
	{
		if (bb->addr == addr)
			return bb;
	}
	
	return NULL;
}

/*
 * CountBBs():
 *
 * Recursively traverses the subroutine and counts how many BBs there are. 
 * When a subroutine call is detected (an outgoing edge is marked as a
 * subroutine that is not the head of this current subroutine), it is not
 * traversed. is_leaf will be cleared if any nested subroutine calls are 
 * detected.
 *
 * NOTE: Remember to set bbindex to 0 and is_leaf to 1 before calling this 
 * function for a new subroutine.
 */
 
static INT CountBBs(BB *bb)
{	
	INT	i;
	
	if (bb == NULL)
	{
		printf("NULL edge encountered!\n");
		return 0;
	}
	
	/*
	 * Check to see if we've already visited this BB
	 */
	 
	for (i = 0; i < bbindex; i++)
	{
		if (bblist[i] == bb)
			return 0;
	}
	
	/*
	 * If the current BB is a subroutine entry point and it's not
	 * in the list, we've called another subroutine (unless bbindex
	 * is 0, in which case we're just starting the current subroutine.)
	 *
	 * We don't want to traverse nested subroutines!
	 */
	 
	if (bb->is_subroutine && (bbindex != 0))
	{
		is_leaf = 0;	// definitely not a leaf
		return 0;
	}		
		
	/*
	 * Add this new BB to the list (provided we don't exceed the limit)
	 */
	 
	if (bbindex >= MAX_BB)
	{
		printf("too many BBs!\n");
		return 0;
	}
	bblist[bbindex++] = bb;	// record this BB as having been traversed
			
	/*
	 * Count this BB and recurse to its outgoing edges
	 */
	 
	if (bb->is_conditional)
	{
		if (bb->is_indirect)
			return 1 + CountBBs(bb->edge[0].target_bb);	// false edge is known even if indirect
		else
			return 1 + CountBBs(bb->edge[0].target_bb) + CountBBs(bb->edge[1].target_bb);
	}
	else
	{
		if (bb->is_indirect)
			return 1;	// just this BB
		else
		{
			/*
			 * For unconditional subroutine calls, we want to skip them and
			 * continue at the next instruction. Unfortunately, the location
			 * of the next block was not stored because the unconditional 
			 * direct branch was followed. 
			 *
			 * The expression bb->addr+bb->inst_count*4 finds the block following
			 * the BL instruction in the current block.
			 */
			 
			if (bb->edge[0].target_bb != NULL)	// null edge check for safety...
			{
				if (bb->edge[0].target_bb->is_subroutine)	// we want to skip traversing the subroutine				
				{
					is_leaf = 0;
					return 1 + CountBBs(FindBB(bb->addr + bb->inst_count * 4));									
				}					
			}
			
			return 1 + CountBBs(bb->edge[0].target_bb);
		}			
	}
}

int main(int argc, char **argv)
{
	BB		*bb;	
	INT		num_subs, num_bbs;
		
	if (argc <= 1)
	{
		puts("missing file argument");
		exit(0);
	}
	
	bb_head = LoadBBs(argv[1]);
	if (bb_head == NULL)
		exit(1);

	/*
	 * Count the number of subroutines
	 */
	 
	num_subs = 0;
	for (bb = bb_head; bb != NULL; bb = bb->next)
	{
		if (bb->is_subroutine)
			++num_subs;		
	}
	
	printf("\n%d subroutines\n\n================================================================================\n\n", num_subs);
				
	/*
	 * For every subroutine, determine its size and whether it's a leaf
	 */
	 
	for (bb = bb_head; bb != NULL; bb = bb->next)
	{
		if (bb->is_subroutine)
		{
			bbindex = 0;
			is_leaf = 1;
			num_bbs = CountBBs(bb);
			
			printf("%08X:\texecuted %u times, size=%d", bb->addr, bb->exec_count, num_bbs);
			if (is_leaf)
				printf("\t[leaf]");
			printf("\n\n");
		}			
	}

	FreeBBs(bb_head);	
	return 0;
}
