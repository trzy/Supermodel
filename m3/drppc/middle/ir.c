/*
 * middle/ir.c
 *
 * Intermediate Representation (IR) language generation and management.
 */

#include "drppc.h"
#include "toplevel.h"
#include "ir.h"

/*******************************************************************************
 Local Variables
*******************************************************************************/

static CODE_CACHE	ir_cache;

/*
 * NOTE:
 *
 * As for the IR cache/list, we should use a doubly-linked list (obviously) with
 * sentinel, to keep track of the first and last instruction (for forward and
 * backward scans during the optimization pass). Moreover, in otest a NOP was
 * required as the first IR instruction in a BB: the sentinel can hold the
 * required NOP too. Maintaining the sentinel requires initialization only on
 * IR_Init and the sentinel is then never touched anymore (except on ir_cache
 * reset, when we have to link the sentinel to itself).
 */

/*******************************************************************************
 Interface
*******************************************************************************/

INT IR_Init (DRPPC_CFG * cfg)
{
	DRPPC_TRY( AllocCache(&ir_cache, cfg->intermediate_cache_size, cfg->intermediate_cache_guard_size) );

	return DRPPC_OKAY;
}

void IR_Shutdown (void)
{
	FreeCache(&ir_cache);
}

INT IR_BeginBB (void)
{
	// Reset ir_cache and the optimizer vectors.

	return DRPPC_OKAY;
}

INT IR_EndBB (IR ** ir_bb)
{
	// Run the final optimizer pass and return a pointer to the optimized IR BB.

	*ir_bb = (IR *)ir_cache.ptr; // Or the sentinel

	return DRPPC_OKAY;
}
