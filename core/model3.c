/*
 * Sega Model 3 Emulator
 * Copyright (C) 2003 Bart Trzynadlowski, Ville Linde, Stefano Teso
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program (license.txt); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * model3.c
 *
 * Model 3 system emulation.
 *
 * Memory Regions:
 *
 *      All Model 3 memory regions are allocated and freed in this file.
 *      However, RAM and backup RAM are the only RAM regions directly accessed
 *      here. Everything else is passed to its respective subsystem. This
 *      localizes all memory allocation to this single file and enforces
 *      separation between the different modules.
 *
 * VF3 SCSI Note:
 *
 *      In VF3, the SCSI appears at 0xC0000000. We've observed that at this
 *      address on Step 1.5 there is some sort of 68K device. Oh, well :P
 *
 * TODO List:
 *
 *      - von2_rev5_4g has what appears to be a spinning cursor effect when
 *      booting but sometimes the characters look invalid. Need to take a
 *      closer look...
 *
 *      - Investigate backup RAM. Sega Rally 2 seems to want 256KB when R3D
 *      read commands are emulated with the DMA. This is emulated by making
 *      the Step 2.X backup RAM region (starting at 0xFE0C0000) 256KB in size
 *      but it is still kept as 128KB for Step 1.X.
 *
 *      - Do more error checking when parsing games.ini (num_patches > 64,
 *      etc.)
 */

#include "model3.h"

#define PROFILE_MEMORY_OPERATIONS		0

/******************************************************************/
/* Global Configuration Structure                                 */
/******************************************************************/

CONFIG m3_config;

/******************************************************************/
/* Internal Variables                                             */
/******************************************************************/

/*
 * Model 3 Memory Regions
 */

UINT8    *ram = NULL;            // PowerPC RAM
static UINT8    *bram = NULL;           // backup RAM
static UINT8    *sram = NULL;           // sound RAM
static UINT8    *vram = NULL;           // tile generator VRAM (scroll RAM)
static UINT8    *culling_ram_8e = NULL; // Real3D culling RAM
static UINT8    *culling_ram_8c = NULL; // Real3D culling RAM
static UINT8    *polygon_ram = NULL;    // Real3D polygon RAM
static UINT8    *texture_ram = NULL;    // Real3D texture RAM
static UINT8    *crom = NULL;           // CROM (all CROM memory is allocated here)
static UINT8    *crom_bank;             // points to current 8MB CROM bank
static UINT8    *vrom = NULL;           // video ROM
static UINT8	*sprog = NULL;
static UINT8    *srom = NULL;           // sound ROM
static UINT8	*dsbprog = NULL;
static UINT8    *dsbrom = NULL;         // DSB1 ROM

static UINT8 *crom0 = NULL;
static UINT8 *crom1 = NULL;
static UINT8 *crom2 = NULL;
static UINT8 *crom3 = NULL;

/*
 * Other
 */

static UINT ppc_freq;   		// PowerPC clock speed

/*
 * Function Prototypes (for forward references)
 */

static UINT8    model3_sys_read_8(UINT32);
static UINT32   model3_sys_read_32(UINT32);
static void     model3_sys_write_8(UINT32, UINT8);
static void     model3_sys_write_32(UINT32, UINT32);
static UINT8    model3_midi_read(UINT32);
static void     model3_midi_write(UINT32, UINT8);

/******************************************************************/
/* Output                                                         */
/******************************************************************/

void message(int flags, char * fmt, ...)
{
	/* a simple _message_ to the user. */
	/* must be used for unharmful warnings too. */
	/* do not pause the emulator to prompt the user! */
	/* using a cyclic buffer to output a few lines of */
	/* timed messages to the screen would be best. */

	/* integrable in the renderer. */

	/* flags are provided for future expansion (i.e. for */
	/* different text colors/priorities) */

	va_list vl;
    char string[512];

	va_start(vl, fmt);
    vsprintf(string, fmt, vl);
	va_end(vl);

	LOG("model3.log", "%s\n", string);

    puts(string);
    
//	osd_message(flags, string);
}

void error(char * fmt, ...)
{
	/* you can bypass this calling directly osd_error, but i */
	/* prefer it this way. :) */

	char string[256] = "";
	va_list vl;

	va_start(vl, fmt);
	vsprintf(string, fmt, vl);
	va_end(vl);

	osd_error(string);
}

void _log(char * path, char * fmt, ...)
{
	/* logs to a file. */

	/* NOTE: "log" conflicts with math.h, so i'm using _log instead. */
	/* it doesn't make much difference since we're gonna log stuff */
	/* with the LOG macro. */

	if(m3_config.log_enabled)
	{
        char string[1024];
		va_list vl;
		FILE * file;

		file = fopen(path, "ab");
		if(file != NULL)
		{
			va_start(vl, fmt);
			vsprintf(string, fmt, vl);
			va_end(vl);
			fprintf(file, string);
			fclose(file);
		}
	}
}

void _log_init(char * path)
{
	/* resets a file contents. */
	/* since i'm opening the log file with fopen(path, "ab") every */
	/* time (not to lost the file in the case of a crash), it's */
	/* necessary to reset it on startup. */

	FILE * file;

	file = fopen(path, "wb");
	if(file != NULL)
		fclose(file);
}




#if PROFILE_MEMORY_OPERATIONS
#define PROFILE_MEMORY_OP(x, y, z)	profile_memory_operation(x, y, z)
#else
#define PROFILE_MEMORY_OP(x, y, z)
#endif


static UINT64 num_mem_ops = 0;
static UINT64 num_read_ops = 0;
static UINT64 num_write_ops = 0;

static UINT64 num_read8_ops = 0;
static UINT64 num_read16_ops = 0;
static UINT64 num_read32_ops = 0;
static UINT64 num_read64_ops = 0;
static UINT64 num_write8_ops = 0;
static UINT64 num_write16_ops = 0;
static UINT64 num_write32_ops = 0;
static UINT64 num_write64_ops = 0;

static UINT64 num_ram_reads = 0;
static UINT64 num_ram_writes = 0;
static UINT64 num_crom_reads = 0;
static UINT64 num_bankcrom_reads = 0;
static UINT64 num_other_reads = 0;
static UINT64 num_other_writes = 0;

static int profile_memory_operation(int size, int write, UINT32 address)
{
	num_mem_ops++;

	if (write)
	{
		num_write_ops++;

		switch (size)
		{
			case 8:		num_write8_ops++; break;
			case 16:	num_write16_ops++; break;
			case 32:	num_write32_ops++; break;
			case 64:	num_write64_ops++; break;
		}

		if (address < 0x800000)
		{
			num_ram_writes++;
		}
		else
		{
			num_other_writes++;
		}
	}
	else
	{
		num_read_ops++;

		switch (size)
		{
			case 8:		num_read8_ops++; break;
			case 16:	num_read16_ops++; break;
			case 32:	num_read32_ops++; break;
			case 64:	num_read64_ops++; break;
		}

		if (address < 0x800000)
		{
			num_ram_reads++;
		}
		else if (address >= 0xff000000 && address < 0xff800000)
		{
			num_bankcrom_reads++;
		}
		else if (address >= 0xff800000)
		{
			num_crom_reads++;
		}
		else
		{
			num_other_reads++;
		}
	}
}

static void print_memory_profile_stats(void)
{
	printf("Total mem ops: %lld\n", num_mem_ops);
	printf("   Reads:      %lld, %f%%\n", num_read_ops, ((double)num_read_ops / (double)num_mem_ops) * 100.0f);
	printf("   Writes:     %lld, %f%%\n", num_write_ops, ((double)num_write_ops / (double)num_mem_ops) * 100.0f);
	printf("\n");
	printf("   Read8:   %lld, %f%%\n", num_read8_ops, ((double)num_read8_ops / (double)num_read_ops) * 100.0f);
	printf("   Read16:  %lld, %f%%\n", num_read16_ops, ((double)num_read16_ops / (double)num_read_ops) * 100.0f);
	printf("   Read32:  %lld, %f%%\n", num_read32_ops, ((double)num_read32_ops / (double)num_read_ops) * 100.0f);
	printf("   Read64:  %lld, %f%%\n", num_read64_ops, ((double)num_read64_ops / (double)num_read_ops) * 100.0f);
	printf("   Write8:  %lld, %f%%\n", num_write8_ops, ((double)num_write8_ops / (double)num_write_ops) * 100.0f);
	printf("   Write16: %lld, %f%%\n", num_write16_ops, ((double)num_write16_ops / (double)num_write_ops) * 100.0f);
	printf("   Write32: %lld, %f%%\n", num_write32_ops, ((double)num_write32_ops / (double)num_write_ops) * 100.0f);
	printf("   Write64: %lld, %f%%\n", num_write64_ops, ((double)num_write64_ops / (double)num_write_ops) * 100.0f);
	printf("\n");
	printf("   RAM reads:       %lld, %f%%\n", num_ram_reads, ((double)num_ram_reads / (double)num_read_ops) * 100.0f);
	printf("   RAM writes:      %lld, %f%%\n", num_ram_writes, ((double)num_ram_writes / (double)num_write_ops) * 100.0f);
	printf("   BANK CROM reads: %lld, %f%%\n", num_bankcrom_reads, ((double)num_bankcrom_reads / (double)num_read_ops) * 100.0f);
	printf("   CROM reads:      %lld, %f%%\n", num_crom_reads, ((double)num_crom_reads / (double)num_read_ops) * 100.0f);
	printf("   Other reads:     %lld, %f%%\n", num_other_reads, ((double)num_other_reads / (double)num_read_ops) * 100.0f);
	printf("   Other writes:    %lld, %f%%\n", num_other_writes, ((double)num_other_writes / (double)num_write_ops) * 100.0f);
}


static int prot_data_ptr = 0;

static UINT16 vs299_prot_data[] =
{
	0xc800, 0x4a20, 0x5041, 0x4e41, 0x4920, 0x4154, 0x594c, 0x4220,
	0x4152, 0x4953, 0x204c, 0x5241, 0x4547, 0x544e, 0x4e49, 0x2041,
	0x4547, 0x4d52, 0x4e41, 0x2059, 0x4e45, 0x4c47, 0x4e41, 0x2044,
	0x454e, 0x4854, 0x5245, 0x414c, 0x444e, 0x2053, 0x5246, 0x4e41,
	0x4543, 0x4320, 0x4c4f, 0x4d4f, 0x4942, 0x2041, 0x4150, 0x4152,
	0x5547, 0x5941, 0x4220, 0x4c55, 0x4147, 0x4952, 0x2041, 0x5053,
	0x4941, 0x204e, 0x5243, 0x414f, 0x4954, 0x2041, 0x4542, 0x474c,
	0x5549, 0x204d, 0x494e, 0x4547, 0x4952, 0x2041, 0x4153, 0x4455,
	0x2049, 0x4f4b, 0x4552, 0x2041, 0x4544, 0x4d4e, 0x5241, 0x204b,
	0x4f52, 0x414d, 0x494e, 0x2041, 0x4353, 0x544f, 0x414c, 0x444e,
	0x5520, 0x4153, 0x5320, 0x554f, 0x4854, 0x4641, 0x4952, 0x4143,
	0x4d20, 0x5845, 0x4349, 0x204f, 0x5559, 0x4f47, 0x4c53, 0x5641,
	0x4149, 0x4620, 0x5f43, 0x4553, 0x4147
};

static UINT16 swt_prot_data[] =
{
	0xffff,
	0x3d3d, 0x3d3d, 0x203d, 0x5453, 0x5241, 0x5720, 0x5241, 0x2053,
	0x3d3d, 0x3d3d, 0x0a3d, 0x6f43, 0x7970, 0x6952, 0x6867, 0x2074,
	0x4553, 0x4147, 0x4520, 0x746e, 0x7265, 0x7270, 0x7369, 0x7365,
	0x202c, 0x744c, 0x2e64, 0x410a, 0x756d, 0x6573, 0x656d, 0x746e,
	0x5220, 0x4426, 0x4420, 0x7065, 0x2e74, 0x2320, 0x3231, 0x4b0a,
	0x7461, 0x7573, 0x6179, 0x7573, 0x4120, 0x646e, 0x206f, 0x2026,
	0x614b, 0x6f79, 0x6f6b, 0x5920, 0x6d61, 0x6d61, 0x746f, 0x0a6f,
};

static UINT16 fvipers2_prot_data[] =
{
	0x2a2a,
	0x2a2a, 0x2a2a, 0x2a2a, 0x2a2a, 0x2a2a, 0x2a2a, 0x202a, 0x5b5b,
	0x4620, 0x6769, 0x7468, 0x6e69, 0x2067, 0x6956, 0x6570, 0x7372,
	0x3220, 0x5d20, 0x205d, 0x6e69, 0x3c20, 0x4d3c, 0x444f, 0x4c45,
	0x332d, 0x3e3e, 0x4320, 0x706f, 0x7279, 0x6769, 0x7468, 0x2820,
	0x2943, 0x3931, 0x3839, 0x5320, 0x4745, 0x2041, 0x6e45, 0x6574,
	0x7072, 0x6972, 0x6573, 0x2c73, 0x544c, 0x2e44, 0x2020, 0x4120,
	0x6c6c, 0x7220, 0x6769, 0x7468, 0x7220, 0x7365, 0x7265, 0x6576,
	0x2e64, 0x2a20, 0x2a2a, 0x2a2a, 0x2a2a, 0x2a2a, 0x2a2a, 0x2a2a,
};

static UINT16 spikeout_prot_data[] =
{
	0x0000,
	0x4f4d, 0x4544, 0x2d4c, 0x2033, 0x7953, 0x7473, 0x6d65, 0x5020,
	0x6f72, 0x7267, 0x6d61, 0x4320, 0x706f, 0x7279, 0x6769, 0x7468,
	0x2820, 0x2943, 0x3120, 0x3939, 0x2035, 0x4553, 0x4147, 0x4520,
	0x746e, 0x7265, 0x7270, 0x7369, 0x7365, 0x4c2c, 0x4454, 0x202e,
	0x6c41, 0x206c, 0x6972, 0x6867, 0x2074, 0x6572, 0x6573, 0x7672,
	0x6465, 0x202e, 0x2020, 0x0020
};

static UINT16 eca_prot_data[] =
{
	0x0000,
    0x2d2f, 0x202d, 0x4d45, 0x5245, 0x4547, 0x434e, 0x2059, 0x4143,
    0x4c4c, 0x4120, 0x424d, 0x4c55, 0x4e41, 0x4543, 0x2d20, 0x0a2d,
    0x6f43, 0x7970, 0x6952, 0x6867, 0x2074, 0x4553, 0x4147, 0x4520,
    0x746e, 0x7265, 0x7270, 0x7369, 0x7365, 0x202c, 0x744c, 0x2e64,
    0x530a, 0x666f, 0x7774, 0x7261, 0x2065, 0x2652, 0x2044, 0x6544,
    0x7470, 0x202e, 0x3123, 0x660a, 0x726f, 0x7420, 0x7365, 0x0a74,
};

/******************************************************************/
/* PPC Access                                                     */
/******************************************************************/

UINT8 m3_ppc_read_8(UINT32 a)
{
	PROFILE_MEMORY_OP(8, 0, a);

    /*
     * RAM and ROM tested for first for speed
     */

    if (a <= 0x007FFFFF)
	{
        return ram[a];
	}
    else if (a >= 0xFF000000 && a <= 0xFF7FFFFF)
	{
        return crom_bank[a - 0xFF000000];
	}
    else if (a >= 0xFF800000 && a <= 0xFFFFFFFF)
	{
        return crom[a - 0xFF800000];
	}

    switch (a >> 28)
    {
		case 0xc:
		{
			if (a >= 0xC0000000 && a <= 0xC00000FF)         // 53C810 SCSI (Step 1.0)
			{
				if (m3_config.step == 0x10)
					return scsi_read_8(a);
			}
			else if (a >= 0xC1000000 && a <= 0xC10000FF)    // 53C810 SCSI
			{
				return scsi_read_8(a);
			}
			else if (a >= 0xC2000000 && a <= 0xC200001F)    // DMA device
			{
				return dma_read_8(a);
			}

			break;
		}

		case 0xf:
		{
			if ((a >= 0xF0040000 && a <= 0xF004003F) ||
				(a >= 0xFE040000 && a <= 0xFE04003F))       // control area
			{
				return controls_read(a);
			}
			else if ((a >= 0xF0080000 && a <= 0xF00800FF) ||
					 (a >= 0xFE080000 && a <= 0xFE0800FF))  // MIDI?
			{
				return model3_midi_read(a);
			}
			else if ((a >= 0xF0100000 && a <= 0xF010003F) ||
	   				 (a >= 0xFE100000 && a <= 0xFE10003F))  // system control
			{
				return model3_sys_read_8(a);
			}
			else if ((a >= 0xF0140000 && a <= 0xF014003F) ||
					 (a >= 0xFE140000 && a <= 0xFE14003F))  // RTC
			{
				return rtc_read_8(a);
			}
			else if (a >= 0xFEE00000 && a <= 0xFEEFFFFF)    // MPC106 CONFIG_DATA
			{
				return bridge_read_config_data_8(a);
			}
			else if (a >= 0xF9000000 && a <= 0xF90000FF)	// 53C810 SCSI
			{
				return scsi_read_8(a);
			}


			switch (a)
			{
				case 0xF118000C:    // TODO: 8-bit tilegen access is unimplemented
				{
					return 0xff;
				}
			}
        }
        break;
    }

    error("%08X: unknown read8, %08X\n", ppc_get_pc(), a);
    return 0xff;
}

UINT16 m3_ppc_read_16(UINT32 a)
{
	PROFILE_MEMORY_OP(16, 0, a);

    /*
     * RAM and ROM tested for first for speed
     */

    if (a <= 0x007FFFFF)
	{
		return BSWAP16(*(UINT16 *) &ram[a]);
	}
    else if (a >= 0xFF000000 && a <= 0xFF7FFFFF)
	{
        return BSWAP16(*(UINT16 *) &crom_bank[a - 0xFF000000]);
	}
    else if (a >= 0xFF800000 && a <= 0xFFFFFFFF)
	{
        return BSWAP16(*(UINT16 *) &crom[a - 0xFF800000]);
	}

    switch (a >> 28)
	{
		case 0xf:
		{
			if ((a >= 0xF00C0000 && a <= 0xF00DFFFF) ||
				(a >= 0xFE0C0000 && a <= 0xFE0FFFFF))  // backup RAM
			{
				return BSWAP16(*(UINT16 *) &bram[a & 0x3FFFF]);
			}
			else if( a >= 0xF1000000 && a <= 0xF111FFFF )	// tilegen VRAM
			{
				return tilegen_vram_read_16(a);
			}

			switch (a)
			{
				case 0xF0C00CFC:    // MPC105/106 CONFIG_DATA (Sega Bass Fishing)    
				case 0xF0C00CFE:
				case 0xFEE00CFC:    // MPC105/106 CONFIG_DATA
				case 0xFEE00CFE:
				{
					return bridge_read_config_data_16(a);
				}
			}
        }
		break;
	}

    error("%08X: unknown read16, %08X\n", ppc_get_pc(), a);
    return 0xffff;
}

UINT32 m3_ppc_read_32(UINT32 a)
{
	PROFILE_MEMORY_OP(32, 0, a);

    /*
     * RAM and ROM tested for first for speed
     */

    if (a <= 0x007FFFFF)
	{
        return BSWAP32(*(UINT32 *) &ram[a]);
	}
    else if (a >= 0xFF000000 && a <= 0xFF7FFFFF)
	{
        return BSWAP32(*(UINT32 *) &crom_bank[a - 0xFF000000]);
	}
    else if (a >= 0xFF800000 && a <= 0xFFFFFFFF)
	{
        return BSWAP32(*(UINT32 *) &crom[a - 0xFF800000]);
	}

    switch (a >> 28)
    {
	    case 0x8:
		{
			return r3d_read_32(a);
		}

		case 0xc:
		{
			if (a >= 0xC0000000 && a <= 0xC00000FF)         // 53C810 SCSI (Step 1.0)
			{
				if (m3_config.step == 0x10)
					return scsi_read_32(a);
			}
			else if (a >= 0xC1000000 && a <= 0xC10000FF)    // 53C810 SCSI
			{
				return scsi_read_32(a);
			}
			else if (a >= 0xC2000000 && a <= 0xC200001F)    // DMA device
			{
				return dma_read_32(a);
			}

			switch (a)
			{
				case 0xC0000000:
				{
		            //return _C0000000;
					return 0;
				}
				case 0xC0010110:	// LeMans24
				case 0xC0010114:	// LeMans24
				case 0xC0020000:    // Network? Sega Rally 2 at 0x79268
				{
					return 0;
				}
			}
			break;
		}

		case 0xf:
		{
			if ((a >= 0xF0040000 && a <= 0xF004003F) ||     // control area          
				(a >= 0xFE040000 && a <= 0xFE04003F))
			{
				return	(controls_read(a + 0) << 24) |
						(controls_read(a + 1) << 16) |
						(controls_read(a + 2) << 8)  |
						(controls_read(a + 3) << 0);
			}
			else if (a >= 0xF0080000 && a <= 0xF00800FF)    // MIDI?
			{
				return 0xFFFFFFFF;
			}
			else if ((a >= 0xF00C0000 && a <= 0xF00DFFFF) ||
					 (a >= 0xFE0C0000 && a <= 0xFE0FFFFF))  // backup RAM
			{
				return BSWAP32(*(UINT32 *) &bram[a & 0x3FFFF]);
			}
			else if ((a >= 0xF0100000 && a <= 0xF010003F) ||
					 (a >= 0xFE100000 && a <= 0xFE10003F))  // system control
			{
				return model3_sys_read_32(a);
			}
			else if ((a >= 0xF0140000 && a <= 0xF014003F) ||
					 (a >= 0xFE140000 && a <= 0xFE14003F))  // RTC
			{
				return rtc_read_32(a);
			}
			else if (a >= 0xF1000000 && a <= 0xF111FFFF)    // tile generator VRAM
			{
				return tilegen_vram_read_32(a);
			}
			else if (a >= 0xF1180000 && a <= 0xF11800FF)    // tile generator regs
			{
				return tilegen_read_32(a);
			}
			else if (a >= 0xFEE00000 && a <= 0xFEEFFFFF)    // MPC106 CONFIG_DATA
			{
				return bridge_read_config_data_32(a);
			}
			else if (a >= 0xF9000000 && a <= 0xF90000FF)	// 53C810 SCSI
			{
				return scsi_read_32(a);
			}

			switch (a)
			{
				case 0xF0C00CFC:    // MPC105/106 CONFIG_DATA
				{
					return bridge_read_config_data_32(a);
				}

				case 0xFE180000:    // ? SWT
				{
					return 0;
				}

				case 0xFE1A0000:    // ? Virtual On 2 -- important to return 0
				{
					return 0;       // see my message on 13May ("VROM Port?")
				}

				case 0xFE1A001C:
				{
					if (_stricmp(m3_config.game_id, "vs299") == 0 ||
						_stricmp(m3_config.game_id, "vs2v991") == 0)
					{
						UINT32 data = (UINT32)(vs299_prot_data[prot_data_ptr++]) << 16;
						if (prot_data_ptr > 0x65)
						{
							prot_data_ptr = 0;
						}
						return data;
					}
					else if (_stricmp(m3_config.game_id, "swtrilgy") == 0 ||
							 _stricmp(m3_config.game_id, "swtrilga") == 0)
					{
						UINT32 data = (UINT32)swt_prot_data[prot_data_ptr++] << 16;
						if (prot_data_ptr > 0x38)
						{
							prot_data_ptr = 0;
						}
						return data;
					}
					else if (_stricmp(m3_config.game_id, "fvipers2") == 0)
					{
						UINT32 data = (UINT32)fvipers2_prot_data[prot_data_ptr++] << 16;
						if (prot_data_ptr >= 0x41)
						{
							prot_data_ptr = 0;
						}
						return data;
					}
					else if (_stricmp(m3_config.game_id, "spikeout") == 0 ||
							 _stricmp(m3_config.game_id, "spikeofe") == 0)
					{
						UINT32 data = (UINT32)spikeout_prot_data[prot_data_ptr++] << 16;
						if (prot_data_ptr >= 0x55)
						{
							prot_data_ptr = 0;
						}
						return data;
					}
					else if (_stricmp(m3_config.game_id, "eca") == 0)
					{
						UINT32 data = (UINT32)(eca_prot_data[prot_data_ptr++]) << 16;
						if (prot_data_ptr >= 0x31)
						{
							prot_data_ptr = 0;
						}
						
						return data;
					}
					else
					{
						return 0xffffffff;
					}
				}
			}
        }
        break;
    }

    error("%08X: unknown read32, %08X\n", ppc_get_pc(), a);
    return 0xFFFFFFFF;
}

UINT64 m3_ppc_read_64(UINT32 a)
{
    UINT64  d;
	PROFILE_MEMORY_OP(64, 0, a);

    d = m3_ppc_read_32(a + 0);
    d <<= 32;
    d |= m3_ppc_read_32(a + 4);

    return d;
}

void m3_ppc_write_8(UINT32 a, UINT8 d)
{
	PROFILE_MEMORY_OP(8, 1, a);

    /*
     * RAM tested for first for speed
     */

    if (a <= 0x007FFFFF)
    {
        ram[a] = d;
        return;
    }

    switch (a >> 28)
    {
    case 0xC:

        if (a >= 0xC0000000 && a <= 0xC00000FF)         // 53C810 SCSI
        {
            if (m3_config.step == 0x10)
            {
                scsi_write_8(a, d);
                return;
            }
        }
        else if (a >= 0xC2000000 && a <= 0xC200001F)    // DMA device
        {
            dma_write_8(a, d);
            return;
        }
		else if (a >= 0xc3800000 && a <= 0xc380001f)	// Daytona 2 protection/bank-switch
		{
			crom_bank = &crom[0x800000 + (((~d) & 0xf) * 0x800000)];
			return;
		}

        switch (a)
        {
        case 0xC0010180:    // ? Lost World, PC = 0x11B510
        case 0xC1000014:    // ? Lost World, PC = 0xFF80098C
        case 0xC1000038:    // ? Sega Rally, PC = 0x7B1F0
        case 0xC1000039:    // ? Lost World, PC = 0x118BD8
		case 0xC100003B:	// Scud Race Plus
            return;
        }

        break;

    case 0xF:

        if ((a >= 0xF0040000 && a <= 0xF004003F) ||    // control area
            (a >= 0xFE040000 && a <= 0xFE04003F))       
        {
            controls_write(a, d);
            return;
        }
        else if ((a >= 0xF0080000 && a <= 0xF00800FF) ||
                 (a >= 0xFE080000 && a <= 0xFE0800FF))  // MIDI?
        {
            model3_midi_write(a, d);
            return;
        }
        else if ((a >= 0xF0100000 && a <= 0xF010003F) ||
                 (a >= 0xFE100000 && a <= 0xFE10003F))  // system control
        {
            model3_sys_write_8(a, d);
            return;
        }
        else if ((a >= 0xF0140000 && a <= 0xF014003F) ||
                 (a >= 0xFE140000 && a <= 0xFE14003F))  // RTC? (Sega Rally 2)
		{
            rtc_write(a, d);
			return;
		}
        else if (a >= 0xF8FFF000 && a <= 0xF8FFF0FF)    // MPC105 regs
        {
            bridge_write_8(a, d);
            return;
        }
        else if (a >= 0xFEE00000 && a <= 0xFEEFFFFF)    // MPC106 CONFIG_DATA
        {
            bridge_write_config_data_8(a, d);
            return;
        }
        else if (a >= 0xF9000000 && a <= 0xF90000FF)	// 53C810 SCSI
        {
            scsi_write_8(a, d);
            return;
        }

        switch (a)
        {
        case 0xFE000004:    // Sega Rally 2
        case 0xF118000C:    // Harley Davidson (tilegen 8-bit)

            return;
        
        }

        break;
    }

    error("%08X: unknown write8, %08X = %02X\n", ppc_get_pc(), a, d);
}

void m3_ppc_write_16(UINT32 a, UINT16 d)
{
	PROFILE_MEMORY_OP(16, 1, a);

    /*
     * RAM tested for first for speed
     */

    if (a <= 0x007FFFFF)
    {
        *(UINT16 *) &ram[a] = BSWAP16(d);
        return;
    }

    switch (a >> 28)
    {
    case 0xC:

        if (a >= 0xC2000000 && a <= 0xC200001F) // DMA device
        {
            dma_write_16(a, d);
            return;
        }

        break;

    case 0xF:

        if (a >= 0xF8FFF000 && a <= 0xF8FFF0FF)         // MPC105 regs
        {
            bridge_write_16(a, d);
            return;
        }
        else if (a >= 0xFEE00000 && a <= 0xFEEFFFFF)    // MPC106 CONFIG_DATA
        {
            bridge_write_config_data_16(a, d);
            return;
        }
        else if ((a >= 0xF00C0000 && a <= 0xF00DFFFF) ||
                 (a >= 0xFE0C0000 && a <= 0xFE0FFFFF))  // backup RAM
        {
            *(UINT16 *) &bram[a & 0x3FFFF] = BSWAP16(d);
            return;
        }

        switch (a)
        {
        case 0xF0C00CFC:    // MPC105/106 CONFIG_DATA
            bridge_write_config_data_16(a, d);
            return;
        }

        break;
    }

    error("%08X: unknown write16, %08X = %04X\n", ppc_get_pc(), a, d);
}

void m3_ppc_write_32(UINT32 a, UINT32 d)
{
	PROFILE_MEMORY_OP(32, 1, a);

    /*
     * RAM tested for first for speed
     */

    if (a <= 0x007FFFFF)
    {
        *(UINT32 *) &ram[a] = BSWAP32(d);
        return;
    }

    switch (a >> 28)
    {
		case 0x8:
		case 0x9:
		{
			r3d_write_32(a, d);     // Real3D memory regions
	        return;
		}

		case 0xc:
		{
			if (a >= 0xC0000000 && a <= 0xC00000FF)         // 53C810 SCSI
			{
				if (m3_config.step == 0x10)
				{
					scsi_write_32(a, d);
					return;
				}
			}
			else if (a >= 0xC1000000 && a <= 0xC10000FF)    // 53C810 SCSI
			{
				scsi_write_32(a, d);
				return;
			}
			else if (a >= 0xC2000000 && a <= 0xC200001F)    // DMA device
			{
				dma_write_32(a, d);
	            return;
			}

			switch (a)
			{
				case 0xC0000000:    // latched value
				{
					//_C0000000 = d;
					return;
				}
				case 0xC0010180:	// Network ? Scud Race at 0xB2A4
				{
					return;
				}
				case 0xC0020000:    // Network? Sega Rally 2 at 0x79264
				{
					return;
				}
			}
			break;
		}

		case 0xf:
		{
			if ((a >= 0xF0040000 && a <= 0xF004003F) ||     // control area
				(a >= 0xFE040000 && a <= 0xFE04003F))       
			{
	            controls_write(a + 0, (UINT8) (d >> 24));
		        controls_write(a + 1, (UINT8) (d >> 16));
		        controls_write(a + 2, (UINT8) (d >> 8));
				controls_write(a + 3, (UINT8) (d >> 0));
				return;
			}
			else if ((a >= 0xF00C0000 && a <= 0xF00DFFFF) ||
					 (a >= 0xFE0C0000 && a <= 0xFE0FFFFF))  // backup RAM
			{
				*(UINT32 *) &bram[a & 0x3FFFF] = BSWAP32(d);
				return;
			}
			else if ((a >= 0xF0100000 && a <= 0xF010003F) ||
				     (a >= 0xFE100000 && a <= 0xFE10003F))  // system control
			{
				model3_sys_write_32(a, d);
				return;
			}
			else if ((a >= 0xF0140000 && a <= 0xF014003F) ||
				     (a >= 0xFE140000 && a <= 0xFE14003F))  // RTC
			{
				rtc_write(a, (UINT8) (d >> 24));
				return;
			}
			else if (a >= 0xFE180000 && a <= 0xFE19FFFF)    // ?
			{
				// LOG("model3.log", "security %08X = %04X\n", a, d >> 16);
				return;
			}
			else if (a >= 0xF1000000 && a <= 0xF111FFFF)    // tile generator VRAM
			{
				tilegen_vram_write_32(a, d);
				return;
			}
			else if (a >= 0xF1180000 && a <= 0xF11800FF)    // tile generator regs
			{
				tilegen_write_32(a, d);
				return;
			}
			else if (a >= 0xF8FFF000 && a <= 0xF8FFF0FF)    // MPC105 regs
			{
				bridge_write_32(a, d);
				return;
			}
			else if (a >= 0xFEC00000 && a <= 0xFEDFFFFF)    // MPC106 CONFIG_ADDR
			{
				bridge_write_config_addr_32(a, d);
				return;
			}
			else if (a >= 0xFEE00000 && a <= 0xFEEFFFFF)    // MPC106 CONFIG_DATA
			{
				bridge_write_config_data_32(a, d);
				return;
			}
			else if (a >= 0xF9000000 && a <= 0xF90000FF)	// 53C810 SCSI
			{
				scsi_write_32(a, d);
				return;
			}

			switch (a)
			{
				case 0xF0800CF8:    // MPC105/106 CONFIG_ADDR
				{
					bridge_write_config_addr_32(a, d);
					return;
				}
				case 0xF0C00CFC:    // MPC105/106 CONFIG_DATA
				{
					bridge_write_config_data_32(a, d);
					return;
				}
				case 0xFE1A0000:    // ? Virtual On 2
				{
					return;
				}
				case 0xFE1A0010:
				{       
					return;
				}
				case 0xFE1A0014:
				{
					LOG("model3.log", "security reset  = %08X\n", d);
					return;
				}
				case 0xFE1A0018:
				{
					LOG("model3.log", "security key    = %08X\n", d);
					return;
				}
			}
		}
        break;
    }

    error("%08X: unknown write32, %08X = %08X\n", ppc_get_pc(), a, d);
}

void m3_ppc_write_64(UINT32 a, UINT64 d)
{
	PROFILE_MEMORY_OP(64, 1, a);

    m3_ppc_write_32(a + 0, (UINT32) (d >> 32));
    m3_ppc_write_32(a + 4, (UINT32) d);
}



void model3_dma_transfer(UINT32 src, UINT32 dst, int length, BOOL swap_words)
{
	UINT32 *s;
	if (src < 0x800000)
	{
		s = (UINT32 *)&ram[src];
	}
	else if (src >= 0xff000000 && src < 0xff800000)
	{
		s = (UINT32 *)&crom_bank[src - 0xff000000];
	}
	else if (src >= 0xff800000)
	{
		s = (UINT32 *)&crom[src - 0xff800000];
	}
	else
	{
		error("model3_dma_transfer: source = %08X\n", src);
	}

	switch ((dst >> 24) & 0xff)
	{
		case 0x8c:	r3d_dma_culling_ram_8c(s, dst, length, swap_words); break;
		case 0x8e:	r3d_dma_culling_ram_8e(s, dst, length, swap_words); break;
		case 0x94:	r3d_dma_texture_ram(s, dst, length, swap_words); break;
		case 0x98:	r3d_dma_polygon_ram(s, dst, length, swap_words); break;

		default:
		{
			int i;
			if (swap_words)
			{
				for (i=0; i < length; i+=4)
				{
					UINT32 d = *s++;
					r3d_write_32(dst, d);
					dst += 4;
				}
			}
			else
			{
				for (i=0; i < length; i+=4)
				{
					UINT32 d = BSWAP32(*s++);
					r3d_write_32(dst, d);
					dst += 4;
				}
			}
			break;
		}
	}
}



/******************************************************************/
/* System Control (0xFx100000 - 0xFx10003F)                       */
/******************************************************************/

static UINT8    model3_irq_state = 0;	// 0xF0100018
static UINT8    model3_irq_enable = 0;	// 0xF0100014
static UINT8    crom_bank_reg;

/*
 * void m3_add_irq(UINT8 mask);
 *
 * Raises an IRQ (sets its status bit.)
 *
 * Parameters:
 *      mask = Mask corresponding to upper 8 bits of IRQ status register.
 */

void model3_add_irq(UINT8 mask)
{
	model3_irq_state |= mask;
}

/*
 * void m3_remove_irq(UINT8 mask);
 *
 * Removes an IRQ (lowers its status bit.)
 *
 * Parameters:
 *      mask = Mask corresponding to upper 8 bits of IRQ status register.
 */

void model3_remove_irq(UINT8 mask)
{
	model3_irq_state &= ~mask;
}

static UINT32 model3_ppc_irq_callback(void)
{
	return(0); /* no other IRQs in the queue */
}

/*
 * m3_set_crom_bank():
 *
 * Sets the CROM bank register and maps the requested 8MB CROM bank in.
 * Note that all CROMs are stored in the same 72MB buffer which is why 8MB is
 * added (to skip over CROM0-3.)
 */

static void model3_set_crom_bank(UINT8 d)
{
    crom_bank_reg = d;
    crom_bank = &crom[0x800000 + ((~d) & 0xf) * 0x800000];

    LOG("model3.log", "CROM bank = %02X\n", d);
}

static UINT8 model3_sys_read_8(UINT32 a)
{
    static UINT8    x = 0x20;

	switch(a & 0xFF)
	{    
		case 0x08:  // CROM bank
		{
			return crom_bank_reg;
		}

		case 0x10:  // JTAG TAP
		{
			return ((tap_read() & 1) << 5);
		}

		case 0x14:  // IRQ enable
		{
			return model3_irq_enable;
		}

		case 0x18:  // IRQ status
		{
			return model3_irq_state;
		}

		case 0x1C:  // ?
		{
			// LOG("model3.log", "%08X: unknown sys read8, %08X\n", PPC_PC, a);        
			return 0xff;
		}
	}

    LOG("model3.log", "%08X: unknown sys read8, %08X\n", ppc_get_pc(), a);
    message(0, "%08X: unknown sys read8, %08X", ppc_get_pc(), a);
    return 0xff;
}

static UINT32 model3_sys_read_32(UINT32 a)
{
	switch(a & 0xFF)
	{
		case 0x10:  // JTAG TAP
		{
			return ((tap_read() & 1) << (5+24));
		}

		case 0x14:  // IRQ enable
		{
			return (model3_irq_enable << 24);
		}

		case 0x18:  // IRQ status
		{
			return (model3_irq_state << 24);
		}
	}

    LOG("model3.log", "%08X: unknown sys read32, %08X\n", ppc_get_pc(), a);
    message(0, "%08X: unknown sys read32, %08X", ppc_get_pc(), a);
    return 0xffffffff;
}

static void model3_sys_write_8(UINT32 a, UINT8 d)
{
	switch(a & 0xff)
	{
		case 0x00:	// ?
		{
			LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", ppc_get_pc(), a, d);
			return;
		}

		case 0x04:	// ?
		{
			LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", ppc_get_pc(), a, d);
			return;
		}

		case 0x08:  // CROM bank
		{
			model3_set_crom_bank(d);
			return;
		}

		case 0x0c:	// JTAG TAP
		{
			tap_write(
				(d >> 6) & 1,	// TCK
				(d >> 2) & 1,	// TMS
				(d >> 5) & 1,	// TDI
				(d >> 7) & 1	// TRST
			);
			return;
		}

		case 0x14:  // IRQ enable
		{
			model3_irq_enable = d;
	        return;
		}

		case 0x1c:  // ? this may be the LED control
		{
		//  LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", PPC_PC, a, d);
			return;
		}

		case 0x3c:	// ?
		{
			LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", ppc_get_pc(), a, d);
			return;
		}
	}

    LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", ppc_get_pc(), a, d);
    message(0, "%08X: unknown sys write8, %08X = %02X", ppc_get_pc(), a, d);
}

static void model3_sys_write_32(UINT32 a, UINT32 d)
{
	switch(a & 0xff)
	{
		case 0x0c:  // JTAG TAP
		{
			tap_write(
				(d >> (6+24)) & 1,
				(d >> (2+24)) & 1,
				(d >> (5+24)) & 1,
				(d >> (7+24)) & 1
			);
			return;
		}

		case 0x14:  // IRQ mask
		{
			model3_irq_enable = (d >> 24);
			return;
		}

		case 0x1c:  // ?
		{
			LOG("model3.log", "%08X: unknown sys write32, %08X = %08X\n", ppc_get_pc(), a, d);
			return;
		}
	}

    LOG("model3.log", "%08X: unknown sys write32, %08X = %08X\n", ppc_get_pc(), a, d);
    message(0, "%08X: unknown sys write32, %08X = %08X", ppc_get_pc(), a, d);
}

static UINT8 model3_midi_read(UINT32 a)
{
	/* 0xFx0800xx */

    return 0xFF;
//    return(0);
}

static void model3_midi_write(UINT32 a, UINT8 d)
{
	/* 0xFx0800xx */

    //message(0, "%08X: MIDI write, %08X = %02X", ppc_get_pc(), a, d);

	if ((a & 0xf) == 0)
	{
		model3_remove_irq(0x40);
	}
}

/******************************************************************/
/* PCI Command Callback                                           */
/******************************************************************/

static UINT32 pci_command_callback(UINT32 cmd)
{
	int pci_device = (cmd >> 11) & 0x1f;
	int pci_reg = (cmd >> 2) & 0x3f;

    switch (cmd)
    {
		case 0x80006800:    // reg 0 of PCI config header
		{
			if (m3_config.step <= 0x15)
				return 0x16C311DB;  // 0x11DB = PCI vendor ID (Sega), 0x16C3 = device ID
			else
				return 0x178611DB;
		}

		case 0x80007000:    // VF3
		case 0x80007002:    // Sega Rally 2
		{
            return 0x00011000;
		}

		case 0x80008000:	// Daytona 2, protection/bank-switch
		{
			return 0x182711db;
		}

		default:
		{
			LOG("PCI callback: %08X (device %d, reg %d)\n", cmd, pci_device, pci_reg);
			break;
		}
    }

    //LOG("model3.log", "%08X (%08X): PCI command issued: %08X\n", PPC_PC, PPC_LR, cmd);
    return 0;
}

/******************************************************************/
/* Load/Save Stuff                                                */
/******************************************************************/

static void word_swap(UINT8 *src, INT size)
{
    while (size -= 4)
    {
        *((UINT32 *) src) = BSWAP32(*((UINT32 *) src));
        src += sizeof(UINT32);
    }
}

void model3_load_eeprom(void)
{
	char string[512];
	INT i;

	sprintf( string, "%s/%s.epr", m3_config.backup_path, m3_config.game_id );

    if((i = eeprom_load(string)) != MODEL3_OKAY)
	{
		message(0, "Can't load EEPROM from %s (%d)", string, i);
	}
}

void model3_save_eeprom(void)
{
	char string[512];
	INT i;

	sprintf( string, "%s/%s.epr", m3_config.backup_path, m3_config.game_id );

    if((i = eeprom_save(string)) != MODEL3_OKAY)
	{
		message(0, "Can't save EEPROM to %s (%d)", string, i);
	}
}

void model3_load_bram(void)
{
	char string[512];

	sprintf( string, "%s/%s.brm", m3_config.backup_path, m3_config.game_id );

    if(load_file(string, bram, 256*1024))
	{
		message(0, "Can't load Backup RAM from file, creating a new file.");
        memset(bram, 0xFF, 256*1024);
        save_file(string, bram, 256*1024, 0);
	}
}

void model3_save_bram(void)
{
	char string[512];
	sprintf( string, "%s/%s.brm", m3_config.backup_path, m3_config.game_id );

    save_file(string, bram, 256*1024, 0);
}

/*
 * BOOL model3_save_state(CHAR *file);
 *
 * Saves a state.
 *
 * Parameters:
 *      file = Name of save state file to generate.
 *
 * Returns:
 *      MODEL3_OKAY  = Success.
 *      MODEL3_ERROR = Unable to open the save state file.
 */

BOOL model3_save_state(CHAR *file)
{
    FILE    *fp;

    if ((fp = fopen(file, "wb")) == NULL)
    {
        error("Unable to save state to %s", file);
        return MODEL3_ERROR;
    }
   
    /*
     * Write out the main data: PowerPC RAM, backup RAM, and system control
     * registers
     */

    fwrite(ram, sizeof(UINT8), 8*1024*1024, fp);
    fwrite(bram, sizeof(UINT8), 256*1024, fp);
    fwrite(&model3_irq_state, sizeof(UINT8), 1, fp);
    fwrite(&model3_irq_enable, sizeof(UINT8), 1, fp);
    fwrite(&crom_bank_reg, sizeof(UINT8), 1, fp);

    /*
     * Save the rest of the system state
     */

    //ppc_save_state(fp);
    bridge_save_state(fp);
    controls_save_state(fp);
    dma_save_state(fp);
    dsb1_save_state(fp);
    eeprom_save_state(fp);
    r3d_save_state(fp);
    rtc_save_state(fp);
    scsi_save_state(fp);
    tilegen_save_state(fp);
//    scsp_save_state(fp);

    fclose(fp);
    LOG("model3.log", "saved state: %s\n", file);
    return MODEL3_OKAY;
}

/*
 * void m3_load_state(CHAR *file);
 *
 * Loads a state.
 *
 * Parameters:
 *      file = Name of save state file to load.
 *
 * Returns:
 *      MODEL3_OKAY  = Success.
 *      MODEL3_ERROR = Unable to open the save state file.
 */

BOOL model3_load_state(CHAR *file)
{
    FILE    *fp;

    if ((fp = fopen(file, "rb")) == NULL)
    {
        error("Unable to load state from %s", file);
        return MODEL3_ERROR;
    }

    /*
     * Load main data: PowerPC RAM, backup RAM, and system control registers
     */

    fread(ram, sizeof(UINT8), 8*1024*1024, fp);
    fread(bram, sizeof(UINT8), 256*1024, fp);
    fread(&model3_irq_state, sizeof(UINT8), 1, fp);
    fread(&model3_irq_enable, sizeof(UINT8), 1, fp);
    fread(&crom_bank_reg, sizeof(UINT8), 1, fp);
    model3_set_crom_bank(crom_bank_reg);

    /*
     * Load the rest of the system state
     */

    //ppc_load_state(fp);
    bridge_load_state(fp);
    controls_load_state(fp);
    dma_load_state(fp);
    dsb1_load_state(fp);
    eeprom_load_state(fp);
    r3d_load_state(fp);
    rtc_load_state(fp);
    scsi_load_state(fp);
    tilegen_load_state(fp);
//    scsp_load_state(fp);

    fclose(fp);
    LOG("model3.log", "loaded state: %s\n", file);
    return MODEL3_OKAY;
}

/******************************************************************/
/* Machine Execution Loop                                         */
/******************************************************************/

UINT    m3_irq_bit = 0; // debug

static LONGLONG timer_start, timer_end;
static LONGLONG timer_frequency;

static int frame = 0;

void model3_run_frame(void)
{
	/*
	 * Reset all profiling sections
	 */

	PROFILE_SECT_RESET("-");
	PROFILE_SECT_RESET("ppc");
	PROFILE_SECT_RESET("68k");
	PROFILE_SECT_RESET("tilegen");
	PROFILE_SECT_RESET("real3d");
	PROFILE_SECT_RESET("dma");
	PROFILE_SECT_RESET("scsi");

	PROFILE_SECT_ENTRY("-");

	QueryPerformanceFrequency((LARGE_INTEGER*)&timer_frequency);

	if (m3_config.fps_limit)
	{
		double time = 0.0f;
		do
		{
			QueryPerformanceCounter((LARGE_INTEGER*)&timer_end);

			time = (double)(timer_end - timer_start) / (double)(timer_frequency);
		} while (time <= 1.0/60.0);

		timer_start = timer_end;
	}


    /*
     * Run the PowerPC and 68K
     */

    LOG("model3.log", "-- ACTIVE SCAN\n");
    //model3_add_irq(model3_irq_enable & 0x0D);
    //ppc_set_irq_line(1);

	PROFILE_SECT_ENTRY("ppc");
    ppc_execute(ppc_freq / 60);
	PROFILE_SECT_EXIT("ppc");

	PROFILE_SECT_ENTRY("68k");
	PROFILE_SECT_EXIT("68k");

    /*
     * Enter VBlank and update the graphics
     */

    rtc_step_frame();
    render_frame();
    controls_update();

    /*
     * Generate interrupts for this frame and run the VBlank
     */

    LOG("model3.log", "-- VBL\n");

	//if (frame & 1)
	{
		model3_add_irq(model3_irq_enable & 0x4a);
	}

    ppc_set_irq_line(1);

	frame++;

	//PROFILE_SECT_ENTRY("ppc");
    //ppc_execute(100000);
	//PROFILE_SECT_EXIT("ppc");

    //model3_remove_irq(0xFF);    // some games expect a bunch of IRQs to go low after some time
    
	PROFILE_SECT_EXIT("-");
}

void model3_reset(void)
{
	/* the ROM must be already loaded at this point. */
	/* it must _always_ be called when you load a ROM. */

	/* profiler */

	#ifdef _PROFILE_
	profile_cleanup();
	#endif

	/* init log file */

	LOG_INIT("model3.log");
	LOG("model3.log", "XMODEL "VERSION", built on "__DATE__" "__TIME__".\n\n");

	/* reset all the buffers */

    memset(ram, 0, 8*1024*1024);
    memset(vram, 0, 1*1024*1024+2*65536);
	memset(sram, 0, 1*1024*1024);
    memset(bram, 0, 256*1024);

	/* reset all the modules */

	ppc_reset();
    //if (ppc_reset() != PPC_OKAY)
    //    error("ppc_reset failed");

    bridge_reset(m3_config.bridge);
    eeprom_reset();
	scsi_reset();
	dma_reset();

	tilegen_reset();
	r3d_reset();
//    scsp_reset();
//    if(m3_config.flags & GAME_OWN_DSB1) dsb_reset();
    controls_reset(m3_config.flags & (GAME_OWN_STEERING_WHEEL | GAME_OWN_GUN));

    model3_remove_irq(0xFF);
    model3_set_crom_bank(0xFF);

	/* load NVRAMs */

	model3_load_eeprom();
	model3_load_bram();
}

/******************************************************************/
/* File (and ROM) Management                                      */
/******************************************************************/

/*
 * Byteswap a buffer.
 */

static void byteswap(UINT8 *buf, UINT size)
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

static void byteswap32(UINT8 *buf, UINT size)
{
	UINT	i;

	for(i = 0; i < size; i += 4)
		*(UINT32 *)&buf[i] = BSWAP32(*(UINT32 *)&buf[i]);
}

#define NO_ITLV	1
#define ITLV_2	2
#define ITLV_4	4
#define ITLV_16	16

typedef struct
{
	ROMFILE prog[4];
	ROMFILE crom[4][4];
	ROMFILE vrom[16];
	ROMFILE sprog;
	ROMFILE srom[4];
	ROMFILE dsbprog;
	ROMFILE dsbrom[4];
} ROM_LIST;

static BOOL load_rom(UINT8 *buffer, ROMFILE *romfile, UINT32 offset, UINT32 interleave)
{
	int j;
	int size;
	UINT32 crc;
	UINT8 *temp_buffer;

	size = (int)get_file_size_crc(romfile->crc32);
	if (size <= 0)
	{
		return FALSE;
	}

	//message(0, "loading %s\r", romfile->name);
	{
		static longest_line = 0;
		char string[200];
		int length;
		sprintf(string, "Loading %s", romfile->name);
		length = strlen(string);
		longest_line = max(longest_line, length);

		for (j=0; j < longest_line; j++)
		{
			printf(" ");
		}
		printf("\r");
		printf("%s\r", string);
	}

	if (size != romfile->size)
	{
		message(0, "File %s has wrong size ! (%d bytes, should be %d bytes)\n", romfile->name, size, romfile->size);
		return FALSE;
	}

	temp_buffer = (UINT8*)malloc(size);
	if (read_file_crc(romfile->crc32, temp_buffer, size) == FALSE)
	{
		return FALSE;
	}

	/* check file CRC */
	crc = 0;
	crc = crc32(crc, temp_buffer, size);
	if (crc != romfile->crc32)
	{
		message(0, "File %s has wrong CRC: %08X (should be %08X)\n", romfile->name, crc, romfile->crc32);
		return FALSE;
	}

	/* interleave */
	for (j = 0; j < romfile->size; j += 2)
	{
		buffer[offset + (j*interleave) + 0] = temp_buffer[j+0];
		buffer[offset + (j*interleave) + 1] = temp_buffer[j+1];
	}

	SAFE_FREE(temp_buffer);
	return TRUE;
}

static void load_romfiles(ROM_LIST *list)
{
	int i;

	// Load program ROMs
	for (i=0; i < 4; i++)
	{
		if (list->prog[i].load)
		{
			if (load_rom(&crom[0x800000 - (list->prog[0].size * 4)], &list->prog[i], i*2, ITLV_4) == TRUE)
			{
				list->prog[i].load = 0;
			}
		}
	}

	// Load CROMs
	for (i=0; i < 4; i++)
	{
		if (list->crom[0][i].load)
		{
			if (load_rom(crom0, &list->crom[0][i], i*2, ITLV_4) == TRUE)	list->crom[0][i].load = 0;
		}
	}
	for (i=0; i < 4; i++)
	{
		if (list->crom[1][i].load)
		{
			if (load_rom(crom1, &list->crom[1][i], i*2, ITLV_4) == TRUE)	list->crom[1][i].load = 0;
		}
	}
	for (i=0; i < 4; i++)
	{
		if (list->crom[2][i].load)
		{
			if (load_rom(crom2, &list->crom[2][i], i*2, ITLV_4) == TRUE)	list->crom[2][i].load = 0;
		}
	}
	for (i=0; i < 4; i++)
	{
		if (list->crom[3][i].load)
		{
			if (load_rom(crom3, &list->crom[3][i], i*2, ITLV_4) == TRUE)	list->crom[3][i].load = 0;
		}
	}

	// Load VROMs
	for (i=0; i < 16; i++)
	{
		if (list->vrom[i].load)
		{
			if (load_rom(vrom, &list->vrom[i], i*2, ITLV_16) == TRUE)
			{
				list->vrom[i].load = 0;
			}
		}
	}

	// Load sound program
	if (list->sprog.load)
	{
		if (load_rom(sprog, &list->sprog, 0, NO_ITLV) == TRUE)
		{
			list->sprog.load = 0;
		}
	}

	// Load sound ROMs
	for (i=0; i < 4; i++)
	{
		if (list->srom[i].load)
		{
			if (load_rom(&srom[i*0x400000], &list->srom[i], 0, NO_ITLV) == TRUE)
			{
				list->srom[i].load = 0;
			}
		}
	}

	// Load DSB program
	if (list->dsbprog.load)
	{
		if (load_rom(dsbprog, &list->dsbprog, 0, NO_ITLV) == TRUE)
		{
			list->dsbprog.load = 0;
		}
	}

	// Load DSB ROMs
	for (i=0; i < 4; i++)
	{
		if (list->dsbrom[i].load)
		{
			if (load_rom(&dsbrom[i*0x400000], &list->dsbrom[i], 0, NO_ITLV) == TRUE)
			{
				list->dsbrom[i].load = 0;
			}
		}
	}
}

static BOOL load_romset(char *gamename, ROMSET *romset, int num_romsets)
{
	int i, j;
	ROMSET *game = NULL;

	ROM_LIST list;
	int romload_ok;

	memset(&list, 0, sizeof(ROM_LIST));

	// find the game
	for (i=0; i < num_romsets; i++)
	{
		if (_stricmp(romset[i].game, gamename) == 0)
		{
			game = &romset[i];
			break;
		}
	}
	if (game == NULL)
	{
		message(0, "Game %s was not found in gamelist!\n", gamename);
		return FALSE;
	}

	strcpy(m3_config.game_name, game->title);
	message(0, "Loading \"%s\"\n", game->title);

	for (i=0; i < 64; i++)
	{
		if (game->rom[i].size > 0)
		{
			switch (game->rom[i].type)
			{
				case ROMTYPE_PROG0:		memcpy(&list.prog[3], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_PROG1:		memcpy(&list.prog[2], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_PROG2:		memcpy(&list.prog[1], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_PROG3:		memcpy(&list.prog[0], &game->rom[i], sizeof(ROMFILE)); break;

				case ROMTYPE_CROM00:	memcpy(&list.crom[0][3], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM01:	memcpy(&list.crom[0][2], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM02:	memcpy(&list.crom[0][1], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM03:	memcpy(&list.crom[0][0], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM10:	memcpy(&list.crom[1][3], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM11:	memcpy(&list.crom[1][2], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM12:	memcpy(&list.crom[1][1], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM13:	memcpy(&list.crom[1][0], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM20:	memcpy(&list.crom[2][3], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM21:	memcpy(&list.crom[2][2], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM22:	memcpy(&list.crom[2][1], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM23:	memcpy(&list.crom[2][0], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM30:	memcpy(&list.crom[3][3], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM31:	memcpy(&list.crom[3][2], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM32:	memcpy(&list.crom[3][1], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_CROM33:	memcpy(&list.crom[3][0], &game->rom[i], sizeof(ROMFILE)); break;

				case ROMTYPE_VROM00:	memcpy(&list.vrom[ 1], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM01:	memcpy(&list.vrom[ 0], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM02:	memcpy(&list.vrom[ 3], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM03:	memcpy(&list.vrom[ 2], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM04:	memcpy(&list.vrom[ 5], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM05:	memcpy(&list.vrom[ 4], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM06:	memcpy(&list.vrom[ 7], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM07:	memcpy(&list.vrom[ 6], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM10:	memcpy(&list.vrom[ 9], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM11:	memcpy(&list.vrom[ 8], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM12:	memcpy(&list.vrom[11], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM13:	memcpy(&list.vrom[10], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM14:	memcpy(&list.vrom[13], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM15:	memcpy(&list.vrom[12], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM16:	memcpy(&list.vrom[15], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_VROM17:	memcpy(&list.vrom[14], &game->rom[i], sizeof(ROMFILE)); break;

				case ROMTYPE_SPROG:		memcpy(&list.sprog, &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_SROM0:		memcpy(&list.srom[0], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_SROM1:		memcpy(&list.srom[1], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_SROM2:		memcpy(&list.srom[2], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_SROM3:		memcpy(&list.srom[3], &game->rom[i], sizeof(ROMFILE)); break;

				case ROMTYPE_DSBPROG:	memcpy(&list.dsbprog, &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_DSBROM0:	memcpy(&list.dsbrom[0], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_DSBROM1:	memcpy(&list.dsbrom[1], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_DSBROM2:	memcpy(&list.dsbrom[2], &game->rom[i], sizeof(ROMFILE)); break;
				case ROMTYPE_DSBROM3:	memcpy(&list.dsbrom[3], &game->rom[i], sizeof(ROMFILE)); break;
			}
		}
	}

	if (game->cromsize > 0)
	{
		// if CROM banksize = 64MB
		crom0 = &crom[0x00800000];
		crom1 = &crom[0x02800000];
		crom2 = &crom[0x04800000];
		crom3 = &crom[0x06800000];
	}
	else
	{
		crom0 = &crom[0x00800000];
		crom1 = &crom[0x01800000];
		crom2 = &crom[0x02800000];
		crom3 = &crom[0x03800000];
	}

	/* load ROM files into memory */

	// mark the files that need to be loaded
	for (i=0; i < 4; i++)
	{
		if (list.prog[i].size > 0)		list.prog[i].load = 1;
		if (list.crom[0][i].size > 0)	list.crom[0][i].load = 1;
		if (list.crom[1][i].size > 0)	list.crom[1][i].load = 1;
		if (list.crom[2][i].size > 0)	list.crom[2][i].load = 1;
		if (list.crom[3][i].size > 0)	list.crom[3][i].load = 1;
		if (list.vrom[ 0+i].size > 0)	list.vrom[ 0+i].load = 1;
		if (list.vrom[ 4+i].size > 0)	list.vrom[ 4+i].load = 1;
		if (list.vrom[ 8+i].size > 0)	list.vrom[ 8+i].load = 1;
		if (list.vrom[12+i].size > 0)	list.vrom[12+i].load = 1;
		if (list.sprog.size > 0)		list.sprog.load = 1;
		if (list.srom[i].size > 0)		list.srom[i].load = 1;
		if (list.dsbprog.size > 0)		list.dsbprog.load = 1;
		if (list.dsbrom[i].size > 0)	list.dsbrom[i].load = 1;
	}

	if (set_directory_zip("%s/%s.zip", m3_config.rom_path, game->game) == FALSE)
	{
		message(0, "Could not open zip file %s/%s.zip", m3_config.rom_path, game->game);
		return FALSE;
	}

	load_romfiles(&list);

	if (strlen(game->parent) > 0)
	{
		if (set_directory_zip("%s/%s.zip", m3_config.rom_path, game->parent) == FALSE)
		{
			message(0, "Could not open zip file %s/%s.zip", m3_config.rom_path, game->parent);
			return FALSE;
		}

		load_romfiles(&list);
	}

	// check if everything was loaded
	romload_ok = 1;
	for (i=0; i < 4; i++)
	{
		if (list.prog[i].load)
		{
			romload_ok = 0;
			message(0, "File %s was not found", list.prog[i].name);
		}
	}
	for (j=0; j < 4; j++)
	{
		for (i=0; i < 4; i++)
		{
			if (list.crom[j][i].load)
			{
				romload_ok = 0;
				message(0, "File %s was not found", list.crom[j][i].name);
			}
		}
	}
	for (i=0; i < 16; i++)
	{
		if (list.vrom[i].load)
		{
			romload_ok = 0;
			message(0, "File %s was not found", list.vrom[i].name);
		}
	}
	if (list.sprog.load)
	{
		romload_ok = 0;
		message(0, "File %s was not found", list.sprog.name);
	}
	for (i=0; i < 4; i++)
	{
		if (list.srom[i].load)
		{
			romload_ok = 0;
			message(0, "File %s was not found", list.srom[i].name);
		}
	}
	if (list.dsbprog.load)
	{
		romload_ok = 0;
		message(0, "File %s was not found", list.dsbprog.name);
	}
	for (i=0; i < 4; i++)
	{
		if (list.dsbrom[i].load)
		{
			romload_ok = 0;
			message(0, "File %s was not found", list.dsbrom[i].name);
		}
	}

	if (!romload_ok)
	{
		return FALSE;
	}


	/* byteswap buffers */
	byteswap(&crom[8*1024*1024 - (list.prog[0].size * 4)], list.prog[0].size * 4);
	byteswap(crom0, list.crom[0][0].size * 4);
	byteswap(crom1, list.crom[1][0].size * 4);
	byteswap(crom2, list.crom[2][0].size * 4);
	byteswap(crom3, list.crom[3][0].size * 4);
	byteswap(vrom, list.vrom[0].size * 16);
	byteswap32(vrom, list.vrom[0].size * 16);

    /* set stepping */

	m3_config.step = game->step;

    /* set bridge controller */

	if (game->bridge == 0)
        game->bridge = (game->step < 0x20) ? 1 : 2;

	m3_config.bridge = game->bridge;

	// copy controls
	memcpy(&m3_config.controls, &game->controls, sizeof(GAME_CONTROLS));

    /* mirror CROM0 to CROM if needed */

	if ((list.prog[0].size * 4) < 8*1024*1024)
	{
        memcpy(crom, crom0, 8*1024*1024 - list.prog[0].size*4);
	}

	if (game->cromsize < 1)
	{
		memcpy(&crom[0x4800000], crom0, 0x1000000);
		memcpy(&crom[0x5800000], crom1, 0x1000000);
		memcpy(&crom[0x6800000], crom2, 0x1000000);
		memcpy(&crom[0x7800000], crom3, 0x1000000);
	}

	/*
	 * Perhaps mirroring must occur between the CROMx
	 * and the CROMx space, if CROMx is < 16MB?
	 */

    /* mirror lower VROM if necessary */

    if ((list.vrom[0].size * 16) < 64*1024*1024)
	{
        memcpy(&vrom[list.vrom[0].size * 16], vrom, 64*1024*1024 - list.vrom[0].size * 16);
	}

	/* if we're here, everything went fine! */

    message(0, "ROM loaded successfully!");

    /*
     * Debug ROM dump
     */

//    save_file("crom.bin", &crom[0x800000 - romset.crom[0].size * 4], romset.crom[0].size * 4, 0);
//    save_file("crom.bin", crom, 8*1024*1024, 0);
//    save_file("vrom.bin", vrom, 64*1024*1024, 0);
//    save_file("crom0.bin", crom0, 16*1024*1024, 0);
//    save_file("crom1.bin", crom1, 16*1024*1024, 0);
//    save_file("crom2.bin", crom2, 16*1024*1024, 0);

	/*
     * Apply the patches
     */

	for( i=0; i < game->num_patches; i++ )
    {
        switch (game->patch[i].size)
        {
        case 8:
            crom[game->patch[i].address] = BSWAP32(game->patch[i].data);
            break;
        case 16:
            *(UINT16*) &crom[game->patch[i].address] = BSWAP16((UINT16) game->patch[i].data);
            break;
        case 32:
            *(UINT32*) &crom[game->patch[i].address] = BSWAP32(game->patch[i].data);
            break;
        default:
            message(0, "internal error, line %d of %s: invalid patch size (%d)\n", __LINE__, __FILE__, game->patch[i].size);
            break;
        }
	}

	return TRUE;
}


/******************************************************************/
/* Machine Interface                                              */
/******************************************************************/

void model3_shutdown(void)
{
	/* save NVRAMs */

	model3_save_eeprom();
	model3_save_bram();

	/* shutdown all the modules */
    controls_shutdown();
//    scsp_shutdown();
//    if(m3_config.flags & GAME_OWN_DSB1) dsb_reset();
    render_shutdown();
	r3d_shutdown();
	tilegen_shutdown();
	dma_shutdown();
	scsi_shutdown();

	ppc_shutdown();

	/* free any allocated buffer */
	//save_file("rom", crom, 0x800000, 0);
    //save_file("ram", ram, 8*1024*1024, 0);
    //save_file("vram", vram, 1*1024*1024+2*65536, 0);
    //save_file("8e000000", culling_ram_8e, 1*1024*1024, 0);
    //save_file("8c000000", culling_ram_8c, 4*1024*1024, 0);
    //save_file("98000000", polygon_ram, 2*1024*1024, 0);
    //save_file("texture.bin", texture_ram, 2048*2048*2, 0);

    SAFE_FREE(ram);
	SAFE_FREE(vram);
	SAFE_FREE(sram);
	SAFE_FREE(bram);
    SAFE_FREE(culling_ram_8e);
    SAFE_FREE(culling_ram_8c);
    SAFE_FREE(polygon_ram);
    SAFE_FREE(texture_ram);

	SAFE_FREE(crom);
	SAFE_FREE(vrom);

#if PROFILE_MEMORY_OPERATIONS
	print_memory_profile_stats();
#endif
}

static PPC_FETCH_REGION m3_ppc_fetch[3];

static ROMSET romset[256];
static int num_romsets = 0;

BOOL model3_load(void)
{
	int i;
	crom = (UINT8 *)malloc(136*1024*1024);
	vrom = (UINT8 *)malloc(64*1024*1024);
	sprog = (UINT8 *)malloc(0x80000);
	srom = (UINT8 *)malloc(0x1000000);
	dsbprog = (UINT8 *)malloc(0x80000);
	dsbrom = (UINT8 *)malloc(0x1000000);

	if ((crom == NULL) || (vrom == NULL) || (srom == NULL) || (dsbrom == NULL) ||
		(sprog == NULL) || (dsbprog == NULL))
	{
		SAFE_FREE(crom);
		SAFE_FREE(vrom);

		message(0, "ERROR: Couldn't allocate RAM for ROMs");
		return FALSE;
	}

	memset(crom, 0xFF, 136*1024*1024);

	num_romsets = parse_romlist(m3_config.rom_list, romset);
	if (num_romsets == 0)
	{
		return FALSE;
	}

	if (load_romset(m3_config.game_id, romset, num_romsets) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL model3_init(void)
{
	PPC_CONFIG ppc_config;
	/* setup m3_config (which is already partially done in parse_command_line) */

	m3_config.log_enabled = 1;

    /* allocate memory */

    ram = (UINT8 *) malloc(8*1024*1024);
    vram = (UINT8 *) malloc(2*1024*1024);
    sram = (UINT8 *) malloc(1*1024*1024);
    bram = (UINT8 *) malloc(256*1024);
    culling_ram_8e = (UINT8 *) malloc(1*1024*1024);
    culling_ram_8c = (UINT8 *) malloc(4*1024*1024);
    polygon_ram = (UINT8 *) malloc(2*1024*1024);
    texture_ram = (UINT8 *) malloc(2048*2048*2);

    if ((ram == NULL) || (vram == NULL) || (sram == NULL) || (bram == NULL) ||
        (culling_ram_8e == NULL) || (culling_ram_8c == NULL) || (polygon_ram == NULL) ||
        (texture_ram == NULL))
    {
        SAFE_FREE(ram);
        SAFE_FREE(vram);
        SAFE_FREE(sram);
        SAFE_FREE(bram);
        SAFE_FREE(culling_ram_8e);
        SAFE_FREE(culling_ram_8c);
        SAFE_FREE(polygon_ram);
        SAFE_FREE(texture_ram);

        message(0, "Out of memory!");
		return FALSE;
    }

	/* attach m3_shutdown to atexit */

	atexit(model3_shutdown);

    /* setup the PPC */

	if (m3_config.step >= 0x20)
	{
		ppc_config.pvr = PPC_MODEL_603R;
		ppc_config.bus_frequency = BUS_FREQUENCY_66MHZ;
		ppc_config.bus_frequency_multiplier = 0x25;		// multiplier 2.5
	}
    else if (m3_config.step == 0x15)
	{
		ppc_config.pvr = PPC_MODEL_603E;
		ppc_config.bus_frequency = BUS_FREQUENCY_66MHZ;
		ppc_config.bus_frequency_multiplier = 0x15;		// multiplier 1.5
	}
	else
	{
		ppc_config.pvr = PPC_MODEL_603E;
		ppc_config.bus_frequency = BUS_FREQUENCY_66MHZ;
		ppc_config.bus_frequency_multiplier = 0x10;		// multiplier 1.0
	}

	ppc_init(&ppc_config);

	m3_ppc_fetch[0].start = 0;
	m3_ppc_fetch[0].end = 0x7FFFFF;
	m3_ppc_fetch[0].ptr = (UINT32 *)ram;

	m3_ppc_fetch[1].start = 0xFF800000;
	m3_ppc_fetch[1].end = 0xFFFFFFFF;
	m3_ppc_fetch[1].ptr = (UINT32 *)crom;

	m3_ppc_fetch[2].start = 0;
	m3_ppc_fetch[2].end = 0;
	m3_ppc_fetch[2].ptr = (UINT32 *)NULL;

	ppc_set_fetch(m3_ppc_fetch);

	switch (m3_config.step) // set frequency
	{
		case 0x15:  ppc_freq = 100000000; break;    // Step 1.5 PPC @ 100MHz
		case 0x20:  ppc_freq = 166000000; break;    // Step 2.0 PPC @ 166MHz
		case 0x21:  ppc_freq = 166000000; break;    // Step 2.1 PPC @ 166MHz
		default:                                    // assume Step 1.0...
		case 0x10:  ppc_freq = 66000000; break;     // Step 1.0 PPC @ 66MHz
	}

	//ppc_freq = 20000000;

	/* setup the 68K */

	/* A68K INIT! */

	/* setup remaining peripherals -- renderer and sound output is */
	/* already setup at this point. */

	bridge_init(pci_command_callback);
	scsi_init(m3_ppc_read_8, m3_ppc_read_16, m3_ppc_read_32, m3_ppc_write_8, m3_ppc_write_16, m3_ppc_write_32);
	dma_init(m3_ppc_read_32, m3_ppc_write_32);
	tilegen_init(vram);
	r3d_init(culling_ram_8e, culling_ram_8c, polygon_ram, texture_ram, vrom);
	render_init(culling_ram_8e, culling_ram_8c, polygon_ram, texture_ram, vrom);

#ifndef RENDERER_D3D
	osd_renderer_set_memory(polygon_ram, texture_ram, vrom);
#endif

//  scsp_init();
//  if(m3_config.flags & GAME_OWN_DSB1) dsb_reset();
	controls_init();

	return TRUE;
}
