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
 * tilegen.h
 *
 * Tilemap generator header.
 */

#ifndef INCLUDED_TILEGEN_H
#define INCLUDED_TILEGEN_H

extern void     tilegen_update(void);

extern UINT32   tilegen_vram_read_32(UINT32);
extern void     tilegen_vram_write_32(UINT32, UINT32);

extern UINT32   tilegen_read_32(UINT32);
extern void     tilegen_write_32(UINT32, UINT32);

extern void     tilegen_save_state(FILE *);
extern void     tilegen_load_state(FILE *);

extern BOOL     tilegen_set_layer_format(INT, UINT, UINT, UINT, UINT);
extern void     tilegen_reset(void);
extern void     tilegen_init(UINT8 *);
extern void     tilegen_shutdown(void);

#endif  // INCLUDED_TILEGEN_H

