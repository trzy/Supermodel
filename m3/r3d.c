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
 * r3d.c
 *
 * Real3D Model 3 graphics system emulation. The 3D hardware in Model 3 is
 * supposedly based on the Pro-1000.
 */

#include "model3.h"

/******************************************************************/
/* Privates                                                       */
/******************************************************************/

static UINT8    *culling_ram_8e;    // culling RAM at 0x8E000000
static UINT8    *culling_ram_8c;    // culling RAM at 0x8C000000
static UINT8    *polygon_ram;       // polygon RAM at 0x98000000
static UINT8	*vrom;

static UINT8    texture_buffer_ram[1*1024*1024];

static UINT32	vrom_texture_address;
static UINT32	vrom_texture_header;

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

/*
 * void r3d_init(UINT8 *culling_ram_8e_ptr, UINT8 *culling_ram_8c_ptr,
 *               UINT8 *polygon_ram_ptr);
 *
 * Initializes the Real3D graphics emulation.
 *
 * Parameters:
 *      culling_ram_8e_ptr = Pointer to 0x8E000000 culling RAM.
 *      culling_ram_8c_ptr = Pointer to 0x8C000000 culling RAM.
 *      polygon_ram_ptr    = Pointer to polygon RAM.
 */

void r3d_init(UINT8 *culling_ram_8e_ptr, UINT8 *culling_ram_8c_ptr,
              UINT8 *polygon_ram_ptr, UINT8 *vrom_ptr)
{
    culling_ram_8e = culling_ram_8e_ptr;
    culling_ram_8c = culling_ram_8c_ptr;
    polygon_ram = polygon_ram_ptr;
	vrom = vrom_ptr;
}

/*
 * void r3d_shutdown(void);
 *
 * Shuts down the Real3D emulation.
 */

void r3d_shutdown(void)
{
}

/*
 * void r3d_reset(void);
 *
 * Resets the Real3D graphics hardware. RAM is cleared in order to prevent
 * the renderer from drawing garbage and possibly locking up as a result.
 */

void r3d_reset(void)
{
    memset(culling_ram_8e, 0, 1*1024*1024);
    memset(culling_ram_8c, 0, 1*1024*1024);
    memset(polygon_ram, 0, 1*1024*1024);
	tap_reset();
}

/*
 * void r3d_save_state(FILE *fp);
 *
 * Saves the state of the Real3D graphics hardware to a file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void r3d_save_state(FILE *fp)
{
    fwrite(culling_ram_8e, sizeof(UINT8), 1*1024*1024, fp);
    fwrite(culling_ram_8c, sizeof(UINT8), 1*1024*1024, fp);
    fwrite(polygon_ram, sizeof(UINT8), 1*1024*1024, fp);
}

/*
 * void r3d_load_state(FILE *fp);
 *
 * Loads the state of the Real3D graphics hardware from a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void r3d_load_state(FILE *fp)
{
    fread(culling_ram_8e, sizeof(UINT8), 1*1024*1024, fp);
    fread(culling_ram_8c, sizeof(UINT8), 1*1024*1024, fp);
    fread(polygon_ram, sizeof(UINT8), 1*1024*1024, fp);
}

/******************************************************************/
/* Access                                                         */
/******************************************************************/

/*
 * UINT32 r3d_read_32(UINT32 a);
 *
 * Reads a 32-bit word from the Real3D regions.
 *
 * Parameters:
 *      a = Address.
 *
 * Returns:
 *      Data read.
 */

UINT32 r3d_read_32(UINT32 a)
{
    static UINT32   _84000000 = 0;

    switch (a)
    {
    case 0x84000000:    // loop around 0x1174EC in Lost World expects bit
        return (_84000000 ^= 0xFFFFFFFF);
    case 0x84000004:    // unknown, Lost World reads these
    case 0x84000008:
    case 0x8400000C:
    case 0x84000010:
    case 0x84000014:
    case 0x84000018:
    case 0x8400001C:
    case 0x84000020:
        return 0xFFFFFFFF;
    }

    error("Unknown R3D read: %08X: %08X\n", ppc_get_reg(PPC_REG_PC), a);
	return(0);
}

/*
 * void r3d_write_32(UINT32 a, UINT32 d);
 *
 * Writes a 32-bit word to the Real3D regions.
 *
 * Parameters:
 *      a = Address.
 *      d = Data to write.
 */

void r3d_write_32(UINT32 a, UINT32 d)
{
    static UINT32   last_addr;
    UINT            size_x, size_y;

    if (a >= 0x8E000000 && a <= 0x8E0FFFFF)         // culling RAM
    {
        *(UINT32 *) &culling_ram_8e[a & 0xFFFFF] = BSWAP32(d);
        return;
    }
    else if (a >= 0x8C000000 && a <= 0x8C0FFFFF)    // culling RAM
    {
        *(UINT32 *) &culling_ram_8c[a & 0xFFFFF] = BSWAP32(d);
        return;
    }
    else if (a >= 0x98000000 && a <= 0x980FFFFF)    // polygon RAM
    {
        *(UINT32 *) &polygon_ram[a & 0xFFFFF] = BSWAP32(d);
        return;
    }
    else if (a >= 0x94000000 && a <= 0x940FFFFF)    // texture buffer
    {
        d = BSWAP32(d);
        *(UINT32 *) &texture_buffer_ram[a & 0xFFFFF] = d;

        if (a == 0x94000004)    // get size and calculate last word written
        {
            size_x = (d >> 14) & 3;
            size_y = (d >> 17) & 3;
            size_x = 32 << size_x;
            size_y = 32 << size_y;
            last_addr = (0x94000008 + size_x * size_y * 2 - 2) & ~0x3;
            message(0, "texture transfer started: %dx%d, %08X", size_x, size_y, d);
        }
        else if (a == last_addr)    // last word written
        {
            osd_renderer_upload_texture(texture_buffer_ram);
            message(0, "texture upload complete");
        }

        return;
    }
    else if (a >= 0x90000000)
    {
		if(a == 0x90000000) {
			vrom_texture_address = BSWAP32(d);
		} 
		else if(a == 0x90000004) {
			vrom_texture_header = BSWAP32(d);
		}
		else if(a == 0x90000008) {
			UINT32 length = BSWAP32(d);
			osd_renderer_upload_vrom_texture(vrom_texture_header, length, &vrom[(vrom_texture_address & 0x7FFFFF) * 4]);
		}

        return;
    }

    switch (a)
    {
    case 0x88000000:    // 0xDEADDEAD
        return;
    }

    error("Unknown R3D write: %08X: %08X = %08X\n", ppc_get_reg(PPC_REG_PC), a, d);
}

/******************************************************************/
/* Real3D TAP Port                                                */
/******************************************************************/

/* Stefano: Bart, take a look at this code; i'm not sure if it's all right */

typedef enum
{
	TAP_TEST_LOGIC_RESET = 0,
	TAP_RUN_TEST_IDLE,
	TAP_SELECT_DR_SCAN,		TAP_SELECT_IR_SCAN,
	TAP_CAPTURE_DR,			TAP_CAPTURE_IR,
	TAP_SHIFT_DR,			TAP_SHIFT_IR,
	TAP_EXIT1_DR,			TAP_EXIT1_IR,
	TAP_PAUSE_DR,			TAP_PAUSE_IR,
	TAP_EXIT2_DR,			TAP_EXIT2_IR,
	TAP_UPDATE_DR,			TAP_UPDATE_IR,

} TAP_STATE;

static const char * tap_state_name[] =
{
	"Test Logic Reset",
	"Run Test Idle",
	"Select DR Scan",	"Select IR Scan",
	"Capture DR",		"Capture IR",
	"Shift DR",			"Shift IR",
	"Exit1 DR",			"Exit1 IR",
	"Pause DR",			"Pause IR",
	"Exit2 DR",			"Exit2 IR",
	"Update DR",		"Update IR",
};

static UINT32 tap_state;
static UINT32 tap_tdo;

static UINT32 tap_ireg_shift = 0;
static UINT32 tap_ireg[4];			/* 128 bits available (96 used) */

static UINT32 tap_dreg_shift = 0;
static UINT32 tap_dreg[385];		/* 1540 bits available (1537 used) */

void tap_write(UINT32 tck, UINT32 tms, UINT32 tdi, UINT32 trst)
{
	/* currently doesn't return data from the DREG */

	if(tck)
	{
		tap_tdo = 0;

		if(!trst)
		{
			tap_state = TAP_TEST_LOGIC_RESET;
		}
		else
		{
			switch(tap_state)
			{

			case TAP_TEST_LOGIC_RESET:
				if(!tms) tap_state = TAP_RUN_TEST_IDLE;
				break;

			case TAP_RUN_TEST_IDLE:
				if(tms)	tap_state = TAP_SELECT_DR_SCAN;
				break;

			case TAP_SELECT_DR_SCAN:
				if(tms)	tap_state = TAP_SELECT_IR_SCAN;
				else	tap_state = TAP_CAPTURE_DR;
				break;

			case TAP_SELECT_IR_SCAN:
				if(tms)	tap_state = TAP_TEST_LOGIC_RESET;
				else	tap_state = TAP_CAPTURE_IR;
				break;

			case TAP_CAPTURE_DR:
				if(tms)	tap_state = TAP_EXIT1_DR;
				else	tap_state = TAP_SHIFT_DR;
				break;

			case TAP_CAPTURE_IR:
				if(tms)	tap_state = TAP_EXIT1_IR;
				else	tap_state = TAP_SHIFT_IR;
				break;

			case TAP_SHIFT_DR:
				if(tms)	tap_state = TAP_EXIT1_DR;

				tap_tdo = 0;//(tap_ireg[(tap_ireg_shift >> 5) & 3] >> (tap_ireg_shift & 31)) & 1;

				tap_dreg[tap_dreg_shift >> 5] &= ~(1 << (tap_dreg_shift & 31));
				tap_dreg[tap_dreg_shift >> 5] |= (tdi & 1) << (tap_dreg_shift & 31);

				tap_dreg_shift++;

				break;

			case TAP_SHIFT_IR:
				if(tms) tap_state = TAP_EXIT1_IR;

				tap_tdo = (tap_ireg[tap_ireg_shift >> 5] >> (tap_ireg_shift & 31)) & 1;

				tap_ireg[tap_ireg_shift >> 5] &= ~(1 << (tap_ireg_shift & 31));
				tap_ireg[tap_ireg_shift >> 5] |= (tdi & 1) << (tap_ireg_shift & 31);

				tap_ireg_shift++;

				break;

			case TAP_EXIT1_DR:
				if(tms)	tap_state = TAP_UPDATE_DR;
				else	tap_state = TAP_PAUSE_DR;

				/*
				{
				u32 i, nw;

				nw = (tap_dreg_shift + 31) / 32;
				for(i = nw-1; (s32)i >= 0; i--)
					printf(" DREG[%2i] = %08X (%03i)\n", i, tap_dreg[i], tap_dreg_shift);
				}
				*/

				tap_dreg_shift = 0;

				break;

			case TAP_EXIT1_IR:
				if(tms)	tap_state = TAP_UPDATE_IR;
				else	tap_state = TAP_PAUSE_IR;

				/*
				printf(" IREG = %08X %08X %08X %08X (%i)\n", tap_ireg[3], tap_ireg[2], tap_ireg[1], tap_ireg[0], tap_ireg_shift);
				*/

				tap_ireg_shift = 0;

				break;

			case TAP_PAUSE_DR:
				if(tms)	tap_state = TAP_EXIT2_DR;
				break;

			case TAP_PAUSE_IR:
				if(tms)	tap_state = TAP_EXIT2_IR;
				break;

			case TAP_EXIT2_DR:
				if(tms)	tap_state = TAP_UPDATE_DR;
				else	tap_state = TAP_SHIFT_DR;
				break;

			case TAP_EXIT2_IR:
				if(tms) tap_state = TAP_UPDATE_IR;
				else	tap_state = TAP_SHIFT_IR;
				break;

			case TAP_UPDATE_DR:
				if(tms)	tap_state = TAP_SELECT_DR_SCAN;
				else	tap_state = TAP_RUN_TEST_IDLE;
				break;

			case TAP_UPDATE_IR:
				if(tms)	tap_state = TAP_SELECT_DR_SCAN;
				else	tap_state = TAP_RUN_TEST_IDLE;
				break;
			}
		}
	}
}

UINT32 tap_read(void){

	return(tap_tdo);
}

void tap_reset(void){

	tap_state = TAP_TEST_LOGIC_RESET;

	tap_ireg_shift = 0;
	tap_dreg_shift = 0;

	memset(tap_ireg, 0, sizeof(UINT32)*4);
	memset(tap_dreg, 0, sizeof(UINT32)*385);
}

