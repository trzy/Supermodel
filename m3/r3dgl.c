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
 * r3dgl.c
 *
 * An OS-independent OpenGL-based Real3D rendering engine.
 */

#include "model3.h"
#include <gl/gl.h>
#include <gl/glu.h>

static int OutBMP(int f, unsigned txres, unsigned tyres, unsigned char *texture)
{
    FILE        *fp;
    int    i, j;
    unsigned    pixel;
    unsigned char   r, g, b;
    char    filename[32];

    sprintf(filename, "%d.bmp", f);

    if ((fp = fopen(filename,"wb")) == NULL)
        return 1;

	/*
     * Write the BMP file header
     */

	fwrite("BM", sizeof(char), 2, fp);
    i = 14 + 40 + txres*tyres*3;    // size = 14 + 40 + x*y*bpp
    fwrite(&i, sizeof(unsigned), 1, fp);
    fwrite("\0\0\0\0", sizeof(unsigned char), 4, fp);
    i = 40 + 14;                // image offset
    fwrite(&i, sizeof(unsigned), 1, fp);
    i = 40;
    fwrite(&i, sizeof(unsigned), 1, fp);
    i = txres;                        // image width in pixels
    fwrite(&i, sizeof(unsigned), 1, fp);
    i = tyres;                        // image height in pixels
    fwrite(&i, sizeof(unsigned), 1, fp);
    fwrite("\1\0", sizeof(unsigned char), 2, fp);
    i = 24;                         // bits per pixel
    fwrite(&i, sizeof(unsigned short), 1, fp);
    fwrite("\0\0\0\0\0\0\0\0", sizeof(unsigned char), 8, fp);
    fwrite("\0\0\0\0\0\0\0\0", sizeof(unsigned char), 8, fp);
    i = 0;                          // 0 colors in palette (there is none!)
    fwrite(&i, sizeof(unsigned), 1, fp);
    fwrite("\0\0\0\0", sizeof(unsigned char), 4, fp);

    /*
     * Write the image data bottom-up
     */

    for (i = (tyres - 1) * txres * 4; (int) i >= 0; i -= txres * 4)
    {
        for (j = 0; j < txres * 4; j += 4)
        {
            r = texture[i + j + 0];
            g = texture[i + j + 1];
            b = texture[i + j + 2];
            fwrite(&b, sizeof(unsigned char), 1, fp);
            fwrite(&g, sizeof(unsigned char), 1, fp);
            fwrite(&r, sizeof(unsigned char), 1, fp);
        }      
    }

    printf("wrote %s\n", filename);


    fclose(fp);
    return 0;
}

/******************************************************************/
/* Macros                                                         */
/******************************************************************/

#define GETWORDLE(a)    (*(UINT32 *) a) // Real3D is a little endian device
#define GETWORDBE(a)    BSWAP32(*(UINT32 *) a)


/******************************************************************/
/* Privates                                                       */
/******************************************************************/

/*
 * Memory Regions
 */

static UINT8    *culling_ram;   // pointer to Real3D culling RAM
static UINT8    *polygon_ram;   // pointer to Real3D polygon RAM
static UINT8    *vrom;          // pointer to VROM

/*
 * Texture Mapping
 *
 * The smallest Model 3 textures are 32x32 and the total VRAM texture sheet
 * is 2048x2048. Dividing this by 32 gives us 64x64. Each element contains an
 * OpenGL ID for a texture. Larger textures take up multiple spaces in the
 * grid.
 */

static GLint    texture_grid[64*64];
static GLbyte   texture_buffer[512*512*4];  // for 1 texture

/******************************************************************/
/* Model Drawing                                                  */
/******************************************************************/

/*
 * convert_fixed_to_float():
 *
 * Converts from 13.19 fixed-point format to float. Accepts a signed INT32.
 */

static float convert_fixed_to_float(INT32 num)
{
    float   result;

    result = (float) (num >> 19);                   // 13-bit integer
    result += (float) (num & 0x7ffff) / 524288.0f;  // 19-bit fraction

    return result;
}

/*
 * draw_model_be():
 *
 * Draws a complete big endian (VROM) model.
 */

static void draw_model_be(UINT8 *buf)
{
    struct
    {
        GLfloat x, y, z;
        UINT    uv;
    }       v[4], prev_v[4];    
    UINT    i, stop, tex_w, tex_h, u_base, v_base, u_coord, v_coord;    

    do
    {
        /*
         * Select a texture
         */

        tex_w = 32 << ((GETWORDBE(&buf[3*4]) >> 3) & 7);
        tex_h = 32 << ((GETWORDBE(&buf[3*4]) >> 0) & 7);

        u_base = (((GETWORDBE(&buf[4*4]) & 0x1f) << 1) | ((GETWORDBE(&buf[5*4]) & 0x80) >> 7)) * 32;
        v_base = (((GETWORDBE(&buf[5*4]) & 0x1f) << 1) | ((GETWORDBE(&buf[5*4]) & 0x00) >> 5)) * 16;
        
        /*
         * Draw
         */

        stop = buf[1*4+3] & 4;  // last poly?
        if (buf[0*4+3] & 0x40)  // quad
        {
            switch (buf[0*4+3] & 0x0F)
            {
            case 0: // 4 verts

                for (i = 0; i < 4; i++)
                {
                    v[i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x5C * sizeof(UINT8);    // advance past this polygon

                break;

            case 1: // 3 verts

                v[0] = prev_v[0];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 2: // 3 verts

                v[0] = prev_v[1];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 4: // 3 verts

                v[0] = prev_v[2];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 8: // 3 verts

                v[0] = prev_v[3];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 3: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[1];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 5: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 6: // 2 verts

                v[0] = prev_v[1];
                v[1] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 9: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 0xC:   // 2 verts

                v[0] = prev_v[2];
                v[1] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;
            }

            /*
             * Draw the quad w/ OpenGL
             */

            for (i = 0; i < 4; i++) // save all of these vertices
                prev_v[i] = v[i];

            glBindTexture(GL_TEXTURE_2D, texture_grid[(v_base / 32) * 64 + (u_base / 32)]);
            glBegin(GL_QUADS);
            for (i = 0; i < 4; i++)
            {
                u_coord = v[i].uv >> 16;
                v_coord = v[i].uv & 0xFFFF;

                u_coord >>= 3;
                v_coord >>= 3;

                glTexCoord2f((GLfloat) u_coord / (GLfloat) tex_w, (GLfloat) v_coord / (GLfloat) tex_h);
                glVertex3f(v[i].x, v[i].y, v[i].z);
            }
            glEnd();
        }
        else                // triangle
        {
            switch (buf[0*4+3] & 0x0F)
            {
            case 0: // 3 verts

                for (i = 0; i < 3; i++)
                {
                    v[i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);    // advance past this polygon

                break;

            case 1: // 2 verts

                v[0] = prev_v[0];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 2: // 2 verts

                v[0] = prev_v[1];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 4: // 2 verts

                v[0] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 8: // 2 verts

                v[0] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDBE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 3: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[1];
                v[2].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDBE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 5: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[2];
                v[2].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDBE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 6: // 1 vert

                v[0] = prev_v[1];
                v[1] = prev_v[2];
                v[2].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDBE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 9: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[3];
                v[2].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDBE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 0xC:   // 1 vert

                v[0] = prev_v[2];
                v[1] = prev_v[3];
                v[2].x = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDBE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDBE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;
            }

            /*
             * Draw the triangle w/ OpenGL
             */

            for (i = 0; i < 3; i++) // save all of these vertices
                prev_v[i] = v[i];


            glBindTexture(GL_TEXTURE_2D, texture_grid[(v_base / 32) * 64 + (u_base / 32)]);
            glBegin(GL_TRIANGLES);
            for (i = 0; i < 3; i++)
            {
                u_coord = v[i].uv >> 16;
                v_coord = v[i].uv & 0xFFFF;

                u_coord >>= 3;
                v_coord >>= 3;

                glTexCoord2f((GLfloat) u_coord / (GLfloat) tex_w, (GLfloat) v_coord / (GLfloat) tex_h);
                glVertex3f(v[i].x, v[i].y, v[i].z);
            }
            glEnd();
        }
    }
    while (!stop);
}

/*
 * draw_model_le():
 *
 * Draws a complete little endian (polygon RAM) model.
 */

static void draw_model_le(UINT8 *buf)
{
    struct
    {
        GLfloat x, y, z;
        UINT    uv;
    }       v[4], prev_v[4];    
    UINT    i, stop, tex_w, tex_h, u_base, v_base, u_coord, v_coord;    

    do
    {
        /*
         * Select a texture
         */

        tex_w = 32 << ((GETWORDLE(&buf[3*4]) >> 3) & 7);
        tex_h = 32 << ((GETWORDLE(&buf[3*4]) >> 0) & 7);

        u_base = (((GETWORDLE(&buf[4*4]) & 0x1f) << 1) | ((GETWORDLE(&buf[5*4]) & 0x80) >> 7)) * 32;
        v_base = (((GETWORDLE(&buf[5*4]) & 0x1f) << 1) | ((GETWORDLE(&buf[5*4]) & 0x00) >> 5)) * 16;
        
        /*
         * Draw
         */

        stop = buf[1*4+0] & 4;  // last poly?
        if (buf[0*4+0] & 0x40)  // quad
        {
            switch (buf[0*4+0] & 0x0F)
            {
            case 0: // 4 verts

                for (i = 0; i < 4; i++)
                {
                    v[i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x5C * sizeof(UINT8);    // advance past this polygon

                break;

            case 1: // 3 verts

                v[0] = prev_v[0];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 2: // 3 verts

                v[0] = prev_v[1];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 4: // 3 verts

                v[0] = prev_v[2];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 8: // 3 verts

                v[0] = prev_v[3];
                for (i = 0; i < 3; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);

                break;

            case 3: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[1];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 5: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 6: // 2 verts

                v[0] = prev_v[1];
                v[1] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 9: // 2 verts

                v[0] = prev_v[0];
                v[1] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 0xC:   // 2 verts

                v[0] = prev_v[2];
                v[1] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[2 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[2 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[2 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[2 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;
            }

            /*
             * Draw the quad w/ OpenGL
             */

            for (i = 0; i < 4; i++) // save all of these vertices
                prev_v[i] = v[i];

            glBindTexture(GL_TEXTURE_2D, texture_grid[(v_base / 32) * 64 + (u_base / 32)]);
            glBegin(GL_QUADS);
            for (i = 0; i < 4; i++)
            {
                u_coord = v[i].uv >> 16;
                v_coord = v[i].uv & 0xFFFF;

                u_coord >>= 3;
                v_coord >>= 3;

                glTexCoord2f((GLfloat) u_coord / (GLfloat) tex_w, (GLfloat) v_coord / (GLfloat) tex_h);
                glVertex3f(v[i].x, v[i].y, v[i].z);
            }
            glEnd();
        }
        else                // triangle
        {
            switch (buf[0*4+0] & 0x0F)
            {
            case 0: // 3 verts

                for (i = 0; i < 3; i++)
                {
                    v[i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x4C * sizeof(UINT8);    // advance past this polygon

                break;

            case 1: // 2 verts

                v[0] = prev_v[0];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 2: // 2 verts

                v[0] = prev_v[1];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 4: // 2 verts

                v[0] = prev_v[2];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 8: // 2 verts

                v[0] = prev_v[3];
                for (i = 0; i < 2; i++)
                {
                    v[1 + i].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 0*4]));
                    v[1 + i].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 1*4]));
                    v[1 + i].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + i*16 + 2*4]));
                    v[1 + i].uv = GETWORDLE(&buf[0x1C + i*16 + 3*4]);
                }

                buf += 0x3C * sizeof(UINT8);

                break;

            case 3: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[1];
                v[2].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDLE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 5: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[2];
                v[2].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDLE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 6: // 1 vert

                v[0] = prev_v[1];
                v[1] = prev_v[2];
                v[2].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDLE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 9: // 1 vert

                v[0] = prev_v[0];
                v[1] = prev_v[3];
                v[2].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDLE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;

            case 0xC:   // 1 vert

                v[0] = prev_v[2];
                v[1] = prev_v[3];
                v[2].x = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 0*4]));
                v[2].y = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 1*4]));
                v[2].z = convert_fixed_to_float(GETWORDLE(&buf[0x1C + 0*16 + 2*4]));
                v[2].uv = GETWORDLE(&buf[0x1C + 0*16 + 3*4]);

                buf += 0x2C * sizeof(UINT8);

                break;
            }

            /*
             * Draw the triangle w/ OpenGL
             */

            for (i = 0; i < 3; i++) // save all of these vertices
                prev_v[i] = v[i];

            glBindTexture(GL_TEXTURE_2D, texture_grid[(v_base / 32) * 64 + (u_base / 32)]);
            glBegin(GL_TRIANGLES);
            for (i = 0; i < 3; i++)
            {
                u_coord = v[i].uv >> 16;
                v_coord = v[i].uv & 0xFFFF;

                u_coord >>= 3;
                v_coord >>= 3;

                glTexCoord2f((GLfloat) u_coord / (GLfloat) tex_w, (GLfloat) v_coord / (GLfloat) tex_h);
                glVertex3f(v[i].x, v[i].y, v[i].z);
            }
            glEnd();
        }
    }
    while (!stop);
}


/******************************************************************/
/* Scene Drawing                                                  */
/******************************************************************/

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
 * get_matrix():
 *
 * Fetches a matrix from culling RAM and converts it to OpenGL form.
 */

#define CMINDEX(y, x)	(x*4+y)
static void get_matrix(GLfloat *dest, UINT32 src)
{
    dest[CMINDEX(0, 0)] = get_float(&culling_ram[src + 3*4]);
    dest[CMINDEX(0, 1)] = get_float(&culling_ram[src + 4*4]);
    dest[CMINDEX(0, 2)] = get_float(&culling_ram[src + 5*4]);
    dest[CMINDEX(0, 3)] = get_float(&culling_ram[src + 0*4]);

    dest[CMINDEX(1, 0)] = get_float(&culling_ram[src + 6*4]);
    dest[CMINDEX(1, 1)] = get_float(&culling_ram[src + 7*4]);
    dest[CMINDEX(1, 2)] = get_float(&culling_ram[src + 8*4]);
    dest[CMINDEX(1, 3)] = get_float(&culling_ram[src + 1*4]);

    dest[CMINDEX(2, 0)] = get_float(&culling_ram[src + 9*4]);
    dest[CMINDEX(2, 1)] = get_float(&culling_ram[src + 10*4]);
    dest[CMINDEX(2, 2)] = get_float(&culling_ram[src + 11*4]);
    dest[CMINDEX(2, 3)] = get_float(&culling_ram[src + 2*4]);

    dest[CMINDEX(3, 0)] = 0.0;
    dest[CMINDEX(3, 1)] = 0.0;
    dest[CMINDEX(3, 2)] = 0.0;
	dest[CMINDEX(3, 3)] = 1.0;	
}

/*
 * draw_models():
 *
 * Draws the specified list of models.
 */

static void draw_models(UINT32 list_addr)
{
    GLfloat m[4*4]; // Model 3 matrix
    UINT32  i, model_ptr_cmd, model_tab, model_addr, matrix_addr;

    for (i = list_addr; !(GETWORDLE(&culling_ram[i]) & 0x02000000); i += 4)   // go until a stop bit is hit (and don't draw that model)
	{
        /*
         * Get address of the 10-word table
         */

        model_tab = GETWORDLE(&culling_ram[i]);
		model_tab = (model_tab & 0xffff) * 4;
		
        /*
         * Get the address of the model and the command in which the address
         * appeared
         */

        model_ptr_cmd = GETWORDLE(&culling_ram[model_tab + 7*4]);
		model_addr = model_ptr_cmd & 0x1fffff;
		model_addr *= 4;

        /*
         * Get the address of the matrix
         */

        matrix_addr = GETWORDLE(&culling_ram[model_tab + 3*4]) & 0xffff;
		matrix_addr = 0x2000*4 + matrix_addr*12*4;
								
        /*
         * Transform and draw
         */

        get_matrix(m, matrix_addr);
		glPushMatrix();                   
		glMultMatrixf(m);					
        
        if ((model_ptr_cmd & 0xff800000) == 0x01000000)
            draw_model_le(&polygon_ram[model_addr]);
        else
            draw_model_be(&vrom[model_addr]);

        glPopMatrix();
	}

}

/*
 * draw_scene():
 *
 * Draws the scene by traversing each of the major nodes starting at 0.
 */

static void draw_scene(void)
{
    UINT32  i, j;
    BOOL    stop;

    /*
     * Draw each major node
     */

    i = 0;
    stop = 0;
    do
    {
        i = GETWORDLE(&culling_ram[i + 4]); // get address of next block
        if (i == 0x01000000)                // 01000000 == STOP
			stop = TRUE;
		i = (i & 0xffff) * 4;
        if (i == 0) // culling RAM probably hasn't been set up yet
            break;

        j = GETWORDLE(&culling_ram[i + 8]); // get address of 10-word block
		j = (j & 0xffff) * 4;

        draw_models((GETWORDLE(&culling_ram[j + 7 * 4]) & 0xffff) * 4);
    }
    while (!stop);
}

/******************************************************************/
/* Texture Drawing                                                */
/******************************************************************/

static void draw_texture_tile(UINT x, UINT y, UINT8 *buf, UINT w)
{
    static const INT    decode[64] = 
						{
							 0, 1, 4, 5, 8, 9,12,13,
							 2, 3, 6, 7,10,11,14,15,
							16,17,20,21,24,25,28,29,
							18,19,22,23,26,27,30,31,
							32,33,36,37,40,41,44,45,
							34,35,38,39,42,43,46,47,
							48,49,52,53,56,57,60,61,
							50,51,54,55,58,59,62,63
						};
    UINT    xi, yi, pixel_offs, rgb16;
    GLbyte  r, g, b;

	for (yi = 0; yi < 8; yi++)
	{
		for (xi = 0; xi < 8; xi++)
		{
            /*
             * Grab the pixel offset from the decode[] array. We XOR with 1
             * because the Real3D is a little endian device and every word
             * contains 2 16-bit pixels, thus they are swapped
             */

            pixel_offs = decode[(yi * 8 + xi) ^ 1] * 2;

            rgb16 = *(UINT16 *) &buf[pixel_offs];

			b = (rgb16 & 0x1f) << 3;
			g = ((rgb16 >> 5) & 0x1f) << 3;
			r = ((rgb16 >> 10) & 0x1f) << 3;

			/*
			 * Write R, G, B, and alpha. On Model 3, an alpha bit of 1 indicates a transparent
			 * color and 0 is opaque
			 */

            texture_buffer[((y + yi) * w + (x + xi)) * 4 + 0] = r;
            texture_buffer[((y + yi) * w + (x + xi)) * 4 + 1] = g;
            texture_buffer[((y + yi) * w + (x + xi)) * 4 + 2] = b;
            texture_buffer[((y + yi) * w + (x + xi)) * 4 + 3] = (rgb16 & 0x8000) ? 0x00 : 0xff;
        }
    }
}

static void draw_texture(UINT w, UINT h, UINT8 *buf)
{
    UINT    xi, yi;

	for (yi = 0; yi < h * 8; yi += 8)
	{
		for (xi = 0; xi < w * 8; xi += 8)
		{
            draw_texture_tile(xi, yi, buf, w * 8);
            buf += 8 * 8 * 2;   // each texture tile is 8x8 and 16-bit color
		}
	}
}

/*
 * void r3dgl_upload_texture(UINT8 *src);
 *
 * Converts the specified Model 3 texture into OpenGL format and uploads it
 * for use.
 *
 * Parameters:
 *      src = Pointer to texture data with 2-word header.
 */

void r3dgl_upload_texture(UINT8 *src)
{
    static int  f = 0;

    UINT32  flags;
    UINT    tiles_x, tiles_y, xpos, ypos, xi, yi;
    GLint   id;

    flags = GETWORDLE(&src[4]);

    /*
     * Calculate width and height in terms of 8x8 tiles and the
     * position within the 2048x2048 texture sheet
     */

    tiles_x = (flags >> 14) & 3;
    tiles_y = (flags >> 17) & 3;
    tiles_x = (32 << tiles_x) / 8;
    tiles_y = (32 << tiles_y) / 8;

    ypos = ((flags >> 6) & 0x3F) * 16 - 16; 
    xpos = ((flags >> 0) & 0x3F) * 32;

    /*
     * Render the texture into the texture buffer
     */

    if ((flags & 0x0f000000) == 0x01000000) // only render non-mipmap textures
        draw_texture(tiles_x, tiles_y, &src[8]);
    else
        return;

//    OutBMP(f, tiles_x * 8, tiles_y * 8, texture_buffer);
    ++f;

    /*
     * Get a texture ID for this texture and set its properties, then upload
     * it
     */

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, tiles_x * 8, tiles_y * 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_buffer);

    /*
     * Mark the appropriate parts of the texture grid with this texture
     */

    for (yi = 0; yi < (tiles_y * 8) / 32; yi++)
    {
        for (xi = 0; xi < (tiles_x * 8) / 32; xi++)
            texture_grid[(yi + ypos / 32) * 64 + (xi + xpos / 32)] = id;
    }
}

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

/*
 * void r3dgl_update_frame(void);
 *
 * Renders the frame. Z-buffering is assumed to be disabled prior to this
 * function call.
 */

//TODO: Cannot be resized yet, hardcoded for 496x384

void r3dgl_update_frame(void)
{
    /*
     * Enable Z-buffering
     */

    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClear(GL_DEPTH_BUFFER_BIT);

    /*
     * Enable texture mapping
     */

    glEnable(GL_TEXTURE_2D);

    /*
     * Set up a perspective view and then set the matrix mode to modelview.
     * Each Z coordinate is multiplied by 1 to flip the coordinate system
     * (Model 3 uses a left-hand system, OpenGL is right-handed.)
     */

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    gluPerspective(45.0, 496.0 / 384.0, 0.1, 100000.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
    glScalef(1.0, 1.0, -1.0);

    /*
     * Draw the scene
     */

    draw_scene();

    /*
     * Disable anything we enabled here
     */

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
}

/*
 * void r3dgl_init(UINT8 *culling_ram_ptr, UINT8 *polygon_ram_ptr,
 *                 UINT8 *vrom_ptr);
 *
 * Initializes the engine by passing pointers to Real3D memory regions.
 *
 * Parameters:
 *      culling_ram_ptr = Pointer to Real3D culling RAM.
 *      polygon_ram_ptr = Pointer to Real3D polygon RAM.
 *      vrom_ptr        = Pointer to VROM.
 */

void r3dgl_init(UINT8 *culling_ram_ptr, UINT8 *polygon_ram_ptr,
                UINT8 *vrom_ptr)
{
    culling_ram = culling_ram_ptr;
    polygon_ram = polygon_ram_ptr;
    vrom = vrom_ptr;
}
