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
 * rtc.h
 *
 * Real-Time Clock (72421) header.
 */

#ifndef INCLUDED_RTC_H
#define INCLUDED_RTC_H

extern void rtc_init(void);
extern void rtc_shutdown(void);
extern void rtc_reset(void);
extern void rtc_step_frame(void);

extern void rtc_save_state(FILE *);
extern void rtc_load_state(FILE *);

extern UINT8 rtc_read_8(UINT32 a);
extern UINT32 rtc_read_32(UINT32 a);
extern void rtc_write(UINT32 a, UINT8 d);

#endif  // INCLUDED_RTC_H

