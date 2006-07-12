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
 * eeprom.h
 *
 * 93C46 EEPROM emulation header.
 */

#ifndef INCLUDED_EEPROM_H
#define INCLUDED_EEPROM_H

extern UINT8    eeprom_read_bit(void);
extern void     eeprom_set_clk(UINT8 clk);
extern void     eeprom_set_ce(UINT8 ce);
extern void     eeprom_set_data(UINT8 data);

extern void     eeprom_save_state(FILE *);
extern void     eeprom_load_state(FILE *);

extern INT      eeprom_load(CHAR *);
extern INT      eeprom_save(CHAR *);

extern void     eeprom_reset(void);

#endif  // INCLUDED_EEPROM_H
