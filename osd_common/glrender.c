
//#define WIREFRAME
// TODO: measure stats in VROM models (size of changing models, size of chunks, etc.)
// TODO: eventually use VBOs for model cache (update glext.h to OpenGL 1.5)
// TODO: cache dlists for per-model state changes
// TODO: align model cache structures to 4 bytes (use stride param) and profile
// TODO: move all config (i.e. USE_MODEL_CACHE, etc.) to makefile
// TODO: add specular lighting
// TODO: add light emission
// TODO: glFlush() ?

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
 */

#include "model3.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>   // this one can be obtained freely from SGI


/******************************************************************/
/* Configuration                                                  */
/******************************************************************/

// debug
//#define ANALYZE_VROM_MODELS

//#define USE_MODEL_CACHE             // controls VROM model cache
#define CACHE_POLYS_AS_QUADS		// QUADS should be a little faster

#define ENABLE_LIGHTING

/******************************************************************/
/* Private Data                                                   */
/******************************************************************/

/*
 * Memory Regions
 *
 * These will be passed to us before rendering begins.
 */

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

static GLubyte  * layer[4];
static GLuint   layer_texture[4];	// IDs for the 4 layer textures

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

typedef struct
{
	GLfloat x, y, z;    // vertices
	GLfloat nx, ny, nz; // normals
	GLfloat	r, g, b, a;	// colors
	UINT32  uv;         // texture coordinates
	GLfloat u, v;

} VERTEX;

/*
 * Model Cache
 */

#ifdef USE_MODEL_CACHE

typedef struct { GLfloat _0, _1; } VEC2;
typedef struct { GLfloat _0, _1, _2; } VEC3;
typedef struct { GLfloat _0, _1, _2, _3; } VEC4;

typedef struct
{
	UINT	ref_count;	// number of references
	UINT32	addr;		// original address
	UINT	vbo_id;		// vertex buffer object ID
	UINT	index;		// index in the vertex array
	UINT	num_verts;	// number of vertices

} HASH_ENTRY;

#define MODEL_CACHE_SIZE	(256*1024)	// ~150K seems to be enough
#define HASH_SIZE			(2048*1024)	// must be a power of two
#define HASH_FUNC(addr)		((addr >> 2) & (HASH_SIZE - 1))

static HASH_ENTRY * model_cache;
static UINT	cur_model_index;

static VEC3	* model_vertex_array;
static VEC3	* model_normal_array;
static VEC4	* model_color_array;
static VEC2	* model_texcoord_array;

PFNGLBINDBUFFERARBPROC				glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC			glDeleteBuffersARB = NULL;
PFNGLGENBUFFERSARBPROC				glGenBuffersARB = NULL;
PFNGLISBUFFERARBPROC				glIsBufferARB = NULL;
PFNGLBUFFERDATAARBPROC				glBufferDataARB = NULL;
PFNGLBUFFERSUBDATAARBPROC			glBufferSubDataARB = NULL;
PFNGLMAPBUFFERARBPROC				glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC				glUnmapBufferARB = NULL;
PFNGLGETBUFFERPARAMETERIVARBPROC	glGetBufferParameterivARB = NULL;
PFNGLGETBUFFERPOINTERVARBPROC		glGetBufferPointervARB = NULL;

#endif

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

void osd_renderer_invalidate_textures(UINT x, UINT y, int u, int v, UINT w, UINT h, UINT8 *texture, int miplevel)
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
    default:    num_mips = 7; break;
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
/* Model Caching and Drawing                                      */
/******************************************************************/

#define get_word(a)		(*a)

/*
 * Useful Polygon Header Macros
 *
 * The parameter is a 7-element array of 32-bit words (the 7-word header.)
 */

#define STOP_BIT(h)                 (h[1] & 0x04)
#define IS_QUAD(h)                  (h[0] & 0x40)
#define GET_LINK_DATA(h)            (h[0] & 0x0F)
#define IS_UV_16(h)                 (h[1] & 0x40)
#define IS_TEXTURE_ENABLED(h)       (h[6] & 0x04000000)
#define IS_TEXTURE_TRANSPARENT(h)   (h[6] & 0x80000000)
#define IS_LIGHTING_DISABLED(h)     (h[6] & 0x10000)
#define IS_OPAQUE(h)                (h[6] & 0x00800000)
#define COLOR_RED(h)                ((h[4] >> 24) & 0xFF)
#define COLOR_GREEN(h)              ((h[4] >> 16) & 0xFF)
#define COLOR_BLUE(h)               ((h[4] >> 8) & 0xFF)
#define GET_TEXTURE_X(h)            ((((h[4] & 0x1F) << 1) | ((h[5] & 0x80) >> 7)) * 32)
#define GET_TEXTURE_Y(h)            (((h[5] & 0x1F) | ((h[4] & 0x40) >> 1)) * 32)
#define GET_TEXTURE_WIDTH(h)        (32 << ((h[3] >> 3) & 7))
#define GET_TEXTURE_HEIGHT(h)       (32 << ((h[3] >> 0) & 7))
#define GET_TEXTURE_FORMAT(h)       ((h[6] >> 7) & 7)
#define GET_TEXTURE_REPEAT(h)       (h[2] & 3)
#define GET_TRANSLUCENCY(h)         ((h[6] >> 18) & 0x1F)

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
 * Model Cache
 */ 

#ifdef USE_MODEL_CACHE

static BOOL init_model_cache(void)
{
	UINT i;

	// get pointer to VBO interface

	if(glBindBufferARB == NULL)
	{
		glBindBufferARB = (PFNGLBINDBUFFERARBPROC)
			osd_gl_get_proc_address("glBindBufferARB");
		glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)
			osd_gl_get_proc_address("glDeleteBuffersARB");
		glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)
			osd_gl_get_proc_address("glGenBuffersARB");
		glIsBufferARB = (PFNGLISBUFFERARBPROC)
			osd_gl_get_proc_address("glIsBufferARB");
		glBufferDataARB = (PFNGLBUFFERDATAARBPROC)
			osd_gl_get_proc_address("glBufferDataARB");
		glBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)
			osd_gl_get_proc_address("glBufferSubDataARB");
		glMapBufferARB = (PFNGLMAPBUFFERARBPROC)
			osd_gl_get_proc_address("glMapBufferARB");
		glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)
			osd_gl_get_proc_address("glUnmapBufferARB");
		glGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC)
			osd_gl_get_proc_address("glGetBufferParameterivARB");
		glGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC)
			osd_gl_get_proc_address("glGetBufferPointervARB");
	}

	// free any resident VBO

	/*
	for(i = 0; i < HASH_SIZE; i++);
		if(glIsBufferARB(model_cache[i].vbo_id));
			glDeleteBuffersARB(1, &model_cache[i].vbo_id);
	*/

	// free unfreed buffers

	SAFE_FREE(model_cache);
	SAFE_FREE(model_vertex_array);
	SAFE_FREE(model_normal_array);
	SAFE_FREE(model_color_array);
	SAFE_FREE(model_texcoord_array);

	// alloc buffers

	model_cache = (HASH_ENTRY *)malloc(HASH_SIZE * sizeof(HASH_ENTRY));
	model_vertex_array = (VEC3 *)malloc(MODEL_CACHE_SIZE * sizeof(VEC3));
	model_normal_array = (VEC3 *)malloc(MODEL_CACHE_SIZE * sizeof(VEC3));
	model_color_array = (VEC4 *)malloc(MODEL_CACHE_SIZE * sizeof(VEC4));
	model_texcoord_array = (VEC2 *)malloc(MODEL_CACHE_SIZE * sizeof(VEC2));

	// check buffer consistency

	if(	(model_cache == NULL) ||
		(model_vertex_array == NULL) ||
		(model_normal_array == NULL) ||
		(model_color_array == NULL) ||
		(model_texcoord_array == NULL) )
		return FALSE;

	// reset the model cache

	cur_model_index = 0;

	// setup the hash table

	for(i = 0; i < HASH_SIZE; i++)
	{
		model_cache[i].ref_count = 0;
		model_cache[i].addr = 0;
		model_cache[i].vbo_id = (UINT)-1;
		model_cache[i].index = 0;
		model_cache[i].num_verts = 0;
	}

	return TRUE;
}

INLINE void cache_vertex(UINT index, VERTEX * src)
{
	model_vertex_array[index]._0 = src->x;
	model_vertex_array[index]._1 = src->y;
	model_vertex_array[index]._2 = src->z;

	model_normal_array[index]._0 = src->nx;
	model_normal_array[index]._1 = src->ny;
	model_normal_array[index]._2 = src->nz;

	model_color_array[index]._0 = src->r;
	model_color_array[index]._1 = src->g;
	model_color_array[index]._2 = src->b;
	model_color_array[index]._3 = src->a;

	model_texcoord_array[index]._0 = src->u;
	model_texcoord_array[index]._1 = src->v;
}

UINT model_addr = 0;

static BOOL cache_model(UINT32 *m, HASH_ENTRY *h)
{
	VERTEX	v[4], prev_v[4];
    UINT	link_data, i, j, num_verts, index;
    GLfloat	texture_width = 1.0f, texture_height = 1.0f,
			nx, ny, nz, color[4];
	BOOL	end;

	UINT	has_tex = 0, old_tex_fmt = 0, old_tex_x = 0, old_tex_y = 0;
	GLfloat	old_tex_w = 0, old_tex_h = 0;

    if (*m == 0)
        return TRUE;

	if(cur_model_index >= MODEL_CACHE_SIZE)
		return FALSE;

	index = 0;	// number of cached vertices, actually

	h->index = cur_model_index;
	h->ref_count ++;

	#ifdef ANALYZE_VROM_MODELS
	message(0, "============================================================");
	#endif

    do
    {
		// setup polygon info

        num_verts = (m[0] & 0x40) ? 4 : 3;
        link_data = m[0] & 0xF;

		end = m[1] & 4;

        nx = (GLfloat) (((INT32) m[0]) >> 8) / 4194304.0f;
        ny = (GLfloat) (((INT32) m[2]) >> 8) / 4194304.0f;
        nz = (GLfloat) (((INT32) m[3]) >> 8) / 4194304.0f;

        color[0] = (GLfloat) COLOR_RED(m) / 255.0f;
        color[1] = (GLfloat) COLOR_GREEN(m) / 255.0f;
        color[2] = (GLfloat) COLOR_BLUE(m) / 255.0f;
        color[3] = 1.0f;

		#ifdef ANALYZE_VROM_MODELS

		if((IS_TEXTURE_ENABLED(m) ? 1 : 0) != has_tex)
			message(0, "model at %08X: has_tex %u --> %u", model_addr, has_tex, IS_TEXTURE_ENABLED(m) ? 1 : 0);
		has_tex = IS_TEXTURE_ENABLED(m) ? 1 : 0;

		if(IS_TEXTURE_ENABLED(m))
		{
			UINT texture_format, texture_x, texture_y;

			texture_width   = (GLfloat) GET_TEXTURE_WIDTH(m);
			texture_height  = (GLfloat) GET_TEXTURE_HEIGHT(m);
			texture_format	= GET_TEXTURE_FORMAT(m);

			texture_x		= GET_TEXTURE_X(m);
			texture_y		= GET_TEXTURE_Y(m);
			//IS_TEXTURE_TRANSPARENT(m);
			//GET_TEXTURE_REPEAT(m);

			if(old_tex_fmt != texture_format)
				message(0, "model at %08X: tex_fmt %u --> %u", model_addr, old_tex_fmt, texture_format);
			if(old_tex_x != texture_x)
				message(0, "model at %08X: tex_x %u --> %u", model_addr, old_tex_x, texture_x);
			if(old_tex_y != texture_y)
				message(0, "model at %08X: tex_y %u --> %u", model_addr, old_tex_y, texture_y);
			if(old_tex_w != texture_width)
				message(0, "model at %08X: tex_w %f --> %f", model_addr, old_tex_w, texture_width);
			if(old_tex_h != texture_height)
				message(0, "model at %08X: tex_h %f --> %f", model_addr, old_tex_h, texture_height);

			old_tex_fmt = texture_format;
			old_tex_x = texture_x;
			old_tex_y = texture_y;
			old_tex_w = texture_width;
			old_tex_h = texture_height;
		}

		#endif

		// select texture coordinate format

        if (IS_UV_16(m))
            texcoord_divisor = 1.0f;    // 16.0
        else
            texcoord_divisor = 8.0f;    // 13.3

		m += 7;

		// fetch all previous vertices that we need

        i = 0;
        for(j = 0; j < 4; j++)
        {
            if ((link_data & 1))
                v[i++] = prev_v[j];
            link_data >>= 1;
        }

		// fetch remaining vertices

        for( ; i < num_verts; i++)
        {
            v[i].x = convert_vertex_to_float(*m++);
            v[i].y = convert_vertex_to_float(*m++);
            v[i].z = convert_vertex_to_float(*m++);
            v[i].nx = nx;
            v[i].ny = ny;
            v[i].nz = nz;
			v[i].r = color[0];
			v[i].g = color[1];
			v[i].b = color[2];
			v[i].a = color[3];
			v[i].u = convert_texcoord_to_float((*m >> 16) / texture_width);
            v[i].v = convert_texcoord_to_float((*m & 0xFFFF) / texture_height);

			m++;
        }

		// save back old vertices

        for(i = 0; i < num_verts; i++)
            prev_v[i] = v[i];

		// cache all the vertices

		cache_vertex(cur_model_index + index++, &v[0]);
		cache_vertex(cur_model_index + index++, &v[1]);
		cache_vertex(cur_model_index + index++, &v[2]);

#ifdef CACHE_POLYS_AS_QUADS

		if(num_verts == 3)
			cache_vertex(cur_model_index + index++, &v[2]);
		else
			cache_vertex(cur_model_index + index++, &v[3]);
#else
		if(num_verts == 4)
		{
			cache_vertex(cur_model_index + index++, &v[0]);
			cache_vertex(cur_model_index + index++, &v[2]);
			cache_vertex(cur_model_index + index++, &v[3]);
		}
#endif
    }
    while(!end);

	h->num_verts = index;
	cur_model_index += index;

	return TRUE;
}

static BOOL draw_cached_model(UINT addr, UINT32 * m)
{
	HASH_ENTRY * h = &model_cache[HASH_FUNC(addr)];

	model_addr = addr;

	if(h->ref_count == 0)		// never referenced before, cache it
	{
		if(!cache_model(m, h))
		{
			error("draw_cached_model(): model cache overrun!\n");
			return FALSE;
		}
		h->addr = addr;
	}
	else						// already cached
	{
		if(h->addr != addr)		// same hash entry for different models
		{
			error("draw_cached_model(): hash table hit! (%08X)\n", addr);
			return FALSE;
		}
	}

	glDisable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
//	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

#ifdef CACHE_POLYS_AS_QUADS
	glDrawArrays(GL_QUADS, h->index, h->num_verts);
#else
	glDrawArrays(GL_TRIANGLES, h->index, h->num_verts);
#endif

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
//	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glEnable(GL_TEXTURE_2D);

	return TRUE;
}

#endif // USE_MODEL_CACHE

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
	VERTEX	v[4], prev_v[4];
    UINT32  header[7];
    UINT    link_data, texture_width = 1.0f, texture_height = 1.0f;
    INT     i, j, num_verts;
    GLfloat u_coord, v_coord, nx, ny, nz, color[4];

#ifdef WIREFRAME
    glPolygonMode(GL_FRONT, GL_LINE);
    glPolygonMode(GL_BACK, GL_LINE);
    glDisable(GL_TEXTURE_2D);
    glColor3ub(0xFF, 0xFF, 0xFF);
#endif

    //if (get_word(mdl) == 0)
    //    return;

	/*
	 * Draw VROM (static) models
	 */

#ifdef USE_MODEL_CACHE
	if(!little_endian)
	{
		if(draw_cached_model(addr, mdl))
			return;
		// if draw_cached_model() failed, draw the uncached model
	}
#endif

	/*
	 * Draw RAM (dynamic) models
	 */

    do
    {
        /*
         * Fetch the 7 header words
         */

        for (i = 0; i < 7; i++)
            header[i] = get_word(mdl++);

		if( header[6] == 0 )
			return;

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
            v[i].nx = nx;
            v[i].ny = ny;
            v[i].nz = nz;
            v[i].uv = get_word(mdl++);
        }

        /*
         * Set color and material properties.
         *
         * Something is hosed here. The Model 3 works differently. Setting
         * the ambient and diffuse material properties to all 1.0 fixes VON2
         * but causes issues in Scud Race (shadows turn white.)
         */

        color[0] = (GLfloat) COLOR_RED(header) / 255.0f;
        color[1] = (GLfloat) COLOR_GREEN(header) / 255.0f;
        color[2] = (GLfloat) COLOR_BLUE(header) / 255.0f;
        if (IS_OPAQUE(header))
            color[3] = 1.0f;    // TODO: if translucent polygon, modify this
        else
            color[3] = GET_TRANSLUCENCY(header) / 31.0f;

        glColor4fv(color); 
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
        {
            GLfloat v[4] = { 1.0,1.0,1.0,1.0 };
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, v);
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, v);
        }


#ifdef ENABLE_LIGHTING
        if (IS_LIGHTING_DISABLED(header))
            glDisable(GL_LIGHTING);
#endif

        /*
         * Draw it (and save the vertices to prev_v[])
         */

        if (IS_TEXTURE_ENABLED(header))
        {
			texture_width   = GET_TEXTURE_WIDTH(header);
			texture_height  = GET_TEXTURE_HEIGHT(header);

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
            glNormal3f(v[i].nx, v[i].ny, v[i].nz);
            glTexCoord2f(u_coord / (GLfloat) texture_width, v_coord / (GLfloat) texture_height);
            glVertex3f(v[i].x, v[i].y, v[i].z);
        }
        glEnd();

        if (IS_TEXTURE_ENABLED(header))
            glDisable(GL_ALPHA_TEST);
        else
            glEnable(GL_TEXTURE_2D);

#ifdef ENABLE_LIGHTING
        if (IS_LIGHTING_DISABLED(header))
            glEnable(GL_LIGHTING);
#endif
    }
    while (!STOP_BIT(header));  // continue until stop bit is hit
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
        v[0] = light->u;
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
    glScalef(1.0, -1.0, -1.0);  // Model 3 default coordinate system (for lighting)

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

void osd_renderer_begin_3d_scene(void)
{
}

/*
 * void osd_renderer_end(void);
 *
 * Called just after rendering ends for the current frame. Does nothing.
 */

void osd_renderer_end_3d_scene(void)
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
#if 0
        else
            error("osd_renderer_draw_layer: non-uniform color offset\n");
#endif

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

#ifdef ENABLE_LIGHTING
    glEnable(GL_LIGHTING);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif
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
    *buffer = (UINT8 *) layer[layer_num];
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
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, layer[layer_num]);
}

/*
 * void osd_renderer_draw_text(int x, int y, const char* string, DWORD color, BOOL shadow);
 *
 * Draws text on screen
 *
 * Parameters:
 *      x = Left X-coordinate of the text
 *		y = Top Y-coordinate of the text
 *		string = Text string
 *		color = Packed 24-bit RGBA color of the text
 *		shadow = if TRUE, draws a shadow behind the text
 */
void osd_renderer_draw_text(int x, int y, const char* string, DWORD color, BOOL shadow)
{
	// TODO: needs to be implemented
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

void osd_renderer_set_memory(UINT8 *polygon_ram_ptr, UINT8 *texture_ram_ptr,
                             UINT8 *vrom_ptr)
{
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

    osd_renderer_invalidate_textures(0, 0, 0, 0, 2048, 2048, NULL, 0);

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
    
#ifdef ENABLE_LIGHTING
    glEnable(GL_LIGHTING);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
#endif

    /*
     * Enable 2D texture mapping, GL_MODULATE is for the 3D scene, layer
     * rendering will have to use GL_REPLACE.
     */

    glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /*
     * Create the 2D layer textures
     */

	for(i = 0; i < 4; i++)
	{
		SAFE_FREE(layer[i]);
		layer[i] = (GLubyte *) malloc(512*512*4);
	}

    glGenTextures(4, layer_texture);

    for (i = 0; i < 4; i++) // set up properties for each texture
    {
        glBindTexture(GL_TEXTURE_2D, layer_texture[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, layer[i]);
    }

    /*
     * Set vertex format
     */

    if (m3_config.step == 0x10)
        vertex_divisor = 32768.0f;  // 17.15-format vertices
    else
        vertex_divisor = 524288.0f; // 13.19

	/*
	 * Setup model cache
	 */

#ifdef USE_MODEL_CACHE

	if(!init_model_cache())
		error("init_model_cache failed!\n");

	glVertexPointer(3, GL_FLOAT, 0, model_vertex_array);
	glNormalPointer(GL_FLOAT, 0, model_normal_array);
	glColorPointer(4, GL_FLOAT, 0, model_color_array);
	glTexCoordPointer(2, GL_FLOAT, 0, model_texcoord_array);

#endif
}

/*
 * void osd_gl_unset_mode(void);
 *
 * "Unsets" the current mode -- frees textures. osd_gl_set_mode() must be
 * called before the renderer is used again.
 */

void osd_gl_unset_mode(void)
{
    osd_renderer_invalidate_textures(0, 0, 0, 0, 2048, 2048, NULL, 0);
    glDeleteTextures(4, layer_texture);
}

UINT32 osd_renderer_get_features(void)
{
	return 0;
}

// Not supported but the interface requires them
void osd_renderer_get_palette_buffer(UINT32 **buffer, int *width, int *pitch) { };
void osd_renderer_free_palette_buffer(void) { };
void osd_renderer_get_priority_buffer(int layer_num, UINT8 **buffer, int *pitch) { };
void osd_renderer_free_priority_buffer(int layer_num) { };