/*
 * mmap.h
 *
 * Header file for mmap.c
 */

#ifndef INCLUDED_MMAP_H
#define INCLUDED_MMAP_H

#include "drppc.h"

extern INT				MMap_Setup(DRPPC_MMAP *);
extern void				MMap_Clean(void);

extern DRPPC_REGION *	MMap_FindFetchRegion(UINT32);
extern DRPPC_REGION *	MMap_FindRead8Region(UINT32);
extern DRPPC_REGION *	MMap_FindRead16Region(UINT32);
extern DRPPC_REGION *	MMap_FindRead32Region(UINT32);
extern DRPPC_REGION *	MMap_FindWrite8Region(UINT32);
extern DRPPC_REGION *	MMap_FindWrite16Region(UINT32);
extern DRPPC_REGION *	MMap_FindWrite32Region(UINT32);

extern UINT8			MMap_GenericRead8(UINT32);
extern UINT16			MMap_GenericRead16(UINT32);
extern UINT32			MMap_GenericRead32(UINT32);
extern void				MMap_GenericWrite8(UINT32, UINT8);
extern void				MMap_GenericWrite16(UINT32, UINT16);
extern void				MMap_GenericWrite32(UINT32, UINT32);

#endif // INCLUDED_MMAP_H
