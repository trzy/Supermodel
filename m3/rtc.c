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
#include <time.h>

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

static UINT8 get_register(int reg)
{
	time_t current_time;
	struct tm* tms;

	time( &current_time );
	tms = localtime( &current_time );

	switch(reg)
	{
		case 0:		// 1-second digit
			return (tms->tm_sec % 10) & 0xF;
			break;
		case 1:		// 10-seconds digit
			return (tms->tm_sec / 10) & 0x7;
			break;
		case 2:		// 1-minute digit
			return (tms->tm_min % 10) & 0xF;
			break;
		case 3:		// 10-minute digit
			return (tms->tm_min / 10) & 0x7;
			break;
		case 4:		// 1-hour digit
			return (tms->tm_hour % 10) & 0xF;
			break;
		case 5:		// 10-hours digit
			return (tms->tm_hour / 10) & 0x7;
			break;
		case 6:		// 1-day digit (days in month)
			return (tms->tm_mday % 10) & 0xF;
			break;
		case 7:		// 10-days digit
			return (tms->tm_mday / 10) & 0x3;
			break;
		case 8:		// 1-month digit
			return ((tms->tm_mon + 1) % 10) & 0xF;
			break;
		case 9:		// 10-months digit
			return ((tms->tm_mon + 1) / 10) & 0x1;
			break;
		case 10:	// 1-year digit
			return (tms->tm_year % 10) & 0xF;
			break;
		case 11:	// 10-years digit
			return ((tms->tm_year % 100) / 10) & 0xF;
			break;
		case 12:	// day of the week
			return tms->tm_wday & 0x7;
			break;
		case 13:
			return 0;
			break;
		case 14:
			return 0;
			break;
		case 15:
			return 0;
			break;
	}
}

UINT8 rtc_read_8(UINT32 a)
{
	int address = (a >> 2) & 0xF;

	message(0, "RTC: read %08X, %08X", address,ppc_get_reg(PPC_REG_PC));

	return get_register( address );
}

UINT32 rtc_read_32(UINT32 a)
{
	int address = (a >> 2) & 0xF;

	message(0, "RTC: read %08X, %08X", address,ppc_get_reg(PPC_REG_PC));

	return get_register( address) << 24 | 0x30000;	// Bits 0x30000 set to get battery voltage check to pass
}

void rtc_write(UINT32 a, UINT8 d)
{
	a = (a >> 2) & 0xF;
	message(0, "RTC: write %08X = %X", a, d);

	a &= 0xF;

	rtc_reg[a] = d;
}
