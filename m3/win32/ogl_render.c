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
#include <gl\gl.h>
#include <gl\glu.h>
#include <gl\glaux.h>

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


static void draw_rect(INT i, UINT x, UINT y, UINT w, UINT h, GLubyte r, GLubyte g, GLubyte b)
{
    UINT    xi, yi;

    for (yi = y; yi < (y + h); yi++)
    {
        for (xi = x; xi < (x + w); xi++)
        {
            layer[i][yi][xi][0] = r;
            layer[i][yi][xi][1] = g;
            layer[i][yi][xi][2] = b;
            layer[i][yi][xi][3] = 0xff; // opaque
        }
    }
}

static void generate_textures(void)
{
    draw_rect(0, 0, 10, 496, 10, 0xf8, 0x00, 0x00);
    draw_rect(1, 10, 0, 10, 384, 0x00, 0xff, 0x00);
    draw_rect(2, 0, 30, 496, 10, 0x00, 0x00, 0xff);
    draw_rect(3, 30, 0, 10, 384, 0xff, 0xff, 0x00);
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

    generate_textures();

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glShadeModel(GL_SMOOTH);
	
    glClearColor(0.0, 0.0, 0.0, 0.0);   // background color
	
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // nicer perspective view

    /*
     * Enable Z-buffering
     */

//    glClearDepth(1.0);
//    glEnable(GL_DEPTH_TEST);
//    glDepthFunc(GL_LEQUAL);

    /*
     * Enable 2D texturing
     */

//    glEnable(GL_TEXTURE_2D);

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
 *                        UINT8 *vrom_ptr);
 *
 * Initializes the renderer. It assumes a window has already been created.
 * Real3D memory regions are passed to it.
 *
 * Parameters:
 *      culling_ram_8e_ptr = Pointer to Real3D culling RAM at 0x8E000000.
 *      culling_ram_8c_ptr = Pointer to Real3D culling RAM at 0x8C000000.
 *      polygon_ram_ptr    = Pointer to Real3D polygon RAM.
 *      vrom_ptr           = Pointer to VROM.
 */

void osd_renderer_init(UINT8 *culling_ram_8e_ptr, UINT8 *culling_ram_8c_ptr,
                       UINT8 *polygon_ram_ptr, UINT8 *vrom_ptr)
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

    r3dgl_init(culling_ram_8e_ptr, culling_ram_8c_ptr, polygon_ram_ptr, vrom_ptr);
}

void osd_renderer_shutdown(void)
{
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

void osd_renderer_update_frame(void)
{
    INT     i;
    GLfloat z;

    /*
     * Clear the depth buffer
     */

    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    /*
     * Set up an orthogonal projection with corners: (0,0), (1,0), (1, 1),
     * (0,1), in clockwise order
     */

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    gluOrtho2D(0.0, 1.0, 1.0, 0.0);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

    /*
     * Enable blending -- the layers will have the alpha channel set to either
     * 0xFF (opaque) for solid colors, or 0 for transparencies
     */
  
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /*
     * Use the texture replace mode (meaning: paint the textures on the poly)
     */

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    /*
     * Render 4 layers back to front
     */
		
//TODO: perhaps create separate buffers that are 496x384 for the layers if
// this saves space and 1 512x512 texture just to initialize
    for (i = 0; i < 4; i++)
    {
        glBindTexture(GL_TEXTURE_2D, layer_texture[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, &layer[i][0][0][0]);
    }

    z = -1.0;
    for (i = 3; i >= 0; i--)
    {
        glBindTexture(GL_TEXTURE_2D, layer_texture[i]);
        glBegin(GL_QUADS);  // NOTE: glBindTexture() must come before this :(

        glTexCoord2f(0.0, 0.0);                     glVertex3f(0.0, 0.0, z);
        glTexCoord2f(496.0 / 512.0, 0.0);           glVertex3f(1.0, 0.0, z);
        glTexCoord2f(496.0 / 512.0, 384.0 / 512.0); glVertex3f(1.0, 1.0, z);
        glTexCoord2f(0.0, 384.0 / 512.0);           glVertex3f(0.0, 1.0, z);

        glEnd();

        z += 0.25;
    }

    /*
     * Disable blending
     */

	glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    /*
     * Draw 3D scene
     */

    r3dgl_update_frame();

    /*
     * glFlush() may not be needed and it may be advantageous to SwapBuffers()
     * prior to running a frame
     *
     * glFlush() can also be placed before any rendering is done to increase
     * the chances that the GPU is already done rendering, so no time is
     * wasted on glFlush()
     */

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

/*
 * void osd_renderer_upload_texture(UINT32 header, UINT32 length, UINT8 *src,
 *                                  BOOL little_endian);
 *
 * Uploads a Model 3 texture. 
 *
 * Parameters:
 *      header        = Header word containing size and position information.
 *      length        = Header word containing length information.
 *      src           = Pointer to the texture image data (no header words.)
 *      little_endian = True if image data is in little endian format.
 */

void osd_renderer_upload_texture(UINT32 header, UINT32 length, UINT8 *src,
                                 BOOL little_endian)
{
    r3dgl_upload_texture(header, length, src, little_endian);
}
