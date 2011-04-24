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
 * render.h
 *
 * Rendering engine header. OSD renderers will want to include this because
 * some important data structures are defined here.
 */

#ifndef INCLUDED_RENDER_H
#define INCLUDED_RENDER_H

/******************************************************************/
/* Data Structures                                                */
/******************************************************************/

/*
 * MATRIX
 *
 * A 4x4 matrix stored in column-major (OpenGL) format.
 */

typedef float   MATRIX [4*4];

/*
 * VIEWPORT
 *
 * Viewport and projection data. The field-of-view angles are in degrees. The
 * coordinates and size of the viewport are truncated from Model 3's fixed-
 * point formats to integers.
 *
 * The viewport coordinate system maps directly to the Model 3's physical
 * resolution and its origin is at the bottom-left corner of the screen.
 */

typedef struct
{
    UINT    x, y, width, height;    // viewport position and size.
    double  up, down, left, right;  // FOV angles from center
} VIEWPORT;

/*
 * LIGHT
 */

enum {
	LIGHT_PARALLEL,
	LIGHT_POSITIONAL,
	LIGHT_SPOT
};
	
typedef struct
{
	UINT type;
	float u,v,w;		// Direction vector
	float x,y,z;		// Position for point light
	float diffuse_intensity;
	float ambient_intensity;
	UINT32 color;
} LIGHT;

/******************************************************************/
/* Functions                                                      */
/******************************************************************/

extern void render_frame(void);
extern void render_init(UINT8 *, UINT8 *, UINT8 *, UINT8 *, UINT8 *);
extern void render_shutdown(void);

#endif  // INCLUDED_RENDER_H
