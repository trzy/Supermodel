/*
 * back/x86/target.h
 *
 * X86 header file as target CPU.
 */

#ifndef INCLUDED_TARGET_H
#define INCLUDED_TARGET_H

#include "toplevel.h"
#include "ir.h"

/*******************************************************************************
 Local Configuration

 Local definitions that influence the exported configuration symbols.
*******************************************************************************/

//#define USE_CUSTOM_MMAP_HANLDING

/*******************************************************************************
 Target CPU Configuration

 These definitions are used throughout drppc to determine the backend behaviour.
*******************************************************************************/

/*
 * Exported TARGET configuration symbols are:
 *
 * TARGET_ENDIANESS
 *
 *	- It can assume two values: LITTLE_ENDIAN or BIG_ENDIAN.
 *
 * TARGET_IMPLEMENTS_MMAP_HANLDING
 *
 *	- If defined, the backend implements its own (hand-coded assembly) memory
 *	  map region-search and access handlers.
 */

#define LITTLE_ENDIAN		0 // This shouldn't be here!
#define BIG_ENDIAN			1 // This shouldn't be here!

#define TARGET_ENDIANESS	LITTLE_ENDIAN

#ifdef USE_CUSTOM_MMAP_HANDLING
#define TARGET_IMPLEMENTS_MMAP_HANDLING
#endif

/*******************************************************************************
 Interface
*******************************************************************************/

/*
 * Main interface (local naming).
 */

extern INT	X86_Init(CODE_CACHE *);
extern void	X86_Shutdown(void);
extern INT	X86_GenerateBB(DRPPC_BB *, IR *, INT);
extern INT	X86_RunBB(DRPPC_BB *);

/*
 * Main interface (exported symbols).
 */

#define Target_Init			X86_Init
#define Target_Shutdown		X86_Shutdown
#define Target_GenerateBB	X86_GenerateBB
#define Target_RunBB		X86_RunBB


#ifdef USE_CUSTOM_MMAP_HANDLING

/*
 * Memory Map management (local naming).
 */

#ifdef __cplusplus
extern "C" {
#endif

extern INT				X86_MMap_Setup(DRPPC_MMAP *);
extern void				X86_MMap_Clean(void);

extern DRPPC_REGION *	X86_MMap_FindFetchRegion(UINT32);
extern DRPPC_REGION *	X86_MMap_FindRead8Region(UINT32);
extern DRPPC_REGION *	X86_MMap_FindRead16Region(UINT32);
extern DRPPC_REGION *	X86_MMap_FindRead32Region(UINT32);
extern DRPPC_REGION *	X86_MMap_FindWrite8Region(UINT32);
extern DRPPC_REGION *	X86_MMap_FindWrite16Region(UINT32);
extern DRPPC_REGION *	X86_MMap_FindWrite32Region(UINT32);

extern UINT8			X86_MMap_GenericRead8(UINT32);
extern UINT16			X86_MMap_GenericRead16(UINT32);
extern UINT32			X86_MMap_GenericRead32(UINT32);
extern void				X86_MMap_GenericWrite8(UINT32, UINT8);
extern void				X86_MMap_GenericWrite16(UINT32, UINT16);
extern void				X86_MMap_GenericWrite32(UINT32, UINT32);

#ifdef __cplusplus
}
#endif

/*
 * Memory Map management (exported symbols).
 */

#define Target_MMap_Setup				X86_MMap_Setup
#define Target_MMap_Clean				X86_MMap_Clean
#define Target_MMap_FindFetchRegion		X86_MMap_FindFetchRegion
#define Target_MMap_FindRead8Region		X86_MMap_FindRead8Region
#define Target_MMap_FindRead16Region	X86_MMap_FindRead16Region
#define Target_MMap_FindRead32Region	X86_MMap_FindRead32Region
#define Target_MMap_FindWrite8Region	X86_MMap_FindWrite8Region
#define Target_MMap_FindWrite16Region	X86_MMap_FindWrite16Region
#define Target_MMap_FindWrite32Region	X86_MMap_FindWrite32Region
#define Target_MMap_GenericRead8		X86_MMap_GenericRead8
#define Target_MMap_GenericRead16		X86_MMap_GenericRead16
#define Target_MMap_GenericRead32		X86_MMap_GenericRead32
#define Target_MMap_GenericWrite8		X86_MMap_GenericWrite8
#define Target_MMap_GenericWrite16		X86_MMap_GenericWrite16
#define Target_MMap_GenericWrite32		X86_MMap_GenericWrite32

#endif // USE_CUSTOM_MMAP_HANDLING

#endif // INCLUDED_TARGET_H
