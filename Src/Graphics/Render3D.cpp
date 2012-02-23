/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2012 Bart Trzynadlowski, Nik Henson 
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/
 
/*
 * Render3D.cpp
 * 
 * Core module for OpenGL-based Real3D graphics engine.
 *
 *
 * Optimization To-Do List
 * -----------------------
 *
 * 0. Optimize backface culling. Is it possible to compute normal matrix only
 *    when needed? Should also be more careful about OpenGL state info, such as
 *    the winding mode.
 * 1. Do not store matrices in a uniform, use glLoadMatrix() in MODELVIEW mode.
 *    It will no longer be necessary to compute normal matrix!
 * 2. Move stuff into vertex shader (vision by 2048? Subtract of 0.5,0.5 for bilinear filtering?)
 * 3. Just one call to BufferSubData rather than 2
 *    
 * Spotlight
 * ---------
 *
 * Spotlight illumination occurs between two Z ranges within an ellipse
 * specified in coordinates that ought to be relative to the viewport. They
 * actually appear to be defined in terms of physical display coordinates
 * regardless of the size of the viewport, although this has not been 100%
 * confirmed. 
 *
 * The parameters that describe the ellipse in display coordinates are:
 *
 *		cx,cy	Center point.
 *		a,b		Width (or rather, half-width) and height of spotlight.
 *
 * These correspond to the standard form of the ellipse equation:
 *
 *		((x-cx)/a)^2 + ((y-cy)/b)^2 = 1
 *
 * It is trivial to test whether a point lies inside an ellipse by plugging
 * it into the equation and checking to see if it is less than or equal to
 * 1. The a and b parameters appear to be stored as values w and h, which
 * range from 0 to 255 (according to the Scud Race debug menu) but which
 * may be up to 16 bits (this has not been observed). They are already
 * inverted, scaled by the screen size, and squared.
 *
 *		w = (496/a)^2	->	a = 496/sqrt(w)
 *		h = (384/b)^2	->	b = 384/sqrt(h)
 *
 * This is mostly a guess. It is almost certain, however, based on
 * observations of the Scud Race backfire effect that w and h are related
 * to spotlight size in an inverse-square-root fashion. The spotlight in
 * view 3 should be smaller than in view 4, but the values are actually
 * larger. Here is some data:
 *
 *		View 3:
 *			X,Y=247,342
 *			W,H=24,16
 *			N,F=1e-9,200
 *			Car translation length: 4.93
 *		View 4:
 *			X,Y=247,317
 *			W,H=48,32
 *			N,F=1e-9,200
 *			Car translation length: 7.5
 *
 * The translation length is the total translation vector for the car model
 * extracted by applying the scene matrices. Note that sqrt(48/24) = 1.4
 * and 7.5/4.93 = 1.52, a fairly close match.
 *
 * It remains unknown whether the spotlight parameters are relative to the
 * physical display resolution (496x384), as computed here, or the viewport
 * size. What is needed is an example of a spotlight in a viewport whose
 * dimensions are not 496x384.
 *
 * The spotlight near and far ranges are in viewspace (eye) coordinates.
 * The inverse of the near range is specified and the far range is stored
 * as a displacement (I think) from the near range. Color is RGB111.
 *
 * The spotlight should be smooth at the edges. Using the magnitude of the
 * ellipse test works well -- when it is 1.0, the spotlight should be fully
 * attenuated (0 intensity) and when it is 0.0, render at full intensity.
 *
 * Alpha Processing
 * ----------------
 * When processing "alpha" (translucent) polygons, alpha values range from 0.0,
 * completely transparent, to 1.0, completely opaque. This appears to be the 
 * same convention as for Model 3 and corresponds to a blend mode setting of:
 * glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA).
 *
 * For all texels and colors which do not include an alpha channel, for 
 * translucency to work properly, the alpha channel must be set to opaque.
 * Contour textures use T=1 to indicate transparency, therefore their alpha 
 * value must be inverted.
 * 
 * Translucent Polygons
 * --------------------
 * The 32-level polygon translucency appears to be applied as follows
 *
 *		1. If polygon is untextured, fragment color is the polygon color and
 *		   the translucency level becomes the alpha channel.
 *		2. If contour textures are used, the translucency level becomes the
 *		   alpha channel regardless of the contour bit. I assume that contour
 *		   bit processing is still carried out, if enabled, however.
 *		3. If the texture format is RGBA4, translucency is multiplied by texel
 *		   alpha.
 *		4. Other texture formats: ???
 *
 * A simple way to handle this is to force alpha to 1.0 for polygon colors, 
 * discard fragments if required by the contour setting (forcing alpha to 1.0
 * otherwise), and then in the end, multiplying whatever alpha value remains by
 * the translucency level.
 *
 * List of Safeguards
 * ------------------
 * During boot-up, many games load up scene data that cannot feasibly be
 * processed (way too many models). This occurs in Scud Race and Virtual On 2,
 * for example. This is currently being handled by attempting to detect the
 * defective scenes.
 *
 * 		1. Scud Race: the coordinate system matrix is checked for vectors whose
 *		   magnitudes are not 1.0.
 *		2. Virtual On 2: model 0x200000 is not rendered.
 *
 * There are probably better ways of doing it.
 *
 * To-Do List
 * ----------
 * - Can some of the floating point flag attribs be replaced with ints?
 */

#include <cmath>
#include "Supermodel.h"
#include "Graphics/Shaders3D.h"	// fragment and vertex shaders

// Microsoft doesn't provide isnan() and isinf()
#ifdef _MSC_VER
	#include <float.h>
	#define ISNAN(x)	(_isnan(x))
	#define ISINF(x)	(!_finite(x))
#else
	#define ISNAN(x)	(std::isnan(x))
	#define ISINF(x)	(std::isinf(x))
#endif

/******************************************************************************
 Definitions and Constants
******************************************************************************/

// Shader program files
#define VERTEX_SHADER_FILE		"Src/Graphics/Vertex.glsl"
#define FRAGMENT_SHADER_FILE	"Src/Graphics/Fragment.glsl"

// Model cache settings
#define NUM_STATIC_VERTS		700000	// suggested maximum number of static vertices
#define NUM_DYNAMIC_VERTS		64000	// "" dynamic vertices
#define NUM_LOCAL_VERTS			32768	// size of local vertex buffer
#define NUM_STATIC_MODELS		10000	// maximum number of unique static models to cache
#define NUM_DYNAMIC_MODELS		1024	// maximum number of unique dynamic models to cache
#define NUM_DISPLAY_LIST_ITEMS	10000	// maximum number of model instances displayed per frame

// Scene traversal stack
#define STACK_SIZE				1024


/******************************************************************************
 Texture Management 
******************************************************************************/

void CRender3D::DecodeTexture(int format, int x, int y, int width, int height)
{
	int		xi, yi, i;
	UINT16	texel;
	GLfloat	c, a;
	
	x &= 2047;
	y &= 2047;
	
	if ((x+width)>2048 || (y+height)>2048)
		return;
	if (width > 512 || height > 512)
	{
		//ErrorLog("Encountered a texture that is too large (%d,%d,%d,%d)", x, y, width, height);
		return;
	}
	
	// Map Model3 format to texture unit and texture unit to texture sheet number
	unsigned texUnit = fmtToTexUnit[format];
	unsigned texNum = texUnit % numTexIDs; // there may be less texture sheets than texture units (due to lack of video memory)
	
	// Check to see if ALL texture tiles have been properly decoded on current texture sheet
	if ((textureFormat[texNum][y/32][x/32]==format) && (textureWidth[texNum][y/32][x/32]>=width) && (textureHeight[texNum][y/32][x/32]>=height))
		return;

	//printf("Decoding texture format %u: %u x %u @ (%u, %u) sheet %u\n", format, width, height, x, y, texNum);

	// Copy and decode
	i = 0;
	switch (format)
	{
	default:	// Unknown
		
		for (yi = y; yi < (y+height); yi++)
		{
			for (xi = x; xi < (x+width); xi++)
			{
				textureBuffer[i++] = 0.0;	// R
				textureBuffer[i++] = 0.0;	// G
				textureBuffer[i++] = 1.0f;	// B
				textureBuffer[i++] = 1.0f;	// A
			}
		}
		break;
		
	case 0:	// T1RGB5
		for (yi = y; yi < (y+height); yi++)
		{
			for (xi = x; xi < (x+width); xi++)
			{
				textureBuffer[i++] = (GLfloat) ((textureRAM[yi*2048+xi]>>10)&0x1F) * (1.0f/31.0f);	// R
				textureBuffer[i++] = (GLfloat) ((textureRAM[yi*2048+xi]>>5)&0x1F) * (1.0f/31.0f);	// G
				textureBuffer[i++] = (GLfloat) ((textureRAM[yi*2048+xi]>>0)&0x1F) * (1.0f/31.0f);	// B
				textureBuffer[i++] = ((textureRAM[yi*2048+xi]&0x8000)?0.0f:1.0f);					// T
			}
		}
		break;
		
	case 7:	// RGBA4
		for (yi = y; yi < (y+height); yi++)
		{
			for (xi = x; xi < (x+width); xi++)
			{
				textureBuffer[i++] = (GLfloat) ((textureRAM[yi*2048+xi]>>12)&0xF) * (1.0f/15.0f);	// R
				textureBuffer[i++] = (GLfloat) ((textureRAM[yi*2048+xi]>>8)&0xF) * (1.0f/15.0f);	// G
				textureBuffer[i++] = (GLfloat) ((textureRAM[yi*2048+xi]>>4)&0xF) * (1.0f/15.0f);	// B
				textureBuffer[i++] = (GLfloat) ((textureRAM[yi*2048+xi]>>0)&0xF) * (1.0f/15.0f);	// A
			}
		}
		break;

	case 5:	// 8-bit grayscale
		for (yi = y; yi < (y+height); yi++)
	    {
	        for (xi = x; xi < (x+width); xi++)
	        {
				/*
				texel = textureRAM[yi*2048+xi];
				c = (GLfloat) (texel&0xFF) * (1.0f/255.0f);
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = 1.0;
                */
                // Interpret as 8-bit grayscale
                texel = textureRAM[yi*2048+xi];
                c = (GLfloat) texel * (1.0f/255.0f);
                textureBuffer[i++] = c;
                textureBuffer[i++] = c;
                textureBuffer[i++] = c;
                textureBuffer[i++] = 1.0f;
	        }
	    }

		break;
	
	case 4: // 8-bit, L4A4

		for (yi = y; yi < (y+height); yi++)
		{
			for (xi = x; xi < (x+width); xi++)
	        {
                texel = textureRAM[yi*2048+xi];
				//c = (GLfloat) (~texel&0x0F) * (1.0f/15.0f);
				//a = (GLfloat) ((texel>>4)&0xF) * (1.0f/15.0f);
				c = (GLfloat) ((texel>>4)&0xF) * (1.0f/15.0f);	// seems to work better in Lost World (raptor shadows)
				a = (GLfloat) (texel&0xF) * (1.0f/15.0f);
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = a;
			}
		}

		break;
		
	case 6:	// 8-bit grayscale? (How does this differ from format 5? Alpha values?)
		for (yi = y; yi < (y+height); yi++)
		{
			for (xi = x; xi < (x+width); xi++)
			{
				/*
				texel = textureRAM[yi*2048+xi];
				c = (GLfloat) ((texel>>4)&0xF) * (1.0f/15.0f);
				a = (GLfloat) (texel&0xF) * (1.0f/15.0f);
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = a;
				*/
				texel = textureRAM[yi*2048+xi]&0xFF;
                c = (GLfloat) texel * (1.0f/255.0f);
                textureBuffer[i++] = c;
                textureBuffer[i++] = c;
                textureBuffer[i++] = c;
                textureBuffer[i++] = 1.0f;

			}
		}
		break;
			
	case 2:	// Unknown (all 16 bits appear present in Daytona 2, but only lower 8 bits in Le Mans 24)
		for (yi = y; yi < (y+height); yi++)
		{
			for (xi = x; xi < (x+width); xi++)
			{
                texel = textureRAM[yi*2048+xi];
				a = (GLfloat) ((texel>>4)&0xF) * (1.0f/15.0f);	
				c = (GLfloat) (texel&0xF) * (1.0f/15.0f);
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = a;

				//printf("%04X\n", textureRAM[yi*2048+xi]);				
				/*				
				texel = textureRAM[yi*2048+xi]&0xFF;
				c = (GLfloat) texel * (1.0f/255.0f);
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = 1.0f;
				*/
			}
		}
		break;
		
	case 3:	// Interleaved A4L4 (high byte)
		for (yi = y; yi < (y+height); yi++)
		{
			for (xi = x; xi < (x+width); xi++)
			{
				texel = textureRAM[yi*2048+xi]>>8;
				c = (GLfloat) (texel&0xF) * (1.0f/15.0f);
				a = (GLfloat) (texel>>4) * (1.0f/15.0f);
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = a;
			}
		}
		break;
		
	case 1:	// Interleaved A4L4 (low byte)
		for (yi = y; yi < (y+height); yi++)
		{
			for (xi = x; xi < (x+width); xi++)
			{
				// Interpret as A4L4
				texel = textureRAM[yi*2048+xi]&0xFF;
				c = (GLfloat) (texel&0xF) * (1.0f/15.0f);
				a = (GLfloat) (texel>>4) * (1.0f/15.0f);
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = c;
				textureBuffer[i++] = a;
			}
		}
		break;
	}
		
	// Upload the texture
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glActiveTexture(GL_TEXTURE0 + texUnit);	// activate correct texture unit
	glBindTexture(GL_TEXTURE_2D, texIDs[texNum]); // bind correct texture sheet
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_FLOAT, textureBuffer);
	
	// Mark as decoded
	textureFormat[texNum][y/32][x/32] = format;
	textureWidth[texNum][y/32][x/32] = width;
	textureHeight[texNum][y/32][x/32] = height;
}

// Signals that new textures have been uploaded. Flushes model caches. Be careful not to exceed bounds!
void CRender3D::UploadTextures(unsigned x, unsigned y, unsigned width, unsigned height)
{
	unsigned	texNum, xi, yi;
	
	// Make everything red
#ifdef DEBUG
	for (int i = 0; i < 512*512; )
	{
		textureBuffer[i++] = 1.0f;
		textureBuffer[i++] = 0.0f;
		textureBuffer[i++] = 0.0f;
		textureBuffer[i++] = 1.0f;
	}
#endif

	// Update all texture sheets
	for (texNum = 0; texNum < numTexIDs; texNum++)
	{
		for (xi = x/32; xi < (x+width)/32; xi++)
		{
			for (yi = y/32; yi < (y+height)/32; yi++)
			{
				textureFormat[texNum][yi][xi] = -1;
				textureWidth[texNum][yi][xi] = -1;
				textureHeight[texNum][yi][xi] = -1;
			}
		}
	}
}

/******************************************************************************
 Real3D Address Translation
 
 Functions that interpret word-granular Real3D addresses and return pointers.
******************************************************************************/

// Translates 24-bit culling RAM addresses
const UINT32 *CRender3D::TranslateCullingAddress(UINT32 addr)
{
	addr &= 0x00FFFFFF;	// caller should have done this already
	
	if ((addr>=0x800000) && (addr<0x840000))
		return &cullingRAMHi[addr&0x3FFFF];
	else if (addr < 0x100000)
		return &cullingRAMLo[addr];
	
#ifdef DEBUG
	ErrorLog("TranslateCullingAddress(): invalid address: %06X", addr);
#endif
	return NULL;
}

// Translates model references
const UINT32 *CRender3D::TranslateModelAddress(UINT32 modelAddr)
{
	modelAddr &= 0x00FFFFFF;	// caller should have done this already
	
	if (modelAddr < 0x100000)
		return &polyRAM[modelAddr];
	else
		return &vrom[modelAddr];
}


/******************************************************************************
 Matrix Stack
******************************************************************************/

// Macro to generate column-major (OpenGL) index from y,x subscripts
#define CMINDEX(y,x)	(x*4+y)

/*
 * MultMatrix():
 *
 * Multiplies the matrix stack by the specified Real3D matrix. The matrix 
 * index is a 12-bit number specifying a matrix number relative to the base.
 * The base matrix MUST be set up before calling this function.
 */
void CRender3D::MultMatrix(UINT32 matrixOffset)
{
	GLfloat		m[4*4];
	const float	*src = &matrixBasePtr[matrixOffset*12];
	
	if (matrixBasePtr==NULL)	// LA Machineguns
		return;
	m[CMINDEX(0, 0)] = src[3];
    m[CMINDEX(0, 1)] = src[4];
    m[CMINDEX(0, 2)] = src[5];
    m[CMINDEX(0, 3)] = src[0];
    m[CMINDEX(1, 0)] = src[6];
    m[CMINDEX(1, 1)] = src[7];
    m[CMINDEX(1, 2)] = src[8];
    m[CMINDEX(1, 3)] = src[1];
    m[CMINDEX(2, 0)] = src[9];
    m[CMINDEX(2, 1)] = src[10];
    m[CMINDEX(2, 2)] = src[11];
    m[CMINDEX(2, 3)] = src[2];
    m[CMINDEX(3, 0)] = 0.0;
    m[CMINDEX(3, 1)] = 0.0;
    m[CMINDEX(3, 2)] = 0.0;
	m[CMINDEX(3, 3)] = 1.0;	
	
	glMultMatrixf(m);
}

/*
 * InitMatrixStack():
 *
 * Initializes the modelview (model space -> view space) matrix stack and 
 * Real3D coordinate system. These are the last transforms to be applied (and
 * the first to be defined on the stack) before projection.
 *
 * Model 3 games tend to define the following unusual base matrix:
 *
 *		0	0	-1	0
 *		1	0	0	0
 *		0	-1	0	0
 *		0	0	0	1
 *
 * When this is multiplied by a column vector, the output is:
 *
 *		-Z
 *		X
 *		-Y
 *		1
 *
 * My theory is that the Real3D GPU accepts vectors in Z,X,Y order. The games
 * store everything as X,Y,Z and perform the translation at the end. The Real3D
 * also has Y and Z coordinates opposite of the OpenGL convention. This
 * function inserts a compensating matrix to undo these things.
 *
 * NOTE: This function assumes we are in GL_MODELVIEW matrix mode.
 */

void CRender3D::InitMatrixStack(UINT32 matrixBaseAddr)
{
	GLfloat m[4*4];

	// This matrix converts vectors back from the weird Model 3 Z,X,Y ordering
	// and also into OpenGL viewspace (-Y,-Z)
	m[CMINDEX(0,0)]=0.0;	m[CMINDEX(0,1)]=1.0;	m[CMINDEX(0,2)]=0.0;	m[CMINDEX(0,3)]=0.0;
	m[CMINDEX(1,0)]=0.0;	m[CMINDEX(1,1)]=0.0;	m[CMINDEX(1,2)]=-1.0;	m[CMINDEX(1,3)]=0.0;
	m[CMINDEX(2,0)]=-1.0;	m[CMINDEX(2,1)]=0.0;	m[CMINDEX(2,2)]=0.0;	m[CMINDEX(2,3)]=0.0;
	m[CMINDEX(3,0)]=0.0;	m[CMINDEX(3,1)]=0.0;	m[CMINDEX(3,2)]=0.0;	m[CMINDEX(3,3)]=1.0;
	
	if (step > 0x10)
		glLoadMatrixf(m);
	else
	{
		// Scaling seems to help w/ Step 1.0's extremely large coordinates
		GLfloat s = 1.0f/2048.0f;
		glLoadIdentity();
		glScalef(s,s,s);
		glMultMatrixf(m);
	}
	
	// Set matrix base address and apply matrix #0 (coordinate system matrix)
	matrixBasePtr = (float *) TranslateCullingAddress(matrixBaseAddr);
	MultMatrix(0);
}


/******************************************************************************
 Scene Database
 
 Complete scene database traversal and rendering.
******************************************************************************/

/*
 * DrawModel():
 *
 * Draw the specified model (adds it to the display list). This is where vertex
 * buffer overflows and display list overflows will be detected. An attempt is
 * made to salvage the situation if this occurs, so if DrawModel() returns
 * FAIL, it is a serious matter and rendering should be aborted for the frame.
 *
 * The current texture offset state, texOffset, is also used. Models are cached
 * for each unique texOffset.
 */
bool CRender3D::DrawModel(UINT32 modelAddr)
{
	ModelCache		*Cache;
	const UINT32	*model;
	int				lutIdx;
	struct VBORef	*ModelRef;
	
	//if (modelAddr==0x7FFF00)	// Fighting Vipers (this is not polygon data!)
	//	return;
	if (modelAddr == 0x200000)	// Virtual On 2 (during boot-up, causes slow-down)
		return OKAY;
	model = TranslateModelAddress(modelAddr);
	
	// Determine whether model is in polygon RAM or VROM
	if (modelAddr < 0x100000)
		Cache = &PolyCache;
	else
		Cache = &VROMCache;
		
	// Look up the model in the LUT and cache it if necessary
	lutIdx = modelAddr&0xFFFFFF;
	ModelRef = LookUpModel(Cache, lutIdx, texOffset);
	if (NULL == ModelRef)
	{
		// Attempt to cache the model
		ModelRef = CacheModel(Cache, lutIdx, texOffset, model);
		if (NULL == ModelRef)
		{
			// Model could not be cached. Render what we have so far and try again.
			DrawDisplayList(&VROMCache, POLY_STATE_NORMAL);
			DrawDisplayList(&PolyCache, POLY_STATE_NORMAL);
			DrawDisplayList(&VROMCache, POLY_STATE_ALPHA);
			DrawDisplayList(&PolyCache, POLY_STATE_ALPHA);
			ClearModelCache(&VROMCache);
			ClearModelCache(&PolyCache);
			
			// Try caching again...
			ModelRef = CacheModel(Cache, lutIdx, texOffset, model);
			if (NULL == ModelRef)
				return ErrorUnableToCacheModel(modelAddr);	// nothing we can do :(
		}
	}

	// If cache is static then decode all the texture references contained in the cached model
	// before rendering (models in dynamic cache will have been decoded already in CacheModel)
	if (!Cache->dynamic)
		ModelRef->texRefs.DecodeAllTextures(this);

	// Add to display list
	return AppendDisplayList(Cache, false, ModelRef);
}

// Descends into a 10-word culling node
void CRender3D::DescendCullingNode(UINT32 addr)
{
	const UINT32	*node, *lodTable;
	UINT32			matrixOffset, node1Ptr, node2Ptr;
	float			x, y, z, oldTexOffsetX, oldTexOffsetY;
	int				tx, ty;
	UINT16			oldTexOffset;
	
	++stackDepth;
	// Stack depth of 64 is too small for Star Wars Trilogy (Hoth)
	if (stackDepth>=(512+64))	// safety (prevent overflows -- OpenGL matrix stack will still overflow by this point)
	{
		--stackDepth;
		return;
	}

	node = TranslateCullingAddress(addr);
	if (NULL == node)
	{
		--stackDepth;
		return;
	}

	// Debug: texture offset? (NOTE: offsets 1 and 2 don't exist on step 1.0)
	//if (node[0x02]&0xFFFF)
	//	printf("%X -> %02X %04X\n", addr, node[0x00]&0xFF, node[0x02]&0xFFFF);
		
	// Extract known fields
	node1Ptr		= node[0x07-offset];
	node2Ptr		= node[0x08-offset];
	matrixOffset	= node[0x03-offset]&0xFFF;
	x				= *(float *) &node[0x04-offset];
	y				= *(float *) &node[0x05-offset];
	z				= *(float *) &node[0x06-offset];
	
	// Texture offset?
	oldTexOffsetX = texOffsetXY[0];	// save old offsets
	oldTexOffsetY = texOffsetXY[1];
	oldTexOffset = texOffset;
	if (!offset)	// Step 1.5+
	{
		tx = 32*((node[0x02]>>7)&0x3F);
		ty = 32*(node[0x02]&0x3F) + ((node[0x02]&0x4000)?1024:0);	// TODO: 5 or 6 bits for Y coord?
		if ((node[0x02]&0x8000))	// apply texture offsets, else retain current ones
		{
			texOffsetXY[0] = (GLfloat) tx;
			texOffsetXY[1] = (GLfloat) ty;
			texOffset = node[0x02]&0x7FFF;
			//printf("Tex Offset: %d, %d (%08X %08X)\n", tx, ty, node[0x02], node1Ptr);
		}
	}
	
	// Apply matrix and translation
	glPushMatrix();
	if ((node[0x00]&0x10))	// apply translation vector
		glTranslatef(x,y,z);
	else if (matrixOffset)	// multiply matrix, if specified
		MultMatrix(matrixOffset);
		
	// Descend down first link
	if ((node[0x00]&0x08))	// 4-element LOD table
	{
		lodTable = TranslateCullingAddress(node1Ptr);
		if (NULL != lodTable)
		{
			if ((node[0x03-offset]&0x20000000))
				DescendCullingNode(lodTable[0]&0xFFFFFF);
			else
				DrawModel(lodTable[0]&0xFFFFFF);
		}
	}
	else
		DescendNodePtr(node1Ptr);
		
	// Proceed to second link
	glPopMatrix();
	DescendNodePtr(node2Ptr);
	--stackDepth;
	
	// Restore old texture offsets
	texOffsetXY[0] = oldTexOffsetX;
	texOffsetXY[1] = oldTexOffsetY;
	texOffset = oldTexOffset;
}

// A list of pointers. MAME assumes that these may only point to culling nodes.
void CRender3D::DescendPointerList(UINT32 addr)
{
	const UINT32	*list;
	UINT32			nodeAddr;
	int				listEnd;
	
	if (listDepth > 2)	// several Step 2.1 games require this safeguard
		return;
	
	list = TranslateCullingAddress(addr);
	if (NULL == list)
		return;
		
	++listDepth;
		
	// Traverse the list forward and print it out
	listEnd = 0;
	while (1)
	{	
		if ((list[listEnd] & 0x02000000))	// end of list (?)
			break;
		
		if ((list[listEnd] == 0) || (((list[listEnd])>>24) != 0))
		{
			//printf("ATTENTION: Unknown list termination: %08X.\n", list[listEnd]);
			listEnd--;	// back up to last valid list element
			break;
		}
		
		++listEnd;
	}

	// Traverse the list backward and descend into each pointer
	while (listEnd >= 0)
	{
		nodeAddr = list[listEnd]&0x00FFFFFF;	// clear upper 8 bits to ensure this is processed as a culling node
		if (!(list[listEnd]&0x01000000))//Fighting Vipers
		{
			if ((nodeAddr != 0) && (nodeAddr != 0x800800))
			{
				DescendCullingNode(nodeAddr);
			}
			//else
			//	printf("Strange pointers encountered\n");
		}
		--listEnd;
	}
	
	--listDepth;
}

/*
 * DescendNodePtr():
 *
 * The old scene traversal engine. Recursively descends into a node pointer.
 */
void CRender3D::DescendNodePtr(UINT32 nodeAddr)
{		
	// Ignore null links
	if ((nodeAddr&0x00FFFFFF) == 0)
		return;
		
	switch ((nodeAddr>>24)&0xFF)	// pointer type encoded in upper 8 bits
	{
	case 0x00:	// culling node
		DescendCullingNode(nodeAddr&0xFFFFFF);
		break;
	case 0x01:	// model (perhaps bit 1 is a flag in this case?)
	case 0x03:
		DrawModel(nodeAddr&0xFFFFFF);
		break;
	case 0x04:	// pointer list
		DescendPointerList(nodeAddr&0xFFFFFF);
		break;
	default:
		//printf("ATTENTION: Unknown pointer format: %08X\n\n", nodeAddr);
		break;
	}
}

// Draws viewports of the given priority
void CRender3D::RenderViewport(UINT32 addr, int pri)
{
	GLfloat			color[8][3] =			// RGB1 translation
					{
						{ 0.0, 0.0, 0.0 },	// off
						{ 0.0, 0.0, 1.0 },	// blue
						{ 0.0, 1.0, 0.0 },	// green
						{ 0.0, 1.0, 1.0 },	// cyan
						{ 1.0, 0.0, 0.0 }, 	// red
						{ 1.0, 0.0, 1.0 },	// purple
						{ 1.0, 1.0, 0.0 },	// yellow
						{ 1.0, 1.0, 1.0 }	// white
					};
	const UINT32	*vpnode;
	UINT32			nextAddr, nodeAddr, matrixBase;
	int				curPri;
	int				vpX, vpY, vpWidth, vpHeight;
	int				spotColorIdx;
	GLfloat			vpTopAngle, vpBotAngle, fovYDegrees;
	GLfloat			scrollFog, scrollAtt;
	
	// Translate address and obtain pointer
	vpnode = TranslateCullingAddress(addr);
	if (NULL == vpnode)
		return;
		
	curPri		= (vpnode[0x00] >> 3) & 3;	// viewport priority
	nextAddr	= vpnode[0x01] & 0xFFFFFF;	// next viewport
	nodeAddr	= vpnode[0x02];				// scene database node pointer

	// Recursively process next viewport
	if (vpnode[0x01] == 0)		// memory probably hasn't been set up yet, abort
		return;
 	if (vpnode[0x01] != 0x01000000)
		RenderViewport(vpnode[0x01],pri);

	// If the priority doesn't match, do not process
	if (curPri != pri)
		return;
	
	// Fetch viewport parameters (TO-DO: would rounding make a difference?)
	vpX			= (vpnode[0x1A]&0xFFFF)>>4;		// viewport X (12.4 fixed point)
	vpY			= (vpnode[0x1A]>>20)&0xFFF;		// viewport Y (12.4)
	vpWidth		= (vpnode[0x14]&0xFFFF)>>2;		// width (14.2)
	vpHeight	= (vpnode[0x14]>>18)&0x3FFF;	// height (14.2)
	matrixBase 	= vpnode[0x16]&0xFFFFFF;		// matrix base address
	
	// Field of view and clipping
	vpTopAngle	= (float) asin(*(float *)&vpnode[0x0E]);	// FOV Y upper half-angle (radians)
	vpBotAngle	= (float) asin(*(float *)&vpnode[0x12]);	// FOV Y lower half-angle
	fovYDegrees	= (vpTopAngle+vpBotAngle)*(float)(180.0/3.14159265358979323846);
	// TO-DO: investigate clipping planes
	
	// Set up viewport and projection (TO-DO: near and far clipping)
	glMatrixMode(GL_PROJECTION);
 	glLoadIdentity();
	if (g_Config.wideScreen && (vpX==0) && (vpWidth>=495) && (vpY==0) && (vpHeight >= 383))		// only expand viewports that occupy whole screen
	{
		// Wide screen hack only modifies X axis and not the Y FOV
		viewportX 	   = 0;
		viewportY 	   = yOffs + (GLint) ((float)(384-(vpY+vpHeight))*yRatio);
		viewportWidth  = totalXRes;
		viewportHeight = (GLint) ((float)vpHeight*yRatio);
		gluPerspective(fovYDegrees,(GLfloat)viewportWidth/(GLfloat)viewportHeight,0.1f,1e5);	// use actual full screen ratio to get proper X FOV
		//printf("viewportX=%d, viewportY=%d, viewportWidth=%d, viewportHeight=%d\tvpY=%d vpHeight=%d\n", viewportX, viewportY, viewportWidth, viewportHeight, vpY,vpHeight);
	}
	else
	{
		viewportX 	   = xOffs + (GLint) ((float)vpX*xRatio);
		viewportY 	   = yOffs + (GLint) ((float)(384-(vpY+vpHeight))*yRatio);
		viewportWidth  = (GLint) ((float)vpWidth*xRatio);
		viewportHeight = (GLint) ((float)vpHeight*yRatio);
		gluPerspective(fovYDegrees,(GLfloat)vpWidth/(GLfloat)vpHeight,0.1f,1e5);				// use Model 3 viewport ratio
	}
	
	// Lighting (note that sun vector points toward sun -- away from vertex)
 	lightingParams[0] = *(float *) &vpnode[0x05];							// sun X
 	lightingParams[1] = *(float *) &vpnode[0x06];							// sun Y
 	lightingParams[2] = *(float *) &vpnode[0x04];							// sun Z
 	lightingParams[3] = *(float *) &vpnode[0x07];							// sun intensity
 	lightingParams[4] = (float) ((vpnode[0x24]>>8)&0xFF) * (1.0f/255.0f);	// ambient intensity
 	lightingParams[5] = 0.0;	// reserved
 	 	 
 	// Spotlight
	spotColorIdx	= (vpnode[0x20]>>11)&7;					// spotlight color index
	spotEllipse[0] 	= (float) ((vpnode[0x1E]>>3)&0x1FFF);	// spotlight X position (fractional component?)
	spotEllipse[1] 	= (float) ((vpnode[0x1D]>>3)&0x1FFF);	// spotlight Y
	spotEllipse[2] 	= (float) ((vpnode[0x1E]>>16)&0xFFFF);	// spotlight X size (16-bit? May have fractional component below bit 16)
	spotEllipse[3] 	= (float) ((vpnode[0x1D]>>16)&0xFFFF);	// spotlight Y size
	spotRange[0]	= 1.0f/(*(float *) &vpnode[0x21]);		// spotlight start
	spotRange[1]	= *(float *) &vpnode[0x1F];				// spotlight extent
	spotColor[0]	= color[spotColorIdx][0];				// spotlight color
	spotColor[1]	= color[spotColorIdx][1];
	spotColor[2]	= color[spotColorIdx][2];
	//printf("(%g,%g),(%g,%g),(%g,%g) -> \n", spotEllipse[0], spotEllipse[1], spotEllipse[2], spotEllipse[3], spotRange[0], spotRange[1]);
	
	// Spotlight is applied on a per pixel basis, must scale its position and size to screen
	spotEllipse[1] = 384.0f-spotEllipse[1];
	spotRange[1] += spotRange[0];	// limit
	spotEllipse[2] = 496.0f/sqrt(spotEllipse[2]);	// spotlight appears to be specified in terms of physical resolution (unconfirmed)
	spotEllipse[3] = 384.0f/sqrt(spotEllipse[3]);

	// Scale the spotlight to the OpenGL viewport
	spotEllipse[0] = spotEllipse[0]*xRatio + xOffs;
	spotEllipse[1] = spotEllipse[1]*yRatio + yOffs;
	spotEllipse[2] *= xRatio;
	spotEllipse[3] *= yRatio;

 	// Fog
 	fogParams[0] = (float) ((vpnode[0x22]>>16)&0xFF) * (1.0f/255.0f);	// fog color R
	fogParams[1] = (float) ((vpnode[0x22]>>8)&0xFF) * (1.0f/255.0f);	// fog color G
	fogParams[2] = (float) ((vpnode[0x22]>>0)&0xFF) * (1.0f/255.0f);	// fog color B
	fogParams[3] = *(float *) &vpnode[0x23];							// fog density
	fogParams[4] = (float) (INT16) (vpnode[0x25]&0xFFFF)*(1.0f/255.0f);	// fog start
	if (ISINF(fogParams[3]) || ISNAN(fogParams[3]) || ISINF(fogParams[4]) || ISNAN(fogParams[4]))	// Star Wars Trilogy
		fogParams[3] = fogParams[4] = 0.0f;
	
	// Unknown light/fog parameters
	scrollFog = (float) (vpnode[0x20]&0xFF) * (1.0f/255.0f);	// scroll fog
	scrollAtt = (float) (vpnode[0x24]&0xFF) * (1.0f/255.0f);	// scroll attenuation
	//printf("scrollFog = %g, scrollAtt = %g\n", scrollFog, scrollAtt);
	//printf("Fog: R=%02X G=%02X B=%02X density=%g (%X) %d start=%g\n", ((vpnode[0x22]>>16)&0xFF), ((vpnode[0x22]>>8)&0xFF), ((vpnode[0x22]>>0)&0xFF), fogParams[3], vpnode[0x23], (fogParams[3]==fogParams[3]), fogParams[4]);
 	
 	// Clear texture offsets before proceeding
 	texOffsetXY[0] = 0.0;
 	texOffsetXY[1] = 0.0;
 	texOffset = 0x0000;
 	
 	// Set up coordinate system and base matrix
 	glMatrixMode(GL_MODELVIEW);
 	InitMatrixStack(matrixBase);
 	
 	// Safeguard: weird coordinate system matrices usually indicate scenes that will choke the renderer
 	if (NULL != matrixBasePtr)
 	{
 		float	m21, m32, m13;
 		
 		// Get the three elements that are usually set and see if their magnitudes are 1
 		m21 = matrixBasePtr[6];
 		m32 = matrixBasePtr[10];
 		m13 = matrixBasePtr[5];
 		
 		m21 *= m21;
 		m32 *= m32;
 		m13 *= m13;

 		if ((m21>1.05) || (m21<0.95))
 			return;
 		if ((m32>1.05) || (m32<0.95))
 			return;
 		if ((m13>1.05) || (m13<0.95))
 			return;
 	}
 	
 	// Render
 	AppendDisplayList(&VROMCache, true, 0);	// add a viewport display list node
 	AppendDisplayList(&PolyCache, true, 0);
 	stackDepth = 0;
 	listDepth = 0;
 	
 	// Descend down the node link: Use recursive traversal
 	DescendNodePtr(nodeAddr);
}

void CRender3D::RenderFrame(void)
{
	// Begin frame
	ClearErrors();	// must be cleared each frame
	//printf("BEGIN FRAME\n");
	
	// Z buffering (Z buffer is cleared by display list viewport nodes)
	glDepthFunc(GL_LESS);
 	glEnable(GL_DEPTH_TEST);
	
	// Bind Real3D shader program and texture maps
	glUseProgram(shaderProgram);
	for (unsigned fmt = 0; fmt < 8; fmt++)
	{
		// Map Model3 format to texture unit and texture unit to texture sheet number
		unsigned texUnit = fmtToTexUnit[fmt];
		unsigned texNum = texUnit % numTexIDs; // there may be less texture sheets than texture units (due to lack of video memory)
		glActiveTexture(GL_TEXTURE0 + texUnit);	// activate correct texture unit
		glBindTexture(GL_TEXTURE_2D, texIDs[texNum]); // bind correct texture sheet
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// fragment shader performs its own interpolation
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	
	// Enable VBO client states
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableVertexAttribArray(subTextureLoc);
	glEnableVertexAttribArray(texParamsLoc);
	glEnableVertexAttribArray(texFormatLoc);
	glEnableVertexAttribArray(transLevelLoc);
	glEnableVertexAttribArray(lightEnableLoc);
	glEnableVertexAttribArray(shininessLoc);
	glEnableVertexAttribArray(fogIntensityLoc);
	
	// Draw
	//ClearModelCache(&VROMCache);	// debug
	ClearModelCache(&PolyCache);
	for (int pri = 0; pri <= 3; pri++)
	{
		glClear(GL_DEPTH_BUFFER_BIT);
		//ClearModelCache(&PolyCache);
		ClearDisplayList(&PolyCache);
		ClearDisplayList(&VROMCache);
		RenderViewport(0x800000,pri);
		DrawDisplayList(&VROMCache, POLY_STATE_NORMAL);
		DrawDisplayList(&PolyCache, POLY_STATE_NORMAL);
		DrawDisplayList(&VROMCache, POLY_STATE_ALPHA);
		DrawDisplayList(&PolyCache, POLY_STATE_ALPHA);
	}
	glFrontFace(GL_CW);	// restore front face
	
	// Disable VBO client states
	glDisableVertexAttribArray(fogIntensityLoc);
	glDisableVertexAttribArray(shininessLoc);
	glDisableVertexAttribArray(lightEnableLoc);
	glDisableVertexAttribArray(transLevelLoc);
	glDisableVertexAttribArray(texFormatLoc);
	glDisableVertexAttribArray(texParamsLoc);
	glDisableVertexAttribArray(subTextureLoc);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void CRender3D::EndFrame(void)
{
}

void CRender3D::BeginFrame(void)
{
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void CRender3D::AttachMemory(const UINT32 *cullingRAMLoPtr, const UINT32 *cullingRAMHiPtr, const UINT32 *polyRAMPtr, const UINT32 *vromPtr, const UINT16 *textureRAMPtr)
{
	cullingRAMLo = cullingRAMLoPtr;
	cullingRAMHi = cullingRAMHiPtr;
	polyRAM = polyRAMPtr;
	vrom = vromPtr;
	textureRAM = textureRAMPtr;
	DebugLog("Render3D attached Real3D memory regions\n");
}

void CRender3D::SetStep(int stepID)
{
	step = stepID;
	
	if ((step!=0x10) && (step!=0x15) && (step!=0x20) && (step!=0x21))
	{
		DebugLog("Render3D: Unrecognized stepping: %d.%d\n", (step>>4)&0xF, step&0xF);
		step = 0x10;
	}
	
	if (step > 0x10)
	{
		offset = 0;						// culling nodes are 10 words
		vertexFactor = (1.0f/2048.0f);	// vertices are in 13.11 format
	}
	else
	{
		offset = 2;						// 8 words
		vertexFactor = (1.0f/128.0f);	// 17.7
	}
	
	DebugLog("Render3D set to Step %d.%d\n", (step>>4)&0xF, step&0xF);
}
	
bool CRender3D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXResParam, unsigned totalYResParam)
{
	// Allocate memory for texture buffer
	textureBuffer = new(std::nothrow) GLfloat[512*512*4];
	if (NULL == textureBuffer)
		return ErrorLog("Insufficient memory for texture decode buffer.");
		
	glGetError(); // clear error flag
	
	// Create model caches and VBOs
	if (CreateModelCache(&VROMCache, NUM_STATIC_VERTS, NUM_LOCAL_VERTS, NUM_STATIC_MODELS, 0x4000000/4, NUM_DISPLAY_LIST_ITEMS, false))
		return FAIL;
	if (CreateModelCache(&PolyCache, NUM_DYNAMIC_VERTS, NUM_LOCAL_VERTS, NUM_DYNAMIC_MODELS, 0x400000/4, NUM_DISPLAY_LIST_ITEMS, true))
		return FAIL;

	// Initialize lighting parameters (updated as viewports are traversed)
	lightingParams[0] = 0.0;
	lightingParams[1] = 0.0;
	lightingParams[2] = 0.0;
	lightingParams[3] = 0.0;
	lightingParams[4] = 1.0;	// full ambient intensity in case we want to render a standalone model
	lightingParams[5] = 0.0;

	// Resolution and offset within physical display area
    xRatio = (GLfloat) xRes / 496.0f;
    yRatio = (GLfloat) yRes / 384.0f;
    xOffs = xOffset;
    yOffs = yOffset;
    totalXRes = totalXResParam;
    totalYRes = totalYResParam;

	// Query max number of texture units supported by video card to use as upper limit
	GLint glMaxTexUnits;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &glMaxTexUnits);
	unsigned maxTexUnits = max<int>(1, min<int>(g_Config.maxTexUnits, glMaxTexUnits));
	
	// Load shaders. Use multi-sheet shader if requested and possible.
	const char *vsFile = g_Config.vertexShaderFile.size() ? g_Config.vertexShaderFile.c_str() : NULL;
	const char *fsFile = g_Config.fragmentShaderFile.size() ? g_Config.fragmentShaderFile.c_str() : NULL;
	const char *fragmentShaderSource = fragmentShaderSingleSheetSource;	// single texture shader
	if (g_Config.multiTexture)
	{ 
		if (maxTexUnits >= 8)	// can we use the multi-sheet shader?
			fragmentShaderSource = fragmentShaderMultiSheetSource;
		else
			ErrorLog("Your system has too few texture units. Reverting to single texture shader.");
	}
	if (OKAY != LoadShaderProgram(&shaderProgram,&vertexShader,&fragmentShader,vsFile,fsFile,vertexShaderSource,fragmentShaderSource))
		return FAIL;
	
	// Try locating default "textureMap" uniform in shader program
	glUseProgram(shaderProgram); // bind program
	textureMapLoc = glGetUniformLocation(shaderProgram, "textureMap");
	unsigned unitCount = 0;
	if (textureMapLoc != -1)
	{
		// If exists, then bind to first texture unit
		unsigned texUnit = unitCount % maxTexUnits;
		glUniform1i(textureMapLoc, texUnit++);
		unitCount++;
	}
	
	// Try locating "textureMap[0-7]" uniforms in shader program
	for (unsigned fmt = 0; fmt < 8; fmt++)
	{
		char uniformName[12];
		sprintf(uniformName, "textureMap%u", fmt);
		textureMapLocs[fmt] = glGetUniformLocation(shaderProgram, uniformName);	
		if (textureMapLocs[fmt] != -1)
		{
			// If exists, then bind to next texture unit
			unsigned texUnit = unitCount % maxTexUnits;
			glUniform1i(textureMapLocs[fmt], texUnit);
			fmtToTexUnit[fmt] = texUnit;
			unitCount++;
		}
		else
		{
			// Otherwise bind to first texture unit by default
			fmtToTexUnit[fmt] = 0;
		}
	}
	numTexUnits = min<int>(unitCount, maxTexUnits);

	// Check located at least one uniform to bind to a texture unit
	if (numTexUnits == 0)
		return ErrorLog("Fragment shader must contain at least one 'textureMap' uniform.");
	InfoLog("Located and bound %u uniform(s) in GL shader script to %u texture unit(s).", numTexUnits, unitCount);

	// Now try creating texture sheets, one for each texture unit (memory permitting)
	numTexIDs = numTexUnits;
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(numTexIDs, texIDs);
	for (unsigned texNum = 0; texNum < numTexIDs; texNum++)
	{
		glActiveTexture(GL_TEXTURE0 + texNum); // activate correct texture unit
		glBindTexture(GL_TEXTURE_2D, texIDs[texNum]); // bind correct texture sheet
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// fragment shader performs its own interpolation
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2048, 2048, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 0);
		if (glGetError() != GL_NO_ERROR)
		{
			// Probably ran out of video memory, so don't try creating any more texture sheets
			numTexIDs = texNum;
			glGetError(); // clear error flag
			break;
		}
	}

	// Check created at least one texture sheet
	if (numTexIDs == 0)
		return ErrorLog("OpenGL was unable to provide any 2048x2048-texel texture maps.");
	InfoLog("Created %u 2048x2048-texel GL texture map(s)", numTexIDs);

	// Get location of the rest of the uniforms
	modelViewMatrixLoc = glGetUniformLocation(shaderProgram,"modelViewMatrix");
	projectionMatrixLoc = glGetUniformLocation(shaderProgram,"projectionMatrix");
	lightingLoc = glGetUniformLocation(shaderProgram, "lighting");
	spotEllipseLoc = glGetUniformLocation(shaderProgram, "spotEllipse");
	spotRangeLoc = glGetUniformLocation(shaderProgram, "spotRange");
	spotColorLoc = glGetUniformLocation(shaderProgram, "spotColor");
	
	// Get locations of custom vertex attributes
	subTextureLoc = glGetAttribLocation(shaderProgram,"subTexture");
	texParamsLoc = glGetAttribLocation(shaderProgram,"texParams");
	texFormatLoc = glGetAttribLocation(shaderProgram,"texFormat");
	transLevelLoc = glGetAttribLocation(shaderProgram,"transLevel");
	lightEnableLoc = glGetAttribLocation(shaderProgram,"lightEnable");
	shininessLoc = glGetAttribLocation(shaderProgram,"shininess");
	fogIntensityLoc = glGetAttribLocation(shaderProgram,"fogIntensity");
	
	// Additional OpenGL stuff
	glFrontFace(GL_CW);		// polygons are uploaded w/ clockwise winding
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glClearDepth(1.0);
	glEnable(GL_TEXTURE_2D);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Mark all textures as dirty
	UploadTextures(0,0,2048,2048);

	DebugLog("Render3D initialized\n");
	return OKAY;
}

CRender3D::CRender3D(void)
{	
	cullingRAMLo = NULL;
	cullingRAMHi = NULL;
	polyRAM = NULL;
	vrom = NULL;
	textureRAM = NULL;
	textureBuffer = NULL;
	
	// Clear model cache pointers so we can safely destroy them if init fails
	for (int i = 0; i < 2; i++)
	{
		VROMCache.verts[i] = NULL;
		PolyCache.verts[i] = NULL;
		VROMCache.Models = NULL;
		PolyCache.Models = NULL;
		VROMCache.lut = NULL;
		PolyCache.lut = NULL;
		VROMCache.List = NULL;
		PolyCache.List = NULL;
		VROMCache.ListHead[i] = NULL;
		PolyCache.ListHead[i] = NULL;
		VROMCache.ListTail[i] = NULL;
		PolyCache.ListTail[i] = NULL;
	}
	
	DebugLog("Built Render3D\n");
}

CRender3D::~CRender3D(void)
{
	DestroyShaderProgram(shaderProgram,vertexShader,fragmentShader);
	if (glBindBuffer != NULL)	// we may have failed earlier due to lack of OpenGL 2.0 functions	
		glBindBuffer(GL_ARRAY_BUFFER, 0);	// disable VBOs by binding to 0
	glDeleteTextures(numTexIDs, texIDs);
	
	DestroyModelCache(&VROMCache);
	DestroyModelCache(&PolyCache);
	
	cullingRAMLo = NULL;
	cullingRAMHi = NULL;
	polyRAM = NULL;
	vrom = NULL;
	textureRAM = NULL;
	
	if (textureBuffer != NULL)
		delete [] textureBuffer;
	textureBuffer = NULL;

	DebugLog("Destroyed Render3D\n");
}
