/*
 * drppc.h
 *
 * Header file for DRPPC. This is the only header that must be included by
 * external applications.
 */

#ifndef INCLUDED_DRPPC_H
#define INCLUDED_DRPPC_H

#include <stdarg.h>
#include "types.h"

/*******************************************************************************
 Configuration
*******************************************************************************/

#define DRPPC_INST_ADDR_BITS	32

/*******************************************************************************
 Enums
*******************************************************************************/

/*
 * Error Codes &c
 */

typedef enum
{
	// Error conditions

	DRPPC_ERROR = -1,			// Generic error
	DRPPC_INVALID_CONFIG = -2,	// Invalid configuration parameter
	DRPPC_BAD_PC = -3,			// Invalid PC at runtime
	DRPPC_OUT_OF_MEMORY = -4,	// Allocation failed
	DRPPC_COMPILE_ERROR = -5,	// Compilation error
	DRPPC_RUNTIME_ERROR = -6,	// Runtime error

	DRPPC_OKAY	= 0,			// Okay

	// Non-error conditions used internally (defined here for coherency)

	DRPPC_TERMINATOR,			// Used during decode when a branch is hit
	DRPPC_TIMESLICE_ENDED,		// Used by the native code

} DRPPC_ERRNUM;

/*******************************************************************************
 Structs
*******************************************************************************/

/*
 * struct drppc_region
 *
 * It represents a (data or instruction) memory region, a range in the memory
 * map assigned  to the same device. The range is bounded between start and end,
 * inclusive.
 *
 * If ptr is not NULL, the region is directly accessible from the buffer
 * pointed by ptr. Otherwise, handler must be a pointer to the proper memory
 * access function.
 *
 * If volatile_ptr is TRUE, the pointer cannot be inlined in the generated code,
 * else it will be whenever possible. This flag affects only direct access
 * regions.
 *
 * If big_enadian is TRUE, the region is ordered in 32-bit big endian (1234),
 * else it is 32-bit little endian. There's no support for other memory
 * layouts (See, Motorola 68000). This flag affects only direct access regions.
 */

typedef struct drppc_region
{
	UINT32	start, end;
	UINT8	* ptr;
	void	* handler;
	BOOL	volatile_ptr;
	BOOL	big_endian;

} DRPPC_REGION;

/*
 * The following macros encode some standard region types.
 *
 * NOTE: only minimal typecasting is explicitized. This way, type conversion
 * errors will be more consistent in user code.
 */

#define DRPPC_REGION_BUF_BE(start, end, buf, is_volatile) \
			{ start, end, (UINT8 *)(buf), NULL, is_volatile, TRUE }

#define DRPPC_REGION_BUF_LE(start, end, buf, is_volatile) \
			{ start, end, (UINT8 *)(buf), NULL, is_volatile, FALSE }

#define DRPPC_REGION_HANDLER(start, end, handler, is_volatile) \
			{ start, end, NULL, (void *)(handler), is_volatile, FALSE }

#define DRPPC_REGION_END \
			{ 0, 0, NULL, NULL, TRUE, FALSE }

// This is specifically thought as an 'empty' region to be used with the
// macros below.
#define DRPPC_REGION_PLACEHOLDER \
			{ 0xDEADBEEF, 0xFEEDBEE5, NULL, NULL, FALSE, FALSE }

/*
 * The following macros are used to edit REGION variables on-the-fly.
 *
 * NOTE: only minimal typecasting is explicitized. This way, type conversion
 * errors will be more consistent in user code.
 */

#define DRPPC_SET_REGION_BUF_BE(r, _start, _end, buf, is_volatile)	\
		{															\
			r.start			= _start;								\
			r.end			= _end;									\
			r.ptr			= (UINT8 *)buf;							\
			r.handler		= NULL;									\
			r.volatile_ptr	= is_volatile;							\
			r.big_endian	= TRUE;									\
		}

#define DRPPC_SET_REGION_BUF_LE(r, _start, _end, buf, is_volatile)	\
		{															\
			r.start			= _start;								\
			r.end			= _end;									\
			r.ptr			= (UINT8 *)buf;							\
			r.handler		= NULL;									\
			r.volatile_ptr	= is_volatile;							\
			r.big_endian	= FALSE;								\
		}

#define DRPPC_SET_REGION_HANDLER(r, _start, _end, h, is_volatile)	\
		{															\
			r.start			= _start;								\
			r.end			= _end;									\
			r.ptr			= NULL;									\
			r.handler		= (void *)h;							\
			r.volatile_ptr	= is_volatile;							\
			r.big_endian	= FALSE; /* Ignored */					\
		}

/*
 * struct drppc_mmap
 *
 * A struct containing seven differen region arrays, one for each memory access
 * mode. It's used for configuration (during initialization) only.
 */

typedef struct drppc_mmap
{
	DRPPC_REGION	* fetch;

	DRPPC_REGION	* read8;
	DRPPC_REGION	* read16;
	DRPPC_REGION	* read32;

	DRPPC_REGION	* write8;
	DRPPC_REGION	* write16;
	DRPPC_REGION	* write32;

} DRPPC_MMAP;

/*
 * struct drppc_bb
 *
 * The elementary unit for basic block (BB) informations.
 *
 * The total number of executions for the BB is kept in count, while ptr holds
 * the address of the translated BB.
 *
 * If DRPPC_PROFILE is defined, there are a number of additional fields used
 * for profiling.
 */

typedef struct drppc_bb
{
	UINT	count;		// Execution Count
	UINT8	* ptr;		// Native BB Pointer

#ifdef DRPPC_PROFILE
	UINT	osize;		// Size of interpreted (original) BB
	UINT	otime;		// Exec time of interpreted (orifinal) BB

	UINT	nsize;		// Size of recompiled (native) BB
	UINT	ntime;		// Exec time of recompiled (native) BB
#endif

} DRPPC_BB;

/*
 * struct drppc_cfg
 *
 * DRPPC configuration. This structure holds alot of stuff, and must be filled
 * in by the core application before initializing drppc. All of the fields are
 * pretty much self-explanatory.
 *
 * NOTE: parts of this struct take general valence over all contexts drppc
 * runs, others specify context specific 
 */

typedef struct drppc_cfg
{
	// Custom Handlers

	void *		(* Alloc)(UINT);
	void		(* Free)(void *);
	void		(* Print)(CHAR *, ...);

	// Optional BB Lookup Handlers

	INT			(* SetupBBLookup)(struct drppc_cfg *);
	void		(* CleanBBLookup)(void);
	DRPPC_BB *	(* LookupBB)(UINT32, INT *);
	void		(* InvalidateBBLookup)(void);

	// Core Configuration

	UINT		native_cache_size;
	UINT		native_cache_guard_size;

	UINT		intermediate_cache_size;
	UINT		intermediate_cache_guard_size;

	UINT		hot_threshold;

	// Custom BB Lookup Configuration

	UINT		address_bits;
	UINT		page1_bits;
	UINT		page2_bits;
	UINT		offs_bits;
	UINT		ignore_bits;

	// Memory Maps

	DRPPC_MMAP	mmap_cfg;

} DRPPC_CFG;

// Move this in source.h!!!
#define DRPPC_ZERO_THRESHOLD	1	// The exact threshold needed to bypass the
									// profiling stage.

/*******************************************************************************
 Interface
*******************************************************************************/

#include "source.h"

#endif // INCLUDED_DRPPC_H
