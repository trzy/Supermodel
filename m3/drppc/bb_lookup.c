/*
 * bb_lookup.c
 *
 * Default BB Lookup services. Using a three-level paged mechanism.
 */

#include <string.h>	// memset

#include "types.h"
#include "drppc.h"
#include "toplevel.h"
#include "mmap.h"
#include "bb_lookup.h"

/*******************************************************************************
 Local Variables
*******************************************************************************/

static DRPPC_BB **** lut;

static UINT32 page1_shift, page1_mask, page1_num;
static UINT32 page2_shift, page2_mask, page2_num;
static UINT32 offs_shift, offs_mask, offs_num;

/*******************************************************************************
 Macros
*******************************************************************************/

#define GET_PAGE1_NUM(addr)	(((UINT32)(addr) >> page1_shift) & page1_mask)
#define GET_PAGE2_NUM(addr)	(((UINT32)(addr) >> page2_shift) & page2_mask)
#define GET_OFFS_NUM(addr)	(((UINT32)(addr) >> offs_shift) & offs_mask)

/*******************************************************************************
 Local Routines
*******************************************************************************/

/*
 * BOOL IsInstAddr (UINT32 pc);
 *
 * Returns TRUE if pc is a valid instruction address (i.e. it does appear to lie
 * in the range of a fetch[] region), FALSE otherwise.
 */

static BOOL IsInstAddr (UINT32 pc)
{
	return MMap_FindFetchRegion(pc) != NULL;
}

/*
 * INT Handle* (UINT32 addr, UINT page1, UINT page2, UINT offs);
 *
 * These routines handle the possible misses during BB info lookup (see
 * BBLookup_Lookup below).
 */

static INT HandleLv1PageFault (UINT32 addr, UINT page1, UINT page2, UINT offs)
{

#ifdef DRPPC_DEBUG
		printf("Lv1 page fault at %08X = %X,%X,%X\n", addr, page1, page2, offs);
#endif

	/*
	 * Check the PC.
	 */

	if (!IsInstAddr(addr))
		return DRPPC_BAD_PC;

	/*
	 * Allocate a new lv1 page.
	 */

	lut[page1] = (DRPPC_BB ***)Alloc(sizeof(DRPPC_BB **) * page2_num);

	if (lut[page1] == NULL)
		return DRPPC_OUT_OF_MEMORY;

	memset(lut[page1], 0, sizeof(DRPPC_BB **) * page2_num);

	return DRPPC_OKAY;
}

static INT HandleLv2PageFault (UINT32 addr, UINT page1, UINT page2, UINT offs)
{

#ifdef DRPPC_DEBUG
		printf("Lv2 page fault at %08X = %X,%X,%X\n", addr, page1, page2, offs);
#endif

	/*
	 * Check the PC.
	 */

	if (!IsInstAddr(addr))
		return DRPPC_BAD_PC;

	/*
	 * Allocate a new lv2 page.
	 */

	lut[page1][page2] = (DRPPC_BB **)Alloc(sizeof(DRPPC_BB *) * offs_num);

	if (lut[page1][page2] == NULL)
		return DRPPC_OUT_OF_MEMORY;

	memset(lut[page1][page2], 0, sizeof(DRPPC_BB *) * offs_num);

	return DRPPC_OKAY;
}

static INT HandleBBMiss (UINT32 addr, UINT page1, UINT page2, UINT offs)
{

#ifdef DRPPC_DEBUG
	printf("BB miss at %08X = %X,%X,%X\n", addr, page1, page2, offs);
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

	lut[page1][page2][offs] = (DRPPC_BB *)Alloc(sizeof(DRPPC_BB));

	if (lut[page1][page2][offs] == NULL)	// Very unlikely, too
		return DRPPC_OUT_OF_MEMORY;

	memset(lut[page1][page2][offs], 0, sizeof(DRPPC_BB));

	return DRPPC_OKAY;
}

/*******************************************************************************
 Public Routines.
*******************************************************************************/

/*
 * INT BBLookup_Setup (struct drppc_cfg * cfg);
 *
 * Initialize the custom Basic Block lookup mechanism.
 */

INT BBLookup_Setup (struct drppc_cfg * cfg)
{
	UINT32	mask;

	/*
	 * Compute the shift amount, bit mask and number of elements for each of the
	 * address components.
	 */

	offs_shift =	cfg->ignore_bits;
	offs_mask =		(1 << cfg->offs_bits) - 1;
	offs_num =		(1 << cfg->offs_bits);

	page2_shift =	offs_shift + cfg->offs_bits;
	page2_mask =	(1 << cfg->page2_bits) - 1;
	page2_num =		(1 << cfg->page2_bits);

	page1_shift =	page2_shift + cfg->page2_bits;
	page1_mask =	(1 << cfg->page1_bits) - 1;
	page1_num =		(1 << cfg->page1_bits);

	/*
	 * Probe the resulting values.
	 */

	mask = (1 << cfg->ignore_bits) - 1;
	mask ^= offs_mask << offs_shift;
	mask ^= page2_mask << page2_shift;
	mask ^= page1_mask << page1_shift;

	if (cfg->address_bits > DRPPC_INST_ADDR_BITS)
		return DRPPC_INVALID_CONFIG;

	if (mask != ((UINT32)0xFFFFFFFF >> (DRPPC_INST_ADDR_BITS - cfg->address_bits)))
		return DRPPC_INVALID_CONFIG;

	/*
	 * Clean the LUT in case it's allocated.
	 */

	BBLookup_Clean();

	/*
	 * Allocate the lookup table.
	 */

	if((lut = (DRPPC_BB ****)Alloc(sizeof(DRPPC_BB ***) * page1_num)) == NULL)
		return DRPPC_OUT_OF_MEMORY;

	memset(lut, 0, sizeof(DRPPC_BB ***) * page1_num);

	return DRPPC_OKAY;
}

/*
 * void BBLookup_Clean (void);
 *
 * Cleanup the custom BB lookup system.
 */

void BBLookup_Clean (void)
{
	BBLookup_Invalidate ();
}

/*
 * DRPPC_BB * BBLookup_Lookup (UINT32 addr, INT * errcode);
 *
 * This one looks up a BB info struct in the LUT.
 */

DRPPC_BB * BBLookup_Lookup (UINT32 addr, INT * errcode)
{
	UINT page1 = GET_PAGE1_NUM(addr);
	UINT page2 = GET_PAGE2_NUM(addr);
	UINT offs = GET_OFFS_NUM(addr);

	if (lut[page1] == NULL)
		if ((*errcode = HandleLv1PageFault(addr, page1, page2, offs)) != DRPPC_OKAY)
			return NULL;

	if (lut[page1][page2] == NULL)
		if ((*errcode = HandleLv2PageFault(addr, page1, page2, offs)) != DRPPC_OKAY)
			return NULL;

	if (lut[page1][page2][offs] == NULL)
		if ((*errcode = HandleBBMiss(addr, page1, page2, offs)) != DRPPC_OKAY)
			return NULL;

	return lut[page1][page2][offs];
}

/*
 * void BBLookup_Invalidate (void);
 *
 * Used on cache overflow to invalidate the entire LUT.
 */

void BBLookup_Invalidate (void)
{
	UINT	i, j, k;

	if (lut != NULL)
	{
		for (i = 0; i < page1_num; i++)
			if (lut[i] != NULL)
			{
				for (j = 0; j < page2_num; j++)
					if (lut[j] != NULL)
					{
						for (k = 0; k < offs_num; k++)
							if (lut[k] != NULL)
								Free(lut[i][j][k]);
						Free(lut[i][j]);
					}
				Free(lut[i]);
			}
		Free(lut);
	}
}
