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
#include "unzip.h"

/*******************************************************************************
 Macros
*******************************************************************************/

#define SAFE_FREE(p)	if (p != NULL) { free(p); p = NULL; }


/*******************************************************************************
 Variables
*******************************************************************************/

/*
 * Model 3 Memory Space
 */
 
static UINT8	*ram[2] = { NULL, NULL };	// PowerPC RAM (0 = dynarec, 1 = interpreter)
static UINT8    *crom = NULL;				// CROM (all CROM memory is allocated here)
static UINT8    *crom_bank;					// points to current 8MB CROM bank
static UINT8	bram[2][128*1024];

/*
 * drppc Configuration and contexts
 */
 
static DRPPC_CFG	cfg;
static PPC_CONTEXT	context[2];

/*
 * A few functions used by the code below, which use the code above.
 */

static UINT TestIRQCallback (void)
{
	return 0;
}

static UINT32 TestFetch_D (UINT32 addr)
{
	if (addr <= 0x007FFFFF)
		return Byteswap32(*(UINT32 *)&ram[0][addr & 0x7FFFFF]);
	else if (addr >= 0xFF800000)
		return Byteswap32(*(UINT32 *)&crom[addr & 0x7FFFFF]);

	ASSERT(0);	// Impossible!
	return 0;
}

static UINT32 TestFetch_I (UINT32 addr)
{
	if (addr <= 0x007FFFFF)
		return Byteswap32(*(UINT32 *)&ram[1][addr & 0x7FFFFF]);
	else if (addr >= 0xFF800000)
		return Byteswap32(*(UINT32 *)&crom[addr & 0x7FFFFF]);

	ASSERT(0);	// Impossible!
	return 0;
}

static UINT32 rtc_read_8 (UINT32 addr)
{
	return 0;
}

static UINT32 rtc_read_32 (UINT32 addr)
{
	return 0;
}

static void rtc_write_8 (UINT32 addr, UINT32 data)
{

}

static void rtc_write_32 (UINT32 addr, UINT32 data)
{

}

static UINT32 ctrl_read_8 (UINT32 addr)
{
	//printf("0x%08X: ctr read 8 0x%08X\n", context[0].pc, addr);

	return 0x20; // skips infinite loop at 0x246C-0x2488 in Scud Race
}

static void ctrl_write_8 (UINT32 addr, UINT32 data)
{
	//printf("ctrl write 8 0x%08X = 0x%02X\n", addr, data);
}

static void ctrl_write_32 (UINT32 addr, UINT32 data)
{
	//printf("ctrl write 32 0x%08X = 0x%08X\n", addr, data);
}

static UINT32 midi_read_8 (UINT32 addr)
{
	return 0xFF;
}

static void midi_write_8 (UINT32 addr, UINT32 data)
{
	/* nothing. */
}

static UINT32 sys_read_8 (UINT32 addr)
{
	printf("sys read 8 0x%08X\n", addr);
	return 0xFF;
}

static UINT32 sys_read_16 (UINT32 addr)
{
	printf("sys read 16 0x%08X\n", addr);
	return 0xFFFF;
}

static UINT32 sys_read_32 (UINT32 addr)
{
	printf("sys read 32 0x%08X\n", addr);
	return 0xFFFFFFFF;
}

static void sys_write_8 (UINT32 addr, UINT32 data)
{
	printf("sys write 8 0x%08X = 0x%02X\n", addr, data);
}

static void sys_write_16 (UINT32 addr, UINT32 data)
{
	printf("sys write 16 0x%08X = 0x%04X\n", addr, data);
}

static void sys_write_32 (UINT32 addr, UINT32 data)
{
	printf("sys write 32 0x%08X = 0x%08X\n", addr, data);
}

static void bridge_write_8(UINT32 addr, UINT32 data)
{
	printf("bridge write 8 0x%08X = 0x%02X\n", addr, data);
}

static void bridge_write_16(UINT32 addr, UINT32 data)
{
	printf("bridge write 16 0x%08X = 0x%04X\n", addr, data);
}

static void bridge_write_32(UINT32 addr, UINT32 data)
{
	printf("bridge write 32 0x%08X = 0x%08X\n", addr, data);
}

static void bridge_write_data_16(UINT32 addr, UINT32 data)
{
	printf("bridge write 32 0x%08X = 0x%04X\n", addr, data);
}

static UINT32 bridge_read_data_32(UINT32 addr)
{
	printf("bridge read 32 0x%08X\n", addr);
	return 0;
}

static void bridge_write_addr_32(UINT32 addr, UINT32 data)
{
	printf("bridge write 32 0x%08X = 0x%08X\n", addr, data);
}

static void bridge_write_data_32(UINT32 addr, UINT32 data)
{
	printf("bridge write 32 0x%08X = 0x%08X\n", addr, data);
}

static UINT32 tilegen_read_8 (UINT32 addr)
{
	printf("tilegen read 8 0x%08X\n", addr);
	return 0;
}

static UINT32 tilegen_read_16 (UINT32 addr)
{
	printf("tilegen read 16 0x%08X\n", addr);
	return 0;
}

static UINT32 tilegen_read_32 (UINT32 addr)
{
	printf("tilegen read 32 0x%08X\n", addr);
	return 0;
}

static void tilegen_write_8 (UINT32 addr, UINT32 data)
{
	printf("tilegen write 8 0x%08X = 0x%08X\n", addr, data);
}

static void tilegen_write_16 (UINT32 addr, UINT32 data)
{
	printf("tilegen write 16 0x%08X = 0x%08X\n", addr, data);
}

static void tilegen_write_32 (UINT32 addr, UINT32 data)
{
	printf("tilegen write 32 0x%08X = 0x%08X\n", addr, data);
}

static UINT32 tilegen_ram_read_8 (UINT32 addr)
{
	printf("tilegen read 8 0x%08X\n", addr);
	return 0;
}

static UINT32 tilegen_ram_read_16 (UINT32 addr)
{
	printf("tilegen read 16 0x%08X\n", addr);
	return 0;
}

static UINT32 tilegen_ram_read_32 (UINT32 addr)
{
	printf("tilegen read 32 0x%08X\n", addr);
	return 0;
}

static void tilegen_ram_write_8 (UINT32 addr, UINT32 data)
{
	printf("tilegen write 8 0x%08X = 0x%08X\n", addr, data);
}

static void tilegen_ram_write_16 (UINT32 addr, UINT32 data)
{
	printf("tilegen write 16 0x%08X = 0x%08X\n", addr, data);
}

static void tilegen_ram_write_32 (UINT32 addr, UINT32 data)
{
	printf("tilegen write 32 0x%08X = 0x%08X\n", addr, data);
}

static UINT32 network_read_32 (UINT32 addr)
{
	printf("network read 32 0x%08X\n", addr);
	return 0;
}

static void network_write_32 (UINT32 addr, UINT32 data)
{
	printf("network write 32 0x%08X = 0x%08X\n", addr, data);
}

/*
 * PPC Memory Map(s)
 */

static DRPPC_REGION mmap_fetch_i[] =
{
	DRPPC_REGION_HANDLER(0x00000000, 0xFFFFFFFF, TestFetch_I),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_read_i_8[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_PLACEHOLDER,	// CROM Bank
	DRPPC_REGION_PLACEHOLDER,	// CROM
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[1], FALSE),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_read_8),
	DRPPC_REGION_HANDLER(0xF0040000, 0xF004003F, ctrl_read_8),
	DRPPC_REGION_HANDLER(0xF0080000, 0xF00800FF, midi_read_8),
	DRPPC_REGION_HANDLER(0xF0140000, 0xF014003F, rtc_read_8),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_read_8),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_read_8),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_read_i_16[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_PLACEHOLDER,	// CROM Bank
	DRPPC_REGION_PLACEHOLDER,	// CROM
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[1], FALSE),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_read_16),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_read_16),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_read_16),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_read_i_32[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_PLACEHOLDER,	// CROM Bank
	DRPPC_REGION_PLACEHOLDER,	// CROM
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[1], FALSE),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_read_32),
	DRPPC_REGION_HANDLER(0xF0C00CFC, 0xF0C00CFF, bridge_read_data_32),
	DRPPC_REGION_HANDLER(0xC0000000, 0xC0000003, network_read_32),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_read_32),
	DRPPC_REGION_HANDLER(0xF0140000, 0xF014003F, rtc_read_32),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_read_32),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_read_32),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_write_i_8[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_HANDLER(0xF0040000, 0xF004003F, ctrl_write_8),
	DRPPC_REGION_HANDLER(0xF0080000, 0xF00800FF, midi_write_8),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_write_8),
	DRPPC_REGION_HANDLER(0xF8FFF000, 0xF8FFFFFF, bridge_write_8),
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[1], FALSE),
	DRPPC_REGION_HANDLER(0xF0140000, 0xF014003F, rtc_write_8),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_write_8),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_write_8),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_write_i_16[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_write_16),
	DRPPC_REGION_HANDLER(0xF0C00CFC, 0xF0C00CFF, bridge_write_data_16),
	DRPPC_REGION_HANDLER(0xF8FFF000, 0xF8FFFFFF, bridge_write_16),
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[1], FALSE),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_write_8),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_write_8),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_write_i_32[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_HANDLER(0xF0040000, 0xF004003F, ctrl_write_32),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_write_32),
	DRPPC_REGION_HANDLER(0xF0800CF8, 0xF0800CFB, bridge_write_addr_32),
	DRPPC_REGION_HANDLER(0xF0C00CFC, 0xF0C00CFF, bridge_write_data_32),
	DRPPC_REGION_HANDLER(0xF8FFF000, 0xF8FFFFFF, bridge_write_32),
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[1], FALSE),
	DRPPC_REGION_HANDLER(0xC0000000, 0xC0000003, network_write_32),
	DRPPC_REGION_HANDLER(0xC0010180, 0xC0010183, network_write_32),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_write_32),
	DRPPC_REGION_HANDLER(0xF0140000, 0xF014003F, rtc_write_32),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_write_8),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_fetch_d[] =
{
	DRPPC_REGION_HANDLER(0x00000000, 0xFFFFFFFF, TestFetch_D),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_read_d_8[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_PLACEHOLDER,	// CROM Bank
	DRPPC_REGION_PLACEHOLDER,	// CROM
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[0], FALSE),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_read_8),
	DRPPC_REGION_HANDLER(0xF0040000, 0xF004003F, ctrl_read_8),
	DRPPC_REGION_HANDLER(0xF0080000, 0xF00800FF, midi_read_8),
	DRPPC_REGION_HANDLER(0xF0140000, 0xF014003F, rtc_read_8),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_read_8),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_read_8),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_read_d_16[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_PLACEHOLDER,	// CROM Bank
	DRPPC_REGION_PLACEHOLDER,	// CROM
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[0], FALSE),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_read_16),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_read_16),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_read_16),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_read_d_32[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_PLACEHOLDER,	// CROM Bank
	DRPPC_REGION_PLACEHOLDER,	// CROM
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[0], FALSE),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_read_32),
	DRPPC_REGION_HANDLER(0xF0C00CFC, 0xF0C00CFF, bridge_read_data_32),
	DRPPC_REGION_HANDLER(0xC0000000, 0xC0000003, network_read_32),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_read_32),
	DRPPC_REGION_HANDLER(0xF0140000, 0xF014003F, rtc_read_32),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_read_32),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_read_32),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_write_d_8[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_HANDLER(0xF0040000, 0xF004003F, ctrl_write_8),
	DRPPC_REGION_HANDLER(0xF0080000, 0xF00800FF, midi_write_8),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_write_8),
	DRPPC_REGION_HANDLER(0xF8FFF000, 0xF8FFFFFF, bridge_write_8),
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[0], FALSE),
	DRPPC_REGION_HANDLER(0xF0140000, 0xF014003F, rtc_read_8),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_write_d_16[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_write_16),
	DRPPC_REGION_HANDLER(0xF0C00CFC, 0xF0C00CFF, bridge_write_data_16),
	DRPPC_REGION_HANDLER(0xF8FFF000, 0xF8FFFFFF, bridge_write_16),
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[0], FALSE),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_write_8),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_write_8),
	DRPPC_REGION_END
};

static DRPPC_REGION mmap_write_d_32[] =
{
	DRPPC_REGION_PLACEHOLDER,	// RAM
	DRPPC_REGION_HANDLER(0xF0040000, 0xF004003F, ctrl_write_32),
	DRPPC_REGION_HANDLER(0xF0100000, 0xF010003F, sys_write_32),
	DRPPC_REGION_HANDLER(0xF0800CF8, 0xF0800CFB, bridge_write_addr_32),
	DRPPC_REGION_HANDLER(0xF0C00CFC, 0xF0C00CFF, bridge_write_data_32),
	DRPPC_REGION_HANDLER(0xF8FFF000, 0xF8FFFFFF, bridge_write_32),
	DRPPC_REGION_BUF_BE(0xF00C0000, 0xF00DFFFF, bram[0], FALSE),
	DRPPC_REGION_HANDLER(0xC0000000, 0xC0000003, network_write_32),
	DRPPC_REGION_HANDLER(0xC0010180, 0xC0010183, network_write_32),
	DRPPC_REGION_HANDLER(0xF1180000, 0xF11800FF, tilegen_write_32),
	DRPPC_REGION_HANDLER(0xF0140000, 0xF014003F, rtc_write_32),
	DRPPC_REGION_HANDLER(0xF1000000, 0xF111FFFF, tilegen_ram_write_8),
	DRPPC_REGION_END
};

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

typedef struct
{
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
		*(UINT32 *)&buf[i] = Byteswap32(*(UINT32 *)&buf[i]);
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
			sscanf( date, "%d", (int *)&romset->date );
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
            crom[romset.patch[i].address] = Byteswap32(romset.patch[i].data);
            break;
        case 16:
            *(UINT16*) &crom[romset.patch[i].address] = Byteswap16((UINT16) romset.patch[i].data);
            break;
        case 32:
            *(UINT32*) &crom[romset.patch[i].address] = Byteswap32(romset.patch[i].data);
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
 * DynarecPrint():
 *
 * Prints a string to stdout.
 */

static void DynarecPrint (CHAR * format, ...)
{
	char	string[1024];
	va_list	vl;

	va_start(vl, format);
	vsprintf(string, format, vl);
	va_end(vl);

	puts(string);
}

/*
 * AllocMemoryRegions():
 *
 * Allocates all the required Model 3 memory regions. Returns non-zero if
 * failed.
 */
 
static BOOL AllocMemoryRegions(void)
{
	ram[0]	= calloc(8*1024*1024, sizeof(UINT8));
	ram[1]	= calloc(8*1024*1024, sizeof(UINT8));
	crom 	= calloc(72*1024*1024, sizeof(UINT8));
	
	if (ram[0] == NULL || ram[1] == NULL || crom == NULL)
	{
		SAFE_FREE(ram[0]);
		SAFE_FREE(ram[1]);
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
	SAFE_FREE(ram[0]);
	SAFE_FREE(ram[1]);
	SAFE_FREE(crom);
}

/*
 * InitRegions():
 *
 * Initializes the Model 3 memory maps for interpretation and recompilation.
 */

static void InitRegions (void)
{
	// Dynarec

	DRPPC_SET_REGION_BUF_BE(mmap_read_d_8[0],	0x00000000, 0x007FFFFF, ram[0], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_d_8[1],	0xFF000000, 0xFF7FFFFF, crom_bank, TRUE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_d_8[2],	0xFF800000, 0xFFFFFFFF, crom, FALSE);

	DRPPC_SET_REGION_BUF_BE(mmap_read_d_16[0],	0x00000000, 0x007FFFFF, ram[0], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_d_16[1],	0xFF000000, 0xFF7FFFFF, crom_bank, TRUE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_d_16[2],	0xFF800000, 0xFFFFFFFF, crom, FALSE);

	DRPPC_SET_REGION_BUF_BE(mmap_read_d_32[0],	0x00000000, 0x007FFFFF, ram[0], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_d_32[1],	0xFF000000, 0xFF7FFFFF, crom_bank, TRUE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_d_32[2],	0xFF800000, 0xFFFFFFFF, crom, FALSE);

	DRPPC_SET_REGION_BUF_BE(mmap_write_d_8[0],	0x00000000, 0x007FFFFF, ram[0], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_write_d_16[0],	0x00000000, 0x007FFFFF, ram[0], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_write_d_32[0],	0x00000000, 0x007FFFFF, ram[0], FALSE);

	// Interpreter

	DRPPC_SET_REGION_BUF_BE(mmap_read_i_8[0],	0x00000000, 0x007FFFFF, ram[1], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_i_8[1],	0xFF000000, 0xFF7FFFFF, crom_bank, TRUE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_i_8[2],	0xFF800000, 0xFFFFFFFF, crom, FALSE);

	DRPPC_SET_REGION_BUF_BE(mmap_read_i_16[0],	0x00000000, 0x007FFFFF, ram[1], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_i_16[1],	0xFF000000, 0xFF7FFFFF, crom_bank, TRUE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_i_16[2],	0xFF800000, 0xFFFFFFFF, crom, FALSE);

	DRPPC_SET_REGION_BUF_BE(mmap_read_i_32[0],	0x00000000, 0x007FFFFF, ram[1], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_i_32[1],	0xFF000000, 0xFF7FFFFF, crom_bank, TRUE);
	DRPPC_SET_REGION_BUF_BE(mmap_read_i_32[2],	0xFF800000, 0xFFFFFFFF, crom, FALSE);

	DRPPC_SET_REGION_BUF_BE(mmap_write_i_8[0],	0x00000000, 0x007FFFFF, ram[1], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_write_i_16[0],	0x00000000, 0x007FFFFF, ram[1], FALSE);
	DRPPC_SET_REGION_BUF_BE(mmap_write_i_32[0],	0x00000000, 0x007FFFFF, ram[1], FALSE);
}

/*
 * InitDynarec():
 *
 * Initialize the dynamic recompiler and its data structures to a usable state.
 * Returns non-zero if it failed.
 */

static BOOL InitDynarec(void)
{
	InitRegions();

	// Initialize the shared fields.

	cfg.Alloc	= malloc;
	cfg.Free	= free;
	cfg.Print	= DynarecPrint;

	if (DRPPC_OKAY != PowerPC_Init(&cfg))
	{
		printf("PowerPC_Init failed.\n");
		return TRUE;
	}

	// Initialize the dynarec.

	cfg.interpret_only = FALSE;

	cfg.native_cache_size				= 4*1024*1024;
	cfg.native_cache_guard_size			= 16*1024;

	cfg.intermediate_cache_size			= 1*1024*1024;
	cfg.intermediate_cache_guard_size	= 64*1024;

	cfg.hot_threshold		= DRPPC_ZERO_THRESHOLD;

	cfg.SetupBBLookup		= NULL;
	cfg.CleanBBLookup		= NULL;
	cfg.LookupBB			= NULL;
	cfg.InvalidateBBLookup	= NULL;

	cfg.address_bits		= 32;
	cfg.page1_bits			= 8;
	cfg.page2_bits			= 12;
	cfg.offs_bits			= 10;
	cfg.ignore_bits			= 2;

	cfg.mmap_cfg.fetch		= (DRPPC_REGION *)mmap_fetch_d;
	cfg.mmap_cfg.read8		= (DRPPC_REGION *)mmap_read_d_8;
	cfg.mmap_cfg.read16		= (DRPPC_REGION *)mmap_read_d_16;
	cfg.mmap_cfg.read32		= (DRPPC_REGION *)mmap_read_d_32;
	cfg.mmap_cfg.write8		= (DRPPC_REGION *)mmap_write_d_8;
	cfg.mmap_cfg.write16	= (DRPPC_REGION *)mmap_write_d_16;
	cfg.mmap_cfg.write32	= (DRPPC_REGION *)mmap_write_d_32;

	if (DRPPC_OKAY != PowerPC_SetupContext(&cfg, 0x00070000, TestIRQCallback))
	{
		printf("PowerPC_SetupContext failed on dynarec context setup.\n");
		return 1;
	}

	PowerPC_GetContext(&context[0]);

	// Initialize the interpreter.

	cfg.interpret_only = TRUE;

	cfg.native_cache_size				= 0;
	cfg.native_cache_guard_size			= 0;

	cfg.intermediate_cache_size			= 0;
	cfg.intermediate_cache_guard_size	= 0;

	cfg.hot_threshold		= 0x7FFFFFFF;

	cfg.SetupBBLookup		= NULL;
	cfg.CleanBBLookup		= NULL;
	cfg.LookupBB			= NULL;
	cfg.InvalidateBBLookup	= NULL;
	cfg.SetBBLookupInfo		= NULL;

	cfg.address_bits		= 32;
	cfg.page1_bits			= 8;
	cfg.page2_bits			= 12;
	cfg.offs_bits			= 10;
	cfg.ignore_bits			= 2;

	cfg.mmap_cfg.fetch		= (DRPPC_REGION *)mmap_fetch_i;
	cfg.mmap_cfg.read8		= (DRPPC_REGION *)mmap_read_i_8;
	cfg.mmap_cfg.read16		= (DRPPC_REGION *)mmap_read_i_16;
	cfg.mmap_cfg.read32		= (DRPPC_REGION *)mmap_read_i_32;
	cfg.mmap_cfg.write8		= (DRPPC_REGION *)mmap_write_i_8;
	cfg.mmap_cfg.write16	= (DRPPC_REGION *)mmap_write_i_16;
	cfg.mmap_cfg.write32	= (DRPPC_REGION *)mmap_write_i_32;

	if (DRPPC_OKAY != PowerPC_SetupContext(&cfg, 0x00070000, TestIRQCallback))
	{
		printf("PowerPC_SetupContext failed on interpreter context setup.\n");
		return 1;
	}

	PowerPC_GetContext(&context[1]);

	return FALSE;
}

/*
 * BOOL CompareContexts (PPC_CONTEXT *, PPC_CONTEXT *):
 *
 * Returns TRUE if the two contexts differ for some meaningful measure, FALSE
 * otherwise.
 */

static BOOL CompareContexts (PPC_CONTEXT * c0, PPC_CONTEXT * c1)
{
	BOOL	res = FALSE;
	UINT	i;

	// Compare PC
	if (c0->pc != c1->pc)
	{
		printf("PC\tdoesn't match! D:0x%08X I:0x%08X\n", c0->pc, c1->pc);
		res = TRUE;
	}

	// Compare MSR
	if (c0->msr != c1->msr)
	{
		printf("MSR\tdoesn't match! D:0x%08X I:0x%08X\n", c0->msr, c1->msr);
		res = TRUE;
	}

	// Compare R's
	for (i = 0; i < 32; i++)
		if (c0->r[i] != c1->r[i])
		{
			printf("R%u\tdoesn't match! D:0x%08X I:0x%08X\n", i,
					c0->r[i], c1->r[i]);
			res = TRUE;
		}

	// Compare F's
	for (i = 0; i < 32; i++)
		if (c0->f[i].id != c1->f[i].id)
		{
			printf("F%u\tdoesn't match! D:0x%08X%08X I:0x%08X%08X\n", i,
					(UINT32)(c0->f[i].id >> 32), (UINT32)c0->f[i].id,
					(UINT32)(c1->f[i].id >> 32), (UINT32)c1->f[i].id);
			res = TRUE;
		}

	// Compare CR's
	for (i = 0; i < 8; i++)
		if ((c0->cr[i].lt != c1->cr[i].lt) ||
			(c0->cr[i].gt != c1->cr[i].gt) ||
			(c0->cr[i].eq != c1->cr[i].eq) ||
			(c0->cr[i].so != c1->cr[i].so))
		{
			printf("CR%u\tdoesn't match! D:%u,%u,%u,%u I:%u,%u,%u,%u\n", i,
					c0->cr[i].lt, c0->cr[i].gt, c0->cr[i].eq, c0->cr[i].so,
					c1->cr[i].lt, c1->cr[i].gt, c1->cr[i].eq, c1->cr[i].so);
			res = TRUE;
		}

	// Compare XER
	if ((c0->xer.so != c1->xer.so) ||
		(c0->xer.ov != c1->xer.ov) ||
		(c0->xer.ca != c1->xer.ca))
		{
			printf("XER\tdoesn't match! D:%u,%u,%u I:%u,%u,%u\n",
					c0->xer.so, c0->xer.ov, c0->xer.ca,
					c1->xer.so, c1->xer.ov, c1->xer.ca);
			res = TRUE;
		}

	return res;
}

/*
 * int main(int argc, char **argv);
 */

UINT32	__temp;
CHAR	core_id;

int main(int argc, char **argv)
{
	UINT32	bb_addr[2];
	INT		bb_num = 0, errno;

	if (argc <= 1)
	{
		printf("usage: test <romset>\n");
		return 0;
	}

	/*
	 * Set up the dynamic recompiler and load the ROM set
	 */

	if (AllocMemoryRegions())
		exit(1);

	if (LoadROM(argv[1]))
		exit(1);

	if (InitDynarec())
		exit(1);

	/*
	 * Reset both contexts.
	 */

	PowerPC_SetContext(&context[0]);
	if (DRPPC_OKAY != PowerPC_Reset())
	{
		printf("PowerPC_Reset failed on dynarec.\n");
		return -1;
	}
	PowerPC_GetContext(&context[0]);

	PowerPC_SetContext(&context[1]);
	if (DRPPC_OKAY != PowerPC_Reset())
	{
		printf("PowerPC_Reset failed on interpreter.\n");
		return -1;
	}
	PowerPC_GetContext(&context[1]);

	/*
	 * Run the two cores concurrently, one BB at a time. After each BB is
	 * executed, check if the resulting contexts differ. Terminate immediately
	 * if they do.
	 */

	do
	{
		bb_addr[0] = context[0].pc;
		bb_addr[1] = context[1].pc;
		bb_num ++;

		if ((bb_num % 1000000) == 0)
			printf(	"Still running (1M) [BB=0x%08X,0x%08X]\n",
					bb_addr[0],bb_addr[1]);

#if 1
		// 966828	= Dec IRQ
		// 1023364	= Lfs,Stfs
		if (bb_num == 1030717)
		{
			bb_num = bb_num;
		}
#endif

		/*
		 * Run the dynarec for an entire basic block
		 */

		core_id = 'D';
		PowerPC_SetContext(&context[0]);
		PowerPC_Run(1/*0x7FFFFFFF*/, &errno);
		PowerPC_GetContext(&context[0]);

		if (errno != DRPPC_OKAY)
			goto _dynarec_error;

		/*
		 * Run the interpreter till the point where the dynarec stopped
		 */

		core_id = 'I';
		PowerPC_SetContext(&context[1]);
		PowerPC_SetBreakpoint(__temp);
		PowerPC_Run(0x7FFFFFFF, &errno);
		PowerPC_GetContext(&context[1]);

		if (errno != DRPPC_OKAY)
			goto _interp_error;

	} while (!CompareContexts(&context[0], &context[1]));

	printf(	"\n"
			"ERROR: contexts differ at BB number %d:\n"
			"       Dynarec: 0x%08X ... 0x%08X\n"
			"       Interp:  0x%08X ... 0x%08X\n",
			bb_num,
			bb_addr[0], context[0].pc,
			bb_addr[1], context[1].pc);

	goto _shutdown;


_interp_error:
_dynarec_error:

	printf("\nError %i happened at BB %d.\n", errno, bb_num);

_shutdown:

	PowerPC_SetContext(&context[1]);
	PowerPC_Shutdown();

	PowerPC_SetContext(&context[2]);
	PowerPC_Shutdown();

	FreeMemoryRegions();

	return 0;
}
