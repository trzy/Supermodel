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

/*
 * RAM Size:
 * ---------
 *
 * It appears that there is 2MB total of "culling RAM." 1MB appears at
 * 0x8C000000 and the other at 0x8E000000. Step 1.0 and 1.5 appear to have
 * 1MB of polygon RAM, but Step 2.0 (and probably 2.1) clearly uses 2MB.
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
 *               UINT8 *polygon_ram_ptr, UINT8 *vrom_ptr);
 *
 * Initializes the Real3D graphics emulation.
 *
 * Parameters:
 *      culling_ram_8e_ptr = Pointer to 0x8E000000 culling RAM.
 *      culling_ram_8c_ptr = Pointer to 0x8C000000 culling RAM.
 *      polygon_ram_ptr    = Pointer to polygon RAM.
 *      vrom_ptr           = Pointer to VROM.
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
    memset(culling_ram_8c, 0, 2*1024*1024);
    memset(polygon_ram, 0, 2*1024*1024);
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
    fwrite(culling_ram_8c, sizeof(UINT8), 2*1024*1024, fp);
    fwrite(polygon_ram, sizeof(UINT8), 2*1024*1024, fp);
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
    fread(culling_ram_8c, sizeof(UINT8), 2*1024*1024, fp);
    fread(polygon_ram, sizeof(UINT8), 2*1024*1024, fp);
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
    else if (a >= 0x8C000000 && a <= 0x8C1FFFFF)    // culling RAM
    {
        *(UINT32 *) &culling_ram_8c[a & 0x1FFFFF] = BSWAP32(d);
        return;
    }
	else if (a >= 0x8C100000 && a <= 0x8C1FFFFF)	// culling RAM (Scud Race)
	{
		/*
		 * probabily this is a mirror of 0x8C000000 - 0x8C0FFFFF,
		 * as the other is never used when this one is.
		 * i'm leaving everything as is, though, so that you can study them separately.
		 */

        *(UINT32 *) &culling_ram_8c[a & 0x1FFFFF] = BSWAP32(d);
        return;
	}
    else if (a >= 0x98000000 && a <= 0x981FFFFF)    // polygon RAM
    {
        *(UINT32 *) &polygon_ram[a & 0x1FFFFF] = BSWAP32(d);
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

			if(!(d & 0x00800000))
				last_addr = (0x94000008 + size_x * size_y - 1) & ~3;		// 8-bit texture
			else
				last_addr = (0x94000008 + size_x * size_y * 2 - 2) & ~3;	// 16-bit texture

            message(0, "texture transfer started: %dx%d, %08X", size_x, size_y, d);
        }
        else if (a == last_addr)    // last word written
        {
            osd_renderer_upload_texture(*(UINT32 *) &texture_buffer_ram[4], *(UINT32 *) &texture_buffer_ram[0], &texture_buffer_ram[8], 1);
            message(0, "texture upload complete");
        }

        return;
    }

    switch (a)
    {
    case 0x88000000:    // 0xDEADDEAD
        message(0, "88000000 = %08X", BSWAP32(d));
        return;
    case 0x90000000:    // VROM texture address
        vrom_texture_address = BSWAP32(d);
        message(0, "VROM texture address = %08X", BSWAP32(d));
        return;
    case 0x90000004:    
        vrom_texture_header = BSWAP32(d);
        message(0, "VROM texture header = %08X", BSWAP32(d));
        return;
    case 0x90000008:
        osd_renderer_upload_texture(vrom_texture_header, BSWAP32(d), &vrom[(vrom_texture_address & 0x7FFFFF) * 4], 0);
        message(0, "VROM texture length = %08X", BSWAP32(d));
        return;
    case 0x9000000C:    // ? Virtual On 2: These are almost certainly for VROM textures as well (I was too lazy to check :P)
        vrom_texture_address = BSWAP32(d);
        message(0, "90000000C = %08X", BSWAP32(d));
        return;
    case 0x90000010:    // ?
        vrom_texture_header = BSWAP32(d);
        message(0, "900000010 = %08X", BSWAP32(d));
        return;
    case 0x90000014:    // ?
        osd_renderer_upload_texture(vrom_texture_header, BSWAP32(d), &vrom[(vrom_texture_address & 0x7FFFFF) * 4], 0);
        message(0, "900000014 = %08X", BSWAP32(d));
        return;
    case 0x9C000000:    // ?
        message(0, "9C000000 = %08X", BSWAP32(d));
        return;
    case 0x9C000004:    // ?
        message(0, "9C000004 = %08X", BSWAP32(d));
        return;
    case 0x9C000008:    // ?
        message(0, "9C000008 = %08X", BSWAP32(d));
        return;
    }

    error("Unknown R3D write: %08X: %08X = %08X\n", ppc_get_reg(PPC_REG_PC), a, d);
}

/******************************************************************/
/* Real3D TAP Port                                                */
/******************************************************************/

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
	"Test Logic/Reset",
	"Run Test/Idle",
	"Select DR Scan",	"Select IR Scan",
	"Capture DR",		"Capture IR",
	"Shift DR",			"Shift IR",
	"Exit1 DR",			"Exit1 IR",
	"Pause DR",			"Pause IR",
	"Exit2 DR",			"Exit2 IR",
	"Update DR",		"Update IR",
};

/*
 * Private Variables.
 */

static UINT tap_next_state[][2] =
{
//    TMS=0                 TMS=1
	{ TAP_RUN_TEST_IDLE,	TAP_TEST_LOGIC_RESET },	// Test Logic/Reset
	{ TAP_RUN_TEST_IDLE,	TAP_SELECT_DR_SCAN },	// Run Test/Idle
	{ TAP_CAPTURE_DR,		TAP_SELECT_IR_SCAN },	// Select DR Scan
	{ TAP_CAPTURE_IR,		TAP_TEST_LOGIC_RESET },	// Select IR Scan
	{ TAP_SHIFT_DR,			TAP_EXIT1_DR },			// Capture DR
	{ TAP_SHIFT_IR,			TAP_EXIT1_IR },			// Capture IR
	{ TAP_SHIFT_DR,			TAP_EXIT1_DR },			// Shift DR
	{ TAP_SHIFT_IR,			TAP_EXIT1_IR },			// Shift IR
	{ TAP_PAUSE_DR,			TAP_UPDATE_DR },		// Exit1 DR
	{ TAP_PAUSE_IR,			TAP_UPDATE_IR },		// Exit1 IR
	{ TAP_PAUSE_DR,			TAP_EXIT2_DR },			// Pause DR
	{ TAP_PAUSE_IR,			TAP_EXIT2_IR },			// Pause IR
	{ TAP_UPDATE_DR,		TAP_SHIFT_DR },			// Exit2 DR
	{ TAP_UPDATE_IR,		TAP_SHIFT_IR },			// Exit2 IR
	{ TAP_RUN_TEST_IDLE,	TAP_SELECT_DR_SCAN },	// Update DR
	{ TAP_RUN_TEST_IDLE,	TAP_SELECT_IR_SCAN },	// Update IR
};

static UINT tap_state;
static UINT tap_tdo;

static UINT tap_ireg_shift = 0;
static UINT32 tap_ireg[4];			/* 128 bits available (96 used) */
static UINT32 tap_inst[4];

static UINT tap_dreg_shift = 0;
static UINT32 tap_dreg[385];		/* 1540 bits available (1537 used) */

/*
 * void tap_write(UINT tck, UINT tms, UINT tdi, UINT trst);
 *
 * JTAP TAP write port handler.
 */

void tap_write(UINT tck, UINT tms, UINT tdi, UINT trst)
{
	if(tck)
	{
		if(!trst)
			tap_state = TAP_TEST_LOGIC_RESET;
		else
			tap_state = tap_next_state[tap_state][tms];

		tap_tdo = 0;

		switch(tap_state)
		{
		case TAP_CAPTURE_DR:
			break;

		case TAP_SHIFT_DR:
			break;

		case TAP_UPDATE_DR:
			break;

		case TAP_CAPTURE_IR:
			break;

		case TAP_SHIFT_IR:
			break;

		case TAP_UPDATE_IR:
			break;
		}
	}
}

/*
 * UINT tap_read(void);
 *
 * JTAG TAP read port handler.
 */

UINT tap_read(void){

	return(tap_tdo);
}

void tap_reset(void){

	tap_state = TAP_TEST_LOGIC_RESET;

	tap_ireg_shift = 0;
	tap_dreg_shift = 0;

	memset(tap_ireg, 0, sizeof(UINT32)*2);
	memset(tap_dreg, 0, sizeof(UINT32)*385);
}
