/*
 * back/x86/target.c
 *
 * Intel X86 (IA-32) Backend.
 *
 * NOTE: some of the functions declared in target.h are located in the assembly
 * sources in the back/x86/ directory.
 */

#include "drppc.h"
#include "toplevel.h"
#include "target.h"
#include "ir.h"

/*******************************************************************************
 Local Variables
*******************************************************************************/

static CODE_CACHE * cache;

/*******************************************************************************
 Local Routines (used by emit.h -- which is not included yet)
*******************************************************************************/

static void Emit8 (UINT8 val)
{
	*(UINT8 *)cache->ptr = val;
	cache->ptr ++;
}

static void Emit16 (UINT16 val)
{
	*(UINT16 *)cache->ptr = val;
	cache->ptr += 2;
}

static void Emit32 (UINT32 val)
{
	*(UINT32 *)cache->ptr = val;
	cache->ptr += 4;
}

static BOOL CheckCodeCacheOverflow(void)
{
	return cache->ptr >= cache->warning;
}

static void * GetEmitPtr(void)
{
	return cache->ptr;
}

/*******************************************************************************
 Interface
*******************************************************************************/

INT X86_Init (CODE_CACHE * _cache)
{
	ASSERT(_cache != NULL);

	cache = _cache;

	return DRPPC_OKAY;
}

void X86_Shutdown (void)
{
	/* Uhm. Nothing ATM. */
}

INT X86_GenerateBB (DRPPC_BB * bb, IR * ir, INT cycles)
{
	Print("X86_GenerateBB.\n");

	return DRPPC_OKAY;
}

INT X86_RunBB (DRPPC_BB * bb)
{
	Print("X86_RunBB.\n");

	return DRPPC_OKAY;
}
