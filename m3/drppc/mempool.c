/*
 * mempool.c
 *
 * Generic memory pool management.
 */

#include "toplevel.h"
#include "mempool.h"

INT MemPool_Alloc (MEMPOOL * pool, UINT size, UINT guard)
{
	if (pool == NULL)
		return DRPPC_ERROR;

	pool->base		= (UINT8 *)Alloc(size);
	pool->end		= pool->base + size;
	pool->ptr		= pool->base;
	pool->warning	= pool->end - guard;

	if (pool->base == NULL)
		return DRPPC_OUT_OF_MEMORY;
	return DRPPC_OKAY;
}

void MemPool_Free (MEMPOOL * pool)
{
	if (pool != NULL && pool->base != NULL)
	{
		Free(pool->base);
		pool->base = NULL;
	}
}

void MemPool_Reset (MEMPOOL * pool)
{
	pool->ptr = pool->base;
}

void * MemPool_Grab (MEMPOOL * pool, UINT size)
{
	void * ptr = (void *)pool->ptr;
	pool->ptr += size;
	ASSERT(pool->ptr <= pool->warning);
	return ptr;
}
