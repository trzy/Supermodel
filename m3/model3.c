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
 */

#include "model3.h"

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

static UINT8    _FE180000[0x20000];

static UINT8    *ram = NULL;            // PowerPC RAM
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
static UINT8    *srom = NULL;           // sound ROM
static UINT8    *drom = NULL;           // DSB1 ROM

/*
 * Other
 */

static UINT ppc_freq;   		// PowerPC clock speed

/*
 * Function Prototypes (for forward references)
 */

static UINT8    m3_sys_read_8(UINT32);
static UINT32   m3_sys_read_32(UINT32);
static void     m3_sys_write_8(UINT32, UINT8);
static void     m3_sys_write_32(UINT32, UINT32);
static UINT8    m3_midi_read(UINT32);
static void     m3_midi_write(UINT32, UINT8);
static void     save_file(char *, UINT8 *, INT, BOOL);
static INT		load_file(char *, UINT8 *, INT);

/******************************************************************/
/* Output                                                         */
/******************************************************************/

void message(UINT flags, char * fmt, ...)
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

/******************************************************************/
/* Profiling                                                      */
/******************************************************************/

#ifdef _PROFILE_

typedef struct PROFILE_SECTION
{
	CHAR name[24];
	UINT64 total;	// cumulative time elapsed
	UINT64 time;	// time elapsed since section start

	struct PROFILE_SECTION * next;

} PROFILE_SECTION;

static PROFILE_SECTION * section_list = NULL;
static PROFILE_SECTION * last_section = NULL;

/******************************************************************/

static volatile UINT64 read_tsc(void)
{
	UINT32 i, j;

	#if __GNUC__

	__asm__ __volatile__ ("rdtsc" : "=&a" (i), "=&d" (j));

	#else

	__asm__
	{
		push eax
		push edx
		rdtsc
		mov dword [j], edx
		mov dword [i], eax
		pop edx
		pop eax
	}

	#endif

	return( (((UINT64)j) << 32) + ((UINT64)i) );
}

static void reset_section(PROFILE_SECTION * sect)
{
	// reset section stats

	sect->time = 0;
	sect->total = 0;
}

static void start_section(PROFILE_SECTION * sect)
{
	// record the current tsc in the section

	sect->time = read_tsc();
}

static void stop_section(PROFILE_SECTION * sect)
{
	// add the passed ticks to the total time amount

	sect->total += (read_tsc() - sect->time);
}

static void add_section(CHAR * name)
{
	// link a new clean section to the end of the list

	PROFILE_SECTION * section;

	section = (PROFILE_SECTION *)malloc(sizeof(PROFILE_SECTION));

	strcpy(section->name, name);
	reset_section(section);
	section->next = NULL;

	if(section_list == NULL)
	{
		section_list = section;
		last_section = section;
	}
	else
	{
		last_section->next = section;
		last_section = section;
	}
}

static PROFILE_SECTION * find_section(CHAR * name)
{
	// find a section in the list, else return NULL

	PROFILE_SECTION * sect;

	sect = section_list;

	while(sect != NULL)
	{
		if(strcmp(sect->name, name) == 0)
			return(sect);
		sect = sect->next;
	}

	return(NULL);
}

/******************************************************************/

void profile_section_entry(CHAR * name)
{
	PROFILE_SECTION * sect;

	if(NULL == (sect = find_section(name)))
	{
		add_section(name);
		sect = last_section;
	}

	start_section(sect);
}

void profile_section_exit(CHAR * name)
{
	PROFILE_SECTION * sect;

	if(NULL != (sect = find_section(name)))
	{
		stop_section(sect);
	}
}

UINT64 profile_get_stat(CHAR * name)
{
	// return the cumulative time spent executing this section

	PROFILE_SECTION * sect;

	if(NULL != (sect = find_section(name)))
	{
		return(sect->total);
	}

	return(0);
}

void profile_reset_sect(CHAR * name)
{
	PROFILE_SECTION * sect;

	if(NULL != (sect = find_section(name)))
	{
		reset_section(sect);
	}
}

void profile_cleanup(void)
{
	PROFILE_SECTION * sect;

	sect = section_list;

	while(sect != NULL)
	{
		PROFILE_SECTION * temp;

		temp = sect->next;
		free(sect);
		sect = temp;
	}
}

// NOTE: this is the only Model3-specific profile procedure

#define PERC(time) (((FLOAT64)time) / ((FLOAT64)total / 100.0))
 
void profile_print(CHAR * string)
{
	UINT64 total	= profile_get_stat("-");
	UINT64 ppc		= profile_get_stat("ppc");
	UINT64 m68k		= profile_get_stat("68k");
	UINT64 tilegen	= profile_get_stat("tilegen");
	UINT64 real3d	= profile_get_stat("real3d");
	UINT64 dma		= profile_get_stat("dma");
	UINT64 scsi		= profile_get_stat("scsi");

	// odd-looking way to advance a pointer :)

	string = &string[sprintf(string, "----------------------------------------\n")];
	string = &string[sprintf(string, "total     = %3.2f%%\n", PERC(total))];
	string = &string[sprintf(string, "----------------------------------------\n")];
	string = &string[sprintf(string, "ppc       = %3.2f%%\n", PERC(ppc))];
	string = &string[sprintf(string, "68k       = %3.2f%%\n", PERC(m68k))];
	string = &string[sprintf(string, "tilegen   = %3.2f%%\n", PERC(tilegen))];
	string = &string[sprintf(string, "real3d    = %3.2f%%\n", PERC(real3d))];
	string = &string[sprintf(string, "dma       = %3.2f%%\n", PERC(dma))];
	string = &string[sprintf(string, "scsi      = %3.2f%%\n", PERC(scsi))];
	string = &string[sprintf(string, "----------------------------------------\n")];
}

#ifdef PERC
#undef PERC
#endif

/******************************************************************/

static UINT64 profile_read_8 = 0;
static UINT64 profile_read_16 = 0;
static UINT64 profile_read_32 = 0;
static UINT64 profile_read_64 = 0;

static UINT64 profile_write_8 = 0;
static UINT64 profile_write_16 = 0;
static UINT64 profile_write_32 = 0;
static UINT64 profile_write_64 = 0;

UINT is_dma = 0;

#define PROFILE_MEM_ACCESS(type, size) \
			if(!is_dma) profile_##type##_##size++;

#else

#define PROFILE_MEM_ACCESS(type, size);

#endif

/******************************************************************/
/* PPC Access                                                     */
/******************************************************************/

static UINT8 m3_ppc_read_8(UINT32 a)
{
	PROFILE_MEM_ACCESS(read, 8);

    /*
     * RAM and ROM tested for first for speed
     */

    if (a <= 0x007FFFFF)
        return ram[a];
    else if (a >= 0xFF000000 && a <= 0xFF7FFFFF)
        return crom_bank[a - 0xFF000000];
    else if (a >= 0xFF800000 && a <= 0xFFFFFFFF)
        return crom[a - 0xFF800000];

    switch (a >> 28)
    {
    case 0xC:

        if (a >= 0xC0000000 && a <= 0xC00000FF)         // 53C810 SCSI (Step 1.0)
        {
            if (m3_config.step == 0x10)
                return scsi_read_8(a);
        }
        else if (a >= 0xC1000000 && a <= 0xC10000FF)    // 53C810 SCSI
            return scsi_read_8(a);
        else if (a >= 0xC2000000 && a <= 0xC200001F)    // DMA device
            return dma_read_8(a);

        break;

    case 0xF:

        if ((a >= 0xF0040000 && a <= 0xF004003F) ||
            (a >= 0xFE040000 && a <= 0xFE04003F))       // control area
            return controls_read(a);
        else if ((a >= 0xF0080000 && a <= 0xF00800FF) ||
                 (a >= 0xFE080000 && a <= 0xFE0800FF))  // MIDI?
            return m3_midi_read(a);
        else if ((a >= 0xF0100000 && a <= 0xF010003F) ||
                 (a >= 0xFE100000 && a <= 0xFE10003F))  // system control
            return m3_sys_read_8(a);
		else if ((a >= 0xF0140000 && a <= 0xF014003F) ||
                 (a >= 0xFE140000 && a <= 0xFE14003F))  // RTC
            return rtc_read_8(a);
        else if (a >= 0xFEE00000 && a <= 0xFEEFFFFF)    // MPC106 CONFIG_DATA
            return bridge_read_config_data_8(a);
		else if(a >= 0xF9000000 && a <= 0xF90000FF)		// 53C810 SCSI
            return scsi_read_8(a);


        break;
    }

    error("%08X: unknown read8, %08X\n", PPC_PC, a);
    return 0xFF;
}

static UINT16 m3_ppc_read_16(UINT32 a)
{
	PROFILE_MEM_ACCESS(read, 16);

    /*
     * RAM and ROM tested for first for speed
     */

    if (a <= 0x007FFFFF)
        return BSWAP16(*(UINT16 *) &ram[a]);
    else if (a >= 0xFF000000 && a <= 0xFF7FFFFF)
        return BSWAP16(*(UINT16 *) &crom_bank[a - 0xFF000000]);
    else if (a >= 0xFF800000 && a <= 0xFFFFFFFF)
        return BSWAP16(*(UINT16 *) &crom[a - 0xFF800000]);

    switch (a >> 28)
	{
	case 0xF:

        if ((a >= 0xF00C0000 && a <= 0xF00DFFFF) ||
                 (a >= 0xFE0C0000 && a <= 0xFE0DFFFF))  // backup RAM
            return BSWAP16(*(UINT16 *) &bram[a & 0x1FFFF]);

        switch (a)
        {
        case 0xFEE00CFC:    // MPC105/106 CONFIG_DATA
		case 0xFEE00CFE:
            return bridge_read_config_data_16(a);
        }
		break;
	}

    error("%08X: unknown read16, %08X\n", PPC_PC, a);
    return 0xFFFF;
}

static UINT32 m3_ppc_read_32(UINT32 a)
{
	PROFILE_MEM_ACCESS(read, 32);

    /*
     * RAM and ROM tested for first for speed
     */

    if (a <= 0x007FFFFF)
        return BSWAP32(*(UINT32 *) &ram[a]);
    else if (a >= 0xFF000000 && a <= 0xFF7FFFFF)
        return BSWAP32(*(UINT32 *) &crom_bank[a - 0xFF000000]);
    else if (a >= 0xFF800000 && a <= 0xFFFFFFFF)
        return BSWAP32(*(UINT32 *) &crom[a - 0xFF800000]);

    switch (a >> 28)
    {
    case 0x8:

        return r3d_read_32(a);

    case 0xC:

        if (a >= 0xC0000000 && a <= 0xC00000FF)         // 53C810 SCSI (Step 1.0)
        {
            if (m3_config.step == 0x10)
                return scsi_read_32(a);
        }
        else if (a >= 0xC1000000 && a <= 0xC10000FF)    // 53C810 SCSI
            return scsi_read_32(a);
        else if (a >= 0xC2000000 && a <= 0xC200001F)    // DMA device
            return dma_read_32(a);

        switch (a)
        {
        case 0xC0000000:
            //return _C0000000;
            return 0;
        case 0xC0020000:    // Network? Sega Rally 2 at 0x79268
            return 0;
        }

        break;

    case 0xF:

        if ((a >= 0xF0040000 && a <= 0xF004003F) ||     // control area
            (a >= 0xFE040000 && a <= 0xFE04003F))
        {
            return (controls_read(a + 0) << 24) |
                   (controls_read(a + 1) << 16) |
                   (controls_read(a + 2) << 8)  |
                   (controls_read(a + 3) << 0);
        }
        else if (a >= 0xF0080000 && a <= 0xF00800FF)    // MIDI?
            return 0xFFFFFFFF;
        else if ((a >= 0xF00C0000 && a <= 0xF00DFFFF) ||
                 (a >= 0xFE0C0000 && a <= 0xFE0DFFFF))  // backup RAM
            return BSWAP32(*(UINT32 *) &bram[a & 0x1FFFF]);
        else if ((a >= 0xF0100000 && a <= 0xF010003F) ||
                 (a >= 0xFE100000 && a <= 0xFE10003F))  // system control
            return m3_sys_read_32(a);
        else if ((a >= 0xF0140000 && a <= 0xF014003F) ||
                 (a >= 0xFE140000 && a <= 0xFE14003F))  // RTC
            return rtc_read_32(a);
        else if (a >= 0xF1000000 && a <= 0xF111FFFF)    // tile generator VRAM
            return tilegen_vram_read_32(a);
        else if (a >= 0xF1180000 && a <= 0xF11800FF)    // tile generator regs
            return tilegen_read_32(a);
        else if (a >= 0xFEE00000 && a <= 0xFEEFFFFF)    // MPC106 CONFIG_DATA
            return bridge_read_config_data_32(a);
        else if (a >= 0xF9000000 && a <= 0xF90000FF)	// 53C810 SCSI
            return scsi_read_32(a);

        switch (a)
        {
        case 0xF0C00CFC:    // MPC105/106 CONFIG_DATA
            return bridge_read_config_data_32(a);

        case 0xFE1A0000:    // ? Virtual On 2 -- important to return 0
            return 0;       // see my message on 13May ("VROM Port?")
        case 0xFE1A001C:   
            return 0xFFFFFFFF;
        }

        break;
    }

    error("%08X: unknown read32, %08X\n", PPC_PC, a);
    return 0xFFFFFFFF;
}

static UINT64 m3_ppc_read_64(UINT32 a)
{
    UINT64  d;

	PROFILE_MEM_ACCESS(read, 64);

    d = m3_ppc_read_32(a + 0);
    d <<= 32;
    d |= m3_ppc_read_32(a + 4);

    return d;
}

static void m3_ppc_write_8(UINT32 a, UINT8 d)
{
	PROFILE_MEM_ACCESS(write, 8);

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

        switch (a)
        {
        case 0xC0010180:    // ? Lost World, PC = 0x11B510
        case 0xC1000014:    // ? Lost World, PC = 0xFF80098C
        case 0xC1000038:    // ? Sega Rally, PC = 0x7B1F0
        case 0xC1000039:    // ? Lost World, PC = 0x118BD8
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
            m3_midi_write(a, d);
            return;
        }
        else if ((a >= 0xF0100000 && a <= 0xF010003F) ||
                 (a >= 0xFE100000 && a <= 0xFE10003F))  // system control
        {
            m3_sys_write_8(a, d);
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
            return;
        }

        break;
    }

    error("%08X: unknown write8, %08X = %02X\n", PPC_PC, a, d);
}

static void m3_ppc_write_16(UINT32 a, UINT16 d)
{
	PROFILE_MEM_ACCESS(write, 16);

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
                 (a >= 0xFE0C0000 && a <= 0xFE0DFFFF))  // backup RAM
        {
            *(UINT16 *) &bram[a & 0x1FFFF] = BSWAP16(d);
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

    error("%08X: unknown write16, %08X = %04X\n", PPC_PC, a, d);
}

static void m3_ppc_write_32(UINT32 a, UINT32 d)
{
	PROFILE_MEM_ACCESS(write, 32);

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

        r3d_write_32(a, d);     // Real3D memory regions
        return;

    case 0xC:

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
            //_C0000000 = d;
            return;
		case 0xC0010180:	// Network ? Scud Race at 0xB2A4
			return;
        case 0xC0020000:    // Network? Sega Rally 2 at 0x79264
            return;
        }

        break;

    case 0xF:

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
                 (a >= 0xFE0C0000 && a <= 0xFE0DFFFF))  // backup RAM
        {
            *(UINT32 *) &bram[a & 0x1FFFF] = BSWAP32(d);
            return;
        }
        else if ((a >= 0xF0100000 && a <= 0xF010003F) ||
                 (a >= 0xFE100000 && a <= 0xFE10003F))  // system control
        {
            m3_sys_write_32(a, d);
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
            a -= 0xFE180000;
            _FE180000[a/4*2] = d >> 24;
            _FE180000[a/4*2 + 1] = (d >> 16) & 0xFF;
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
            bridge_write_config_addr_32(a, d);
            return;
        case 0xF0C00CFC:    // MPC105/106 CONFIG_DATA
            bridge_write_config_data_32(a, d);
            return;
        case 0xFE1A0000:    // ? Virtual On 2
        case 0xFE1A0010:   
        case 0xFE1A0014:
        case 0xFE1A0018:
            return;
        }

        break;
    }

    error("%08X: unknown write32, %08X = %08X\n", PPC_PC, a, d);
}

static void m3_ppc_write_64(UINT32 a, UINT64 d)
{
	PROFILE_MEM_ACCESS(write, 64);

    m3_ppc_write_32(a + 0, (UINT32) (d >> 32));
    m3_ppc_write_32(a + 4, (UINT32) d);
}

/******************************************************************/
/* System Control (0xFx100000 - 0xFx10003F)                       */
/******************************************************************/

static UINT8    m3_irq_state = 0;   // 0xF0100018
static UINT8    m3_irq_enable = 0;    // 0xF0100014
static UINT8    crom_bank_reg;

/*
 * void m3_add_irq(UINT8 mask);
 *
 * Raises an IRQ (sets its status bit.)
 *
 * Parameters:
 *      mask = Mask corresponding to upper 8 bits of IRQ status register.
 */

void m3_add_irq(UINT8 mask)
{
	m3_irq_state |= mask;
}

/*
 * void m3_remove_irq(UINT8 mask);
 *
 * Removes an IRQ (lowers its status bit.)
 *
 * Parameters:
 *      mask = Mask corresponding to upper 8 bits of IRQ status register.
 */

void m3_remove_irq(UINT8 mask)
{
	m3_irq_state &= ~mask;
}

static UINT32 m3_ppc_irq_callback(void)
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

static void m3_set_crom_bank(UINT8 d)
{
    crom_bank_reg = d;
    crom_bank = &crom[0x800000 + ((~d) & 7) * 0x800000];

    LOG("model3.log", "CROM bank = %02X\n", d);
}

static UINT8 m3_sys_read_8(UINT32 a)
{
    static UINT8    x = 0x20;
	switch(a & 0xFF)
	{    
    case 0x08:  // CROM bank
        return crom_bank_reg;
    case 0x10:  // JTAG TAP
        return ((tap_read() & 1) << 5);
    case 0x14:  // IRQ enable
        return m3_irq_enable;
    case 0x18:  // IRQ status
		return m3_irq_state;
    case 0x1C:  // ?
//        LOG("model3.log", "%08X: unknown sys read8, %08X\n", PPC_PC, a);        
        return 0xFF;
	}

    LOG("model3.log", "%08X: unknown sys read8, %08X\n", PPC_PC, a);
    message(0, "%08X: unknown sys read8, %08X", PPC_PC, a);
    return 0xFF;
}

static UINT32 m3_sys_read_32(UINT32 a)
{
	switch(a & 0xFF)
	{
    case 0x10:  // JTAG TAP
        return ((tap_read() & 1) << (5+24));
    case 0x14:  // IRQ enable
        return (m3_irq_enable << 24);
    case 0x18:  // IRQ status
        return (m3_irq_state << 24);
	}

    LOG("model3.log", "%08X: unknown sys read32, %08X\n", PPC_PC, a);
    message(0, "%08X: unknown sys read32, %08X", PPC_PC, a);
    return 0xFFFFFFFF;
}

static void m3_sys_write_8(UINT32 a, UINT8 d)
{
	switch(a & 0xFF)
	{
	case 0x00:	// ?
        LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", PPC_PC, a, d);
		return;
	case 0x04:	// ?
        LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", PPC_PC, a, d);
		return;
    case 0x08:  // CROM bank
        m3_set_crom_bank(d);
        return;
	case 0x0C:	// JTAG TAP
		tap_write(
			(d >> 6) & 1,
			(d >> 2) & 1,
			(d >> 5) & 1,
			(d >> 7) & 1
		);
		return;
    case 0x14:  // IRQ enable
        m3_irq_enable = d;
        LOG("model3.log", "%08X: IRQ enable = %02X\n", PPC_PC, m3_irq_enable);
        message(0, "%08X: IRQ enable = %02X", PPC_PC, m3_irq_enable);
        return;
    case 0x1C:  // ? this may be the LED control 
//        LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", PPC_PC, a, d);
        return;
	case 0x3C:	// ?
        LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", PPC_PC, a, d);
		return;
	}

    LOG("model3.log", "%08X: unknown sys write8, %08X = %02X\n", PPC_PC, a, d);
    message(0, "%08X: unknown sys write8, %08X = %02X", PPC_PC, a, d);
}

static void m3_sys_write_32(UINT32 a, UINT32 d)
{
	switch(a & 0xFF)
	{
    case 0x0C:  // JTAG TAP
		tap_write(
			(d >> (6+24)) & 1,
			(d >> (2+24)) & 1,
			(d >> (5+24)) & 1,
			(d >> (7+24)) & 1
		);
        return;
    case 0x14:  // IRQ mask
        m3_irq_enable = (d >> 24);
        LOG("model3.log", "%08X: IRQ enable = %02X\n", PPC_PC, m3_irq_enable);
        message(0, "%08X: IRQ enable = %02X", PPC_PC, m3_irq_enable);
        return;
    case 0x1C:  // ?
        LOG("model3.log", "%08X: unknown sys write32, %08X = %08X\n", PPC_PC, a, d);
		return;
	}

    LOG("model3.log", "%08X: unknown sys write32, %08X = %08X\n", PPC_PC, a, d);
    message(0, "%08X: unknown sys write32, %08X = %08X", PPC_PC, a, d);
}

static UINT8 m3_midi_read(UINT32 a)
{
	/* 0xFx0800xx */

    return 0xFF;
//    return(0);
}

static void m3_midi_write(UINT32 a, UINT8 d)
{
	/* 0xFx0800xx */

    message(0, "%08X: MIDI write, %08X = %02X", PPC_PC, a, d);
}

/******************************************************************/
/* PCI Command Callback                                           */
/******************************************************************/

static UINT32 pci_command_callback(UINT32 cmd)
{
    switch (cmd)
    {
    case 0x80006800:    // reg 0 of PCI config header
        if (m3_config.step <= 0x15)
            return 0x16C311DB;  // 0x11DB = PCI vendor ID (Sega), 0x16C3 = device ID
        else
            return 0x178611DB;
    case 0x80007000:    // VF3 
    case 0x80007002:    // Sega Rally 2
            return 0x00011000;
    default:
        break;
    }

    LOG("model3.log", "%08X: PCI command issued: %08X\n", PPC_PC, cmd);
    message(0, "%08X: PCI command issued: %08X", PPC_PC, cmd);
    return 0;
}

/******************************************************************/
/* Load/Save Stuff                                                */
/******************************************************************/

/*
 * Load a file to a buffer.
 */

static INT load_file(char * path, UINT8 * dest, INT size)
{
    FILE * fp;
	INT i;

	if(path == NULL || dest == NULL)
		return(-1);

    if((fp = fopen(path, "rb")) == NULL)
		return(-1);

    fseek(fp, 0, SEEK_END);
    i = ftell(fp);
    fseek(fp, 0, SEEK_SET);

	if(i <= 0)
		return(-1);

    if(fread(dest, 1, size, fp) != (size_t)i)   // file size mismatch
		return(-1);

	return(i);
}

static void word_swap(UINT8 *src, INT size)
{
    while (size -= 4)
    {
        *((UINT32 *) src) = BSWAP32(*((UINT32 *) src));
        src += sizeof(UINT32);
    }
}

/*
 * Save a buffer to a file.
 */

static void save_file(char * path, UINT8 * src, INT size, BOOL byte_reversed)
{
    FILE * fp;

    if (byte_reversed)
        word_swap(src, size);
        
    fp = fopen(path, "wb");
    if(fp != NULL)
	{
        fwrite(src, 1, size, fp);
        fclose(fp);
        message(0, "Wrote %s", path);
	}
    else
        message(0, "Failed to write %s", path);

    if (byte_reversed)
        word_swap(src, size);
}

//TODO: Fix these macros. I removed the path and hacked them up just enough
// to where they will compile. ---Bart

/* ugh ... remove this and use two vars */
#define BUILD_EEPROM_PATH \
			char string[256]; \
            string[0] = '\0'; \
            /*strcpy(string, path);*/ \
            strcat(string, "BACKUP/"); \
            strcat(string, m3_config.game_id); \
			strcat(string, ".EPR");

#define BUILD_BRAM_PATH \
			char string[256]; \
            string[0] = '\0'; \
            /*strcpy(string, path );*/ \
            strcat(string, "BACKUP/"); \
            strcat(string, m3_config.game_id); \
			strcat(string, ".BRM");

void m3_load_eeprom(void)
{
	INT i;

	BUILD_EEPROM_PATH

    if((i = eeprom_load(string)) != MODEL3_OKAY)
	{
		message(0, "Can't load EEPROM from %s (%d)", string, i);
	}
}

void m3_save_eeprom(void)
{
	INT i;

	BUILD_EEPROM_PATH

    if((i = eeprom_save(string)) != MODEL3_OKAY)
	{
		message(0, "Can't save EEPROM to %s (%d)", string, i);
	}
}

void m3_load_bram(void)
{
	BUILD_BRAM_PATH

    if(load_file(string, bram, 128*1024))
	{
		message(0, "Can't load Backup RAM from file, creating a new file.");
		memset(bram, 0xFF, 128*1024);
        save_file(string, bram, 128*1024, 0);
	}
}

void m3_save_bram(void)
{
	BUILD_BRAM_PATH

    save_file(string, bram, 128*1024, 0);
}

/*
 * BOOL m3_save_state(CHAR *file);
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

BOOL m3_save_state(CHAR *file)
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
    fwrite(bram, sizeof(UINT8), 128*1024, fp);
    fwrite(&m3_irq_state, sizeof(UINT8), 1, fp);
    fwrite(&m3_irq_enable, sizeof(UINT8), 1, fp);
    fwrite(&crom_bank_reg, sizeof(UINT8), 1, fp);

    /*
     * Save the rest of the system state
     */

    ppc_save_state(fp);
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

BOOL m3_load_state(CHAR *file)
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
    fread(bram, sizeof(UINT8), 128*1024, fp);
    fread(&m3_irq_state, sizeof(UINT8), 1, fp);
    fread(&m3_irq_enable, sizeof(UINT8), 1, fp);
    fread(&crom_bank_reg, sizeof(UINT8), 1, fp);
    m3_set_crom_bank(crom_bank_reg);

    /*
     * Load the rest of the system state
     */

    ppc_load_state(fp);
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

void m3_run_frame(void)
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

    /*
     * Run the PowerPC and 68K
     */

    LOG("model3.log", "-- ACTIVE SCAN\n");
    m3_add_irq(m3_irq_enable & 0x0C);
    ppc_set_irq_line(1);

	PROFILE_SECT_ENTRY("ppc");
    ppc_run(ppc_freq / 60);
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

    m3_add_irq(m3_irq_enable & 0x42);
//    m3_add_irq(m3_irq_enable);
//    m3_add_irq(m3_irq_enable & 0x02);
    ppc_set_irq_line(1);

	PROFILE_SECT_ENTRY("ppc");
    ppc_run(100000);   
	PROFILE_SECT_EXIT("ppc");

    m3_remove_irq(0xFF);    // some games expect a bunch of IRQs to go low after some time

//    m3_add_irq(0x02);
//    ppc_set_irq_line(1);
#if 0
    m3_add_irq(1 << m3_irq_bit);
    ppc_set_irq_line(1);
    m3_irq_bit = (m3_irq_bit + 1) & 7;
#endif
    
	PROFILE_SECT_EXIT("-");
}

void m3_reset(void)
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
	memset(bram, 0, 128*1024);

	/* reset all the modules */

    if(ppc_reset() != PPC_OKAY)
        error("ppc_reset failed");

	if( !stricmp(m3_config.game_id,"VS2") )
		bridge_reset(2);
	else if( !stricmp(m3_config.game_id, "VS2_98") )
		bridge_reset(2);
	else
    bridge_reset(m3_config.step < 0x20 ? 1 : 2);
    eeprom_reset();
	scsi_reset();
	dma_reset();

	tilegen_reset();
	r3d_reset();
//    scsp_reset();
//    if(m3_config.flags & GAME_OWN_DSB1) dsb_reset();
    controls_reset(m3_config.flags & (GAME_OWN_STEERING_WHEEL | GAME_OWN_GUN));

    m3_remove_irq(0xFF);
    m3_set_crom_bank(0xFF);

	/* load NVRAMs */

	m3_load_eeprom();
	m3_load_bram();
}

/******************************************************************/
/* File (and ROM) Management                                      */
/******************************************************************/

typedef struct
{
    UINT8	name[20];
    UINT	size;
    UINT32	crc32;

} ROMFILE;

typedef struct
{
	char	id[20];
	char	superset_id[20];
	char	title[48];
	char	manuf[16];
    UINT	year;
    INT		step;
    FLAGS	flags;

	ROMFILE	crom[4];
	ROMFILE	crom0[4];
	ROMFILE	crom1[4];
	ROMFILE	crom2[4];
	ROMFILE	crom3[4];
	ROMFILE	vrom[16];
	ROMFILE	srom[3];
	ROMFILE	dsb_rom[5];

} ROMSET;

static ROMSET m3_rom_list[] =
{
#include "rom_list.h"
};

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

/*
 * Load a batch of files, (word)interleaving as needed.
 */

#define NO_ITLV	1
#define ITLV_2	2
#define ITLV_4	4
#define ITLV_16	16

static INT load_romfile(UINT8 * buff, char * dir, ROMFILE * list, UINT32 itlv)
{
	CHAR string[256], temp[256];
	UINT i, j;

	/* build the ROMSET path */

//    strcpy(string, m3_path);
    strcpy(string, "roms/");
    strcat(string, dir);
	strcat(string, "/");

	/* check if it's an unused ROMFILE, if so skip it */

	if(list[0].size == 0)
		return(MODEL3_OKAY);

	/* check interleave size for validity */

	if(	itlv != NO_ITLV &&
		itlv != ITLV_2 &&
		itlv != ITLV_4 &&
		itlv != ITLV_16 )
		return(MODEL3_ERROR);

	/* load all the needed files linearly */

	for(i = 0; i < itlv; i++)
	{
		FILE * file;

		/* all the files must be the same size */

		if(list[i].size != list[0].size)
			return(MODEL3_ERROR);

		/* build the file path */

		strcpy(temp, string);
		strcat(temp, list[i].name);

		printf("loading \"%s\"\n", temp); // temp

		if((file = fopen(temp, "rb")) == NULL)
			return(MODEL3_ERROR);

		/* load interleaved (16-bit at once) */

        for (j = 0; j < list[0].size; j += 2)
        {
            buff[i*2+j*itlv+0] = fgetc(file);
            buff[i*2+j*itlv+1] = fgetc(file);
        }

		fclose(file);
	}

	/* eveyrthing went right */

	return(MODEL3_OKAY);
}

/*
 * void m3_unload_rom(void);
 *
 * Unload any previously loaded ROM.
 */

void m3_unload_rom(void)
{    
	SAFE_FREE(crom);
	SAFE_FREE(vrom);
	SAFE_FREE(srom);
}

/*
 * BOOL m3_load_rom(CHAR *id);
 *
 * Load a ROM set.
 *
 * Parameters:
 *      id = Name of ROM set (see rom_list.h.)
 *
 * Returns:
 *      M3_OKAY  = Success.
 *      M3_ERROR = Unable to load the ROMs.
 */

BOOL m3_load_rom(CHAR * id)
{
    CHAR    string[256] = "";
    ROMSET  * romset = NULL;
    FILE    * file;
    UINT8   * crom0 = NULL, * crom1 = NULL, * crom2 = NULL, * crom3 = NULL;
    UINT    i, size;

	if(id[0] == '\0')
		error("No ROM selected");

	/* unload any previous ROM */

    m3_unload_rom();

	/* look for the ROM ID in the ROM list */

	size = sizeof(m3_rom_list) / sizeof(m3_rom_list[0]);

	for(i = 0; i < size; i++)
	{
        if(!stricmp(m3_rom_list[i].id, id))
		{
			romset = &m3_rom_list[i];
			break;
		}
	}

	if(romset == NULL)
		return(MODEL3_ERROR);

	message(0, "Loading \"%s\"\n", romset->title);

	/* load the mother ROMSET if needed */

	if(romset->superset_id[0] != '\0')
        if(m3_load_rom(romset->superset_id) != MODEL3_OKAY)
			return(MODEL3_ERROR);

	/* allocate as much memory as needed, if not already allocated by mother */

	if(crom == NULL)
	{
		crom = (UINT8 *)malloc(72*1024*1024);
        vrom = (UINT8 *)malloc(64*1024*1024);
        srom = (UINT8 *)malloc(romset->srom[0].size + romset->srom[1].size + romset->srom[2].size);
		drom = (UINT8 *)malloc(romset->dsb_rom[0].size +
							   romset->dsb_rom[1].size + romset->dsb_rom[2].size +
							   romset->dsb_rom[3].size + romset->dsb_rom[4].size);

		if((crom == NULL) || (vrom == NULL) || (srom == NULL) || (drom == NULL))
		{
			SAFE_FREE(crom);
			SAFE_FREE(vrom);
			SAFE_FREE(srom);
			SAFE_FREE(drom);

			return(MODEL3_ERROR);
		}

        memset(crom, 0xFF, 72*1024*1024);
	}

	crom0 = &crom[0x00800000];
	crom1 = &crom[0x01800000];
	crom2 = &crom[0x02800000];
	crom3 = &crom[0x03800000];

	/* load ROM files into memory */

//    strcpy(string, m3_path);
    strcpy(string, id);
    strcat(string, ".zip");
	file = fopen(string, "rb");

	if(file == NULL)
	{
		/* load from directory */

        if(romset->crom[0].size && load_romfile(&crom[8*1024*1024 - (romset->crom[0].size * 4)], id, romset->crom, ITLV_4))
			error("Can't load CROM");

		if(romset->crom0[0].size && load_romfile(crom0, id, romset->crom0, ITLV_4))
			error("Can't load CROM0");

		if(romset->crom1[0].size && load_romfile(crom1, id, romset->crom1, ITLV_4))
			error("Can't load CROM1");

		if(romset->crom2[0].size && load_romfile(crom2, id, romset->crom2, ITLV_4))
			error("Can't load CROM2");

		if(romset->crom3[0].size && load_romfile(crom3, id, romset->crom3, ITLV_4))
			error("Can't load CROM3");

		if(romset->vrom[0].size && load_romfile(vrom, id, romset->vrom, ITLV_16))
			error("Can't load VROM");

		if(romset->srom[0].size && load_romfile(srom, id, romset->srom, NO_ITLV))
			error("Can't load SROM Program");

		if(romset->srom[1].size && load_romfile(&srom[romset->srom[0].size], id, &romset->srom[1], ITLV_2))
			error("Can't load SROM Samples");

		if(romset->dsb_rom[0].size && load_romfile(drom, id, romset->dsb_rom, NO_ITLV))
			error("Can't load DSB ROM Program");

		if(romset->dsb_rom[1].size && load_romfile(&drom[romset->dsb_rom[0].size], id, &romset->dsb_rom[1], ITLV_4))
			error("Can't load DSB ROM Samples");

		/* byteswap buffers */

        byteswap(&crom[8*1024*1024 - (romset->crom[0].size * 4)], romset->crom[0].size * 4);
        byteswap(crom0, romset->crom0[0].size * 4);
        byteswap(crom1, romset->crom1[0].size * 4);
		byteswap(crom2, romset->crom2[0].size * 4);
		byteswap(crom3, romset->crom3[0].size * 4);
		byteswap(vrom, romset->vrom[0].size * 16);
		byteswap32(vrom, romset->vrom[0].size * 16);

		// TODO: swap SROM and DSB ROMs
	}
	else
	{
		fclose(file);

		/* load from ZIP */
	}

    /* set stepping */

    m3_config.step = romset->step;

    /* set game flags */

    if(romset->flags & GAME_OWN_STEERING_WHEEL)
        m3_config.flags |= GAME_OWN_STEERING_WHEEL;
    if(romset->flags & GAME_OWN_GUN)
        m3_config.flags |= GAME_OWN_GUN;
    if(romset->dsb_rom[0].name[0] != '\0')
        m3_config.flags |= GAME_OWN_DSB1;

    /* mirror CROM0 to CROM if needed */

	if((romset->crom[0].size * 4) < 8*1024*1024)
        memcpy(crom, crom0, 8*1024*1024 - romset->crom[0].size*4);

	/*
	 * Perhaps mirroring must occur between the CROMx
	 * and the CROMx space, if CROMx is < 16MB?
	 */

    /* mirror lower VROM if necessary */

    if ((romset->vrom[0].size * 16) < 64*1024*1024)
        memcpy(&vrom[romset->vrom[0].size * 16], vrom, 64*1024*1024 - romset->vrom[0].size * 16);

	/* if we're here, everything went fine! */

    message(0, "ROM loaded succesfully!");

    /*
     * Debug ROM dump
     */

//    save_file("crom.bin", &crom[0x800000 - romset->crom[0].size * 4], romset->crom[0].size * 4, 0);
//    save_file("crom.bin", crom, 8*1024*1024, 0);
//    save_file("vrom.bin", vrom, 64*1024*1024, 0);
//    save_file("crom0.bin", crom0, 16*1024*1024, 0);
//    save_file("crom1.bin", crom1, 16*1024*1024, 0);
//    save_file("crom2.bin", crom2, 16*1024*1024, 0);


    /*
     * Patches
     */

    if (!stricmp(id, "VF3"))
    {
//        *(UINT32 *)&crom[0x710000 + 0x47C8] = BSWAP32(0x000076B4);
        *(UINT32 *)&crom[0x710000 + 0x47CC] = BSWAP32(0x00007730);

        

        *(UINT32 *)&crom[0x710000 + 0x1C48] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x1C20] = BSWAP32(0x60000000);

        *(UINT32 *)&crom[0x710000 + 0x61C50] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x3C7C] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x3E54] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x25B0] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x25D0] = BSWAP32(0x60000000);
#if 0
        *(UINT32 *)&crom[0x710000 + 0x61C50] = BSWAP32(0x60000000);

        *(UINT32 *)&crom[0x710000 + 0x8050] = BSWAP32(0x60000000);  // related to RTC test
        *(UINT32 *)&crom[0x710000 + 0x4C788] = BSWAP32(0x4E800020);

        *(UINT32 *)&crom[0x710000 + 0x4C0BC] = BSWAP32(0x60000000); // related to battery test
        *(UINT32 *)&crom[0x710000 + 0x4C0C0] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x4C0C4] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x4C0C8] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x4C0CC] = BSWAP32(0x60000000);
        *(UINT32 *)&crom[0x710000 + 0x4C0D0] = BSWAP32(0x60000000);

        *(UINT32 *)&crom[0x710000 + 0x1E94C] = BSWAP32(0x4E800020); //???
#endif
    }
    else if(!stricmp(id, "LOST"))
	{
		/*
		 * this patch fixes the weird "add r4,r4,r4" with a "addi r4,r4,4".
		 * there's another weird add instruction just before this one, but
		 * it seems like it's never used.
		 */

		*(UINT32 *)&crom[0x7374F4] = BSWAP32((14 << 26) | (4 << 21) | (4 << 16) | 4);
	}
    else if(!stricmp(id, "SCUD"))
	{
        *(UINT32 *)&crom[0x799DE8] = BSWAP32(0x00050208);   // debug menu

        /*
         * Not required. See note in SCUDE.
         */
        
#if 0
        *(UINT32 *)&crom[0x700194] = 0x00000060;    // Timebase Skip
        *(UINT32 *)&crom[0x712734] = 0x00000060;    // Speedup
        *(UINT32 *)&crom[0x717EC8] = 0x2000804E;
        *(UINT32 *)&crom[0x71AEBC] = 0x00000060;    // Loop Skip
#endif

        /*
         * Required.
         *
         * In SCUD, unlike the SCUDE dump, the link ID is not single by
         * default, so we patch it here.
         */

		*(UINT32 *)&crom[0x71277C] = 0x00000060;
        *(UINT32 *)&crom[0x74072C] = 0x00000060;    // ...

        *(UINT32 *)&crom[0x710000 + 0x275C] = BSWAP32(0x60000000);  // allows game to start

        *(UINT8  *)&crom[0x787B36] = 0x00;          // Link ID: 00 = single, 01 = master, 02 = slave
        *(UINT8  *)&crom[0x787B30] = 0x00;
	}
    else if (!stricmp(id, "SCUDE"))
    {
        /*
         * None of these patches are required, they're simply here to make
         * the game boot faster.
         *
         * NOTE: It would be wise to periodically check to make sure that
         * the game still runs with these patches off, especially when playing
         * with the EEPROM code.
         */

#if 0
        *(UINT32 *)&crom[0x700194] = 0x00000060;    // Timebase Skip
        *(UINT32 *)&crom[0x712734] = 0x00000060;    // Speedup
        *(UINT32 *)&crom[0x717EC8] = 0x2000804E;
        *(UINT32 *)&crom[0x71AEBC] = 0x00000060;    // Loop Skip
#endif

        /*
         * These patches are still required :(
         */

        *(UINT32 *)&crom[0x71277C] = 0x00000060;    // ?
        *(UINT32 *)&crom[0x74072C] = 0x00000060;    // ?

        *(UINT32 *)&crom[0x710000 + 0x275C] = BSWAP32(0x60000000);
    }
    else if (!stricmp(id, "VON2"))
    {
        /*
         * Patch an annoyingly long delay loop to: xor r3,r3,r3; nop
         */

        *(UINT32 *) &crom[0x1B0] = 0x781A637C;
        *(UINT32 *) &crom[0x1B4] = 0x00000060;

        /*
         * VON2 will run without any of these patches, but this makes it
         * start up much faster ;)
         */

//#if 0
        *(UINT32 *) &crom[0x189168] = 0x00000060;

        *(UINT32 *) &crom[0x1890AC] = 0x00000060;
        *(UINT32 *) &crom[0x1890B8] = 0x00000060;

        *(UINT32 *) &crom[0x1888A8] = 0x00000060;
        *(UINT32 *) &crom[0x18CA14] = 0x34010048;   // skip ASIC test (required)
        *(UINT32 *) &crom[0x1891C8] = 0x00000060;   // (required)
//#endif
    }
	else if( !stricmp(id, "VS2") ) {
		// Loops if irq 0x20 is set (0xF0100018)
		*(UINT32 *) &crom[0x70C000 + 0x1DE0] = BSWAP32(0x60000000);

		// Loops if memory at 0x15C5B0 != 0
		*(UINT32 *) &crom[0x70C000 + 0xAF48] = BSWAP32(0x60000000);

		// Weird loop
		*(UINT32 *) &crom[0x70C000 + 0x26F0] = BSWAP32(0x60000000);
		*(UINT32 *) &crom[0x70C000 + 0x2710] = BSWAP32(0x60000000);

		// Waiting for decrementer == 0
		*(UINT32 *) &crom[0x70C000 + 0x14940] = BSWAP32(0x60000000);
	}
	else if( !stricmp(id, "VS2_98") ) {
		// Weird loop (see VS2)
        // MIDI-related. Perhaps the sound hardware generates many interrupts
        // per frame?        
        *(UINT32 *) &crom[0x600000 + 0x28EC] = BSWAP32(0x60000000);
        *(UINT32 *) &crom[0x600000 + 0x290C] = BSWAP32(0x60000000);

		// Waiting for decrementer
        *(UINT32 *) &crom[0x600000 + 0x14F2C] = BSWAP32(0x60000000);
	}
    else if (!stricmp(id, "SRALLY2"))
    {
//        *(UINT32 *) &crom[0x30] = BSWAP32(0x60000000);

//        *(UINT32 *) &crom[0x78930] = BSWAP32(0x60000000);

        /*
         * Note: 7B560 checks for some value which may need to be returned
         * by the TAP. This is handled by the first patch.
         */
        *(UINT32 *) &crom[0x7C0C4] = BSWAP32(0x60000000);
        *(UINT32 *) &crom[0x7C0C8] = BSWAP32(0x60000000);
        *(UINT32 *) &crom[0x7C0CC] = BSWAP32(0x60000000);

//        *(UINT32 *) &crom[0x78894] = BSWAP32(0x60000000);
    }

	return(MODEL3_OKAY);
}

/******************************************************************/
/* Machine Interface                                              */
/******************************************************************/

void m3_shutdown(void)
{
	/* save NVRAMs */

	m3_save_eeprom();
	m3_save_bram();

	/* detach any loaded ROM */

    m3_unload_rom();

	/* shutdown all the modules */

	osd_input_shutdown();
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

    save_file("ram", ram, 8*1024*1024, 0);
    save_file("vram", vram, 1*1024*1024+2*65536, 0);
    save_file("8e000000", culling_ram_8e, 1*1024*1024, 0);
    save_file("8c000000", culling_ram_8c, 2*1024*1024, 0);
    save_file("98000000", polygon_ram, 2*1024*1024, 0);
    save_file("fe180000", _FE180000, 0x20000, 0);
    save_file("texture.bin", texture_ram, 2048*2048*2, 0);

    SAFE_FREE(ram);
	SAFE_FREE(vram);
	SAFE_FREE(sram);
	SAFE_FREE(bram);
    SAFE_FREE(culling_ram_8e);
    SAFE_FREE(culling_ram_8c);
    SAFE_FREE(polygon_ram);
    SAFE_FREE(texture_ram);

	#ifdef _PROFILE_
	{
	UINT64 total_read, total_write, total;

	total_read = profile_read_8 + profile_read_16 + profile_read_32 + profile_read_64;
	total_write = profile_write_8 + profile_write_16 + profile_write_32 + profile_write_64;
	total = total_read + total_write;

	fprintf(stderr,
			"memory access stats:\n"
			" read   8 : %2.6f%% of total reads,  %2.6f%% of total accesses\n"
			" read  16 : %2.6f%% of total reads,  %2.6f%% of total accesses\n"
			" read  32 : %2.6f%% of total reads,  %2.6f%% of total accesses\n"
			" read  64 : %2.6f%% of total reads,  %2.6f%% of total accesses\n"
			" write  8 : %2.6f%% of total writes, %2.6f%% of total accesses\n"
			" write 16 : %2.6f%% of total writes, %2.6f%% of total accesses\n"
			" write 32 : %2.6f%% of total writes, %2.6f%% of total accesses\n"
			" write 64 : %2.6f%% of total writes, %2.6f%% of total accesses\n",

			(FLOAT64)profile_read_8 / ((FLOAT64)total_read / 100.0),
			(FLOAT64)profile_read_8 / ((FLOAT64)total / 100.0),

			(FLOAT64)profile_read_16 / ((FLOAT64)total_read / 100.0),
			(FLOAT64)profile_read_16 / ((FLOAT64)total / 100.0),

			(FLOAT64)profile_read_32 / ((FLOAT64)total_read / 100.0),
			(FLOAT64)profile_read_32 / ((FLOAT64)total / 100.0),

			(FLOAT64)profile_read_64 / ((FLOAT64)total_read / 100.0),
			(FLOAT64)profile_read_64 / ((FLOAT64)total / 100.0),

			(FLOAT64)profile_write_8 / ((FLOAT64)total_write / 100.0),
			(FLOAT64)profile_write_8 / ((FLOAT64)total / 100.0),

			(FLOAT64)profile_write_16 / ((FLOAT64)total_write / 100.0),
			(FLOAT64)profile_write_16 / ((FLOAT64)total / 100.0),

			(FLOAT64)profile_write_32 / ((FLOAT64)total_write / 100.0),
			(FLOAT64)profile_write_32 / ((FLOAT64)total / 100.0),

			(FLOAT64)profile_write_64 / ((FLOAT64)total_write / 100.0),
			(FLOAT64)profile_write_64 / ((FLOAT64)total / 100.0)
	);

	// generate profile informations

	profile_cleanup();

	}
	#endif
}

static PPC_FETCH_REGION m3_ppc_fetch[2];

void m3_init(void)
{
	/* setup m3_config (which is already partially done in parse_command_line) */

	m3_config.log_enabled = 1;

	/* load the ROM -- if specified on command line */

    if(m3_config.game_id[0] != '\0')
        if(m3_load_rom(m3_config.game_id) != MODEL3_OKAY)
			error("Can't load ROM %s\n", m3_config.game_id);

    /* allocate memory */

    ram = (UINT8 *) malloc(8*1024*1024);
    vram = (UINT8 *) malloc(2*1024*1024);
    sram = (UINT8 *) malloc(1*1024*1024);
    bram = (UINT8 *) malloc(128*1024);
    culling_ram_8e = (UINT8 *) malloc(1*1024*1024);
    culling_ram_8c = (UINT8 *) malloc(2*1024*1024);
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

        error("Out of memory!");
    }

	/* attach m3_shutdown to atexit */

	atexit(m3_shutdown);

    /* setup the PPC */

    if(ppc_init() != PPC_OKAY)
		error("ppc_init failed.");

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

    if (m3_config.step >= 0x20)
		ppc_set_reg(PPC_REG_PVR, 0x00070100);
    else
		ppc_set_reg(PPC_REG_PVR, 0x00060104);

    ppc_set_irq_callback(m3_ppc_irq_callback);

    ppc_set_read_8_handler((void *)m3_ppc_read_8);
    ppc_set_read_16_handler((void *)m3_ppc_read_16);
    ppc_set_read_32_handler((void *)m3_ppc_read_32);
    ppc_set_read_64_handler((void *)m3_ppc_read_64);

    ppc_set_write_8_handler((void *)m3_ppc_write_8);
    ppc_set_write_16_handler((void *)m3_ppc_write_16);
    ppc_set_write_32_handler((void *)m3_ppc_write_32);
    ppc_set_write_64_handler((void *)m3_ppc_write_64);

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
    render_init(culling_ram_8e, culling_ram_8c, polygon_ram, texture_ram, vrom);
    r3d_init(culling_ram_8e, culling_ram_8c, polygon_ram, texture_ram, vrom);
//    scsp_init();
//    if(m3_config.flags & GAME_OWN_DSB1) dsb_reset();
    controls_init();
	osd_input_init();
}
