/*
 * back/x86/x86.h
 *
 * X86 header file as target CPU.
 */

#ifndef INCLUDED_TARGET_H
#define INCLUDED_TARGET_H

#include "toplevel.h"
#include "mempool.h"
#include "middle/ir.h"

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

#define NUM_TARGET_NATIVE_REGS	6	/* EAX, EBX, ECX, EDX, ESI, EDI -- EBP and ESP have special uses */

/*******************************************************************************
 Interface
*******************************************************************************/

extern INT	Target_Init(MEMPOOL *, UINT32 *, void *, REG_DESC *, void (*)(INT), INT (*)(UINT32));
extern void	Target_Shutdown(void);
extern INT	Target_GenerateBB(DRPPC_BB *, IR *);
extern INT	Target_RunBB(DRPPC_BB *);
extern void	Target_Disasm (CHAR *, UINT8 *, UINT);

/*******************************************************************************
 Custom Memory Map Handling Interface
*******************************************************************************/

#ifdef USE_CUSTOM_MMAP_HANDLING

#ifdef __cplusplus
extern "C" {
#endif

// see native.asm (move these to native.h?)

extern INT				Target_MMap_Setup(DRPPC_MMAP *);
extern void				Target_MMap_Clean(void);

extern DRPPC_REGION *	Target_MMap_FindFetchRegion(UINT32);
extern DRPPC_REGION *	Target_MMap_FindRead8Region(UINT32);
extern DRPPC_REGION *	Target_MMap_FindRead16Region(UINT32);
extern DRPPC_REGION *	Target_MMap_FindRead32Region(UINT32);
extern DRPPC_REGION *	Target_MMap_FindWrite8Region(UINT32);
extern DRPPC_REGION *	Target_MMap_FindWrite16Region(UINT32);
extern DRPPC_REGION *	Target_MMap_FindWrite32Region(UINT32);

extern UINT8			Target_MMap_GenericRead8(UINT32);
extern UINT16			Target_MMap_GenericRead16(UINT32);
extern UINT32			Target_MMap_GenericRead32(UINT32);
extern void				Target_MMap_GenericWrite8(UINT32, UINT8);
extern void				Target_MMap_GenericWrite16(UINT32, UINT16);
extern void				Target_MMap_GenericWrite32(UINT32, UINT32);

#ifdef __cplusplus
}
#endif

#endif // USE_CUSTOM_MMAP_HANDLING


#endif // INCLUDED_TARGET_H
