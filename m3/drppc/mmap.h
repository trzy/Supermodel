/*
 * mmap.h
 *
 * Header file for mmap.c
 */

#ifndef INCLUDED_MMAP_H
#define INCLUDED_MMAP_H

#include "drppc.h"
#include TARGET_CPU_HEADER

#ifdef TARGET_IMPLEMENTS_MMAP_HANDLING

/*
 * Use the target CPU memory map handlers, if they're available.
 */

#define MMap_Setup				Target_MMap_Setup
#define MMap_Clean				Target_MMap_Clean
#define MMap_FindFetchRegion	Target_MMap_FindFetchRegion
#define MMap_FindRead8Region	Target_MMap_FindRead8Region
#define MMap_FindRead16Region	Target_MMap_FindRead16Region
#define MMap_FindRead32Region	Target_MMap_FindRead32Region
#define MMap_FindWrite8Region	Target_MMap_FindWrite8Region
#define MMap_FindWrite16Region	Target_MMap_FindWrite16Region
#define MMap_FindWrite32Region	Target_MMap_FindWrite32Region
#define MMap_GenericRead8		Target_MMap_GenricRead8
#define MMap_GenericRead16		Target_MMap_GenricRead16
#define MMap_GenericRead32		Target_MMap_GenricRead32
#define MMap_GenericWrite8		Target_MMap_GenricWrite8
#define MMap_GenericWrite16		Target_MMap_GenricWrite16
#define MMap_GenericWrite32		Target_MMap_GenricWrite32

#else // !TARGET_IMPLEMENTS_MMAP_HANDLING

/*
 * Else, use the default C handlers.
 */

extern INT				MMap_Setup(DRPPC_MMAP *);
extern void				MMap_Clean(void);

extern DRPPC_REGION *	MMap_FindFetchRegion(UINT32);
extern DRPPC_REGION *	MMap_FindRead8Region(UINT32);
extern DRPPC_REGION *	MMap_FindRead16Region(UINT32);
extern DRPPC_REGION *	MMap_FindRead32Region(UINT32);
extern DRPPC_REGION *	MMap_FindWrite8Region(UINT32);
extern DRPPC_REGION *	MMap_FindWrite16Region(UINT32);
extern DRPPC_REGION *	MMap_FindWrite32Region(UINT32);

extern UINT32			MMap_GenericRead8(UINT32);
extern UINT32			MMap_GenericRead16(UINT32);
extern UINT32			MMap_GenericRead32(UINT32);
extern void				MMap_GenericWrite8(UINT32, UINT32);
extern void				MMap_GenericWrite16(UINT32, UINT32);
extern void				MMap_GenericWrite32(UINT32, UINT32);

#endif // TARGET_IMPLEMENTS_MMAP_HANDLING

#endif // INCLUDED_MMAP_H
