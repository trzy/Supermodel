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
 * win32/win_gl.c
 *
 * Win32 OpenGL support. All of this will have to be rewritten to support
 * run-time mode changes and fullscreen modes.
 *
 * The only osd_renderer function implemented here is osd_renderer_blit().
 */

#include "model3.h"
#include "osd_common/osd_gl.h"
#include <gl\gl.h>
#include <gl\glu.h>
#include <gl\glext.h>

/*
 * Window Context
 */

static HGLRC        hrc = 0;            // OpenGL rendering context
static HDC          hdc = 0;            // GDI device context
static HINSTANCE    hinstance;          // application instance
extern HWND         main_window;        // window handle

/*
 * void win_gl_init(UINT xres, UINT yres);
 *
 * Sets the rendering mode and initializes OpenGL. 
 *
 * Parameters:
 *      xres       = Horizontal resolution in pixels.
 *      yres       = Vertical resolution.
 */

void win_gl_init(UINT xres, UINT yres)
{
	GLuint							pixel_format;
	static PIXELFORMATDESCRIPTOR	pfd =			// must this be static?
	{
        sizeof(PIXELFORMATDESCRIPTOR),              // size of structure
        1,                                          // version (must be 1)
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,                              // RGBA pixels
        32,                                         // color depth
		0, 0, 0, 0, 0, 0,							// color bits ignored
		0,											// no alpha buffer
		0,											// ignore shift bit
		0,											// no accumulation buffer
		0, 0, 0, 0,									// accumulation bits ignored
        24,                                         // 24-bit z-buffer
        0,                                          // no stencil buffer
		0,											// no auxilliary buffer
		PFD_MAIN_PLANE,								// main drawing layer
		0,											// reserved
		0, 0, 0										// layer masks ignored
	};

    /*
     * Get window instance
     */

	hinstance = GetModuleHandle(NULL);	// get window instance

	/*
	 * Get device context and find a pixel format
	 */

    if (!(hdc = GetDC(main_window)))
        osd_error("Unable to create an OpenGL device context.");

	if (!(pixel_format = ChoosePixelFormat(hdc, &pfd)))	// find matching pixel format
        osd_error("Unable to find the required pixel format.");

	if (!SetPixelFormat(hdc, pixel_format, &pfd))
        osd_error("Unable to set the required pixel format.");

	/*
     * Get the rendering context and perform some GL initialization
	 */

	if (!(hrc = wglCreateContext(hdc)))
        osd_error("Unable to create an OpenGL rendering context.");

	if (!wglMakeCurrent(hdc, hrc))
        osd_error("Unable to activate the OpenGL rendering context.");

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

void win_gl_shutdown(void)
{
    osd_gl_unset_mode();

	/*
	 * If there is a rendering context, release it
	 */

	if (hrc)
	{
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hrc);
	}

	/*
	 * If there is a device context, release it
	 */

	if (hdc)
        ReleaseDC(main_window, hdc);
}

/*
 * void osd_renderer_blit(void);
 *
 * Swaps the buffers to display what has been rendered in the last frame.
 */

void osd_renderer_blit(void)
{
    SwapBuffers(hdc);
}
