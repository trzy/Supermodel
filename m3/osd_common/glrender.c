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
 * OpenGL renderer.
 *
 * Address Notes:
 *
 *      Addresses seem to take the form: CCAAAAAA, where A is the address and
 *      C is a command. The lower 1 or 2 bits of CC is probably part of the
 *      address as well. Addresses are word-granular.
 *
 *      0x04800600  -- Draw the list at 0x800600 (8E culling RAM.)
 *      0x040301EA  -- Draw the list at 0x0301EA (8C culling RAM.)
 *      0x018AA963  -- Draw the model at 0x18AA963 (VROM.)
 *      0x01010000  -- Draw the model at 0x1010000 (polygon RAM.)
 *
 *      Address 0x0000000 = Culling RAM @ 0x8C000000
 *              0x0800000 = Culling RAM @ 0x8E000000
 *              0x1000000 = Polygon RAM @ 0x98000000
 *              0x1800000 = VROM
 *
 *      If we assume that 0x8C000000 is the base of the Real3D memory space as
 *      the PowerPC sees it, then 0x0800000*4+0x8C000000=0x8E000000. However,
 *      0x1000000*4+0x8C000000 != 0x98000000. Also, 0x88000000 on the PowerPC
 *      side is definitely something to do with the Real3D. It seems that
 *      the addresses in culling RAM indicate that the Real3D has an internal
 *      address space of its own.
 */

#include "model3.h"
#include "osd_gl.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>   // this one can be obtained freely from SGI

/******************************************************************/
/* Macros (and Inlines)                                           */
/******************************************************************/

#define R3D_LOG
//#define R3D_LOG           LOG
#define LOG_MODEL       0

#define WIREFRAME       0
#define LIGHTING        1
#define FOGGING			1

#ifndef PI
#define PI          3.14159265
#endif
#define RAD2DEG(a)  (((a) * 180.0) / PI)

static UINT32 GETWORDLE(UINT8 *a)
{
	return(*(UINT32 *) a);
}

static UINT32 GETWORDBE(UINT8 *a)
{
	return(BSWAP32(*(UINT32 *) a));
}

/******************************************************************/
/* Privates                                                       */
/******************************************************************/

/*
 * Layers
 *
 * Each layer is a 512x512 RGBA texture.
 */

static GLubyte  layer[4][512][512][4];  // 4MB of RAM, ouch!
static GLuint   layer_texture[4];       // IDs for the 4 layer textures

/*
 * Memory Regions
 */

static UINT8    *culling_ram_8e;    // pointer to Real3D culling RAM
static UINT8    *culling_ram_8c;    // pointer to Real3D culling RAM
static UINT8    *polygon_ram;       // pointer to Real3D polygon RAM
static UINT8    *texture_ram;       // pointer to Real3D texture RAM
static UINT8    *vrom;              // pointer to VROM

/*
 * Resolution and Resolution Ratios
 *
 * The ratio of the OpenGL physical resolution to the Model 3 resolution is
 * pre-calculated.
 */

static UINT     xres, yres;
static float    xres_ratio, yres_ratio;

/*
 * Texture Mapping
 *
 * The smallest Model 3 textures are 32x32 and the total VRAM texture sheet
 * is 2048x2048. Dividings this by 32 gives us 64x64. Each element contains an
 * OpenGL ID for a texture.
 */

static GLint    texture_grid[64*64];        // 0 indicates unused
static GLbyte   texture_buffer[512*512*4];  // for 1 texture

/*
 * Function Prototypes (for forward references)
 */

static void draw_block(UINT8 *);
static void make_texture(UINT, UINT, UINT, UINT, UINT, UINT);

/******************************************************************/
/* Model Drawing                                                  */
/******************************************************************/

static float    vertex_divisor, texcoord_divisor;

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

typedef struct
{
	GLfloat	x, y, z;
	GLfloat nx, ny, nz;
	UINT	uv;
} R3D_VTX;

/*
 * draw_model();
 *
 * Draws a complete model.
 */

UINT r3d_test_model = 0;
UINT r3d_test_bit = 0;
UINT r3d_test_word = 0;
UINT r3d_test_bit_default = 0;

static void draw_model(UINT8 *buf, UINT little_endian)
{
    UINT32  (* GETWORD)(UINT8 *buf);
	R3D_VTX	v[4], prev_v[4];
    UINT    i, stop, tex_enable, poly_opaque, poly_trans, light_enable;
    UINT    tex_w, tex_h, tex_fmt, tex_rep_mode, u_base, v_base;
    GLfloat u_coord, v_coord, n[3], color[4];

    if (buf == NULL)
        return;

	if(little_endian)
		GETWORD = GETWORDLE;
	else
		GETWORD = GETWORDBE;

	if(GETWORD(&buf[0*4]) == 0)
		return;

    do
    {
		UINT really_draw = 1;

		#if LOG_MODEL
		R3D_LOG("model3.log",
				"# model\n"
				" 00: %08X\n"
				" 01: %08X\n"
				" 02: %08X\n"
				" 03: %08X\n"
				" 04: %08X\n"
				" 05: %08X\n"
				" 06: %08X\n\n",
				GETWORD(&buf[0*4]),
				GETWORD(&buf[1*4]),
				GETWORD(&buf[2*4]),
				GETWORD(&buf[3*4]),
				GETWORD(&buf[4*4]),
				GETWORD(&buf[5*4]),
				GETWORD(&buf[6*4])
		);
		#endif

		if(r3d_test_model)
		{
			really_draw = r3d_test_bit_default;
			if(GETWORD(&buf[r3d_test_word*4]) & (1 << r3d_test_bit))
				really_draw = r3d_test_bit_default ^ 1;
		}

		/*
		really_draw = 0;
		if((GETWORD(&buf[3*4]) & 0xC0) != 0xC0)
			really_draw = 1;
		*/

		// temp
		/*
		0x0C800000 in VON2, attract mode, cavern level
		if((GETWORD(&buf[6*4]) & 0x04000000) == 0 &&
			GETWORD(&buf[5*4]) & ~(0x00400000|0x00100000))
			error("Unknown word 5 = %08X on untextured poly\n", GETWORD(&buf[5*4]));
		*/

		// temp
		if(!(GETWORD(&buf[0*4]) & 0x80) && GETWORD(&buf[0*4]) & 0xEC000000)
			error("Unknown word 0 = %08X on bit 0x80\n", GETWORD(&buf[0*4]));

		/*
		 * Retrieves the normal
		 */

		n[0] = (GLfloat) (((INT32)GETWORD(&buf[1*4])) >> 8) / 4194304.0f;
		n[1] = (GLfloat) (((INT32)GETWORD(&buf[2*4])) >> 8) / 4194304.0f;
		n[2] = (GLfloat) (((INT32)GETWORD(&buf[3*4])) >> 8) / 4194304.0f;

        /*
         * Select a texture
         */

        if (GETWORD(&buf[1*4]) & 0x40)
            texcoord_divisor = 1.0f;    // 16.0 format
        else
            texcoord_divisor = 8.0f;    // 13.3 format

        tex_fmt = (GETWORD(&buf[6*4]) >> 7) & 7;
//        tex_fmt = (GETWORD(&buf[6*4]) >> 8) & 3;
        tex_rep_mode = GETWORD(&buf[2*4]) & 3;

        tex_w = 32 << ((GETWORD(&buf[3*4]) >> 3) & 7);
        tex_h = 32 << ((GETWORD(&buf[3*4]) >> 0) & 7);

        u_base = ((GETWORD(&buf[4*4]) & 0x1F) << 1) | ((GETWORD(&buf[5*4]) & 0x80) >> 7);
        v_base = (GETWORD(&buf[5*4]) & 0x1F) | ((GETWORD(&buf[4*4]) & 0x40) >> 1);
        u_base *= 32;
        v_base *= 32;

        /*
         * Draw
         */

        tex_enable = GETWORD(&buf[6*4]) & 0x04000000;	// texture enable flag
        poly_trans = GETWORD(&buf[6*4]) & 0x80000000;   // transparency bit processing
		poly_opaque = GETWORD(&buf[6*4]) & 0x00800000;	// polygon opaque
        light_enable = GETWORD(&buf[6*4]) & 0x00010000; // lighting (0 = on)
        stop = GETWORD(&buf[1*4]) & 4; 					// last poly?

//        if (!poly_opaque && poly_trans) // translucent AND transparency????
//            error("poly_opaque=0 and poly_trans=1");

        if (!tex_enable)
            glDisable(GL_TEXTURE_2D);

#if LIGHTING
        if (light_enable)
            glDisable(GL_LIGHTING);
#endif

        if (tex_fmt == 7)   // RGBA4
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        if (poly_trans)
        {
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.95f);
        }

		color[0] = (GLfloat) ((GETWORD(&buf[4*4]) >> 24) & 0xFF) / 255.0f;
		color[1] = (GLfloat) ((GETWORD(&buf[4*4]) >> 16) & 0xFF) / 255.0f;
		color[2] = (GLfloat) ((GETWORD(&buf[4*4]) >>  8) & 0xFF) / 255.0f;

		if(poly_opaque)
			color[3] = 1.0f;
		else
			color[3] = (GLfloat) ((((GETWORD(&buf[6*4]) >> 18) - 1) & 0x1F) * 8) / 255.0f;

		glColor4fv(color);
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);

		#if 0
		// temp
		if (GETWORD(&buf[0*4]) & 0x80)
			glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, (UINT)(UINT8)((GETWORD(&buf[0*4]) >> 26) << 2));
		else
			glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 0);
		#endif

        if (GETWORD(&buf[0*4]) & 0x40)  // quad
        {
            switch (GETWORD(&buf[0*4]) & 0x0F)
            {
            case 7: // 1 vert
                really_draw = 0;
                buf += 0x2C * sizeof(UINT8);
                break;

            case 0: // 4 verts

                for (i = 0; i < 4; i++)
                {
                    v[i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[i].nx = n[0];
					v[i].ny = n[1];
					v[i].nz = n[2];
                    v[i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x5C * sizeof(UINT8);    // advance past this polygon

                break;

            case 1: // 3 verts

                v[0] = prev_v[0];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[1 + i].nx = n[0];
					v[1 + i].ny = n[1];
					v[1 + i].nz = n[2];
                    v[1 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 2: // 3 verts

                v[0] = prev_v[1];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[1 + i].nx = n[0];
					v[1 + i].ny = n[1];
					v[1 + i].nz = n[2];
                    v[1 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 4: // 3 verts

                v[0] = prev_v[2];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[1 + i].nx = n[0];
					v[1 + i].ny = n[1];
					v[1 + i].nz = n[2];
                    v[1 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 8: // 3 verts

                v[0] = prev_v[3];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[1 + i].nx = n[0];
					v[1 + i].ny = n[1];
					v[1 + i].nz = n[2];
                    v[1 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 3: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[1];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[2 + i].nx = n[0];
					v[2 + i].ny = n[1];
					v[2 + i].nz = n[2];
                    v[2 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 5: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[2 + i].nx = n[0];
					v[2 + i].ny = n[1];
					v[2 + i].nz = n[2];
                    v[2 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 6: // 2 verts

                v[0] = prev_v[1];
                v[1] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[2 + i].nx = n[0];
					v[2 + i].ny = n[1];
					v[2 + i].nz = n[2];
                    v[2 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 9: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[2 + i].nx = n[0];
					v[2 + i].ny = n[1];
					v[2 + i].nz = n[2];
                    v[2 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;
            
            case 0xC:   // 2 verts

                v[0] = prev_v[2];
                v[1] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[2 + i].nx = n[0];
					v[2 + i].ny = n[1];
					v[2 + i].nz = n[2];
                    v[2 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;
            }

            /*
             * Draw the quad w/ OpenGL
             */

            for (i = 0; i < 4; i++) // save all of these vertices
                prev_v[i] = v[i];

            make_texture(u_base, v_base, tex_w, tex_h, tex_fmt, tex_rep_mode);

			if(really_draw)
			{

            glBegin(GL_QUADS);
            for (i = 0; i < 4; i++)
            {
                u_coord = convert_texcoord_to_float(v[i].uv >> 16);
                v_coord = convert_texcoord_to_float(v[i].uv & 0xFFFF);

				glNormal3f(v[i].nx, v[i].ny, v[i].nz);
                glTexCoord2f(u_coord / (GLfloat) tex_w, v_coord / (GLfloat) tex_h);
                glVertex3f(v[i].x, v[i].y, v[i].z);
            }
            glEnd();

			} // really_draw
        }
        else                // triangle
        {
            switch (GETWORD(&buf[0*4]) & 0x0F)
            {
            case 7: // 1 vert
                really_draw = 0;
                buf += 0x1C * sizeof(UINT8);
                break;

            case 0: // 3 verts

                for (i = 0; i < 3; i++)
                {
                    v[i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[i].nx = n[0];
					v[i].ny = n[1];
					v[i].nz = n[2];
                    v[i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);    // advance past this polygon

                break;

            case 1: // 2 verts

                v[0] = prev_v[0];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[1 + i].nx = n[0];
					v[1 + i].ny = n[1];
					v[1 + i].nz = n[2];
                    v[1 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 2: // 2 verts

                v[0] = prev_v[1];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[1 + i].nx = n[0];
					v[1 + i].ny = n[1];
					v[1 + i].nz = n[2];
                    v[1 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 4: // 2 verts

                v[0] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[1 + i].nx = n[0];
					v[1 + i].ny = n[1];
					v[1 + i].nz = n[2];
                    v[1 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 8: // 2 verts

                v[0] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_vertex_to_float(GETWORD(&buf[0x1C + i*16 + 2*4]));
					v[1 + i].nx = n[0];
					v[1 + i].ny = n[1];
					v[1 + i].nz = n[2];
                    v[1 + i].uv = GETWORD(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 3: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[1];
                v[2].x = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 2*4]));
				v[2].nx = n[0];
				v[2].ny = n[1];
				v[2].nz = n[2];
                v[2].uv = GETWORD(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 5: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[2];
                v[2].x = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 2*4]));
				v[2].nx = n[0];
				v[2].ny = n[1];
				v[2].nz = n[2];
                v[2].uv = GETWORD(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 6: // 1 vert

                v[0] = prev_v[1];
                v[1] = prev_v[2];
                v[2].x = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 2*4]));
				v[2].nx = n[0];
				v[2].ny = n[1];
				v[2].nz = n[2];
                v[2].uv = GETWORD(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 9: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[3];
                v[2].x = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 2*4]));
				v[2].nx = n[0];
				v[2].ny = n[1];
				v[2].nz = n[2];
                v[2].uv = GETWORD(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 0xC:   // 1 vert

                v[0] = prev_v[2];
                v[1] = prev_v[3];
                v[2].x = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_vertex_to_float(GETWORD(&buf[0x1C + 0*16 + 2*4]));
				v[2].nx = n[0];
				v[2].ny = n[1];
				v[2].nz = n[2];
                v[2].uv = GETWORD(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;
            }

            /*
             * Draw the triangle w/ OpenGL
             */

            for (i = 0; i < 3; i++) // save all of these vertices
                prev_v[i] = v[i];

            make_texture(u_base, v_base, tex_w, tex_h, tex_fmt, tex_rep_mode);

			if(really_draw)
			{

            glBegin(GL_TRIANGLES);
            for (i = 0; i < 3; i++)
            {
                u_coord = convert_texcoord_to_float(v[i].uv >> 16);
                v_coord = convert_texcoord_to_float(v[i].uv & 0xFFFF);

				glNormal3f(v[i].nx, v[i].ny, v[i].nz);
                glTexCoord2f(u_coord / (GLfloat) tex_w, v_coord / (GLfloat) tex_h);
                glVertex3f(v[i].x, v[i].y, v[i].z);
            }
            glEnd();

			} // really_draw
        }

        if (tex_fmt == 7)
            glDisable(GL_BLEND);

        if (poly_trans)
            glDisable(GL_ALPHA_TEST);

#if LIGHTING
        if (light_enable)
            glEnable(GL_LIGHTING);
#endif

        if (!tex_enable)
            glEnable(GL_TEXTURE_2D);
    }
    while (!stop);
}

/******************************************************************/
/* Scene Drawing                                                  */
/******************************************************************/

static UINT8    *matrix_base;   // pointer to base of matrix table

#define CMINDEX(y, x)	(x*4+y)

/*
 * get_float():
 *
 * Fetches a single-precision float.
 */

static float get_float(UINT8 *src)
{
    union
    {
        float   f;
        UINT32  u;
    } f;

    f.u = GETWORDLE(src);

    return f.f;
}

/*
 * translate_r3d_address():
 *
 * Translates an address from the Real3D internal space into a usable pointer.
 */

static UINT8 *translate_r3d_address(UINT32 addr)
{
    if (addr <= 0x007FFFF)                              // 8C culling RAM
        return &culling_ram_8c[addr * 4];
    else if (addr >= 0x0800000 && addr <= 0x083FFFF)    // 8E culling RAM
    {
        addr &= 0x003FFFF;
        return &culling_ram_8e[addr * 4];
    }
    else if (addr >= 0x1000000 && addr <= 0x107FFFF)    // polygon RAM
    {
        addr &= 0x007FFFF;
        return &polygon_ram[addr * 4];
    }
    else if (addr >= 0x1800000 && addr <= 0x1FFFFFF)    // VROM
    {
        addr &= 0x07FFFFF;
        return &vrom[addr * 4];
    }
//    else
//        LOG("model3.log", "unknown R3D addr = %08X\n", addr);
    return NULL;
    // this one is a kludge to satisfy VON2 -- it probably has no relation to
    // polygon RAM, though
#if 0
    else if (addr >= 0x1200000 && addr <= 0x127FFFF)
    {
        addr &= 0x007FFFF;
        return &polygon_ram[addr * 4];
    }

    error("Unable to translate Real3D address: %08X\n", addr);
    return NULL;
#endif
}

/*
 * set_matrix_base():
 *
 * Sets the base matrix table address.
 */

static void set_matrix_base(UINT32 addr)
{
    matrix_base = translate_r3d_address(addr);
}

/*
 * init_coord_system():
 *
 * Uses the base matrix (a special case) to set up a coordinate system.
 *
 * NOTE: The order of translation values may be different as well.
 */

static void init_coord_system(void)
{
    GLfloat m[4*4];

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(1.0, -1.0, -1.0);

    m[CMINDEX(2, 0)] = get_float(&matrix_base[3*4]);
    m[CMINDEX(2, 1)] = get_float(&matrix_base[4*4]);
    m[CMINDEX(2, 2)] = get_float(&matrix_base[5*4]);
    m[CMINDEX(2, 3)] = get_float(&matrix_base[0*4]);

    m[CMINDEX(0, 0)] = get_float(&matrix_base[6*4]);
    m[CMINDEX(0, 1)] = get_float(&matrix_base[7*4]);
    m[CMINDEX(0, 2)] = get_float(&matrix_base[8*4]);
    m[CMINDEX(0, 3)] = get_float(&matrix_base[1*4]);

    m[CMINDEX(1, 0)] = get_float(&matrix_base[9*4]);
    m[CMINDEX(1, 1)] = get_float(&matrix_base[10*4]);
    m[CMINDEX(1, 2)] = get_float(&matrix_base[11*4]);
    m[CMINDEX(1, 3)] = get_float(&matrix_base[2*4]);

    m[CMINDEX(3, 0)] = 0.0;
    m[CMINDEX(3, 1)] = 0.0;
    m[CMINDEX(3, 2)] = 0.0;
    m[CMINDEX(3, 3)] = 1.0; 

    glMultMatrixf(m);
}

/*
 * get_matrix():
 *
 * Fetches a matrix from 8E culling RAM and converts it to OpenGL form.
 */

static void get_matrix(GLfloat *dest, UINT32 matrix_addr)
{
    UINT8   *src;

    src = &matrix_base[matrix_addr*4];

    dest[CMINDEX(0, 0)] = get_float(&src[3*4]);
    dest[CMINDEX(0, 1)] = get_float(&src[4*4]);
    dest[CMINDEX(0, 2)] = get_float(&src[5*4]);
    dest[CMINDEX(0, 3)] = get_float(&src[0*4]);

    dest[CMINDEX(1, 0)] = get_float(&src[6*4]);
    dest[CMINDEX(1, 1)] = get_float(&src[7*4]);
    dest[CMINDEX(1, 2)] = get_float(&src[8*4]);
    dest[CMINDEX(1, 3)] = get_float(&src[1*4]);

    dest[CMINDEX(2, 0)] = get_float(&src[9*4]);
    dest[CMINDEX(2, 1)] = get_float(&src[10*4]);
    dest[CMINDEX(2, 2)] = get_float(&src[11*4]);
    dest[CMINDEX(2, 3)] = get_float(&src[2*4]);

    dest[CMINDEX(3, 0)] = 0.0;
    dest[CMINDEX(3, 1)] = 0.0;
    dest[CMINDEX(3, 2)] = 0.0;
	dest[CMINDEX(3, 3)] = 1.0;	
}

/*
 * set_viewport_and_perspective():
 *
 * Sets the viewport and perspective for the current scene descriptor.
 */

static void set_viewport_and_perspective(UINT8 *scene_buf)
{
    double  fov_left, fov_right, fov_up, fov_down, fov_x, fov_y;
    UINT    x, y, w, h;

    /*
     * Set viewport
     */

    x = GETWORDLE(&scene_buf[0x1A*4]) & 0xFFFF;
    y = GETWORDLE(&scene_buf[0x1A*4]) >> 16;
    w = GETWORDLE(&scene_buf[0x14*4]) & 0xFFFF;
    h = GETWORDLE(&scene_buf[0x14*4]) >> 16;

    x >>= 4;    // position is 12.4
    y >>= 4;
    w >>= 2;
    h >>= 2;    // size is 14.2

    x = (UINT) ((float) x * xres_ratio);    // X position of window in physical screen space
    y = (UINT) ((float) (384 - (y + h)) * yres_ratio);  // Y position
    glViewport(x, y, w * xres_ratio, h * yres_ratio);

    /*
     * Set the field of view
     */

    fov_left = asin(get_float(&scene_buf[0x0C*4]));
    fov_right = asin(get_float(&scene_buf[0x10*4]));
    fov_up = asin(get_float(&scene_buf[0x0E*4]));
    fov_down = asin(get_float(&scene_buf[0x12*4]));

    fov_x = fov_left + fov_right;
    fov_y = fov_up + fov_down;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
//    gluPerspective(RAD2DEG(fov_y), (GLfloat) w / (GLfloat) h, 0.1, 100000.0);
    gluPerspective(RAD2DEG(fov_y), (GLfloat) w / (GLfloat) h, 0.1, 10000000.0);

#if 0
    {
        /*
         * This code doesn't work quite right
         */

        GLdouble    ls, rs, ts, bs, n, f;

        ls = get_float(&scene_buf[0x0C*4]);
        rs = get_float(&scene_buf[0x10*4]);
        ts = get_float(&scene_buf[0x0E*4]);
        bs = get_float(&scene_buf[0x12*4]);
        n = 0.1;
        f = 10000000.0;

        glFrustum(-(ls * n), rs * n, -(bs * n), ts * n, n, f);

    }
#endif
}

/*
 * draw_list():
 *
 * Processes a list. Each list element references a 10-word block.
 */

static void draw_list(UINT8 *list)
{
    UINT8   *list_ptr;
    UINT32  addr;
    
    list_ptr = list;

    /*
     * Go to end of list
     */

    while (1)
    {
        addr = GETWORDLE(list_ptr);
        if ((addr & 0x02000000))            // last pointer in list
            break;
        if (addr == 0 || addr == 0x800800)  // safeguard in case memory hasn't been set up
        {
            list_ptr -= 4;
            break;
        }
        list_ptr += 4;
    }

    while (list_ptr >= list)
    {
        addr = GETWORDLE(list_ptr);

		R3D_LOG("model3.log", " ## list: draw block at %08X\n\n", addr);

        if ((addr & 0x03000000) == 0x03000000)  // VON2
            addr &= ~0x03000000;

        draw_block(translate_r3d_address(addr & 0x01FFFFFF));
        list_ptr -= 4;  // next element
    }
}

/*
 * draw_pointer_list();
 *
 * Processes a 4-word pointer list, of those used in Scud Race.
 * The meaning of bit 0x01000000 is unknown.
 */

static void draw_pointer_list(UINT8 *buf)
{
	if(GETWORDLE(&buf[0*4]) & 0x00800000)	// a model in VROM
	{
		R3D_LOG("model3.log",
				" ## pointer list: draw model at %08X,%08X,%08X,%08X\n",
				GETWORDLE(&buf[0*4]),
				GETWORDLE(&buf[1*4]),
				GETWORDLE(&buf[2*4]),
				GETWORDLE(&buf[3*4])
		);
//		draw_model(&vrom[(GETWORDLE(&buf[0*4]) & 0x00FFFFFF) * 4], 0);	// crashes Scud Race in-game
	}
	else
	{
		R3D_LOG("model3.log",
				" ## pointer list: draw block at %08X,%08X,%08X,%08X\n",
				GETWORDLE(&buf[0*4]),
				GETWORDLE(&buf[1*4]),
				GETWORDLE(&buf[2*4]),
				GETWORDLE(&buf[3*4])
		);
		draw_block(translate_r3d_address(GETWORDLE(&buf[0*4]) & 0x00FFFFFF));
	}
}

/*
 * draw_block():
 *
 * Processes a 10-word block. These blocks can either reference models or
 * lists.
 */

static void draw_block(UINT8 *block)
{
    GLfloat m[4*4];
    UINT32  matrix, addr, next_ptr;

    /*
     * Blocks may apparently be chained together in a linked list. We're not
     * yet certain if these link pointers may reference lists or models, but
     * for now, we're assuming they cannot.
     */

    while (1)
    {
        if (m3_config.step == 0x10)
        {
			/*
			 * Model 3 Step 1.0 block tables are only 8-word long:
			 * they lack scaling and texture selection words.
			 */

			R3D_LOG("model3.log",
					"#\n"
					"# block:\n"
					"#\n"
					"\n"
					"00: %08X\n"
					"01: %08X\n"
					"02: %3.5f\n"
					"03: %3.5f\n"
					"04: %3.5f\n"
					"05: %08X\n"
					"06: %08X\n"
					"07: %08X\n"
					"\n",
					GETWORDLE(&block[0 * 4]),
					GETWORDLE(&block[1 * 4]),
					get_float(&block[2 * 4]),
					get_float(&block[3 * 4]),
					get_float(&block[4 * 4]),
					GETWORDLE(&block[5 * 4]),
					GETWORDLE(&block[6 * 4]),
					GETWORDLE(&block[7 * 4])
			);

	        /*
	         * Multiply by the specified matrix -- apparently, if the number is 0,
	         * it should not be used (Note that matrix 0 is reserved for coordinate
			 * system selection)
	         */

            glPushMatrix();
            matrix = (GETWORDLE(&block[1*4]) & 0xFFF);
            if (matrix != 0)
            {
                get_matrix(m, matrix * 12);
                glMultMatrixf(m);
            }
            glTranslatef(get_float(&block[2*4]), get_float(&block[3*4]), get_float(&block[4*4]));

	        /*
	         * Draw a model or process a list. If the address is of the form
	         * 04XXXXXX, then the address points to a list, otherwise it points to
	         * a model.
	         */

            addr = GETWORDLE(&block[5*4]);

            if(GETWORDLE(&block[0*4]) & 0x08)
            {
				error("step 1.0, block header references a pointer list (%08X)\n", addr);
            }
            else
            {
                switch ((addr >> 24) & 0xFF)
                {
                case 0x00:  // block
                    if(addr != 0 && addr != 0xFFFF)
                    {
                        R3D_LOG("model3.log", " ## block: draw block at %08X\n\n", addr);
                        draw_block(translate_r3d_address(addr & 0x01FFFFFF));
                    }
                    break;
                case 0x01:  // model
                case 0x03:  // model in VROM (Scud Race)
                    if(addr != 0 && addr != 0xFFFF)
                    {
                        R3D_LOG("model3.log", " ## block: draw model at %08X\n\n", addr);
                        if ((addr & 0x01FFFFFF) >= 0x01800000)  // VROM
                            draw_model(translate_r3d_address(addr & 0x01FFFFFF), 0);
                        else                                    // polygon RAM
                            draw_model(translate_r3d_address(addr & 0x01FFFFFF), 1);
                    }
                    break;
                case 0x04:  // list
                    R3D_LOG("model3.log", " ## block: draw list at %08X\n\n", addr);
                    if ((addr & 0x01FFFFFF) >= 0x018000000) error("List in VROM %08X\n", addr);
                    draw_list(translate_r3d_address(addr & 0x01FFFFFF));
                    break;
                default:
                    error("Unable to handle Real3D address: %08X\n", addr);
                    break;
                }
            }

            /*
             * Pop the matrix
             */

            glPopMatrix();
    
            /*
             * Advance to next block in list
             */

//#if 0
            next_ptr = GETWORDLE(&block[6*4]);
            if ((next_ptr & 0x01000000) || (next_ptr == 0)) // no more links
                break;

            block = translate_r3d_address(next_ptr);
//#endif
//            break;
        }
        else	// step 1.5+
        {
			R3D_LOG("model3.log",
					"#\n"
					"# block:\n"
					"#\n"
					"\n"
					"00: %08X\n"
					"01: %08X\n"
					"02: %08X\n"
					"03: %08X\n"
					"04: %3.5f\n"
					"05: %3.5f\n"
					"06: %3.5f\n"
					"07: %08X\n"
					"08: %08X\n"
					"09: %08X\n"
					"\n",
					GETWORDLE(&block[0 * 4]),
					GETWORDLE(&block[1 * 4]),
					GETWORDLE(&block[2 * 4]),
					GETWORDLE(&block[3 * 4]),
					get_float(&block[4 * 4]),
					get_float(&block[5 * 4]),
					get_float(&block[6 * 4]),
					GETWORDLE(&block[7 * 4]),
					GETWORDLE(&block[8 * 4]),
					GETWORDLE(&block[9 * 4])
			);

			// temp
			if((GETWORDLE(&block[3*4]) & 0x000FF000) == 0 &&
				GETWORDLE(&block[0*4]) & 0x00000008)
				error("Unknown word 3 = %08X\n", GETWORDLE(&block[3*4]));

	        /*
	         * Multiply by the specified matrix -- apparently, if the number is 0,
	         * it should not be used (Note that matrix 0 is reserved for coordinate
			 * system selection)
	         */

	        glPushMatrix();
            matrix = (GETWORDLE(&block[3*4]) & 0xFFF);
	        if (matrix != 0)
	        {
	            get_matrix(m, matrix * 12);
	            glMultMatrixf(m);
	        }
	        glTranslatef(get_float(&block[4*4]), get_float(&block[5*4]), get_float(&block[6*4]));

			// TODO: scaling (word 1) -- only used in Scud Race so far
			// TODO: texture select (word 2)

	        /*
	         * Draw a model or process a list. If the address is of the form
	         * 04XXXXXX, then the address points to a list, otherwise it points to
	         * a model.
	         */

	        addr = GETWORDLE(&block[7*4]);

			if(GETWORDLE(&block[0*4]) & 0x08)
			{
				/*
				 * The block references a 4-element list (Scud Race).
				 * The value of bit 0x01000000 in the address assumes another
				 * (currently unknown) meaning.
				 */

				if(addr & 0xFE000000)
					error("Invalid list address: %08X\n", addr);

				draw_pointer_list(translate_r3d_address(addr & 0x00FFFFFF));
			}
			else
			{
		        switch ((addr >> 24) & 0xFF)
		        {
				case 0x00:	// block
					if(addr != 0)
					{
						R3D_LOG("model3.log", " ## block: draw block at %08X\n\n", addr);
						draw_block(translate_r3d_address(addr & 0x01FFFFFF));
					}
					break;
				case 0x01:	// model
				case 0x03:	// model in VROM (Scud Race)
					if(addr != 0)
					{
						R3D_LOG("model3.log", " ## block: draw model at %08X\n\n", addr);
						if ((addr & 0x01FFFFFF) >= 0x01800000)  // VROM
                            draw_model(translate_r3d_address(addr & 0x01FFFFFF), 0);
						else                                    // polygon RAM
                            draw_model(translate_r3d_address(addr & 0x01FFFFFF), 1);
					}
					break;
		        case 0x04:  // list
					R3D_LOG("model3.log", " ## block: draw list at %08X\n\n", addr);
		            if ((addr & 0x01FFFFFF) >= 0x018000000) error("List in VROM %08X\n", addr);
		            draw_list(translate_r3d_address(addr & 0x01FFFFFF));
		            break;
		        default:
		            error("Unable to handle Real3D address: %08X\n", addr);
		            break;
	    	    }
			}

	        /*
	         * Pop the matrix
	         */

	        glPopMatrix();

	        /*
	         * Advance to next block in list
	         */

	        next_ptr = GETWORDLE(&block[8*4]);
	        if ((next_ptr & 0x01000000) || (next_ptr == 0)) // no more links
	            break;

	        block = translate_r3d_address(next_ptr);
        }
    }
}

/*
 * draw_viewport():
 *
 * Draws all of the scenes which belong to the specified viewport. Culling RAM
 * is traversed starting at 0. The Z buffer is cleared each time.
 */

static void draw_viewport(UINT pri, UINT32 addr)
{
	UINT8	*buf;
    UINT32  next_addr, ptr;

    buf = translate_r3d_address(addr);

    /*
     * Recurse until the last node is reached
     */
    
    next_addr = GETWORDLE(&buf[1*4]);   // get address of next block
    if (next_addr == 0)                 // culling RAM probably hasn't been set up yet
        return;
    if (next_addr != 0x01000000)        // still more nodes to go...
        draw_viewport(pri, next_addr);

    /*
     * Draw this node if the priority is okay
     */

    if (pri == ((GETWORDLE(&buf[0x00*4]) >> 3) & 3))
    {
        /*
         * Get the pointer and check if it's valid
         */

        ptr = GETWORDLE(&buf[2*4]); // get address of 10-word block
        ptr = (ptr & 0xFFFF) * 4;
        if (ptr == 0)   // culling RAM probably hasn't been set up yet
            return;

        message(0, "processing scene descriptor %08X: %08X  %08X  %08X",
                addr,
                GETWORDLE(&buf[0*4]),
                GETWORDLE(&buf[1*4]),
                GETWORDLE(&buf[2*4])
        );

        R3D_LOG("model3.log",
                "#\n"
                "# scene at %08X\n"
                "#\n"
                "\n"
                "00: %08X\n"
                "01: %08X\n"
                "02: %08X\n"
                "03: %3.5f\n"
                "04: %3.5f\n"
                "05: %3.5f\n"
                "06: %3.5f\n"
                "07: %3.5f\n"
                "08: %3.5f\n"
                "09: %3.5f\n"
                "0A: %3.5f\n"
                "0B: %3.5f\n"
                "0C: %3.5f\n"
                "0D: %3.5f\n"
                "0E: %3.5f\n"
                "0F: %3.5f\n"
                "10: %3.5f\n"
                "11: %3.5f\n"
                "12: %3.5f\n"
                "13: %3.5f\n"
                "14: %08X\n"
                "15: %08X\n"
                "16: %08X\n"
                "17: %08X\n"
                "18: %08X\n"
                "19: %08X\n"
                "1A: %08X\n"
                "1B: %08X\n"
                "1C: %08X\n"
                "1D: %08X\n"
                "1E: %08X\n"
                "1F: %3.5f\n"
                "20: %3.5f\n"
                "21: %3.5f\n"
                "22: %08X\n"
                "23: %08X\n"
                "24: %08X\n"
                "25: %08X\n"
                "\n",
                addr,
                GETWORDLE(&buf[0x00 * 4]),
                GETWORDLE(&buf[0x01 * 4]),
                GETWORDLE(&buf[0x02 * 4]),
                get_float(&buf[0x03 * 4]),
                get_float(&buf[0x04 * 4]),
                get_float(&buf[0x05 * 4]),
                get_float(&buf[0x06 * 4]),
                get_float(&buf[0x07 * 4]),
                get_float(&buf[0x08 * 4]),
                get_float(&buf[0x09 * 4]),
                get_float(&buf[0x0A * 4]),
                get_float(&buf[0x0B * 4]),
                get_float(&buf[0x0C * 4]),
                get_float(&buf[0x0D * 4]),
                get_float(&buf[0x0E * 4]),
                get_float(&buf[0x0F * 4]),
                get_float(&buf[0x10 * 4]),
                get_float(&buf[0x11 * 4]),
                get_float(&buf[0x12 * 4]),
                get_float(&buf[0x13 * 4]),
                GETWORDLE(&buf[0x14 * 4]),
                GETWORDLE(&buf[0x15 * 4]),
                GETWORDLE(&buf[0x16 * 4]),
                GETWORDLE(&buf[0x17 * 4]),
                GETWORDLE(&buf[0x18 * 4]),
                GETWORDLE(&buf[0x19 * 4]),
                GETWORDLE(&buf[0x1A * 4]),
                GETWORDLE(&buf[0x1B * 4]),
                GETWORDLE(&buf[0x1C * 4]),
                GETWORDLE(&buf[0x1D * 4]),
                GETWORDLE(&buf[0x1E * 4]),
                get_float(&buf[0x1F * 4]),
                get_float(&buf[0x20 * 4]),
                get_float(&buf[0x21 * 4]),
                GETWORDLE(&buf[0x22 * 4]),
                GETWORDLE(&buf[0x23 * 4]),
                GETWORDLE(&buf[0x24 * 4]),
                GETWORDLE(&buf[0x25 * 4])
        );

        set_viewport_and_perspective(buf);
        set_matrix_base(GETWORDLE(&buf[0x16*4]));
        init_coord_system();

        #if LIGHTING
        {
        /*
         * Until material lighting properties aren't emulated
         * properly, light will never look decent on flat polys.
         * Moreover, we still lack emulation of shading selection
         * (Real3D docs enumerate flat, fixed, gouraud and
         * specular shading, selectable per polygon).
         *
         * Note that lighting has plain bad problems with untextured
         * polys.
         */

        GLfloat vect[4];
        GLfloat temp[4];

        /*
         * Directional light
         */

        vect[0] = -get_float(&buf[0x05 * 4]);
        vect[1] = -get_float(&buf[0x06 * 4]);
        vect[2] = get_float(&buf[0x04 * 4]);
        vect[3] = 0.0f;

        temp[0] = \
        temp[1] = \
        temp[2] = get_float(&buf[0x07 * 4]);
        temp[3] = 1.0f;

        glLightfv(GL_LIGHT0, GL_POSITION, vect);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, temp);

        /*
         * Positional light
         */

        /* This kind of lights uses a X and Y position,
         * a X and Y size, a start point and an extent.
         * It seems to describe a lighting volume
         * (considering the start and extent values as
         * Z coords). Currently unemulated.
         */

        /*
         * Ambient light
         */

        temp[0] = \
        temp[1] = \
        temp[2] = (GLfloat)((GETWORDLE(&buf[0x24 * 4]) >> 8) & 0xFF) / 256.0f;
        temp[3] = 1.0f;

        glLightfv(GL_LIGHT0, GL_AMBIENT, temp);

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);

        }
        #endif

        #if FOGGING
        {
        /*
         * Very primitive fog implementation.
         */

        GLfloat fog_color[4];

        fog_color[0] = (GLfloat)((GETWORDLE(&buf[0x22 * 4]) >> 16) & 0xFF) / 256.0f;
        fog_color[1] = (GLfloat)((GETWORDLE(&buf[0x22 * 4]) >>  8) & 0xFF) / 256.0f;
        fog_color[2] = (GLfloat)((GETWORDLE(&buf[0x22 * 4]) >>  0) & 0xFF) / 256.0f;
        fog_color[3] = 1.0f;

        glFogi(GL_FOG_MODE, GL_EXP2);
        //glFogi(GL_FOG_DISTANCE_MODE_NV, GL_EYE_RADIAL_NV);
        glFogf(GL_FOG_DENSITY, get_float(&buf[0x23 * 4]));
        glFogf(GL_FOG_START, (GLfloat)(GETWORDLE(&buf[0x25 * 4]) & 0xFF) / 256.0f);
        glFogf(GL_FOG_END, 10000.0f);
        glFogfv(GL_FOG_COLOR, fog_color);

        glEnable(GL_FOG);
        }
        #endif

        switch((GETWORDLE(&buf[2*4]) >> 24) & 0xFE)
        {
        case 0x00:  // block
            R3D_LOG("model3.log", " ## scene: draw block at %08X\n\n", ptr);
            draw_block(&culling_ram_8e[ptr]);
            break;

        case 0x04:  // list
            R3D_LOG("model3.log", " ## scene: draw list at %08X\n\n", ptr);
        //        draw_list(&culling_ram_8e[ptr]);
            break;

        default:
            error("Unknown scene descriptor link %08X\n", GETWORDLE(&buf[2*4]));
        }

        #if FOGGING
        glDisable(GL_FOG);
        #endif

        #if LIGHTING
        glDisable(GL_LIGHTING);
        #endif
    }
}

/******************************************************************/
/* Texture Management                                             */
/******************************************************************/

/*
 * make_texture():
 *
 * Given the coordinates of a texture and its size within the texture sheet,
 * an OpenGL texture is created and uploaded. The texture will also be
 * selected so that the caller may use it.
 *
 * The format is just bits 9 and 8 of polygon header word 7. The repeat mode
 * is bits 0 and 1 of word 2.
 */

static void make_texture(UINT x, UINT y, UINT w, UINT h, UINT format, UINT rep_mode)
{
    UINT    xi, yi;
    GLint   tex_id;
    UINT16  rgb16;
    UINT8   gray8;

    if (w > 512 || h > 512)
        return;

    tex_id = texture_grid[(y / 32) * 64 + (x / 32)];
    if (tex_id != 0)    // already exists, bind and exit
    {
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (rep_mode & 2) ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (rep_mode & 1) ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT);
        return;
    }

    /*
     * Formats:
     *
     *      0 = 16-bit A1RGB5
     *      1 = 4-bit grayscale, field 0x000F
     *      2 = ?
     *      3 = ?
     *      4 = ?
     *      5 = 8-bit grayscale
     *      6 = 4-bit grayscale, field 0x00F0 or 0xF000 -- not sure yet
     *      7 = RGBA4
     */

    switch (format)
	{
//    case 2: // mono 4-bit
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
                texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 3] = 0xFF;
	        }
	    }

		break;

    case 6: // mono 4-bit
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
                texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >> 4) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 3] = 0xFF;
	        }
	    }

		break;

    case 1: // mono 4-bit
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                rgb16 = *(UINT16 *) &texture_ram[((y + yi) * 2048 + (x + xi)) * 2];
                texture_buffer[((yi * w) + xi) * 4 + 0] = ((rgb16 >> 0) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 1] = ((rgb16 >> 0) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 2] = ((rgb16 >> 0) & 0xF) << 4;
                texture_buffer[((yi * w) + xi) * 4 + 3] = 0xFF;
	        }
	    }

		break;

    case 0: // 16-bit, ARGB1555

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

//    case 2:  // 8-bit, L8
//    case 4:
    case 5:

	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                gray8 = texture_ram[(y + yi) * 2048 + (x + xi)];
                texture_buffer[((yi * w) + xi) * 4 + 0] = gray8;
                texture_buffer[((yi * w) + xi) * 4 + 1] = gray8;
                texture_buffer[((yi * w) + xi) * 4 + 2] = gray8;
                texture_buffer[((yi * w) + xi) * 4 + 3] = (UINT8) 0xFF;
	        }
	    }

		break;

//    case 3: // 16-bit, ARGB4444
    case 7:

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

    default:    // unknown, draw attention to it by making it bright green
        LOG("model3.log", "unhandled texture @ %d,%d (%dx%d), format=%d\n", x, y, w, h, format);
	    for (yi = 0; yi < h; yi++)
	    {
	        for (xi = 0; xi < w; xi++)
	        {
                texture_buffer[((yi * w) + xi) * 4 + 0] = 0x00;
                texture_buffer[((yi * w) + xi) * 4 + 1] = 0xFF;
                texture_buffer[((yi * w) + xi) * 4 + 2] = 0x00;
                texture_buffer[((yi * w) + xi) * 4 + 3] = (UINT8) 0xFF;
	        }
	    }

		break;

	}

    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (rep_mode & 2) ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (rep_mode & 1) ? GL_MIRRORED_REPEAT_ARB : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_buffer);

    texture_grid[(y / 32) * 64 + (x / 32)] = tex_id;    // mark texture as used
}

/*
 * void osd_renderer_remove_textures(UINT x, UINT y, UINT w, UINT h);
 *
 * Frees all textures in the region described and marks those positions as
 * unused.
 *
 * Parameters:
 *      x = X position within the 2048x2048 texture sheet of the area in
 *          pixel coordinates.
 *      y = Y position.
 *      w = Width in pixels.
 *      h = Height in pixels.
 */

void osd_renderer_remove_textures(UINT x, UINT y, UINT w, UINT h)
{
    UINT    yi;

    x /= 32;
    y /= 32;
    w /= 32;
    h /= 32;

    for (yi = 0; yi < h; yi++)
    {
        glDeleteTextures(w, &texture_grid[(yi + y) * 64 + x]);
        memset((UINT8 *) &texture_grid[(yi + y) * 64 + x], 0, w * sizeof(GLint));
    }
}

/******************************************************************/
/* Top-Level Rendering Code                                       */
/******************************************************************/

/*
 * do_3d():
 *
 * Renders the frame.
 *
 * Assumes the following on entry:
 *      - Z-buffering is disabled
 *      - 2D texturing is enabled
 *      - Alpha blending is disabled
 *
 * The entry state is restored on exit (except for texture enviroment mode).
 */

static void do_3d(void)
{
	#if LIGHTING

	const GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const GLfloat full[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);			// remove default ambient light

	/*
	 * These are dummy values: we don't actually know where these are
	 * located in 3D RAM and how they're selected.
	 */

	//glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, full);		// full material ambient reflection
	//glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, full);		// full material diffuse reflection

	#endif

//    if (m3_config.step ==0x10)return;

    /*
     * Enable Z-buffering
     */

    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    /*
     * Draw the complete scene consisting of 4 viewports.
     *
     * Viewport 3 has highest priority (top-most) and must be drawn last
     * because a Z-buffer clear is performed each time.
     */

	#if WIREFRAME
    glPolygonMode(GL_FRONT, GL_LINE);
    glPolygonMode(GL_BACK, GL_LINE);
    glDisable(GL_TEXTURE_2D);
	#endif

    glClear(GL_DEPTH_BUFFER_BIT);
    draw_viewport(0, 0x00800000);
    glClear(GL_DEPTH_BUFFER_BIT);
    draw_viewport(1, 0x00800000);
    glClear(GL_DEPTH_BUFFER_BIT);
    draw_viewport(2, 0x00800000);
    glClear(GL_DEPTH_BUFFER_BIT);
    draw_viewport(3, 0x00800000);

	#if WIREFRAME
    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_FILL);
    glEnable(GL_TEXTURE_2D);
	#endif

    /*
     * Disable anything we enabled here
     */

    glDisable(GL_DEPTH_TEST);
}

/*
 * set_ortho():
 *
 * Set up an orthogonal projection with corners: (0,0), (1,0), (1, 1), (0,1),
 * in clockwise order and set up a viewport.
 */

static void set_ortho(void)
{
    glViewport(0, 0, xres, yres);

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

    glBegin(GL_QUADS);
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

    do_3d();

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
    osd_gl_swap_buffers();
}

/******************************************************************/
/* Initialization and Shutdown                                    */
/******************************************************************/

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

static void init_gl(UINT x, UINT y)
{
    INT i;

    xres = x;
    yres = y;

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
     * Configure the tile generator
     */

    tilegen_set_layer_format(32, 0, 8, 16, 24);
}

/*
 * deinit_gl():
 *
 * Frees up textures.
 */

static void deinit_gl(void)
{
    osd_renderer_remove_textures(0, 0, 2048, 2048);
    glDeleteTextures(4, layer_texture);
}

/*
 * void osd_renderer_set_mode(BOOL fullscreen, UINT xres, UINT yres);
 *
 * Sets the rendering mode. After this, the renderer may draw freely.
 *
 * Parameters:
 *      fullscreen = Non-zero if a full-screen mode is desired, 0 for a
 *                   windowed mode.
 *      xres       = Horizontal resolution in pixels.
 *      yres       = Vertical resolution.
 */

void osd_renderer_set_mode(BOOL fullscreen, UINT xres, UINT yres)
{
    xres_ratio = (float) xres / 496.0;
    yres_ratio = (float) yres / 384.0;

    osd_gl_set_mode(fullscreen, xres, yres);
    init_gl(xres, yres);
}

/*
 * void osd_renderer_unset_mode(void);
 *
 * "Unsets" the current mode.
 */

void osd_renderer_unset_mode(void)
{
    deinit_gl();
    osd_gl_unset_mode();
}

/*
 * void osd_renderer_init(UINT8 *culling_ram_8e_ptr,
 *                        UINT8 *culling_ram_8c_ptr, UINT8 *polygon_ram_ptr,
 *                        UINT8 *texture_ram_ptr, UINT8 *vrom_ptr);
 *
 * Initializes the engine by passing pointers to Real3D memory regions. Also
 * allocates memory for textures.
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
    atexit(osd_renderer_shutdown);

    culling_ram_8e = culling_ram_8e_ptr;
    culling_ram_8c = culling_ram_8c_ptr;
    polygon_ram = polygon_ram_ptr;
    texture_ram = texture_ram_ptr;
    vrom = vrom_ptr;

    memset(texture_grid, 0, 64 * 64 * sizeof(GLint));   // clear texture IDs

    if (m3_config.step == 0x10)
        vertex_divisor = 32768.0f;  // 17.15-format vertices
    else
        vertex_divisor = 524288.0f; // 13.19
}

/*
 * void osd_renderer_shutdown(void);
 *
 * Shuts down the renderer.
 */

void osd_renderer_shutdown(void)
{
}

/******************************************************************/
/* Tile Generator Interface                                       */
/******************************************************************/

void osd_renderer_get_layer_buffer(UINT layer_num, UINT8 **buffer, UINT *pitch)
{
    *pitch = 512;   // currently all layer textures are 512x512
    *buffer = (UINT8 *) &layer[layer_num][0][0][0];
}

void osd_renderer_free_layer_buffer(UINT layer_num)
{
}
