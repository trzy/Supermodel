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
 * rtc.c
 *
 * Real-Time Clock (72421) emulation.
 */

#include "model3.h"

static UINT8 rtc_step = 0;
static UINT8 rtc_reg[0x10]; /* 4-bit wide */

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

void rtc_init(void)
{

}

void rtc_shutdown(void)
{

}

void rtc_reset(void)
{
	rtc_step = 0;

	memset(rtc_reg, 0, 0x10);
}

void rtc_step_frame(void)
{

}

/*
 * void rtc_save_state(FILE *fp);
 *
 * Saves the state of the real-time clock to a file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void rtc_save_state(FILE *fp)
{
    // do nothing yet because this file isn't complete or used, it appears
}

/*
 * void rtc_load_state(FILE *fp);
 *
 * Loads the state of the real-time clock from a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void rtc_load_state(FILE *fp)
{
    // do nothing yet because this file isn't complete or used, it appears
}

/******************************************************************/
/* Access                                                         */
/******************************************************************/

UINT8 rtc_read(UINT32 a)
{
	message(0, "RTC: read %08X", a);

	a &= 0xF;

	return(rtc_reg[a]);
}

void rtc_write(UINT32 a, UINT8 d)
{
	message(0, "RTC: write %08X = %X", a, d);

	a &= 0xF;

	rtc_reg[a] = d;
}
