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
 * r3dgl.h
 *
 * Header for the OS-independent OpenGL rendering engine.
 */

#ifndef INCLUDED_R3DGL_H
#define INCLUDED_R3DGL_H

extern void r3dgl_update_frame(void);
extern void r3dgl_set_resolution(UINT, UINT);
extern void r3dgl_init(UINT8 *, UINT8 *, UINT8 *, UINT8 *, UINT8 *);
extern void r3dgl_shutdown(void);

#endif  // INCLUDED_R3DGL_H
