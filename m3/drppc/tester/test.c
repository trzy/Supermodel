/*
 * tester/test.c
 *
 * Simple test platform. The test program will load Supermodel ROM sets and
 * execute them using a minimalistic emulation of just the ROM and RAM regions.
 * 
 * NOTE: Yes, this test package is an ugly hack job ;)
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drppc.h"
#include "cfgparse.h"
#include "file.h"

/*******************************************************************************
 Macros
*******************************************************************************/

#define SAFE_FREE(p)	if (p != NULL) { free(p); p = NULL; }


/*******************************************************************************
 Functions
 
 These need to be fleshed out...
*******************************************************************************/

static UINT32 TestFetch (UINT32 addr){	printf("TestFetch ( %08X )\n", addr); return 0; }

static UINT8  TestRead8  (UINT32 addr){ return 0; }
static UINT16 TestRead16 (UINT32 addr){ return 0; }
static UINT32 TestRead32 (UINT32 addr){ return 0; }

static void TestWrite8  (UINT32 addr, UINT8 data){ }
static void TestWrite16 (UINT32 addr, UINT16 data){ }
static void TestWrite32 (UINT32 addr, UINT32 data){ }

static UINT TestIRQCallback (void){ return 0; }


/*******************************************************************************
 Variables
*******************************************************************************/

/*
 * drppc Configuration
 */
 
static DRPPC_CFG cfg;

/*
 * PPC Memory Map
 */
 
#define NULL_REGION { 0, 0, NULL, NULL, FALSE }

static DRPPC_REGION	mmap_fetch[]	= { { 0x00000000, 0xFFFFFFFF, NULL, (void *)TestFetch, FALSE }, NULL_REGION };
static DRPPC_REGION	mmap_read8[]	= { { 0x00000000, 0xFFFFFFFF, NULL, (void *)TestRead8, FALSE }, NULL_REGION };
static DRPPC_REGION	mmap_read16[]	= { { 0x00000000, 0xFFFFFFFF, NULL, (void *)TestRead16, FALSE }, NULL_REGION };
static DRPPC_REGION	mmap_read32[]	= { { 0x00000000, 0xFFFFFFFF, NULL, (void *)TestRead32, FALSE }, NULL_REGION };
static DRPPC_REGION	mmap_write8[]	= { { 0x00000000, 0xFFFFFFFF, NULL, (void *)TestWrite8, FALSE }, NULL_REGION };
static DRPPC_REGION	mmap_write16[]	= { { 0x00000000, 0xFFFFFFFF, NULL, (void *)TestWrite16, FALSE }, NULL_REGION };
static DRPPC_REGION	mmap_write32[]	= { { 0x00000000, 0xFFFFFFFF, NULL, (void *)TestWrite32, FALSE }, NULL_REGION };

/*
 * Model 3 Memory Space
 */
 
static UINT8	*ram = NULL;            // PowerPC RAM
static UINT8    *crom = NULL;           // CROM (all CROM memory is allocated here)
static UINT8    *crom_bank;             // points to current 8MB CROM bank


/*******************************************************************************
 ROM Loading
 
 ROMs are loaded based on entries in the game database.
*******************************************************************************/

/*
 * ROM File Data Structures
 */
 
typedef struct
{
    CHAR	name[20];
    UINT	size;
    UINT32	crc32;

} ROMFILE;

typedef struct {
    UINT32  address;
    UINT32  data;
    INT     size;   // 8, 16, or 32
} PATCH;

typedef struct
{
	char	id[20];
	char	superset_id[20];
	char	title[96];
	char	manuf[32];
    UINT	date;
    INT		step;
    INT     bridge;
    FLAGS	flags;
	INT		num_patches;
	PATCH	patch[64];

	ROMFILE ic[61];

} ROMSET;

/*
 * Byteswap a buffer.
 */

static void ByteSwap(UINT8 *buf, UINT size)
{
    UINT    i;
    UINT8   tmp;

    for (i = 0; i < size; i += 2)
    {
        tmp = buf[i];
        buf[i] = buf[i + 1];
        buf[i + 1] = tmp;
    }
}

static void ByteSwap32(UINT8 *buf, UINT size)
{
	UINT	i;

	for(i = 0; i < size; i += 4)
		*(UINT32 *)&buf[i] = BSWAP32(*(UINT32 *)&buf[i]);
}

/*
 * GetROMSet():
 *
 * Get ROM set (denoted by id string) information from external file database
 * (file) into a ROMSET structure (romset.) 
 */

static BOOL GetROMSet(char* id, char* file, ROMSET* romset)
{
	BOOL	found, end;
	int 	romsets = 0;
	char	*arg, *param;

	if (cfgparser_set_file(file) != 0 ) {
		printf("Couldn't open ROM list file %s\n",file);
		return 1;
	}
	strcpy( romset->id, id );

	found = FALSE;
	while( cfgparser_step() == 0 && !found )
	{
		arg = cfgparser_get_lhs();

		/* Find the romset id */
		if( arg != NULL ) {
			if( stricmp(arg, "ID") == 0 ) {
				param = cfgparser_get_rhs_string(0);

				if( stricmp(param, id) == 0 ) {
					found = TRUE;
					break;
				}				
			}
		}
	}
	if( !found ) {
		printf("Romset %s not found in %s\n", id, file);
		return 1;
	}

	/* Parse the romset */
	end = FALSE;
	while( cfgparser_step() == 0 && !end )
	{
		arg = cfgparser_get_lhs();
		if( arg == NULL )
			break;

		if( stricmp(arg, "ID") == 0 ) {
			end = TRUE;
		}
		else if( stricmp(arg, "NAME") == 0 ) {
			char* name = cfgparser_get_rhs();
			strcpy( romset->title, name );
		}
		else if( stricmp(arg, "PARENT") == 0 ) {
			char* name = cfgparser_get_rhs();
			strcpy( romset->superset_id, name );
		}
		else if( stricmp(arg, "MANUFACTURER") == 0 ) {
			char* manuf = cfgparser_get_rhs();
			strcpy( romset->manuf, manuf );
		}
		else if( stricmp(arg, "DATE") == 0 ) {
			char* date = cfgparser_get_rhs_string(0);
			sscanf( date, "%d", &romset->date );
		}
		else if( stricmp(arg, "STEP") == 0 ) {
			float s;
			char* step = cfgparser_get_rhs_string(0);
			sscanf( step, "%f", &s );
			romset->step = ((int)(s) << 4) | (int)((s - floor(s)) * 10);
		}
        else if(stricmp(arg, "BRIDGE") == 0) {
            char *bridge = cfgparser_get_rhs();
            if (stricmp(bridge, "MPC105") == 0)
                romset->bridge = 1;
            else if (stricmp(bridge, "MPC106") == 0)
                romset->bridge = 2;
            else
            {
                romset->bridge = 0;
                printf("WARNING: unknown bridge controller type (%s), using default\n", bridge);
            }
        }
		else if( stricmp(arg, "PATCH") == 0 ) {
			cfgparser_get_rhs_number( &romset->patch[romset->num_patches].address, 0 );
            cfgparser_get_rhs_number( &romset->patch[romset->num_patches].data, 1 );
            romset->patch[romset->num_patches].size = 32;
			romset->num_patches++;
		}
        else if( stricmp(arg, "PATCH8") == 0 ) {
			cfgparser_get_rhs_number( &romset->patch[romset->num_patches].address, 0 );
            cfgparser_get_rhs_number( &romset->patch[romset->num_patches].data, 1 );
            romset->patch[romset->num_patches].size = 8;
			romset->num_patches++;
		}
		else if( stricmp(arg, "CONTROLLER") == 0 ) {
			/* TODO: Handle this */
		}
		else if( strncmp(arg, "IC", 2) == 0 ) {
            char* icnum, *name;
			int ic_number = 0;

			/* get ic number */
			icnum = &arg[2];
			sscanf( icnum, "%d", &ic_number );

			name = cfgparser_get_rhs_string( 0 );

			if( name != NULL )
				strcpy( romset->ic[ic_number].name, name );

			cfgparser_get_rhs_number( &romset->ic[ic_number].size, 1 );
			cfgparser_get_rhs_number( &romset->ic[ic_number].crc32, 2 );
		}
		else {
			printf("%s: Unknown argument %s at line %d\n", file, arg, cfgparser_get_line_number() );
		}
	}
	if( !end && !cfgparser_eof() ) {
		printf("%s: Parse error at line %d\n", file, cfgparser_get_line_number() );
		return 1;
	}

	cfgparser_unset_file();

    /*
     * Set some defaults if not specified.
     *
     * NOTE: More things need to be checked for here.
     */

    if (romset->bridge == 0)
        romset->bridge = (romset->step < 0x20) ? 1 : 2;

	return 0;
}


/*
 * LoadROMFile():
 * 
 * Load a batch of files, (word) interleaving as needed. The list pointer
 * should point to the first file to load and the number of entries read from
 * the list will depend on the interleaving parameter. 
 *
 * Returns non-zero on error. 
 */

#define NO_ITLV	1
#define ITLV_2	2
#define ITLV_4	4
#define ITLV_16	16

static BOOL LoadROMFile(UINT8 * buff, ROMFILE * list, UINT32 itlv)
{
	UINT i, j;

	/* check if it's an unused ROMFILE, if so skip it */

	if(list[0].size == 0)
		return 0;

	/* check interleave size for validity */

	if(	itlv != NO_ITLV &&
		itlv != ITLV_2 &&
		itlv != ITLV_4 &&
		itlv != ITLV_16 )
		return 1;

	/* load all the needed files linearly */

	for(i = 0; i < itlv; i++)
	{
        FILE * file;
        UINT   size;
		UINT8* temp_buffer;
		UINT32 crc;

		/* all the files must be the same size */

		if(list[i].size != list[0].size)
			return 1;

		printf("loading %s\n", list[i].name); // temp

		size = get_file_size(list[i].name);
		if( size <= 0 ) {
			printf("File %s not found.\n", list[i].name);
			return 1;
		}
		if( size != list[i].size ) {
			printf("File %s has wrong size ! (%d bytes, should be %d bytes)\n", list[i].name, size, list[i].size);
			return 1;
		}

		temp_buffer = (UINT8*)malloc(size);
		if( !read_file(list[i].name, temp_buffer, size) )
			return 1;

		/* check file CRC */
		crc = 0;
		crc = crc32(crc, temp_buffer, size);
		if( crc != list[i].crc32 ) {
			printf("File %s has wrong CRC: %08X (should be %08X)\n", list[i].name, crc, list[i].crc32);
			return 1;
		}

		/* interleave */

        for (j = 0; j < list[0].size; j += 2)
        {
            buff[i*2+j*itlv+0] = temp_buffer[j];
            buff[i*2+j*itlv+1] = temp_buffer[j+1];
        }

		SAFE_FREE(temp_buffer);
	}

	/* everything went right */

	return 0;
}

BOOL LoadROM(CHAR * id)
{
    CHAR    string[256] = "";
    ROMSET  romset;
	ROMFILE list_crom[4], list_crom0[4], list_crom1[4], list_crom2[4], list_crom3[4];	
    UINT8   * crom0 = NULL, * crom1 = NULL, * crom2 = NULL, * crom3 = NULL;
    INT     i;

	memset( &romset, 0, sizeof(ROMSET) );
	if (GetROMSet(id, "games.ini", &romset) )
		return 1;
	
	printf("Loading \"%s\"\n", romset.title);

	/* set rom lists for rom loading */
	for( i=0; i < 4; i++ ) {
		memcpy( &list_crom[i], &romset.ic[17 + (3-i)], sizeof(ROMFILE) );
		memcpy( &list_crom0[i], &romset.ic[1 + (3-i)], sizeof(ROMFILE) );
		memcpy( &list_crom1[i], &romset.ic[5 + (3-i)], sizeof(ROMFILE) );
		memcpy( &list_crom2[i], &romset.ic[9 + (3-i)], sizeof(ROMFILE) );
		memcpy( &list_crom3[i], &romset.ic[13 + (3-i)], sizeof(ROMFILE) );
	}
	
	/* load the mother ROMSET if needed */

	if(romset.superset_id[0] != '\0')
        if(LoadROM(romset.superset_id))
			return 1;

	/* set up CROM pointers within the larger 72MB CROM space for convenience */

	crom0 = &crom[0x00800000];
	crom1 = &crom[0x01800000];
	crom2 = &crom[0x02800000];
	crom3 = &crom[0x03800000];

	/* load ROM files into memory */

	/* first try to find a zip file */
	if( !set_directory_zip("roms/%s.zip", romset.id) ) {
		/* if that fails, use a normal directory */
		set_directory("roms/%s", romset.id);
	}
	
    if( list_crom[0].size && LoadROMFile(&crom[8*1024*1024 - (list_crom[0].size * 4)], list_crom, ITLV_4) )
	{
		printf("Can't load CROM\n");
		return 1;
	}

    if( list_crom0[0].size && LoadROMFile(crom0, list_crom0, ITLV_4) )
	{
		printf("Can't load CROM0\n");
		return 1;
	}

    if( list_crom1[0].size && LoadROMFile(crom1, list_crom1, ITLV_4) )
	{
		printf("Can't load CROM1\n");
		return 1;
	}

    if( list_crom2[0].size && LoadROMFile(crom2, list_crom2, ITLV_4) )
	{
		printf("Can't load CROM2\n");
		return 1;
	}

    if( list_crom3[0].size && LoadROMFile(crom3, list_crom3, ITLV_4) )
	{
		printf("Can't load CROM3\n");
		return 1;
	}
    
	/* byteswap buffers */
	ByteSwap(&crom[8*1024*1024 - (list_crom[0].size * 4)], list_crom[0].size * 4);
	ByteSwap(crom0, list_crom0[0].size * 4);
	ByteSwap(crom1, list_crom1[0].size * 4);
	ByteSwap(crom2, list_crom2[0].size * 4);
	ByteSwap(crom3, list_crom3[0].size * 4);	

    /* mirror CROM0 to CROM if needed */

	if((list_crom[0].size * 4) < 8*1024*1024)
        memcpy(crom, crom0, 8*1024*1024 - list_crom[0].size*4);

	if((list_crom0[0].size * 4) <= 8*1024*1024)
		memcpy(&crom0[0x800000], crom0, 0x800000);
	if((list_crom1[0].size * 4) <= 8*1024*1024)
		memcpy(&crom1[0x800000], crom1, 0x800000);
	if((list_crom2[0].size * 4) <= 8*1024*1024)
		memcpy(&crom2[0x800000], crom2, 0x800000);
	if((list_crom3[0].size * 4) <= 8*1024*1024)
		memcpy(&crom3[0x800000], crom3, 0x800000);

	/* if we're here, everything went fine! */

    printf("ROM loaded successfully!\n");

	/*
     * Apply the patches
     */

	for( i=0; i < romset.num_patches; i++ )
    {
        switch (romset.patch[i].size)
        {
        case 8:
            crom[romset.patch[i].address] = BSWAP32(romset.patch[i].data);
            break;
        case 16:
            *(UINT16*) &crom[romset.patch[i].address] = BSWAP16((UINT16) romset.patch[i].data);
            break;
        case 32:
            *(UINT32*) &crom[romset.patch[i].address] = BSWAP32(romset.patch[i].data);
            break;
        default:
            printf("internal error, line %d of %s: invalid patch size (%d)\n", __LINE__, __FILE__, romset.patch[i].size);
            break;
        }
	}

	return 0;
}


/*******************************************************************************
 Main Program
*******************************************************************************/

/*
 * AllocMemoryRegions():
 *
 * Allocates all the required Model 3 memory regions. Returns non-zero if
 * failed.
 */
 
static BOOL AllocMemoryRegions(void)
{
	ram 	= calloc(8*1024*1024, sizeof(UINT8));
	crom 	= calloc(72*1024*1024, sizeof(UINT8));
	
	if (ram == NULL || crom == NULL)
	{
		SAFE_FREE(ram);
		SAFE_FREE(crom);
		return 1;
	}
	
	return 0;
}

/*
 * FreeMemoryRegions():
 *
 * Frees all memory regions.
 */
 
static void FreeMemoryRegions(void)
{
	SAFE_FREE(ram);
	SAFE_FREE(crom);
}

/*
 * InitDynarec():
 *
 * Initialize the dynamic recompiler and its data structures to a usable state.
 * Returns non-zero if it failed.
 */
 
static BOOL InitDynarec(void)
{
	cfg.Alloc	= malloc;
	cfg.Free	= free;

	cfg.SetupBBLookup		= NULL;
	cfg.CleanBBLookup		= NULL;
	cfg.LookupBB			= NULL;
	cfg.InvalidateBBLookup	= NULL;

	cfg.native_cache_size		= 4*1024*1024;
	cfg.native_cache_guard_size	= 16*1024;

	cfg.intermediate_cache_size			= 1*1024*1024;
	cfg.intermediate_cache_guard_size	= 64*1024;

	cfg.hot_threshold	= 1000;

	cfg.address_bits	= 32;
	cfg.page1_bits		= 8;
	cfg.page2_bits		= 12;
	cfg.offs_bits		= 10;
	cfg.ignore_bits		= 2;

	cfg.mmap_cfg.fetch		= mmap_fetch;
	cfg.mmap_cfg.read8		= mmap_read8;
	cfg.mmap_cfg.read16		= mmap_read16;
	cfg.mmap_cfg.read32		= mmap_read32;
	cfg.mmap_cfg.write8		= mmap_write8;
	cfg.mmap_cfg.write16	= mmap_write16;
	cfg.mmap_cfg.write32	= mmap_write32;

	if (DRPPC_OKAY != PowerPC_Init(&cfg, 0x00070000, TestIRQCallback))
	{
		printf("PowerPC_Init failed.\n");
		return 1;
	}

	return 0;
}

/*
 * int main(int argc, char **argv);
 */
 
int main(int argc, char **argv)
{
	ROMSET	romset;
	INT		i;
	
	if (argc <= 1)
	{
		printf("usage: test <romset>\n");
		return 0;
	}
	
	/*
	 * Set up the dynamic recompiler and load the ROM set
	 */
	 
	for (i = 0; i < 4; i++)
	{
	if (AllocMemoryRegions())
		exit(1);
	if (LoadROM(argv[1]))
		exit(1);					
		
	if (InitDynarec())
		exit(1);

	if (DRPPC_OKAY != PowerPC_Reset())
	{
		printf("PowerPC_Reset failed.\n");
		return -1;
	}	

	//printf("PowerPC_Run = %i\n", PowerPC_Run(1000));
	
	FreeMemoryRegions();
	}	

	return 0;
}
