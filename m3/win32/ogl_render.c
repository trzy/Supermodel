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
 * win32/ogl_render.c
 *
 * Win32 OpenGL renderer.
 */

#include "model3.h"

#define BPP 16

/*
 * Layers
 *
 * Each layer is a 512x512 RGBA texture.
 */

static GLubyte  layer[4][512][512][4];  // 4MB of RAM, ouch!
static GLuint   layer_texture[4];       // IDs for the 4 layer textures

/*
 * Window Context
 */

static HGLRC        hrc = 0;            // OpenGL rendering context
static HDC          hdc = 0;            // GDI device context
extern HWND         main_window;        // window handle
static HINSTANCE    hinstance;          // application instance

/*
 * is_extension_supported():
 *
 * Returns true if the OpenGL extension is supported.
 */

static BOOL is_extension_supported(CHAR *extname)
{
    CHAR    *p, *end;
    INT     extname_len, n;

    p = (CHAR *) glGetString(GL_EXTENSIONS);
    if (p == 0)
        return 0;
    extname_len = strlen(extname);
    end = p + strlen(p);

    while (p < end)
    {
        n = strcspn(p, " ");
        if (extname_len == n)
        {
            if (strncmp(extname, p, n) == 0)
                return 1;
        }
        p += (n + 1);
    }

    return 0;
}

/*
 * init_gl():
 *
 * OpenGL initialization.
 *
 * NOTE: Textures should probably be created here for the layers.
 */

static void init_gl(void)
{
    INT i;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glShadeModel(GL_SMOOTH);
    glClearColor(0.0, 0.0, 0.0, 0.0);   // background color	
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // nicer perspective view

    /*
     * Check extensions
     */

    if (!is_extension_supported("GL_ARB_texture_mirrored_repeat"))
        osd_error("Your OpenGL implementation does not support mirrored texture repeating!");

    /*
     * Create the 2D layer textures
     */

    glGenTextures(4, layer_texture);

    for (i = 0; i < 4; i++) // set up properties for each texture
    {
        glBindTexture(GL_TEXTURE_2D, layer_texture[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, &layer[i][0][0][0]);
    }

    /*
     * Set viewport size. We don't set up any projection matrix stuff because
     * that must be done on a per-frame update basis
     */

    glViewport(0, 0, 496, 384); //TODO: replace with actual res

    /*
     * Configure the tile generator
     */

    tilegen_set_layer_format(32, 0, 8, 16, 24);
}

/*
 * void osd_renderer_init(UINT8 *culling_ram_8e_ptr,
 *                        UINT8 *culling_ram_8c_ptr, UINT8 *polygon_ram_ptr,
 *                        UINT8 *texture_ram_ptr, UINT8 *vrom_ptr);
 *
 * Initializes the renderer. It assumes a window has already been created.
 * Real3D memory regions are passed to it.
 *
 * Parameters:
 *      culling_ram_8e_ptr = Pointer to Real3D culling RAM at 0x8E000000.
 *      culling_ram_8c_ptr = Pointer to Real3D culling RAM at 0x8C000000.
 *      polygon_ram_ptr    = Pointer to Real3D polygon RAM.
 *      texture_ram_ptr    = Pointer to Real3D texture RAM.
 *      vrom_ptr           = Pointer to VROM.
 */

void osd_renderer_init(UINT8 *culling_ram_8e_ptr, UINT8 *culling_ram_8c_ptr,
                       UINT8 *polygon_ram_ptr, UINT8 *texture_ram_ptr,
                       UINT8 *vrom_ptr)
{
	GLuint							pixel_format;
	static PIXELFORMATDESCRIPTOR	pfd =			// must this be static?
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_TYPE_RGBA,
		0,											// bits per pixel, set this up later!!!
		0, 0, 0, 0, 0, 0,							// color bits ignored
		0,											// no alpha buffer
		0,											// ignore shift bit
		0,											// no accumulation buffer
		0, 0, 0, 0,									// accumulation bits ignored
		16,											// 16-bit z-buffer
		0,											// no stencil buffer
		0,											// no auxilliary buffer
		PFD_MAIN_PLANE,								// main drawing layer
		0,											// reserved
		0, 0, 0										// layer masks ignored
	};

    /*
     * Call osd_renderer_shutdown() when program quits
     */

    atexit(osd_renderer_shutdown);

    /*
     * Color depth is hard-coded to BPP for now -- it's really all we need
     * for Model 3
     */

    pfd.cColorBits = BPP;

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
	
    init_gl();

    /*
     * Initialize the OS-independent rendering engine
     */

    r3dgl_init(culling_ram_8e_ptr, culling_ram_8c_ptr, polygon_ram_ptr, texture_ram_ptr, vrom_ptr);
}

void osd_renderer_shutdown(void)
{
    r3dgl_shutdown();

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

void osd_renderer_reset(void)
{

}

/*
 * set_ortho():
 *
 * Set up an orthogonal projection with corners: (0,0), (1,0), (1, 1), (0,1),
 * in clockwise order and set up a viewport.
 */

static void set_ortho(void)
{
    glViewport(0, 0, 496, 384); //TODO: replace with actual res

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    gluOrtho2D(0.0, 1.0, 1.0, 0.0);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

/*
 * render_layer():
 *
 * Renders one of the 2D layers at the specified Z.
 */

static void render_layer(INT layer, GLfloat z)
{
    glBindTexture(GL_TEXTURE_2D, layer_texture[layer]);
    glBegin(GL_QUADS);  // NOTE: glBindTexture() must come before this :(

    glTexCoord2f(0.0, 0.0);                     glVertex3f(0.0, 0.0, z);
    glTexCoord2f(496.0 / 512.0, 0.0);           glVertex3f(1.0, 0.0, z);
    glTexCoord2f(496.0 / 512.0, 384.0 / 512.0); glVertex3f(1.0, 1.0, z);
    glTexCoord2f(0.0, 384.0 / 512.0);           glVertex3f(0.0, 1.0, z);

    glEnd();
}

/*
 * void osd_renderer_update_frame(void);
 *
 * Renders the entire frame.
 */

void osd_renderer_update_frame(void)
{
    INT     i;
    GLfloat z;

    /*
     * Clear the depth and color buffers and use the texture replace mode
     * (meaning: paint the textures on the poly)
     */

    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    /*
     * Render 2 back layers
     */

    for (i = 2; i < 4; i++)
    {
        glBindTexture(GL_TEXTURE_2D, layer_texture[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, &layer[i][0][0][0]);
    }

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    set_ortho();
    glEnable(GL_BLEND); // enable blending, an alpha of 0xFF is opaque, 0 is transparent
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    render_layer(3, -1.00);
    render_layer(2, -0.75);
    glDisable(GL_BLEND);

	/*
	 * Render Real3D scene
	 */

    r3dgl_update_frame();

	/*
	 * Render 2 front layers
	 */

    for (i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, layer_texture[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, &layer[i][0][0][0]);
    }

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    set_ortho();
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    render_layer(1, -0.50);
    render_layer(0, -0.25);
    glDisable(GL_BLEND);

    /*
     * glFlush() may not be needed and it may be advantageous to SwapBuffers()
     * prior to running a frame
     *
     * glFlush() can also be placed before any rendering is done to increase
     * the chances that the GPU is already done rendering, so no time is
     * wasted on glFlush()
     */

    glDisable(GL_TEXTURE_2D);
    glFlush();
    SwapBuffers(hdc);
}

UINT osd_renderer_get_layer_depth(void)
{
    return 0;
}

void osd_renderer_get_layer_buffer(UINT layer_num, UINT8 **buffer, UINT *pitch)
{
    *pitch = 512;   // currently all layer textures are 512x512
    *buffer = (UINT8 *) &layer[layer_num][0][0][0];
}

void osd_renderer_free_layer_buffer(UINT layer_num)
{
}
