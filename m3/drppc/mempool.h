/*
 * mempool.h
 *
 * Generic memory pool management.
 */

#ifndef INCLUDED_MEMPOOL_H
#define INCLUDED_MEMPOOL_H

#include "drppc.h"

/*******************************************************************************
 Data Structures
*******************************************************************************/

typedef struct
{
	UINT8	* base;
	UINT8	* ptr;
	UINT8	* warning;
	UINT8	* end;

} MEMPOOL;

/*******************************************************************************
 Interface
*******************************************************************************/

/*
 * extern INT MemPool_Alloc(MEMPOOL *, UINT, UINT);
 *
 * Allocates a memory pool passed as argument, with the given size and the given
 * warning area size.
 */

extern INT	MemPool_Alloc(MEMPOOL *, UINT, UINT);

/*
 * extern void MemPool_Free(MEMPOOL *);
 *
 * Frees the given memory pool.
 */

extern void	MemPool_Free(MEMPOOL *);

/*
 * extern void MemPool_Reset(MEMPOOL *);
 *
 * Resets the given memory pool internal status. It doesn't necessarily *clear*
 * the memory pool buffer.
 */

extern void	MemPool_Reset(MEMPOOL *);

/*
 * extern void * MemPool_Grab(MEMPOOL *, UINT);
 *
 * It returns a buffer of given size from the given memory pool. It cannot fail.
 */

extern void	* MemPool_Grab(MEMPOOL *, UINT);

#endif // INCLUDED_MEMPOOL_H
