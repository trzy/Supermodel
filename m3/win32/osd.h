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
 * osd.h
 *
 * OS-dependent stuff. This header is visible to all modules of the emulator
 * and covers the GUI, renderer, sound, input, as well as OS-specific
 * definitions and typedefs required by the emulator.
 */

#ifndef INCLUDED_OSD_H
#define INCLUDED_OSD_H

/******************************************************************/
/* OSD Includes                                                   */
/******************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef RENDERER_D3D
#include <d3d9.h>
#include <d3dx9.h>
#else // RENDERER_GL
#include <gl\gl.h>
#include <gl\glu.h>
#endif 
#include <dinput.h>

#include "osd_types.h"  // provides fundamental data types

/******************************************************************/
/* OSD Definitions                                                */
/******************************************************************/

#define INLINE static __inline__

/******************************************************************/
/* OSD Data Structures                                            */
/******************************************************************/

/*
 * OSD_CONTROLS Structure
 *
 * Holds the current state of the controls. Filled by the input code and used
 * by the control emulation code.
 */

typedef struct
{
    /*
     * Common to all games
     */

    UINT8   system_controls[2]; // maps directly to Fx040004 banks 0 and 1
    UINT8   game_controls[2];   // map directly to Fx040008 and Fx04000C

    /*
     * For games with guns
     *
     * The gun positions are reported in screen coordinates. The emulator will
     * make the appropriate adjustments. Gun coordinates should range from
     * (0,0), the upper-left corner, to (495,383), the lower-right corner.
     */

    UINT    gun_x[2], gun_y[2]; // gun positions for players 1 (0) and 2 (1)
    BOOL    gun_acquired[2];    // gun acquired status for players 1 and 2
                                // 0 = acquired, 1 = lost
} OSD_CONTROLS;

/******************************************************************/
/* OSD GUI                                                        */
/******************************************************************/

extern void osd_main(void);

extern void osd_message();
extern void osd_error();

/******************************************************************/
/* Renderer                                                       */
/******************************************************************/

extern void osd_renderer_init(UINT8 *, UINT8 *, UINT8 *, UINT8 *);
extern void osd_renderer_shutdown(void);
extern void osd_renderer_reset(void);
extern void osd_renderer_update_frame(void);
extern UINT osd_renderer_get_layer_depth(void);
extern void osd_renderer_get_layer_buffer(UINT, UINT8 **, UINT *);
extern void osd_renderer_free_layer_buffer(UINT);
extern void osd_renderer_upload_texture(UINT32, UINT32, UINT8 *, BOOL);

/******************************************************************/
/* Sound Output                                                   */
/******************************************************************/

/******************************************************************/
/* Input                                                           */
/******************************************************************/

extern OSD_CONTROLS * osd_input_update_controls(void);

#endif  // INCLUDED_OSD_H

