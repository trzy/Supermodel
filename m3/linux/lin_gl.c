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
 * linux/lin_gl.c
 *
 * Linux OpenGL support.
 */

#include "model3.h"
#include "osd_gl.h"

/*
 * void osd_gl_set_mode(BOOL fullscreen, UINT xres, UINT yres);
 *
 * Called by osd_renderer_set_mode() to set the rendering mode.
 *
 * Parameters:
 *      fullscreen = Non-zero if a full-screen mode is desired, 0 for a
 *                   windowed mode.
 *      xres       = Horizontal resolution in pixels.
 *      yres       = Vertical resolution.
 */

void osd_gl_set_mode(BOOL fullscreen, UINT xres, UINT yres)
{
}

/*
 * void osd_gl_unset_mode(void);
 *
 * Called by osd_renderer_unset_mode() to "unsets" the current mode meaning it
 * releases the rendering and device contexts.
 */

void osd_gl_unset_mode(void)
{
}

/*
 * void osd_gl_swap_buffers(void);
 *
 * Swaps the buffers to display what has been rendered in the last frame.
 * Called by osd_renderer_update_frame().
 */

void osd_gl_swap_buffers(void)
{
    SDL_GL_SwapBuffers();
}

