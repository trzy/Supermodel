/*
 * mmap.c
 *
 * Memory map implementation.
 */

#include "types.h"
#include "drppc.h"
#include "toplevel.h"
#include "mmap.h"
#include TARGET_CPU_HEADER
#include SOURCE_CPU_HEADER

#ifndef TARGET_IMPLEMENTS_MMAP_HANDLING

/******************************************************************************
 Local Variables
******************************************************************************/

static DRPPC_REGION * fetch;
static DRPPC_REGION * read8,  * read16,  * read32;
static DRPPC_REGION * write8, * write16, * write32;

/******************************************************************************
 Local Routines
******************************************************************************/

static DRPPC_REGION * MMap_FindRegion(DRPPC_REGION * table, UINT32 addr)
{
	// Stupid linear search

	while (table->end != 0)
	{
		if (table->start <= addr && table->end > addr)
			return table;
		table ++;
	}
	return NULL;
}

/******************************************************************************
 Interface
******************************************************************************/

INT MMap_Setup (DRPPC_MMAP * cfg)
{
	fetch	= cfg->fetch;

	read8	= cfg->read8;
	read16	= cfg->read16;
	read32	= cfg->read32;

	write8	= cfg->write8;
	write16	= cfg->write16;
	write32	= cfg->write32;

	if (!fetch || !read8 || !read16 || !read32 ||
		!write8 || !write16 || !write32)
		return DRPPC_ERROR;

	return DRPPC_OKAY;
}

void MMap_Clean (void)
{
	// Do nothing
}

/*
 * Region retrival
 */

DRPPC_REGION * MMap_FindFetchRegion(UINT32 addr)
{
	return MMap_FindRegion(fetch, addr);
}

DRPPC_REGION * MMap_FindRead8Region(UINT32 addr)
{
	return MMap_FindRegion(read8, addr);
}

DRPPC_REGION * MMap_FindRead16Region(UINT32 addr)
{
	return MMap_FindRegion(read16, addr);
}

DRPPC_REGION * MMap_FindRead32Region(UINT32 addr)
{
	return MMap_FindRegion(read32, addr);
}

DRPPC_REGION * MMap_FindWrite8Region(UINT32 addr)
{
	return MMap_FindRegion(write8, addr);
}

DRPPC_REGION * MMap_FindWrite16Region(UINT32 addr)
{
	return MMap_FindRegion(write16, addr);
}

DRPPC_REGION * MMap_FindWrite32Region(UINT32 addr)
{
	return MMap_FindRegion(write32, addr);
}

/*
 * Runtime generic memory access
 */

UINT32 MMap_GenericRead8 (UINT32 addr)
{
	DRPPC_REGION	* region;
	UINT32			data;

	if ((region = MMap_FindRead8Region(addr)) == NULL)
	{
		printf("Invalid GenericRead8 from 0x%08X\n", addr);
		exit(1);
		return 0xFF;
	}

	if (region->ptr == NULL)
		data = (UINT32)((UINT8 (*)(UINT32))region->handler)(addr);
	else if (region->big_endian == TRUE)
		data = (UINT32)(UINT8)(READ_8_BE(region->ptr, (addr - region->start)));
	else
		data = (UINT32)(UINT8)(READ_8_LE(region->ptr, (addr - region->start)));

//	if (region->ptr == NULL)
//		printf("Generic read 8 0x%08X returns 0x%08X\n", addr, data);

	return data;
}

UINT32 MMap_GenericRead16 (UINT32 addr)
{
	DRPPC_REGION	* region;
	UINT32			data;

	if ((region = MMap_FindRead16Region(addr)) == NULL)
	{
		printf("Invalid GenericRead16 from 0x%08X\n", addr);
		exit(1);
		return 0xFFFF;
	}

	if (region->ptr == NULL)
		data = (UINT32)((UINT16 (*)(UINT32))region->handler)(addr);
	else if (region->big_endian == TRUE)
		data = (UINT32)(READ_16_BE(region->ptr, (addr - region->start)));
	else
		data = (UINT32)(READ_16_LE(region->ptr, (addr - region->start)));

//	if (region->ptr == NULL)
//		printf("Generic read 16 0x%08X returns 0x%08X\n", addr, data);

	return data;
}

UINT32 MMap_GenericRead32 (UINT32 addr)
{
	DRPPC_REGION	* region;
	UINT32			data;

	if (addr == 0xF014003C)
	{ addr = addr; }

	if ((region = MMap_FindRead32Region(addr)) == NULL)
	{
		printf("Invalid GenericRead32 from 0x%08X\n", addr);
		exit(1);
		return 0xFFFFFFFF;
	}

	if (region->ptr == NULL)
		data = ((UINT32 (*)(UINT32))region->handler)(addr);
	else if (region->big_endian == TRUE)
		data = READ_32_BE(region->ptr, (addr - region->start));
	else
		data = READ_32_LE(region->ptr, (addr - region->start));

//	if (region->ptr == NULL)
//		printf("Generic read 32 0x%08X returns 0x%08X\n", addr, data);

	return data;
}

void MMap_GenericWrite8 (UINT32 addr, UINT32 data)
{
	DRPPC_REGION * region;

	if ((region = MMap_FindWrite8Region(addr)) == NULL)
	{
		printf("Invalid GenericWrite8 from 0x%08X, 0x%02X\n", addr, data);
		exit(1);
		return;
	}

//	if (region->ptr == NULL)
//		printf("Generic write 8 0x%08X = 0x%02X\n", addr, data);

	if (region->ptr == NULL)
		((void (*)(UINT32, UINT32))region->handler)(addr, data);
	else if (region->big_endian == TRUE)
		WRITE_8_BE(region->ptr, (addr - region->start), (UINT8)data);
	else
		WRITE_8_LE(region->ptr, (addr - region->start), (UINT8)data);
}

void MMap_GenericWrite16 (UINT32 addr, UINT32 data)
{
	DRPPC_REGION * region;

	if ((region = MMap_FindWrite16Region(addr)) == NULL)
	{
		printf("Invalid GenericWrite16 from 0x%08X, 0x%04X\n", addr, data);
		exit(1);
		return;
	}

//	if (region->ptr == NULL)
//		printf("Generic write 16 0x%08X = 0x%04X\n", addr, data);

	if (region->ptr == NULL)
		((void (*)(UINT32, UINT32))region->handler)(addr, data);
	else if (region->big_endian == TRUE)
		WRITE_16_BE(region->ptr, (addr - region->start), (UINT16)data);
	else
		WRITE_16_LE(region->ptr, (addr - region->start), (UINT16)data);
}

extern CHAR core_id;

void MMap_GenericWrite32 (UINT32 addr, UINT32 data)
{
	DRPPC_REGION * region;

	if ((region = MMap_FindWrite32Region(addr)) == NULL)
	{
		printf("Invalid GenericWrite32 from 0x%08X, 0x%08X\n", addr, data);
		exit(1);
		return;
	}

	if (addr >= 0x001EFFB0 && addr <= 0x001EFFB8)
		printf("[%c] 0x%08X = 0x%08X\n", core_id, addr, data);

//	if (region->ptr == NULL)
//		printf("Generic write 32 0x%08X = 0x%08X\n", addr, data);

	if (region->ptr == NULL)
		((void (*)(UINT32, UINT32))region->handler)(addr, data);
	else if (region->big_endian == TRUE)
		WRITE_32_BE(region->ptr, (addr - region->start), data);
	else
		WRITE_32_LE(region->ptr, (addr - region->start), data);
}

#endif // TARGET_IMPLEMENTS_MMAP_HANDLING
