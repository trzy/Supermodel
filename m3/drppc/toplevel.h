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
		if (!(x)){ Print("%s, %i: assert failed.\n", __FILE__, __LINE__); exit(1); }
#define ASSERTEX(x,y) \
		if (!(x)){ Print("%s, %i: assert failed.\n", __FILE__, __LINE__); exit(1); y }
#else
#define ASSERT(x)
#define ASSERTEX(x,y)
#endif

/*
 * This simple macro is the standard method to propagate error codes backward.
 */

#define DRPPC_TRY(func_call)						\
		{											\
			INT ret;								\
			if (DRPPC_OKAY != (ret = func_call))	\
			{										\
				printf("try failed at %s, line %u\n", __FILE__, __LINE__);	\
				return ret;							\
			}										\
		}

/*
 * Auxiliary routines for memory access. These aren't converted to macros to
 * retain some kind of type checking. Also, the older versions of the memory
 * access functions weren't working correnctly for unaligned addresses.
 */

static UINT16 Byteswap16 (UINT16 val)
{
	return ((val >> 8) & 0x00FF) | ((val << 8) & 0xFF00);
}

static UINT32 Byteswap32 (UINT32 val)
{
	return (((val >> 24) & 0x000000FF) | ((val >>  8) & 0x0000FF00) |
			((val <<  8) & 0x00FF0000) | ((val << 24) & 0xFF000000));
}

static UINT8 _Read8 (void * ptr, UINT32 offs)
{
	UINT8 * buf = (UINT8 *)ptr;

	return buf[offs];
}

static UINT16 _Read16 (void * ptr, UINT32 offs)
{
	UINT8 * buf = (UINT8 *)ptr;

	return *(UINT16 *)&buf[offs];
}

static UINT32 _Read32 (void * ptr, UINT32 offs)
{
	UINT8 * buf = (UINT8 *)ptr;

	return *(UINT32 *)&buf[offs];
}

static void _Write8 (void * ptr, UINT32 offs, UINT8 data)
{
	((UINT8 *)ptr)[offs] = data;
}

static void _Write16 (void * ptr, UINT32 offs, UINT16 data)
{
	*(UINT16 *)&(((UINT8 *)ptr)[offs]) = data;
}

static void _Write32 (void * ptr, UINT32 offs, UINT32 data)
{
	*(UINT32 *)&(((UINT8 *)ptr)[offs]) = data;
}

static UINT8 _Read8Swap (void * ptr, UINT32 offs)
{
	return _Read8(ptr, offs);
}

static UINT16 _Read16Swap (void * ptr, UINT32 offs)
{
	return Byteswap16(_Read16(ptr, offs));
}

static UINT32 _Read32Swap (void * ptr, UINT32 offs)
{
	return Byteswap32(_Read32(ptr, offs));
}

static void _Write8Swap (void * ptr, UINT32 offs, UINT8 data)
{
	_Write8(ptr, offs, data);
}

static void _Write16Swap (void * ptr, UINT32 offs, UINT16 data)
{
	_Write16(ptr, offs, Byteswap16(data));
}

static void _Write32Swap (void * ptr, UINT32 offs, UINT32 data)
{
	_Write32(ptr, offs, Byteswap32(data));
}

/*
 * Memory Access Macros
 *
 * These macros are used to access memory buffers in little endian (LE) and in
 * big endian (BE) formats. They take the form READ/WRITE_SIZE_FORMAT, and two
 * or three, in case of write access, arguments:
 *
 *	- ptr is a pointer to the buffer to access,
 *
 *	- offs is the byte-granular offset of the data within the buffer,
 *
 *	- on write, the data value to write.
 *
 * TARGET_ENDIANESS is usually read from back/$(TARGET_CPU)/target.h.
 */

#if 0	//#if TARGET_ENDIANESS == BIG_ENDIAN

#define READ_8_BE(ptr, offs)			_Read8(ptr, offs)
#define READ_16_BE(ptr, offs)			_Read16(ptr, offs)
#define READ_32_BE(ptr, offs)			_Read32(ptr, offs)

#define WRITE_8_BE(ptr, offs, data)		_Write8(ptr, offs, data)
#define WRITE_16_BE(ptr, offs, data)	_Write16(ptr, offs, data)
#define WRITE_32_BE(ptr, offs, data)	_Write32(ptr, offs, data)

#define READ_8_LE(ptr, offs)			_Read8Swap(ptr, offs)
#define READ_16_LE(ptr, offs)			_Read16Swap(ptr, offs)
#define READ_32_LE(ptr, offs)			_Read32Swap(ptr, offs)

#define WRITE_8_LE(ptr, offs, data)		_Write8Swap(ptr, offs, data)
#define WRITE_16_LE(ptr, offs, data)	_Write16Swap(ptr, offs, data)
#define WRITE_32_LE(ptr, offs, data)	_Write32Swap(ptr, offs, data)

#else

#define READ_8_LE(ptr, offs)			_Read8(ptr, offs)
#define READ_16_LE(ptr, offs)			_Read16(ptr, offs)
#define READ_32_LE(ptr, offs)			_Read32(ptr, offs)

#define WRITE_8_LE(ptr, offs, data)		_Write8(ptr, offs, data)
#define WRITE_16_LE(ptr, offs, data)	_Write16(ptr, offs, data)
#define WRITE_32_LE(ptr, offs, data)	_Write32(ptr, offs, data)

#define READ_8_BE(ptr, offs)			_Read8Swap(ptr, offs)
#define READ_16_BE(ptr, offs)			_Read16Swap(ptr, offs)
#define READ_32_BE(ptr, offs)			_Read32Swap(ptr, offs)

#define WRITE_8_BE(ptr, offs, data)		_Write8Swap(ptr, offs, data)
#define WRITE_16_BE(ptr, offs, data)	_Write16Swap(ptr, offs, data)
#define WRITE_32_BE(ptr, offs, data)	_Write32Swap(ptr, offs, data)

#endif

/*******************************************************************************
 Custom Data Types
*******************************************************************************/

/*
 * Register descriptor (arrays of these are called context descriptors.)
 */

typedef struct
{
	void	* ptr;

} REG_DESC;

/*******************************************************************************
 Generic Stuff
*******************************************************************************/

extern void *	(* Alloc)(UINT);
extern void		(* Free)(void *);
extern void		(* Print)(CHAR *, ...);

extern INT		SetupBasicServices(DRPPC_CFG *);

#endif // INCLUDED_TOPLEVEL_H
