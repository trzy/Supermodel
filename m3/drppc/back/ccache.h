/*
 * back/ccache.h
 *
 * Interface to the code cache manager.
 */

#ifndef INCLUDED_CCACHE_H
#define INCLUDED_CCACHE_H

#include "drppc.h"
#include "toplevel.h"
#include "mempool.h"

extern void SetCodeCache (MEMPOOL *);
extern BOOL CheckCodeCacheOverflow (void);
extern void Emit8 (UINT8);
extern void Emit16 (UINT16);
extern void Emit32 (UINT32);
extern void * GetEmitPtr (void);
extern void SetEmitPtr (void *);

#endif // INCLUDED_CCACHE_H
