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
 * Linux/SDL OpenGL support. All of this will have to be rewritten to support
 * run-time mode changes and fullscreen modes.
 *
 * The only osd_renderer function implemented here is osd_renderer_blit().
 */

#include "model3.h"
#include "osd_common/osd_gl.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

/*
 * void win_gl_init(UINT xres, UINT yres);
 *
 * Sets the rendering mode and initializes OpenGL. 
 *
 * Parameters:
 *      xres       = Horizontal resolution in pixels.
 *      yres       = Vertical resolution.
 */

void lin_gl_init(UINT xres, UINT yres)
{
    /*
     * Check for mirrored texture repeat extension
     */

    if (osd_gl_check_extension("GL_ARB_texture_mirrored_repeat"))
        osd_error("Your OpenGL implementation does not support mirrored texture repeating!");
    if (osd_gl_check_extension("GL_ARB_texture_env_combine"))
        osd_error("Your OpenGL implementation does not support texture combiner operations!");

    /*
     * Initialize GL engine
     */

    osd_gl_set_mode(xres, yres);       
}

/*
 * void win_gl_shutdown(void);
 *
 * Shuts down the rendering mode mode meaning it releases the rendering and
 * device contexts.
 */

void lin_gl_shutdown(void)
{
    osd_gl_unset_mode();
}

/*
 * void osd_renderer_blit(void);
 *
 * Swaps the buffers to display what has been rendered in the last frame.
 */

void osd_renderer_blit(void)
{
    SDL_GL_SwapBuffers();
}

/*
 * void * osd_gl_get_proc_address(const char *);
 *
 * Calls wglGetProcAddress.
 */

void * osd_gl_get_proc_address(const CHAR * id)
{
	void * ptr = glXGetProcAddressARB(id);

	if (ptr == NULL)
		error("GL proc %s not found!\n", id);
	else
		message(0, "found GL proc %s!", id);

	return ptr;
}

