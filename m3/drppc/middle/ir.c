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

CODE_CACHE	ir_cache;

/*******************************************************************************
 Interfac
*******************************************************************************/

INT IR_Init (DRPPC_CFG * cfg)
{
	INT	ret;

	if ((ret = AllocCache(&ir_cache, cfg->intermediate_cache_size,
						  cfg->intermediate_cache_guard_size)) != DRPPC_OKAY)
		return ret;

	return DRPPC_OKAY;
}

INT IR_Reset (void)
{
	ResetCache(&ir_cache);

	return DRPPC_OKAY;
}

void IR_Shutdown (void)
{
	FreeCache(&ir_cache);
}
