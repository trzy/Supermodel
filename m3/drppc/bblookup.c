/*
 * bblookup.c
 *
 * Default BB Lookup services. Using a three-level paged mechanism.
 */

#include <string.h>	// memset

#include "types.h"
#include "drppc.h"
#include "toplevel.h"
#include "mmap.h"
#include "bblookup.h"

/*******************************************************************************
 Local Variables
*******************************************************************************/

typedef struct
{
	DRPPC_BB	**** lut;

	UINT32		page1_shift, page1_mask, page1_num;
	UINT32		page2_shift, page2_mask, page2_num;
	UINT32		offs_shift, offs_mask, offs_num;

} BBLOOKUP_INFO;

static BBLOOKUP_INFO * info;

/*******************************************************************************
 Macros
*******************************************************************************/

#define GET_PAGE1_NUM(addr)	(((UINT32)(addr) >> info->page1_shift) & info->page1_mask)
#define GET_PAGE2_NUM(addr)	(((UINT32)(addr) >> info->page2_shift) & info->page2_mask)
#define GET_OFFS_NUM(addr)	(((UINT32)(addr) >> info->offs_shift) & info->offs_mask)

/*******************************************************************************
 Local Routines
*******************************************************************************/

/*
 * IsInstAddr():
 *
 * Returns TRUE if pc is a valid instruction address (i.e. it does appear to lie
 * in the range of a fetch[] region), FALSE otherwise.
 */

static BOOL IsInstAddr (UINT32 pc)
{
	return MMap_FindFetchRegion(pc) != NULL;
}

/*
 * Handle*():
 *
 * These routines handle the possible misses during BB info lookup (see
 * BBLookup_Lookup below).
 */

static INT HandleLv1PageFault (UINT32 addr, UINT page1, UINT page2, UINT offs)
{

#ifdef DRPPC_DEBUG
	Print("Lv1 page fault at %08X = %X,%X,%X", addr, page1, page2, offs);
#endif

	/*
	 * Check the PC.
	 */

	if (!IsInstAddr(addr))
		return DRPPC_BAD_PC;

	/*
	 * Allocate a new lv1 page.
	 */

	info->lut[page1] = (DRPPC_BB ***)Alloc(sizeof(DRPPC_BB **) * info->page2_num);

	if (info->lut[page1] == NULL)
		return DRPPC_OUT_OF_MEMORY;

	memset(info->lut[page1], 0, sizeof(DRPPC_BB **) * info->page2_num);

	return DRPPC_OKAY;
}

static INT HandleLv2PageFault (UINT32 addr, UINT page1, UINT page2, UINT offs)
{

#ifdef DRPPC_DEBUG
	Print("Lv2 page fault at %08X = %X,%X,%X", addr, page1, page2, offs);
#endif

	/*
	 * Check the PC.
	 */

	if (!IsInstAddr(addr))
		return DRPPC_BAD_PC;

	/*
	 * Allocate a new lv2 page.
	 */

	info->lut[page1][page2] = (DRPPC_BB **)Alloc(sizeof(DRPPC_BB *) * info->offs_num);

	if (info->lut[page1][page2] == NULL)
		return DRPPC_OUT_OF_MEMORY;

	memset(info->lut[page1][page2], 0, sizeof(DRPPC_BB *) * info->offs_num);

	return DRPPC_OKAY;
}

static INT HandleBBMiss (UINT32 addr, UINT page1, UINT page2, UINT offs)
{

#ifdef DRPPC_DEBUG
	Print("BB miss at %08X = %X,%X,%X", addr, page1, page2, offs);
#endif

	/*
	 * Check the PC.
	 *
	 * NOTE: the PC will hardly be invalid, unless page1_bits+page2_bits is
	 * very small.
	 */

//	if (!IsInstAddr(addr))
//		return DRPPC_BAD_PC;

	/*
	 * Allocate a new BB info block.
	 */

	info->lut[page1][page2][offs] = (DRPPC_BB *)Alloc(sizeof(DRPPC_BB));

	if (info->lut[page1][page2][offs] == NULL)	// Very unlikely, too
		return DRPPC_OUT_OF_MEMORY;

	memset(info->lut[page1][page2][offs], 0, sizeof(DRPPC_BB));

	return DRPPC_OKAY;
}

/*******************************************************************************
 Public Routines
*******************************************************************************/

/*
 * INT BBLookup_Setup (struct drppc_cfg * cfg);
 *
 * Initialize the custom Basic Block lookup mechanism.
 *
 * Parameters:
 *		cfg = Pointer to configuration data.
 *
 * Returns:
 *		DRPPC_OKAY if successful.
 *		DRPPC_INVALID_CONFIG if the configuration data for the lookup tables is
 *      not valid.
 *		DRPPC_OUT_OF_MEMORY if there wasn't enough memory to allocate the
 *		tables.
 */

INT BBLookup_Setup (struct drppc_cfg * cfg, void ** _info)
{
	UINT32	mask;

	ASSERT(cfg);
	ASSERT(_info);

	/*
	 * Allocate a new BBLOOKUP_INFO in the current context.
	 */

	if (!(*_info = Alloc(sizeof(BBLOOKUP_INFO))))
		return DRPPC_OUT_OF_MEMORY;

	info = *_info;

	/*
	 * Compute the shift amount, bit mask and number of elements for each of the
	 * address components.
	 */

	info->offs_shift =	cfg->ignore_bits;
	info->offs_mask =	(1 << cfg->offs_bits) - 1;
	info->offs_num =		(1 << cfg->offs_bits);

	info->page2_shift =	info->offs_shift + cfg->offs_bits;
	info->page2_mask =	(1 << cfg->page2_bits) - 1;
	info->page2_num =	(1 << cfg->page2_bits);

	info->page1_shift =	info->page2_shift + cfg->page2_bits;
	info->page1_mask =	(1 << cfg->page1_bits) - 1;
	info->page1_num =	(1 << cfg->page1_bits);

	/*
	 * Probe the resulting values.
	 */

	mask = (1 << cfg->ignore_bits) - 1;
	mask ^= info->offs_mask  << info->offs_shift;
	mask ^= info->page2_mask << info->page2_shift;
	mask ^= info->page1_mask << info->page1_shift;

	if (cfg->address_bits > DRPPC_INST_ADDR_BITS)
		return DRPPC_INVALID_CONFIG;

	if (mask != ((UINT32)0xFFFFFFFF >> (DRPPC_INST_ADDR_BITS - cfg->address_bits)))
		return DRPPC_INVALID_CONFIG;

	/*
	 * Allocate the lookup table.
	 */

	if((info->lut = (DRPPC_BB ****)Alloc(sizeof(DRPPC_BB ***) * info->page1_num)) == NULL)
		return DRPPC_OUT_OF_MEMORY;

	memset(info->lut, 0, sizeof(DRPPC_BB ***) * info->page1_num);

	return DRPPC_OKAY;
}

/*
 * void BBLookup_Clean (void);
 *
 * Cleanup the custom BB lookup system and releases the BB lut.
 */

void BBLookup_Clean (void)
{
	BBLookup_Invalidate ();

	if (info->lut != NULL)
	{
		Free(info->lut);
		info->lut = NULL;
	}
}

/*
 * void BBLookup_SetLookupInfo (void * _info);
 *
 * Set current BB lookup info.
 */

void BBLookup_SetLookupInfo (void * _info)
{
	ASSERT(_info);

	info = _info;
}

/*
 * DRPPC_BB * BBLookup_Lookup (UINT32 addr, INT * errcode);
 *
 * This one looks up a BB info struct in the LUT.
 *
 * Parameters:
 *		addr    = Address of the basic block entry point to look up.
 *		errcode = If an error occured looking the block up, the error
 *				  code is returned here. Possible error codes are:
 *						DRPPC_OUT_OF_MEMORY if out of memory.
 *						DRPPC_BAD_PC if the address is bad and cannot be used
 *						to create a new block.
 *
 * Returns:
 *		The basic block or NULL if the lookup failed for any reason.
 */

DRPPC_BB * BBLookup_Lookup (UINT32 addr, INT * errcode)
{
	UINT page1 = GET_PAGE1_NUM(addr);
	UINT page2 = GET_PAGE2_NUM(addr);
	UINT offs =	 GET_OFFS_NUM(addr);

	if (info->lut[page1] == NULL)
		if ((*errcode = HandleLv1PageFault(addr, page1, page2, offs)) != DRPPC_OKAY)
			return NULL;

	if (info->lut[page1][page2] == NULL)
		if ((*errcode = HandleLv2PageFault(addr, page1, page2, offs)) != DRPPC_OKAY)
			return NULL;

	if (info->lut[page1][page2][offs] == NULL)
		if ((*errcode = HandleBBMiss(addr, page1, page2, offs)) != DRPPC_OKAY)
			return NULL;

	return info->lut[page1][page2][offs];
}

/*
 * void BBLookup_Invalidate (void);
 *
 * Used on cache overflow to invalidate the entire LUT.
 */

void BBLookup_Invalidate (void)
{
#ifdef DRPPC_PROFILE
	UINT	lv1_page_count = 0,	lv2_page_count = 0,	bb_count = 0;
	UINT	total_interp_size = 0, total_native_size = 0;
	UINT	total_interp_time = 0, total_native_time = 0;
#endif

	UINT	i, j, k;

	if (info->lut != NULL)
	{
		for (i = 0; i < info->page1_num; i++)
		{
			if (info->lut[i] != NULL)
			{
				for (j = 0; j < info->page2_num; j++)
				{
					if (info->lut[i][j] != NULL)
					{
						for (k = 0; k < info->offs_num; k++)
						{
							if (info->lut[i][j][k] != NULL)
							{
#ifdef DRPPC_PROFILE
								DRPPC_BB * bb = info->lut[i][j][k];
								total_interp_size += bb->interp_size;
								total_native_size += bb->native_size;
								total_interp_time += bb->interp_exec_time;
								total_native_time += bb->native_exec_time;
								bb_count ++;
#endif
								Free(info->lut[i][j][k]);
								info->lut[i][j][k] = NULL;
							}
						}
#ifdef DRPPC_PROFILE
						lv2_page_count ++;
#endif
						Free(info->lut[i][j]);
						info->lut[i][j] = NULL;
					}
				}
#ifdef DRPPC_PROFILE
				lv1_page_count ++;
#endif
				Free(info->lut[i]);
				info->lut[i] = NULL;
			}
		}
		info->lut = NULL;
	}

#ifdef DRPPC_PROFILE
	if (bb_count > 0)
	{
		UINT total_size;

		total_size = sizeof(DRPPC_BB ***) * info->page1_num +
					 sizeof(DRPPC_BB **)  * info->page2_num * lv1_page_count +
					 sizeof(DRPPC_BB *)   * info->offs_num  * lv2_page_count +
					 sizeof(DRPPC_BB)     * bb_count;

		Print
		(
			"\n"
            "============================================================================\n"
            " BB lookup stats:\n"
			"  stored    1 Lv1 Page, %u Bytes (fixed)\n"
			"  stored %4u Lv2 Page(s), %u Bytes each\n"
			"  stored %4u Lv3 Page(s), %u Bytes each\n"
			"  stored %4u BB structs, %u Bytes each\n"
			"  for a total of %u Bytes (%f kB), %u Bytes/BB on average\n"
			" \n"
            " Code size stats:\n"
			"  Average expansion ratio is %f\n"
            "  interp average: %u Bytes\n"
			"  native average: %u Bytes\n"
			" \n"
            " Execution time stats:\n"
			"  average speedup is %f\n"
			"  interp average: %u cycles/BB\n"
            "  native average: %u cycles/BB\n\n"
            "============================================================================\n"
			"\n",

			sizeof(DRPPC_BB ***) * info->page1_num,

			lv1_page_count, sizeof(DRPPC_BB **) * info->page2_num,

			lv2_page_count, sizeof(DRPPC_BB *) * info->offs_num,

			bb_count, sizeof(DRPPC_BB),

			total_size, (float)total_size / 1024.0f, total_size / bb_count,
			
			(total_interp_size == 0) ? 0 : total_native_size / total_interp_size,
			total_interp_size / bb_count,
			total_native_size / bb_count,

			(total_interp_time == 0) ? 0 : total_native_time / total_interp_time,
			total_interp_time / bb_count,
			total_native_time / bb_count
		);

		ASSERT(0);
	}
#endif
}
