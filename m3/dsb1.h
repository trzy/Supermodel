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
 * dsb1.h
 *
 * DSB1 Digital Sound Board header.
 */

#ifndef _DSB1_H_
#define _DSB1_H_

extern void dsb1_init(void);
extern void dsb1_shutdown(void);
extern void dsb1_reset(void);

extern void dsb1_save_state(FILE *);
extern void dsb1_load_state(FILE *);

#endif /* _DSB1_H_ */

