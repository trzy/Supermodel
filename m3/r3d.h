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
 * r3d.h
 *
 * Real3D Model 3 graphics system header.
 */

#ifndef INCLUDED_R3D_H
#define INCLUDED_R3D_H

extern void     r3d_init(UINT8 *, UINT8 *, UINT8 *);
extern void     r3d_shutdown(void);
extern void     r3d_reset(void);

extern void     r3d_save_state(FILE *);
extern void     r3d_load_state(FILE *);

extern UINT32   r3d_read_32(UINT32 a);
extern void     r3d_write_32(UINT32 a, UINT32 d);

extern void     tap_reset(void);
extern UINT32   tap_read(void);
extern void     tap_write(UINT32 tck, UINT32 tms, UINT32 tdi, UINT32 trst);

#endif  // INCLUDED_R3D_H

