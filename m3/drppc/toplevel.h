/*
 * toplevel.h
 *
 * Main header file.
 */

#ifndef INCLUDED_TOPLEVEL_H
#define INCLUDED_TOPLEVEL_H

#include <stdio.h>	// see printf in ASSERT
#include <stdlib.h>	// see exit in ASSERT
#include <stdarg.h>	// see Error

#include "types.h"
#include "drppc.h"

/*******************************************************************************
 Macros
*******************************************************************************/

#ifdef DRPPC_DEBUG
#define ASSERT(x) \
		if (!(x)){ printf("%s, %i: assert failed.\n", __FILE__, __LINE__); exit(1); }
#else
#define ASSERT(x)
#endif

/*
 * This simple macro is the standard method to propagate error codes backward.
 */

#define DRPPC_TRY(func_call)						\
		{											\
			INT ret;								\
			if (DRPPC_OKAY != (ret = func_call))	\
				return ret;							\
		}

/*
 * Auxiliary macros for memory access.
 */

#ifdef _MSC_VER	// Stefano: VC keeps complaining about illegal conversion to UINT16
#pragma warning (disable : 4244)
#endif

static UINT16 BSWAP16 (UINT16 val)
{
	return ((val >> 8) & 0x00FF) | ((val << 8) & 0xFF00);
}

static UINT32 BSWAP32 (UINT32 val)
{
	return (((val >> 24) & 0x000000FF) | ((val >>  8) & 0x0000FF00) |
			((val <<  8) & 0x00FF0000) | ((val << 24) & 0xFF000000));
}

#ifdef _MSC_VER
//#pragma pop (warning : C4244)
#endif

#define READ_8(ptr, offs)				(((UINT8 *)(ptr))[offs])
#define READ_16(ptr, offs)				(((UINT16 *)(ptr))[(offs)/2])
#define READ_32(ptr, offs)				(((UINT32 *)(ptr))[(offs)/4])

#define WRITE_8(ptr, offs, data)		(((UINT8 *)(ptr))[offs] = (UINT8)(data))
#define WRITE_16(ptr, offs, data)		(((UINT16 *)(ptr))[(offs)/2] = (UINT16)(data))
#define WRITE_32(ptr, offs, data)		(((UINT32 *)(ptr))[(offs)/4] = (UINT32)(data))

#define READ_8_SWAP(ptr, offs)			READ_8(ptr, offs)
#define READ_16_SWAP(ptr, offs)			BSWAP16(READ_16(ptr, offs))
#define READ_32_SWAP(ptr, offs)			BSWAP32(READ_32(ptr, offs))

#define WRITE_8_SWAP(ptr, offs, data)	WRITE_8(ptr, offs, data)
#define WRITE_16_SWAP(ptr, offs, data)	WRITE_16(ptr, offs, BSWAP16(data))
#define WRITE_32_SWAP(ptr, offs, data)	WRITE_32(ptr, offs, BSWAP32(data))

/*
 * Memory Access Macros
 *
 * These macros are used to access memory buffers in little endian (LE) and in
 * big endian (BE) formats. They take the form READ/WRITE_SIZE_FORMAT, and two
 * or three, in case of write access, arguments:
 *
 *	- ptr is a pointer to the buffer to access, and
 *
 *	- offs is the byte-granular offset of the data within the buffer,
 *
 *	- on write, the data value to write.
 *
 * TARGET_ENDIANESS is usually read from back/$(TARGET_CPU)/target.h.
 */

#if TARGET_ENDIANESS == BIG_ENDIAN

#define READ_8_BE(ptr, offs)			READ_8(ptr, offs)
#define READ_16_BE(ptr, offs)			READ_16(ptr, offs)
#define READ_32_BE(ptr, offs)			READ_32(ptr, offs)

#define WRITE_8_BE(ptr, offs, data)		WRITE_8(ptr, offs, data)
#define WRITE_16_BE(ptr, offs, data)	WRITE_16(ptr, offs, data)
#define WRITE_32_BE(ptr, offs, data)	WRITE_32(ptr, offs, data)

#define READ_8_LE(ptr, offs)			READ_8_SWAP(ptr, offs)
#define READ_16_LE(ptr, offs)			READ_16_SWAP(ptr, offs)
#define READ_32_LE(ptr, offs)			READ_32_SWAP(ptr, offs)

#define WRITE_8_LE(ptr, offs, data)		WRITE_8_SWAP(ptr, offs, data)
#define WRITE_16_LE(ptr, offs, data)	WRITE_16_SWAP(ptr, offs, data)
#define WRITE_32_LE(ptr, offs, data)	WRITE_32_SWAP(ptr, offs, data)

#elif TARGET_ENDIANESS == LITTLE_ENDIAN

#define READ_8_LE(ptr, offs)			READ_8(ptr, offs)
#define READ_16_LE(ptr, offs)			READ_16(ptr, offs)
#define READ_32_LE(ptr, offs)			READ_32(ptr, offs)

#define WRITE_8_LE(ptr, offs, data)		WRITE_8(ptr, offs, data)
#define WRITE_16_LE(ptr, offs, data)	WRITE_16(ptr, offs, data)
#define WRITE_32_LE(ptr, offs, data)	WRITE_32(ptr, offs, data)

#define READ_8_BE(ptr, offs)			READ_8_SWAP(ptr, offs)
#define READ_16_BE(ptr, offs)			READ_16_SWAP(ptr, offs)
#define READ_32_BE(ptr, offs)			READ_32_SWAP(ptr, offs)

#define WRITE_8_BE(ptr, offs, data)		WRITE_8_SWAP(ptr, offs, data)
#define WRITE_16_BE(ptr, offs, data)	WRITE_16_SWAP(ptr, offs, data)
#define WRITE_32_BE(ptr, offs, data)	WRITE_32_SWAP(ptr, offs, data)

#else
#define TARGET_ENDIANESS is either undefined or holds an invalid value!
#endif

/*******************************************************************************
 Custom Types
*******************************************************************************/

typedef struct
{
	UINT8	* base;
	UINT8	* ptr;
	UINT8	* warning;
	UINT8	* end;

} CODE_CACHE;

/*******************************************************************************
 Generic Stuff
*******************************************************************************/

extern void *	(* Alloc)(UINT);
extern void		(* Free)(void *);
extern void		(* Print)(CHAR *, ...);

extern INT		SetupBasicServices(DRPPC_CFG *);
extern INT		AllocCache(CODE_CACHE *, UINT, UINT);
extern void		FreeCache(CODE_CACHE *);
extern void		ResetCache(CODE_CACHE *);

#endif // INCLUDED_TOPLEVEL_H
