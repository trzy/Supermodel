/*
 * back/x86/target.c
 *
 * Intel X86 (IA-32) Backend.
 */

#include "drppc.h"
#include "toplevel.h"
#include "target.h"

/*******************************************************************************

*******************************************************************************/
/*
static void Emit8 (UINT8 val)
{
	*(UINT8 *)cache.ptr = val;
	cache.ptr ++;
}

static void Emit16 (UINT16 val)
{
	*(UINT16 *)cache.ptr = val;
	cache.ptr += 2;
}

static void Emit32 (UINT32 val)
{
	*(UINT32 *)cache.ptr = val;
	cache.ptr += 4;
}

static BOOL CheckCodeCacheOverflow(void)
{
	return cache.ptr >= cache.warning;
}
*/
/*******************************************************************************

*******************************************************************************/

INT X86_Init (CODE_CACHE * cache)
{
	return 0;
}

INT RunNative (DRPPC_BB * bb)
{
	return DRPPC_OKAY;
}
