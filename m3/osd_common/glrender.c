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
 * osd_common/glrender.c
 *
 * OpenGL renderer backend. All OS-independent GL code is located here.
 * osd_renderer_blit() is the only function not defined here.
 *
 * TODO: glFlush() ?
 */

#include "model3.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>   // this one can be obtained freely from SGI


/******************************************************************/
/* Private Data                                                   */
/******************************************************************/

/*
 * Memory Regions
 *
 * These will be passed to us before rendering begins.
 */

static UINT8    *culling_ram_8e;    // pointer to Real3D culling RAM
static UINT8    *culling_ram_8c;    // pointer to Real3D culling RAM
static UINT8    *polygon_ram;       // pointer to Real3D polygon RAM
static UINT8    *texture_ram;       // pointer to Real3D texture RAM
static UINT8    *vrom;              // pointer to VROM

/*
 * Texture Mapping
 *
 * The smallest Model 3 textures are 32x32 and the total VRAM texture sheet
 * is 2048x2048. Dividings this by 32 gives us 64x64. Each element contains an
 * OpenGL ID for a texture. Because of 4bpp textures, 4 entries per texture
 * must be maintained.
 */

static GLint    texture_grid[64*64*4];      // 0 indicates unused
static GLbyte   texture_buffer[512*512*4];  // for 1 texture

/*
 * Mipmapping
 *
 * The bottom right of each 2048x1024 texture page contains the smallest
 * mipmaps and the top-left has full-sized textures. These tables are used
 * to determine where in a texture page a given mipmap is.
 */

static UINT     mip_x[12] = {    0, 1024, 1536, 1792, 1920, 1984,
                              2016, 2032, 2040, 2044, 2046, 2047
                            };
static UINT     mip_y[12] = {    0,  512,  768,  896,  960,  992,
                              1008, 1016, 1020, 1022, 1023,  0
                            };
static UINT     mip_scale[7] = { 1, 2, 4, 8, 16, 32, 64 };

/*
 * Layers
 *
 * Each layer is a 512x512 RGBA texture.
 */

static GLubyte  layer[4][512][512][4];  // 4MB of RAM, ouch!
static GLuint   layer_texture[4];       // IDs for the 4 layer textures

/*
 * Resolution and Ratios
 *
 * The ratio of the OpenGL physical resolution to the Model 3 resolution is
 * pre-calculated and passed to us via osd_gl_set_mode().
 */

static UINT     xres, yres;
static float    xres_ratio, yres_ratio;

/*
 * Vertex and Texture Coordinate Configuration
 *
 * Vertices are in a 17.15 format on Step 1.0 and 13.19 on everything else.
 * Texture coordinates can be configured on a per-polygon basis (13.3, 16.0.)
 */

static float    vertex_divisor, texcoord_divisor;

/******************************************************************/
/* Macros                                                         */
/******************************************************************/

/*
 * Polygon Header Data Extraction
 *
 * The parameter is a 7-element array of 32-bit words (the 7-word header.)
 */

#define IS_QUAD(h)                  (h[0] & 0x40)
#define IS_UV_16(h)                 (h[1] & 0x40)
#define IS_TEXTURE_ENABLED(h)       (h[6] & 0x04000000)
#define IS_TEXTURE_TRANSPARENT(h)   (h[6] & 0x80000000)
#define GET_LINK_DATA(h)            (h[0] & 0x0F)
#define COLOR_RED(h)                ((h[4] >> 24) & 0xFF)
#define COLOR_GREEN(h)              ((h[4] >> 16) & 0xFF)
#define COLOR_BLUE(h)               ((h[4] >> 8) & 0xFF)
#define GET_TEXTURE_X(h)            ((((h[4] & 0x1F) << 1) | ((h[5] & 0x80) >> 7)) * 32)
#define GET_TEXTURE_Y(h)            (((h[5] & 0x1F) | ((h[4] & 0x40) >> 1)) * 32)
#define GET_TEXTURE_WIDTH(h)        (32 << ((h[3] >> 3) & 7))
#define GET_TEXTURE_HEIGHT(h)       (32 << ((h[3] >> 0) & 7))
#define GET_TEXTURE_FORMAT(h)       ((h[6] >> 7) & 7)
#define GET_TEXTURE_REPEAT(h)       (h[2] & 3)
 
/******************************************************************/
/* Texture Management                                             */
/******************************************************************/

/*
 * void osd_renderer_invalidate_textures(UINT x, UINT y, UINT w, UINT h);
 *
 * Invalidates all textures that are within the rectangle in texture memory
 * defined by the parameters.
 *
 * Parameters:
 *      x = Texture pixel X coordinate in texture memory.
 *      y = Y coordinate.
 *      w = Width of rectangle in pixels.
 *      h = Height.
 */

void osd_renderer_invalidate_textures(UINT x, UINT y, UINT w, UINT h)
{
    UINT    yi;

    x /= 32;
    y /= 32;
    w /= 32;
    h /= 32;

//TODO: do we need to use w*4 here to take care of planes?
    for (yi = 0; yi < h; yi++)
    {
        glDeleteTextures(w, &texture_grid[(yi + y) * (64*4) + x]);
        memset((UINT8 *) &texture_grid[(yi + y) * (64*4) + x], 0, w * sizeof(GLint));
    }
}

/*
 * decode_texture():
 *
 * Decodes a single texture into texture_buffer[].
 */

static void decode_texture(UINT x, UINT y, UINT w, UINT h, UINT format)
{
    UINT    xi, yi;
    UINT16  rgb16;
    UINT8   gray8;

    /*
     * Formats:
     *
     *      0 = 16-bit A1RGB5
     *      1 = 4-bit grayscale, field 0x000F
     *      2 = 4-bit grayscale, field 0x00F0
     *      3 = 4-bit grayscale, field 0x0F00
     *      4 = 8-bit A4L4
     *      5 = 8-bit grayscale
     *      6 = 4-bit grayscale, field 0xF000 (?)
     *      7 = RGBA4
     */

    switch (format)
	{
    case 0: // 16-bit, A1RGB5
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
                texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 10) & 0x1F) << 3;
	            texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >> 5) & 0x1F) << 3;
                texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >> 0) & 0x1F) << 3;
                texture_buffer[((yi * w) + xi) * 4 + 3] = (rgb16 & 0x8000) ? 0 : 0xFF;
	        }
	    }
		break;
    case 1: // 4-bit grayscale
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
                texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 0) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >> 0) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >> 0) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 3] = (GLbyte) 0xFF;
	        }
	    }
		break;
    case 2: // 4-bit grayscale
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
                texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 3] = (GLbyte) 0xFF;
	        }
	    }
		break;
    case 3: // 4-bit grayscale
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
                texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 8) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >> 8) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >> 8) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 3] = (GLbyte) 0xFF;
	        }
	    }
		break;
    case 4: // 8-bit, A4L4
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi / 2)) * 2];
                gray8 = (rgb16 >> (!(xi & 1) * 8)) & 0xFF;
                texture_buffer[((yi * w) + (xi ^ 1)) * 4 + 0] = ~(gray8 & 0x0F) << 4;
                texture_buffer[((yi * w) + (xi ^ 1)) * 4 + 1] = ~(gray8 & 0x0F) << 4;
                texture_buffer[((yi * w) + (xi ^ 1)) * 4 + 2] = ~(gray8 & 0x0F) << 4;
                texture_buffer[((yi * w) + (xi ^ 1)) * 4 + 3] = (GLbyte) (gray8 & 0xF0); // fixme
	        }
	    }
		break;
    case 5: // 8-bit grayscale
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi / 2)) * 2];
                gray8 = (rgb16 >> (!(xi & 1) * 8)) & 0xFF;
                texture_buffer[((yi * w) + (xi ^ 1)) * 4 + 0] = gray8;
                texture_buffer[((yi * w) + (xi ^ 1)) * 4 + 1] = gray8;
                texture_buffer[((yi * w) + (xi ^ 1)) * 4 + 2] = gray8;
                texture_buffer[((yi * w) + (xi ^ 1)) * 4 + 3] = (GLbyte) 0xFF;
	        }
	    }
		break;
    case 6: // 4-bit grayscale
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
                texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 12) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >> 12) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >> 12) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 3] = (GLbyte) 0xFF;
	        }
	    }
		break;
    case 7: // 16-bit, RGBA4
		for (yi = 0; yi < h; yi++)
		{
			for (xi = 0; xi < w; xi++)
			{
       			rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
				texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 12) & 0xF) << 4;
				texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >>  8) & 0xF) << 4;
				texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >>  4) & 0xF) << 4;
				texture_buffer[((yi * w) + xi) * 4 + 3] = ((rgb16 >>  0) & 0xF) << 4;
			}
		}
		break;
	}
}

/*
 * bind_texture():
 *
 * Given the coordinates of a texture and its size within the texture sheet,
 * an OpenGL texture is created (along with its mipmaps) and uploaded. The
 * texture will also be selected so that the caller may use it.
 *
 * The format is just bits 9 and 8 of polygon header word 7. The repeat mode
 * is bits 0 and 1 of word 2. The texture Y coordinate includes the page.
 */

static void bind_texture(UINT x, UINT y, UINT w, UINT h, UINT format, UINT rep_mode)
{
    UINT    plane, page, num_mips, i, mx, my, mwidth, mheight;
    GLint   tex_id;

    if (w > 512 || h > 512) // error!
    {
        LOG("texture.log", "%d,%d %dx%d\n", x, y, w, h);
        return;
    }

    /*
     * Although we treat texture memory as a 2048x2048 buffer, it's actually
     * split into 2 2048x1024 pages. We must extract this from the Y
     * coordinate.
     */

    page = y & 0x400;   // page = 1024 or 0, can be added to Y coordinate
    y &= 0x3FF;         // Y coordinate within page

    /*
     * The lesser of the 2 dimensions determines the number of mipmaps that
     * are defined. A dimension (width, height) divided by a mip_scale[]
     * element yields the dimension of the mipmap.
     */

    switch (min(w, h))
    {
    case 32:    num_mips = 3; break;
    case 64:    num_mips = 4; break;
    case 128:   num_mips = 5; break;
    case 256:   num_mips = 6; break;
    defaul:		num_mips = 7; break;
    }

    /*
     * The texture grid is a 64x64 array where each element corresponds to
     * a 32x32 texture in texture RAM (2048x2048 pixels total.) Because 4-bit
     * textures are stored like 16-bit textures with 4 different fields,
     * we need 4 "planes" per element in the texture grid, because 1 32x32
     * texture slot may actually contain 4 different 4-bit textures.
     */

    switch (format) // determine which plane for 4bpp textures
    {
    case 1:     plane = 3; break;
    case 2:     plane = 2; break;
    case 3:     plane = 1; break;
    default:    plane = 0; break;
    }

    /*
     * If the texture is already cached, bind it and we're done
     */

    tex_id = texture_grid[((y + page) / 32) * (64*4) + (x / 32) + plane];
    if (tex_id != 0)    // already exists, bind and exit
    {
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (rep_mode & 2) ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (rep_mode & 1) ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT);
        return;
    }

    /*
     * Decode the texture (all mipmaps), bind it, upload it, and mark it in
     * the grid as used
     */

    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (rep_mode & 2) ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (rep_mode & 1) ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, num_mips - 1);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, (GLfloat) num_mips - 1);
    
    for (i = 0; i < num_mips; i++)
    {       
        mx      = mip_x[i] + x / mip_scale[i];
        my      = mip_y[i] + y / mip_scale[i];
        mwidth  = w / mip_scale[i];
        mheight = h / mip_scale[i];

        decode_texture(mx, my + page, mwidth, mheight, format);
        glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA8, mwidth, mheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_buffer);
    }

    texture_grid[((y + page) / 32) * (64*4) + (x / 32) + plane] = tex_id;    // mark texture as used
}

/******************************************************************/
/* Model Drawing                                                  */
/******************************************************************/

/*
 * convert_vertex_to_float():
 *
 * Converts from a fixed-point vertex to a float. Accepts a signed INT32.
 */

static float convert_vertex_to_float(INT32 num)
{
    return (float) num / vertex_divisor;
}

/*
 * convert_texcoord_to_float():
 *
 * Converts a texture coordinate into a floating point number.
 */

static float convert_texcoord_to_float(UINT32 num)
{
    return (float) num / texcoord_divisor;
}

/*
 * get_word_little():
 *
 * Fetches a 32-bit word in little-endian format.
 */

static UINT32 get_word_little(UINT32 *a)
{
    return *a;
}

/*
 * get_word_big():
 *
 * Fetches a 32-bit word in big-endian format.
 */

static UINT32 get_word_big(UINT32 *a)
{
    return BSWAP32(*a);
}

/*
 * void osd_renderer_draw_model(UINT32 *mdl, UINT32 addr, BOOL little_endian);
 *
 * Draws a model.
 *
 * Parameters:
 *      mdl           = Pointer to model to draw.
 *      addr          = Real3D address (for debugging purposes, in case it
 *                      needs to be printed.
 *      little_endian = True if memory is in little endian format.
 */

void osd_renderer_draw_model(UINT32 *mdl, UINT32 addr, BOOL little_endian)
{
    struct
    {
        GLfloat x, y, z;    // vertices
        UINT32  uv;         // texture coordinates
    }       v[4], prev_v[4];
    UINT32  (*get_word)(UINT32 *);
    UINT32  header[7];
    UINT    link_data, texture_width, texture_height;
    INT     i, j, num_verts;
    GLfloat u_coord, v_coord, nx, ny, nz, color[4];

#if WIREFRAME
    glPolygonMode(GL_FRONT, GL_LINE);
    glPolygonMode(GL_BACK, GL_LINE);
    glDisable(GL_TEXTURE_2D);
    glColor3ub(0xFF, 0xFF, 0xFF);
#endif
    if (little_endian)  // set appropriate read handler
        get_word = get_word_little;
    else
        get_word = get_word_big;

    do
    {
        /*
         * Fetch the 7 header words
         */

        for (i = 0; i < 7; i++)
            header[i] = get_word(mdl++);

        /*
         * Get normals
         */

        nx = (GLfloat) (((INT32) header[1]) >> 8) / 4194304.0f;
        ny = (GLfloat) (((INT32) header[2]) >> 8) / 4194304.0f;
        nz = (GLfloat) (((INT32) header[3]) >> 8) / 4194304.0f;

        /*
         * Fetch the all of the vertices
         */

        num_verts = IS_QUAD(header) ? 4 : 3;
        link_data = GET_LINK_DATA(header);

        i = 0;
        for (j = 0; j < 4; j++) // fetch all previous vertices that we need
        {
            if ((link_data & 1))
                v[i++] = prev_v[j];
            link_data >>= 1;
        }

        for ( ; i < num_verts; i++) // fetch remaining vertices
        {
            v[i].x = convert_vertex_to_float(get_word(mdl++));
            v[i].y = convert_vertex_to_float(get_word(mdl++));
            v[i].z = convert_vertex_to_float(get_word(mdl++));
            v[i].uv = get_word(mdl++);
        }

        /*
         * Set color and material properties
         */

        color[0] = (GLfloat) COLOR_RED(header) / 255.0f;
        color[1] = (GLfloat) COLOR_GREEN(header) / 255.0f;
        color[2] = (GLfloat) COLOR_BLUE(header) / 255.0f;
        color[3] = 1.0f;    // TODO: if translucent polygon, modify this
        glColor4fv(color);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);

        // TODO: specular

        /*
         * Draw it (and save the vertices to prev_v[])
         */

        texture_width   = GET_TEXTURE_WIDTH(header);
        texture_height  = GET_TEXTURE_HEIGHT(header);

        if (IS_TEXTURE_ENABLED(header))
        {
            bind_texture(GET_TEXTURE_X(header), GET_TEXTURE_Y(header),
                         texture_width, texture_height,
                         GET_TEXTURE_FORMAT(header), GET_TEXTURE_REPEAT(header));
            if (IS_TEXTURE_TRANSPARENT(header))
            {
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0.95f);
            }                
        }
        else
            glDisable(GL_TEXTURE_2D);

        if (IS_UV_16(header))
            texcoord_divisor = 1.0f;    // 16.0 texture coordinates
        else
            texcoord_divisor = 8.0f;    // 13.3

        glBegin((num_verts == 4) ? GL_QUADS : GL_TRIANGLES);
        for (i = 0; i < num_verts; i++)
        {
            prev_v[i] = v[i];
            u_coord = convert_texcoord_to_float(v[i].uv >> 16);
            v_coord = convert_texcoord_to_float(v[i].uv & 0xFFFF);
            glNormal3f(nx, ny, nz);
            glTexCoord2f(u_coord / (GLfloat) texture_width, v_coord / (GLfloat) texture_height);
            glVertex3f(v[i].x, v[i].y, v[i].z);
        }
        glEnd();

        if (IS_TEXTURE_ENABLED(header))
            glDisable(GL_ALPHA_TEST);
        else
            glEnable(GL_TEXTURE_2D);
    }
    while (!(header[1] & 4));   // continue until stop bit is hit
}

/******************************************************************/
/* Lighting                                                       */
/******************************************************************/

/*
 * void osd_renderer_set_light(INT light_num, LIGHT *light);
 *
 * Sets a light.
 *
 * Parameters:
 *      light_num = Which light number. 
 *      light     = Light data.
 */

void osd_renderer_set_light(INT light_num, LIGHT *light)
{
    GLfloat v[4];

    switch (light->type)
    {
    case LIGHT_PARALLEL:
        v[0] = -light->u;
        v[1] = light->v;
        v[2] = light->w;
        v[3] = 0.0f;    // this is a directional light
        glLightfv(GL_LIGHT0 + light_num, GL_POSITION, v);

        v[0] = v[1] = v[2] = light->diffuse_intensity;      // R, G, B
        v[3] = 1.0f;
        glLightfv(GL_LIGHT0 + light_num, GL_DIFFUSE, v);

        v[0] = v[1] = v[2] = light->ambient_intensity;      // R, G, B
        v[3] = 1.0f;
        glLightfv(GL_LIGHT0 + light_num, GL_AMBIENT, v);

        break;
    default:
        error("Unhandled light type: %d", light->type);
        break;
    }

    glEnable(GL_LIGHT0 + light_num);
}

/******************************************************************/
/* Viewport and Projection                                        */
/******************************************************************/

/*
 * void osd_renderer_set_coordinate_system(const MATRIX m);
 *
 * Applies the coordinate system matrix and makes adjustments so that the
 * Model 3 coordinate system is properly handled.
 *
 * Parameters:
 *      m = Matrix.
 */

void osd_renderer_set_coordinate_system(const MATRIX m)
{
    glLoadIdentity();
    glScalef(1.0, -1.0, -1.0);
    glMultMatrixf(m);
}

/*
 * void osd_renderer_set_viewport(const VIEWPORT *vp);
 *
 * Sets up a viewport. Enables Z-buffering.
 *
 * Parameters:
 *      vp = Viewport and projection parameters.
 */

void osd_renderer_set_viewport(const VIEWPORT *vp)
{
    glViewport((UINT) ((float) vp->x * xres_ratio),
               (UINT) ((float) (384.0f - (vp->y + vp->height)) * yres_ratio),
               (GLint) (vp->width * xres_ratio),
               (GLint) (vp->height * yres_ratio));

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(vp->up + vp->down,
                   (GLfloat) vp->width / (GLfloat) vp->height,
                   0.1, 1000000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

/******************************************************************/
/* Matrix Stack                                                   */
/*                                                                */
/* Matrices are stored in OpenGL's column major format so they    */
/* can be passed directly to OpenGL matrix functions.             */
/******************************************************************/

/*
 * void osd_renderer_multiply_matrix(MATRIX m);
 *
 * Multiplies the top of the matrix stack by the specified matrix
 *
 * Parameters:
 *      m = Matrix to multiply.
 */

void osd_renderer_multiply_matrix(MATRIX m)
{
    glMultMatrixf(m);
}

/*
 * void osd_renderer_translate_matrix(float x, float y, float z);
 *
 * Translates the top of the matrix stack.
 *
 * Parameters:
 *      x = Translation along X axis.
 *      y = Y axis.
 *      z = Z axis.
 */

void osd_renderer_translate_matrix(float x, float y, float z)
{
    glTranslatef(x, y, z);
}

/*
 * void osd_renderer_push_matrix(void);
 *
 * Pushes a matrix on to the stack. The matrix pushed is the former top of the
 * stack.
 */

void osd_renderer_push_matrix()
{
    glPushMatrix();
}

/*
 * void osd_renderer_pop_matrix(void);
 *
 * Pops a matrix off the top of the stack.
 */

void osd_renderer_pop_matrix(void)
{
    glPopMatrix();
}

/******************************************************************/
/* Misc. State Management                                         */
/******************************************************************/

/*
 * void osd_renderer_begin(void);
 *
 * Called just before rendering begins for the current frame. Does nothing.
 */

void osd_renderer_begin(void)
{
}

/*
 * void osd_renderer_end(void);
 *
 * Called just after rendering ends for the current frame. Does nothing.
 */

void osd_renderer_end(void)
{
}

/*
 * void osd_renderer_clear(BOOL fbuf, BOOL zbuf);
 *
 * Clears the frame and/or Z-buffer.
 *
 * Parameters:
 *      fbuf = If true, framebuffer (color buffer) is cleared.
 *      zbuf = If true, Z-buffer is cleared.
 */

void osd_renderer_clear(BOOL fbuf, BOOL zbuf)
{
    GLbitfield  buffers = 0;

    if (fbuf)   buffers |= GL_COLOR_BUFFER_BIT;
    if (zbuf)   buffers |= GL_DEPTH_BUFFER_BIT;

    glClear(buffers);
}

/******************************************************************/
/* Tile Layer Interface                                           */
/******************************************************************/

static BOOL		coff_enabled = FALSE;
static GLfloat	coff[3];

void osd_renderer_set_color_offset(BOOL is_enabled,
								   FLOAT32 r,
								   FLOAT32 g,
								   FLOAT32 b)
{
	if((coff_enabled = is_enabled) != FALSE)
	{
		coff[0] = r;
		coff[1] = g;
		coff[2] = b;
	}
}

/*
 * void osd_renderer_draw_layer(UINT layer_num);
 *
 * Draws a layer. Disables Z-buffering and creates an orthogonal projection.
 *
 * Parameters:
 *      layer_num = Layer to draw.
 */

void osd_renderer_draw_layer(UINT layer_num)
{
	GLfloat	temp[3];	// combiner color

    /*
     * Disable lighting and set the replace texture mode
     */

    glDisable(GL_LIGHTING);

	if(!coff_enabled)	// no color offset
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
	else
	{
		/*
		 * The color combiner operations is set as follows.
		 *
		 * final_color = texture_color ± primary_color
		 * final_alpha = texture_alpha
		 *
		 * OpenGL's color combiner doesn't allow specification of individual
		 * color component operations (to my knowledge) -- Stefano.
		 */

		// setup color combiner
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);

		if(coff[0] > 0.0f && coff[1] > 0.0f && coff[2] > 0.0f)
		{
			// set combiner mode to ADD
			glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_ADD);
			temp[0] = coff[0];
			temp[1] = coff[1];
			temp[2] = coff[2];
		}
		else if(coff[0] < 0.0f && coff[1] < 0.0f && coff[2] < 0.0f)
		{
			// set combiner mode to SUB
			glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_SUBTRACT);
			temp[0] = -coff[0];
			temp[1] = -coff[1];
			temp[2] = -coff[2];
		}
		else
			error("osd_renderer_draw_layer: non-uniform color offset\n");

		// setup color combiner operation
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);

		// setup alpha combiner operation
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0f);

		// setup primary fragment color
		glColor3fv(temp);
	}

    /*
     * Set orthogonal projection and disable Z-buffering and lighting
     */

    glDisable(GL_DEPTH_TEST);

    glViewport(0, 0, xres, yres);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    gluOrtho2D(0.0, 1.0, 1.0, 0.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

    /*
     * Draw the texture
     */

    glEnable(GL_BLEND); // enable blending, an alpha of 0xFF is opaque, 0 is transparent
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, layer_texture[layer_num]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);                     glVertex3f(0.0, 0.0, 0.0);
    glTexCoord2f(496.0 / 512.0, 0.0);           glVertex3f(1.0, 0.0, 0.0);
    glTexCoord2f(496.0 / 512.0, 384.0 / 512.0); glVertex3f(1.0, 1.0, 0.0);
    glTexCoord2f(0.0, 384.0 / 512.0);           glVertex3f(0.0, 1.0, 0.0);
    glEnd();

    glDisable(GL_BLEND);

    /*
     * Re-enable lighting
     */

    glEnable(GL_LIGHTING);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
 * void osd_renderer_get_layer_buffer(UINT layer_num, UINT8 **buffer,
 *                                    UINT *pitch);
 *
 * Obtains a layer buffer for the caller (the tilegen) to render to.
 *
 * Parameters:
 *      layer_num = The layer number (0-3.)
 *      buffer    = A pointer to the buffer pointer to set to the layer
 *                  memory.
 *      pitch     = Width of layer buffer in pixels will be written to this
 *                  pointer.
 */

void osd_renderer_get_layer_buffer(UINT layer_num, UINT8 **buffer, UINT *pitch)
{
    *pitch = 512;   // currently all layer textures are 512x512
    *buffer = (UINT8 *) &layer[layer_num][0][0][0];
}

/*
 * void osd_renderer_free_layer_buffer(UINT layer_num);
 *
 * Releases the layer buffer. It is not actually freed, this is primarily for
 * APIs that need an "unlock" call after drawing to a texture is done. Here,
 * we use it to upload the layer texture.
 *
 * Parameters:
 *      layer_num = Layer number.
 */

void osd_renderer_free_layer_buffer(UINT layer_num)
{
    glBindTexture(GL_TEXTURE_2D, layer_texture[layer_num]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, &layer[layer_num][0][0][0]);
}

/******************************************************************/
/* Initialization and Set Up                                      */
/******************************************************************/

/*
 * void osd_renderer_set_memory(UINT8 *culling_ram_8e_ptr,
 *                              UINT8 *culling_ram_8c_ptr,
 *                              UINT8 *polygon_ram_ptr,
 *                              UINT8 *texture_ram_ptr,
 *                              UINT8 *vrom_ptr);
 *
 * Receives the Real3D memory regions.
 *
 * Currently, this function checks the Model 3 stepping and configures the
 * renderer appropriately.
 *
 * Parameters:
 *      culling_ram_8e_ptr = Pointer to Real3D culling RAM at 0x8E000000.
 *      culling_ram_8c_ptr = Pointer to Real3D culling RAM at 0x8C000000.
 *      polygon_ram_ptr    = Pointer to Real3D polygon RAM.
 *      texture_ram_ptr    = Pointer to Real3D texture RAM.
 *      vrom_ptr           = Pointer to VROM.
 */

void osd_renderer_set_memory(UINT8 *culling_ram_8e_ptr,
                             UINT8 *culling_ram_8c_ptr,
                             UINT8 *polygon_ram_ptr, UINT8 *texture_ram_ptr,
                             UINT8 *vrom_ptr)
{
    culling_ram_8e = culling_ram_8e_ptr;
    culling_ram_8c = culling_ram_8c_ptr;
    polygon_ram = polygon_ram_ptr;
    texture_ram = texture_ram_ptr;
    vrom = vrom_ptr;

    if (m3_config.step == 0x10)
        vertex_divisor = 32768.0f;  // 17.15-format vertices
    else
        vertex_divisor = 524288.0f; // 13.19
}

/*
 * BOOL osd_gl_check_extension(CHAR *extname);
 *
 * Checks if an extension is supported.
 *
 * Parameters:
 *      extname = String containing the extension name.
 *
 * Returns:
 *      Non-zero if the extension is not supported.
 */

BOOL osd_gl_check_extension(CHAR *extname)
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
                return 0;
        }
        p += (n + 1);
    }

    return 1;   // not supported
}

/*
 * void osd_gl_set_mode(UINT new_xres, UINT new_yres);
 *
 * Receives the OpenGL physical resolution and reconfigures appropriately.
 * Also generates layer textures and configures some miscellaneous stuff.
 *
 * NOTE: Do NOT call this a second time unless you call osd_gl_unset_mode()
 * first.
 *
 * Parameters:
 *      xres = X resolution in pixels.
 *      yres = Y resolution in pixels.
 */

void osd_gl_set_mode(UINT new_xres, UINT new_yres)
{
    const GLfloat   zero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    INT             i;

    osd_renderer_invalidate_textures(0, 0, 2048, 2048);

    xres = new_xres;
    yres = new_yres;
    xres_ratio = (float) xres / 496.0f; // Model 3 resolution is 496x384
    yres_ratio = (float) yres / 384.0f;

    /*
     * Misc. init
     */

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glShadeModel(GL_SMOOTH);
    glClearColor(0.0, 0.0, 0.0, 0.0);   // background color 
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // nicer perspective view

    glClearDepth(1.0);

    /*
     * Disable default ambient lighting and enable lighting
     */
    
    glEnable(GL_LIGHTING);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);

    /*
     * Enable 2D texture mapping, GL_MODULATE is for the 3D scene, layer
     * rendering will have to use GL_REPLACE.
     */

    glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

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
     * Configure the tile generator
     */

    tilegen_set_layer_format(32, 0, 8, 16, 24);

    /*
     * Set vertex format
     */

    if (m3_config.step == 0x10)
        vertex_divisor = 32768.0f;  // 17.15-format vertices
    else
        vertex_divisor = 524288.0f; // 13.19
}

/*
 * void osd_gl_unset_mode(void);
 *
 * "Unsets" the current mode -- frees textures. osd_gl_set_mode() must be
 * called before the renderer is used again.
 */

void osd_gl_unset_mode(void)
{
    osd_renderer_invalidate_textures(0, 0, 2048, 2048);
    glDeleteTextures(4, layer_texture);
}
