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

/*******************************************************************************
 Error Routines
*******************************************************************************/

void Error (CHAR * format, ...)
{
	// Stefano: why don't we ask a pointer to an Error handler in DRPPC_CFG ?
	// This way the user isn't forced to use stdout for output.

	char	string[1024];
	va_list	vl;

	va_start(vl, format);
	vsprintf(string, format, vl);
	va_end(vl);

	puts(string);
	exit(-1);
}

/*******************************************************************************
 Code Caches

 Native code and intermediate code caches management.
*******************************************************************************/

INT SetupAllocHandlers (DRPPC_CFG * cfg)
{
	if ((Alloc = cfg->Alloc) == NULL || (Free = cfg->Free) == NULL)
		return DRPPC_INVALID_CONFIG;

	return DRPPC_OKAY;
}

INT AllocCache (CODE_CACHE * cache, UINT size, UINT guard)
{
	if (cache->base)
		Free(cache->base);

	cache->base = (UINT8 *)Alloc(size);
	cache->end = cache->base + size;
	cache->ptr = cache->base;
	cache->warning = cache->end - guard;

	if (cache->base == NULL)
		return DRPPC_ERROR;

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
