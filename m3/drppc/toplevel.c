/*
 * toplevel.c
 *
 * General purpose source file. It holds a number of basic routines used all
 * through drppc.
 */

#include <stdio.h>
#include <stdarg.h>
#include "types.h"
#include "drppc.h"
#include "toplevel.h"

/*******************************************************************************
 Global Variables
*******************************************************************************/

void *	(* Alloc)(UINT);
void	(* Free)(void *);
void	(* Print)(CHAR *, ...);

/*******************************************************************************
 Basic Services

 That stands memory allocation, output, etc.
*******************************************************************************/

INT SetupBasicServices (DRPPC_CFG * cfg)
{
	if ((Alloc = cfg->Alloc) == NULL || (Free = cfg->Free) == NULL)
		return DRPPC_INVALID_CONFIG;

	if ((Print = cfg->Print) == NULL)
		return DRPPC_INVALID_CONFIG;

	return DRPPC_OKAY;
}

/*******************************************************************************
 Code Caches

 Native code and intermediate code caches management.
*******************************************************************************/

INT AllocCache (CODE_CACHE * cache, UINT size, UINT guard)
{
	cache->base = (UINT8 *)Alloc(size);
	cache->end = cache->base + size;
	cache->ptr = cache->base;
	cache->warning = cache->end - guard;

	if (cache->base == NULL)
		return DRPPC_OUT_OF_MEMORY;

	return DRPPC_OKAY;
}

void FreeCache (CODE_CACHE * cache)
{
	if (cache->base)
	{
		Free(cache->base);
		cache->base = NULL;
	}
}

void ResetCache (CODE_CACHE * cache)
{
	cache->ptr = cache->base;
}
