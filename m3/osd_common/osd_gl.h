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
 * osd_common/osd_gl.h
 *
 * Header file which defines the OSD OpenGL interface.
 */

#ifndef INCLUDED_OSD_COMMON_OSD_GL_H
#define INCLUDED_OSD_COMMON_OSD_GL_H

/******************************************************************/
/* OSD OpenGL Functions                                           */
/******************************************************************/

extern void osd_gl_swap_buffers(void);
extern void osd_gl_set_mode(BOOL, UINT, UINT);
extern void osd_gl_unset_mode(void);

#endif  // INCLUDED_OSD_COMMON_OSD_GL_H

