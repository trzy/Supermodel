/*
 * back/ccache.c
 *
 * Basic code cache manipulation routines.
 */

#include "drppc.h"
#include "toplevel.h"
#include "mempool.h"
#include "ccache.h"

MEMPOOL	* cache = NULL;

void SetCodeCache (MEMPOOL * _cache)
{
	ASSERT(_cache);

	cache = _cache;
}

void * GetEmitPtr (void)
{
	return cache->ptr;
}

void SetEmitPtr (void * ptr)
{
	cache->ptr = ptr;

	ASSERT(!CheckCodeCacheOverflow());
}

BOOL CheckCodeCacheOverflow (void)
{
	return cache->ptr >= cache->warning;
}

void Emit8 (UINT8 val)
{
	*(UINT8 *)cache->ptr = val;
	cache->ptr ++;
	ASSERT(!CheckCodeCacheOverflow());
}

void Emit16 (UINT16 val)
{
	*(UINT16 *)cache->ptr = val;
	cache->ptr += 2;
	ASSERT(!CheckCodeCacheOverflow());
}

void Emit32 (UINT32 val)
{
	*(UINT32 *)cache->ptr = val;
	cache->ptr += 4;
	ASSERT(!CheckCodeCacheOverflow());
}
