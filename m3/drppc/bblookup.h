/*
 * bblookup.h
 *
 * Default BB Lookup services.
 */

#ifndef INCLUDED_BB_LOOKUP_H
#define INCLUDED_BB_LOOKUP_H

#include "types.h"
#include "drppc.h"

/*******************************************************************************
 Interface
*******************************************************************************/

extern INT			BBLookup_Setup(DRPPC_CFG *, void **);
extern void			BBLookup_Clean(void);
extern DRPPC_BB	*	BBLookup_Lookup(UINT32, INT *);
extern void			BBLookup_Invalidate(void);
extern void			BBLookup_SetLookupInfo(void *);

#endif // INCLUDED_BB_LOOKUP_H
