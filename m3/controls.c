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

//TODO: In controls_write(): Is EEPROM only written when bank_select = 1?
//      This makes little sense because banking affects reg 4 only, right?

/*
 * controls.c
 *
 * Control emulation.
 */

/*
 * Lost World Gun Notes:
 * ---------------------
 *
 *  MCU Command Reg = 0x24 (byte)
 *  MCU Arg Reg     = 0x28 (byte)
 *
 *  Procedure for issuing commands:
 *      1. Arg written first, then command.
 *      2. Waits on reg 0x34 (see Stefano's post to model3 group for details.)
 *      3. Reads a byte from 0x2C, then a byte from 0x30.
 *
 *  Command 0x00: Set address (in arg reg.)
 *  Command 0x87: Read gun data.
 *
 * Virtual On 2 Notes:
 * -------------------
 *
 *  Control Area Offset 0x08:   Lever 1, LRUD??TS (T = Turbo, S = Shot Trigger)
 *                      0x0C:   Lever 2
 *                      0x18:   Dipswitch, bits 7-0 = DS1-8
 *
 * Dipswitch bit 0x20 (DS3) should be cleared otherwise some test is performed
 * by Virtual On 2 at boot-up.
 */

#include "model3.h"

/******************************************************************/
/* Privates                                                       */
/******************************************************************/

/*
 * Register Set
 */

static UINT8    controls_reg[0x40];
static UINT8    controls_reg_bank[2];
static BOOL     controls_bank_select;

/*
 * Gun Data
 *
 * Gun data is read by communicating with the MCU. An address is passed and
 * some data is read back. The total number of data bytes that the MCU stores
 * and can address is unknown, but I assume 16 (Stefano: Is this correct?)
 *
 * Here is how gun data is stored:
 *
 * Offset 0:    player 1 gun X position, lower 8 bits
 *        1:    player 1 gun X position, upper 3 bits
 *        2:    player 1 gun Y position, lower 8 bits
 *        3:    player 1 gun Y position, upper 3 bits
 *        4:    player 2 gun X position, lower 8 bits
 *        5:    player 2 gun X position, upper 3 bits
 *        6:    player 2 gun Y position, lower 8 bits
 *        7:    player 2 gun Y position, upper 3 bits
 *        8:    bit 0 = player 1 gun position status (0=aquired 1=lost)
 *              bit 1 = player 2 gun position status (0=aquired 1=lost)
 *
 * I think Stefano also said that the X and Y may be reversed and that the gun
 * coordinates are only 10 bits, not 11.
 */

static UINT8    gun_data[16];
static INT      gun_data_ptr;   // which address within gun_data[]


static UINT8	steering_data[8];
static INT		steering_control = 0;	// Currently used register
/*
 * Handler for Analogue Controls
 */

static UINT32 (* controls_analogue_handler);

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

void controls_shutdown(void)
{

}

void controls_init(void)
{

}

void controls_reset(UINT32 flags)
{
    memset(controls_reg_bank, 0xFF, 2);
    memset(controls_reg, 0xFF, 0x40);
    memset(controls_reg, 0x00, 8);

//    controls_reg[0x18] = 0xDF;  // clear bit 0x20 for Virtual On 2
    controls_reg[0x18] = 0x00;

    /*
     * NOTE: There will be problems if multiple flags are specified
     */

#if 0
    if((flags & GAME_OWN_STEERING_WHEEL))
        controls_analogue_handler = controls_wheel_handler;
    else if((flags & GAME_OWN_LIGHT_GUN))
        controls_analogue_handler = controls_gun_handler;
	else
        controls_analogue_handler = controls_nop_handler;
#endif
}

void controls_update(void)
{
    OSD_CONTROLS    *c;
    UINT            gun_x, gun_y;

    c = osd_input_update_controls();
    
    controls_reg_bank[0] = c->system_controls[0];
    controls_reg_bank[1] = c->system_controls[1];

    controls_reg[0x08] = c->game_controls[0];
    controls_reg[0x0C] = c->game_controls[1];

    /*
     * TEMP: This should be changed so that only games with guns have this
     * updated
     */

    /*
     * Gun Coordinates:
     *
     * Coordinates returned by the MCU are sight position coordinates. The OSD
     * input code returns coordinates in the range of (0,0)-(485,383).
     *
     * X adjustment: 400 must be added to the sight position in order to
     * locate the gun at screen X = 0.
     *
     * Y adjustment: Sight coordinate 0 maps to screen coordinate 272. Sight
     * coordinate 272 maps to screen coordinate 0. Sight coordinate 383 maps
     * to -111.
     */

    gun_y = c->gun_y[0];
//    if (gun_y <= 272)
//        gun_y = 272 - gun_y;
//    else
    gun_x = c->gun_x[0];// + 400;
    gun_data[0] = gun_y & 0xFF;
    gun_data[1] = (gun_y >> 8) & 7;
    gun_data[2] = gun_x & 0xFF;
    gun_data[3] = (gun_x >> 8) & 7;

    gun_y = c->gun_y[1];
    gun_x = c->gun_x[1] + 400;
    gun_data[4] = gun_y & 0xFF;
    gun_data[5] = (gun_y >> 8) & 7;
    gun_data[6] = gun_x & 0xFF;
    gun_data[7] = (gun_x >> 8) & 7;

    gun_data[8] = (!!c->gun_acquired[0]) | ((!!c->gun_acquired[1]) << 1);

	// Steering Wheel controls
	// Register 0: Steering wheel, 0x80 is center
	// Register 1: Acceleration, 0x00 - 0xFF
	// Register 2: Brake, 0x00 - 0xFF
	// Registers 3 - 7 are either unused or unknown

	steering_data[0] = (c->steering + 0x80) & 0xFF;
	steering_data[1] = c->acceleration & 0xFF;
	steering_data[2] = c->brake & 0xFF;
}

/*
 * void controls_save_state(FILE *fp);
 *
 * Saves the state of the controls to a file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void controls_save_state(FILE *fp)
{
    fwrite(controls_reg, sizeof(UINT8), 0x40, fp);
    fwrite(controls_reg_bank, sizeof(UINT8), 2, fp);
    fwrite(&controls_bank_select, sizeof(BOOL), 1, fp);
}

/*
 * void controls_load_state(FILE *fp);
 *
 * Loads the state of the controls from a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void controls_load_state(FILE *fp)
{
    fread(controls_reg, sizeof(UINT8), 0x40, fp);
    fread(controls_reg_bank, sizeof(UINT8), 2, fp);
    fread(&controls_bank_select, sizeof(BOOL), 1, fp);
}

/******************************************************************/
/* Access                                                         */
/******************************************************************/

/* these are only 8-bit wide, byte-aligned */
/* additional data/address adjustement must be performed by the main handler */

UINT8 controls_read(UINT32 a)
{
    a &= 0x3F;

    switch (a)
    {
    case 0x4:
        // should this only be done when bank_select = 1?
        controls_reg_bank[1] &= ~0x20;  // clear data bit before ORing it in
        controls_reg_bank[1] |= ((eeprom_read() & 1) << 5) ;
        return controls_reg_bank[controls_bank_select];
	case 0x3C:
		// I don't know if the register to be read can be selected.
		steering_control++;

		return steering_data[(steering_control - 1) & 0x7];
    }

//    LOG("model3.log", "CTRL %02X READ\n", a);

    return controls_reg[a];
}

void controls_write(UINT32 a, UINT8 d)
{
    a &= 0x3F;
    controls_reg[a] = d;

	switch(a)
	{
	case 0x0:
        controls_bank_select = d & 1;
        if(controls_bank_select == 1)
        {
        	eeprom_write(
				(d & 0x40) ? 1 : 0,
				(d & 0x80) ? 1 : 0,
				(d & 0x20) ? 1 : 0,
				(d & 0x10) ? 1 : 0
			);
        }
        break;
    case 0x24:  // MCU command
        switch (d)
        {
        case 0x00:  // set address for gun data (it's already in the arg reg)
            gun_data_ptr = controls_reg[0x28] & 0xF;
            break;
        case 0x87:  // read data back from gun
            controls_reg[0x2c] = 0; // unused?
            controls_reg[0x30] = gun_data[gun_data_ptr];
            break;
        }
        break;
	}

//    LOG("model3.log", "CTRL%02X = %02X\n", a, d);
}

