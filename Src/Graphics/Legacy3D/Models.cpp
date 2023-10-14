/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2016 Bart Trzynadlowski, Nik Henson 
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
 * Models.cpp
 *
 * Model parsing, caching, and drawing.
 *
 * Polygon header bits:
 *
 *    31  30  29  28  27  26  25  24  23  22  21  20  19  18  17  16  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 0 | S | S | S | S | S | S | CW| ID| ID| ID| ID| ID| ID| ID| ID| ID| ID| ID| ID| ID| ID| ID|DS1|DS2| SE|TYP|PNT|SMO|LNK|LNK|LNK|LNK|
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 1 | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X | X |EDG|TAS|FIX|DBL|SMT|END|COL|LOS|
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 2 | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |MTX|MTX|MTX|MTE|MLD|MLD|TXM|TYM|
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 3 | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z | Z |TXS|TYS|TXW|TXW|TXW|TXH|TXH|TXH|
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 4 |S/R|S/R|S/R|S/R|S/R|S/R|S/R|S/R|S/G|S/G|S/G|S/G|I/G|I/G|I/G|I/G|I/B|I/B|I/B|I/B|I/B|I/B|I/B|I/B|MAP| TP|TX?| TX| TX| TX| TX| TX|
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 5 |TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP|TNP| TX|TY?|TY?| TY| TY| TY| TY| TY|
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 6 |TRN|MOF|MOF|MOF|MOF|MOF|MOF|MOF|OPQ| TL| TL| TL| TL| TL|PAT|LUM|FOG|FOG|FOG|FOG|FOG|TEN|TFM|TFM|TFM|SHI|SHI|HIP|LAY|TLM|TLM|TLM|
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 *  S   Specular                  Specular coefficient
 *  CW  Clockwise                 Clockwise data
 *  ID  Node Identifier           A numerical identifier that increments for each polygon in the model
 *  DS1 Discard 1                 Indicates polygon is invalid and must be discarded
 *  DS2 Discard 2                 Indicates polygon is invalid and must be discarded
 *  SE  Enable specular           Enable specular lighting
 *  TYP Vertex Count (Type)       0 = Triangle, 1 = Quad
 *  PNT Polygon is Points         ?
 *  SMO Smoothing                 ?
 *  LNK Link                      Strip link information
 *  X   Normal X                  Lighting normal X component (2.22 fixed-point)
 *  Y   Normal Y                  
 *  Z   Normal Z                  
 *  EDG Edge On Translucency      ?
 *  TAS Texture Address Scale     Texture coordinate format: 0 = 13.3 unsigned fixed-point, 1 = unsigned 16 bits
 *  FIX Fixed Shading             0 = Vertex normals are present, 1 = Vertex normal X contains fixed shading intensity
 *  DBL Double-sided              0 = Single-sided polygon, 1 = Double-sided
 *  SMT Smooth Shading            0 = Flat shading (use polygon normal), 1 = Smooth shading (use vertex normals)
 *  END Last polygon              1 = Last polygon in model
 *  COL Actual Color              Polygon color format: 0 = Color table index (S, I), 1 = RGB
 *  LOS No LOS Return             ?
 *  MTX Microtexture Map Select   
 *  MTE Microtexture Enable       0 = No microtexture, 1 = Microtexture enabled
 *  MLD Microtexture Min LOD      ?
 *  TXM X Mirror                  Texture repeat mode: 0 = Wrap, 1 = Mirror
 *  TYM Y Mirror                  
 *  TXS X Wrap Smoothing          0 = No wrap smoothing, 1 = Affects texture sampling across wrapped boundary
 *  TYS Y Wrap Smoothing          
 *  TXW Map Size X                Texture map size: 000 = 32, 001 = 64, 010 = 128, 011 = 256, 100 = 512
 *  TXH Map Size Y                
 *  S   Sensor color index        Color table index when using sensor colors (unknown how to enable)
 *  I   Color index               Color table index for non-sensor color mode
 *  R   Red                     
 *  G   Green                   
 *  B   Blue                    
 *  MAP Translator Map Select     ?
 *  TP  Even Bank Select          Texture page: 0 = First 2048x1024 page, 1 = Second page
 *  TX  Texture X                 Texture X position within page in units of 32 pixels
 *  TY  Texture Y                 Texture Y position within page in units of 32 pixels
 *  TNP Texture NP                ?
 *  TRN Contour Texture Enable    0 = No transparency, 1 = Process A1RGB5 pixels with A set as transparent (note: affects other alpha formats, too)
 *  MOF Translator Map Offset     ?
 *  OPQ Opaque                    0 = Polygon is translucent (32 levels of transparency), 1 = Polygon is opaque
 *  TL  Transparency Level        32 levels of transparency
 *  PAT Tanslucency Pattern Select
 *  LUM Luminous Feature          0 = Lighting enabled, 1 = Lighting disabled (luminous)
 *  FOG Light Modifier            Effect of fog on luminous polygons (0 = full fog effect, 31 = no fog)
 *  TEN Enable Texture Modulation 0 = Do not texture, 1 = Texture
 *  TFM Texture Mode              Texture format          
 *  SHI Shininess                 
 *  HIP High Priority             1 = High priority polygon (drawn on top of all polygons in viewport w/out Z testing?)
 *  LAY Layered Polygon           1 = Layered polygon (some sort of stencil processing)
 *  TLM Translucency Mode         ?
 *
 * TO-DO List:
 * -----------
 * - More should be predecoded into the polygon structures, so that things like
 *   texture base coordinates are not re-decoded in two different places!
 */

#include <cmath>
#include <cstring>
#include "Supermodel.h"
#include "Legacy3D.h"

#ifdef DEBUG
extern int g_testPolyHeaderIdx;
extern uint32_t g_testPolyHeaderMask;
#endif


namespace Legacy3D {

/******************************************************************************
 Definitions and Constants
******************************************************************************/

/*
 * VBO Vertex Layout
 *
 * All vertex information is stored in an array of GLfloats. Offset and size
 * information is defined here for now.
 */
#define VBO_VERTEX_OFFSET_X               0   // vertex X
#define VBO_VERTEX_OFFSET_Y               1   // vertex Y
#define VBO_VERTEX_OFFSET_Z               2   // vertex Z
#define VBO_VERTEX_OFFSET_NX              3   // normal X
#define VBO_VERTEX_OFFSET_NY              4   // normal Y
#define VBO_VERTEX_OFFSET_NZ              5   // normal Z
#define VBO_VERTEX_OFFSET_R               6   // color (untextured polys) and material (textured polys) R
#define VBO_VERTEX_OFFSET_G               7   // color and material G   
#define VBO_VERTEX_OFFSET_B               8   // color and material B
#define VBO_VERTEX_OFFSET_TRANSLUCENCE    9   // translucence level (0.0 fully transparent, 1.0 opaque)
#define VBO_VERTEX_OFFSET_LIGHTENABLE     10  // lighting enabled (0.0 luminous, 1.0 light enabled)
#define VBO_VERTEX_OFFSET_SPECULAR        11  // specular coefficient (0.0 if disabled)
#define VBO_VERTEX_OFFSET_SHININESS       12  // shininess (specular power)
#define VBO_VERTEX_OFFSET_FOGINTENSITY    13  // fog intensity (0.0 no fog applied, 1.0 all fog applied)
#define VBO_VERTEX_OFFSET_U               14  // texture U coordinate (in texels, relative to sub-texture)
#define VBO_VERTEX_OFFSET_V               15  // texture V coordinate
#define VBO_VERTEX_OFFSET_TEXTURE_X       16  // sub-texture parameters, X (position in overall texture map, in texels)
#define VBO_VERTEX_OFFSET_TEXTURE_Y       17  // "" Y ""
#define VBO_VERTEX_OFFSET_TEXTURE_W       18  // sub-texture parameters, width of texture in texels
#define VBO_VERTEX_OFFSET_TEXTURE_H       19  // "" height of texture in texels
#define VBO_VERTEX_OFFSET_TEXPARAMS_EN    20  // texture parameter: ==1 texturing enabled, ==0 disabled (per-polygon)
#define VBO_VERTEX_OFFSET_TEXPARAMS_TRANS 21  // texture parameter: >=0 use transparency bit, <0 no transparency (per-polygon)
#define VBO_VERTEX_OFFSET_TEXPARAMS_UWRAP 22  // texture parameters: U wrap mode: ==1 mirrored repeat, ==0 normal repeat
#define VBO_VERTEX_OFFSET_TEXPARAMS_VWRAP 23  // "" V wrap mode ""
#define VBO_VERTEX_OFFSET_TEXFORMAT       24  // texture format 0-7 (also ==0 indicates contour texture - see also texParams.trans)
#define VBO_VERTEX_OFFSET_TEXMAP          25  // texture map number
#define VBO_VERTEX_SIZE                   26  // total size (may include padding for alignment)


/******************************************************************************
 Math Routines
******************************************************************************/

// Macro to generate column-major (OpenGL) index from y,x subscripts
#define CMINDEX(y,x)  (x*4+y)

static void CrossProd(GLfloat out[3], const GLfloat a[3], const GLfloat b[3])
{
  out[0] = a[1]*b[2]-a[2]*b[1];
  out[1] = a[2]*b[0]-a[0]*b[2];
  out[2] = a[0]*b[1]-a[1]*b[0];
}

// 3x3 matrix used (upper-left of m[])
static void MultMat3Vec3(GLfloat out[3], const GLfloat m[4*4], const GLfloat v[3])
{
  out[0] = m[CMINDEX(0,0)]*v[0]+m[CMINDEX(0,1)]*v[1]+m[CMINDEX(0,2)]*v[2];
  out[1] = m[CMINDEX(1,0)]*v[0]+m[CMINDEX(1,1)]*v[1]+m[CMINDEX(1,2)]*v[2];
  out[2] = m[CMINDEX(2,0)]*v[0]+m[CMINDEX(2,1)]*v[1]+m[CMINDEX(2,2)]*v[2];
}

static GLfloat Sign(GLfloat x)
{
  if (x > 0.0f)
    return 1.0f;
  else if (x < 0.0f)
    return -1.0f;
  return 0.0f;
}

// Inverts and transposes a 3x3 matrix (upper-left of the 4x4), returning a 
// 4x4 matrix with the extra components undefined (do not use them!)
static void InvertTransposeMat3(GLfloat out[4*4], GLfloat m[4*4])
{
  GLfloat invDet;
  GLfloat a00 = m[CMINDEX(0,0)], a01 = m[CMINDEX(0,1)], a02 = m[CMINDEX(0,2)];
  GLfloat a10 = m[CMINDEX(1,0)], a11 = m[CMINDEX(1,1)], a12 = m[CMINDEX(1,2)];
  GLfloat a20 = m[CMINDEX(2,0)], a21 = m[CMINDEX(2,1)], a22 = m[CMINDEX(2,2)];
  
  invDet = 1.0f/(a00*(a22*a11-a21*a12)-a10*(a22*a01-a21*a02)+a20*(a12*a01-a11*a02));
  out[CMINDEX(0,0)] = invDet*(a22*a11-a21*a12);   out[CMINDEX(1,0)] = invDet*(-(a22*a01-a21*a02));  out[CMINDEX(2,0)] = invDet*(a12*a01-a11*a02);
  out[CMINDEX(0,1)] = invDet*(-(a22*a10-a20*a12));  out[CMINDEX(1,1)] = invDet*(a22*a00-a20*a02);   out[CMINDEX(2,1)] = invDet*(-(a12*a00-a10*a02));
  out[CMINDEX(0,2)] = invDet*(a21*a10-a20*a11);   out[CMINDEX(1,2)] = invDet*(-(a21*a00-a20*a01));  out[CMINDEX(2,2)] = invDet*(a11*a00-a10*a01);
}

/*
static void PrintMatrix(GLfloat m[4*4])
{
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 3; j++)
      printf("%g\t", m[CMINDEX(i,j)]);
    printf("\n");
  }
}
*/


/******************************************************************************
 Display Lists
 
 Every instance of a model encountered in the scene database during rendering
 is stored in the display list along with its current transformation matrices
 and other state information. Display lists are bound to model caches for 
 performance: only one VBO has to be bound for an entire display list.
 
 Binding display lists to model caches may cause priority problems among 
 alpha polygons. Therefore, it may be necessary in the future to decouple them.
******************************************************************************/   
    
// Draws the display list
void CLegacy3D::DrawDisplayList(ModelCache *Cache, POLY_STATE state)
{
  // Bind and activate VBO (pointers activate currently bound VBO)
  glBindBuffer(GL_ARRAY_BUFFER, Cache->vboID);
  glVertexPointer(3, GL_FLOAT, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_X*sizeof(GLfloat))); 
  glNormalPointer(GL_FLOAT, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_NX*sizeof(GLfloat))); 
  glTexCoordPointer(2, GL_FLOAT, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_U*sizeof(GLfloat)));
  glColorPointer(3, GL_FLOAT, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_R*sizeof(GLfloat)));
  if (subTextureLoc != -1)   glVertexAttribPointer(subTextureLoc, 4, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TEXTURE_X*sizeof(GLfloat)));
  if (texParamsLoc != -1)    glVertexAttribPointer(texParamsLoc, 4, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TEXPARAMS_EN*sizeof(GLfloat)));
  if (texFormatLoc != -1)    glVertexAttribPointer(texFormatLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TEXFORMAT*sizeof(GLfloat)));
  if (texMapLoc != -1)       glVertexAttribPointer(texMapLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TEXMAP*sizeof(GLfloat)));
  if (transLevelLoc != -1)   glVertexAttribPointer(transLevelLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TRANSLUCENCE*sizeof(GLfloat)));
  if (lightEnableLoc != -1)  glVertexAttribPointer(lightEnableLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_LIGHTENABLE*sizeof(GLfloat)));
  if (shininessLoc != -1)    glVertexAttribPointer(shininessLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_SHININESS*sizeof(GLfloat)));
  if (specularLoc != -1)     glVertexAttribPointer(specularLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_SPECULAR*sizeof(GLfloat)));
  if (fogIntensityLoc != -1) glVertexAttribPointer(fogIntensityLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_FOGINTENSITY*sizeof(GLfloat)));
  
  // Set up state
  if (state == POLY_STATE_ALPHA)
  {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
  else
  {
    glDisable(GL_BLEND);
  }
  bool stencilEnabled = false;
  glDisable(GL_STENCIL_TEST);
  
  // Draw if there are items in the list
  const DisplayList *D = Cache->ListHead[state];
  while (D != NULL)
  { 
    if (D->isViewport)
    {
      if (D->next != NULL)  // if nothing follows, no point in doing this
      {
        if (!D->next->isViewport)
        {
          if (lightingLoc != -1)         glUniform3fv(lightingLoc, 2, D->Data.Viewport.lightingParams);
          if (projectionMatrixLoc != -1) glUniformMatrix4fv(projectionMatrixLoc, 1, GL_FALSE, D->Data.Viewport.projectionMatrix);
          glFogf(GL_FOG_DENSITY, D->Data.Viewport.fogParams[3]);
          glFogf(GL_FOG_START, D->Data.Viewport.fogParams[4]);
          glFogfv(GL_FOG_COLOR, &(D->Data.Viewport.fogParams[0]));
          if (spotEllipseLoc != -1)      glUniform4fv(spotEllipseLoc, 1, D->Data.Viewport.spotEllipse);
          if (spotRangeLoc != -1)        glUniform2fv(spotRangeLoc, 1, D->Data.Viewport.spotRange);
          if (spotColorLoc != -1)        glUniform3fv(spotColorLoc, 1, D->Data.Viewport.spotColor);
          glViewport(D->Data.Viewport.x, D->Data.Viewport.y, D->Data.Viewport.width, D->Data.Viewport.height);
        }
      }
    }
    else
    {
      const DisplayList::ModelInstance &Model = D->Data.Model;
      if (stencilEnabled != Model.useStencil)
      {
        if (Model.useStencil)
          glEnable(GL_STENCIL_TEST);
        else
          glDisable(GL_STENCIL_TEST);
        stencilEnabled = Model.useStencil;
      }
      if (Model.frontFace == -GL_CW)
      {
        // No backface culling (all normals have lost their Z component)
        glDisable(GL_CULL_FACE);
      }
      else
      {
        // Use appropriate winding convention
        GLint frontFace;
        glGetIntegerv(GL_FRONT_FACE, &frontFace);
        if (frontFace != Model.frontFace)
          glFrontFace(Model.frontFace);
      }
      if (modelViewMatrixLoc != -1)
        glUniformMatrix4fv(modelViewMatrixLoc, 1, GL_FALSE, Model.modelViewMatrix);
      glDrawArrays(GL_TRIANGLES, Model.index, Model.numVerts);
      if (Model.frontFace == -GL_CW)
        glEnable(GL_CULL_FACE);
    }
    D = D->next;
  }
}

// Appends an instance of a model or viewport to the display list, copying over the required state information
bool CLegacy3D::AppendDisplayList(ModelCache *Cache, bool isViewport, const struct VBORef *Model)
{
  if ((Cache->listSize+2) > Cache->maxListSize) // a model may have 2 states (viewports are added to both display lists)
    return FAIL;
    //return ErrorLog("Display list is full.");
  
  // Insert states into the display list
  size_t lm = 0;
  for (size_t i = 0; i < 2; i++)
  {
    if (isViewport)
    {
      // Get index for new display list item and advance to next one
      lm = Cache->listSize++;
      
      // Viewport parameters
      Cache->List[lm].Data.Viewport.x = viewportX;
      Cache->List[lm].Data.Viewport.y = viewportY;
      Cache->List[lm].Data.Viewport.width = viewportWidth;
      Cache->List[lm].Data.Viewport.height = viewportHeight;
      
      // Copy over lighting and fog state
      memcpy(Cache->List[lm].Data.Viewport.lightingParams, lightingParams, sizeof(lightingParams));
      memcpy(Cache->List[lm].Data.Viewport.fogParams, fogParams, sizeof(fogParams));
      memcpy(Cache->List[lm].Data.Viewport.spotEllipse, spotEllipse, sizeof(spotEllipse));
      memcpy(Cache->List[lm].Data.Viewport.spotRange, spotRange, sizeof(spotRange));
      memcpy(Cache->List[lm].Data.Viewport.spotColor, spotColor, sizeof(spotColor));
      
      // Copy projection matrix
      glGetFloatv(GL_PROJECTION_MATRIX, Cache->List[lm].Data.Viewport.projectionMatrix);
    }
    else if (Model->numVerts[i] > 0)  // vertices exist for this state
    { 
      // Get index for new display list item and advance to next one
      lm = Cache->listSize++;
      
      // Point to VBO for current model and state
      Cache->List[lm].Data.Model.index = Model->index[i];
      Cache->List[lm].Data.Model.numVerts = Model->numVerts[i];

      // Misc. parameters
      Cache->List[lm].Data.Model.useStencil = Model->useStencil;
      
      // Copy modelview matrix
      glGetFloatv(GL_MODELVIEW_MATRIX, Cache->List[lm].Data.Model.modelViewMatrix);
      
      /*
       * Determining if winding was reversed (but not polygon normal):
       *
       * Real3D performs backface culling in view space based on the
       * polygon normal unlike OpenGL, which uses the computed normal
       * from the edges (in screen space) of the polygon. Consequently,
       * it is possible to create a matrix that mirrors an axis without
       * rotating the normal, which in turn flips the polygon winding and
       * makes it invisible in OpenGL but not on Real3D, because the 
       * normal is still facing the right way.
       *
       * To detect such a situation, we create a fictitious polygon with
       * edges X = [1 0 0] and Y = [0 1 0], with normal Z = [0 0 1]. We 
       * rotate the edges by the matrix then compute a normal P, which is
       * what OpenGL would use for culling. We transform the normal Z by
       * the normal matrix (normals are special and must be multiplied by
       * Transpose(Inverse(M)), not M). If the Z components of P and the
       * transformed Z vector have opposite signs, the OpenGL winding 
       * mode must be switched in order to draw correctly. The X axis may
       * have been flipped, for example, changing the winding mode while
       * leaving the polygon normal unaffected. OpenGL would erroneously
       * discard these polygons, so we flip the winding convention, 
       * ensuring they are drawn correctly.
       *
       * We have to adjust the Z vector (fictitious normal) by the sign
       * of the Z axis specified by the coordinate system matrix (#0).
       * This is described further in InsertPolygon(), where the vertices
       * are ordered in clockwise fashion.
       */
      static const GLfloat x[3] = { 1.0f, 0.0f, 0.0f };
      static const GLfloat y[3] = { 0.0f, 1.0f, 0.0f };
      const GLfloat z[3] = { 0.0f, 0.0f, -1.0f*matrixBasePtr[0x5] };
      GLfloat m[4*4];
      GLfloat xT[3], yT[3], zT[3], pT[3];

      InvertTransposeMat3(m,Cache->List[lm].Data.Model.modelViewMatrix);
      MultMat3Vec3(xT,Cache->List[lm].Data.Model.modelViewMatrix,x);
      MultMat3Vec3(yT,Cache->List[lm].Data.Model.modelViewMatrix,y);
      MultMat3Vec3(zT,m,z);
      CrossProd(pT,xT,yT);
      
      float s = Sign(zT[2]*pT[2]);
      if (s < 0.0f)
        Cache->List[lm].Data.Model.frontFace = GL_CCW;
      else if (s > 0.0f)
        Cache->List[lm].Data.Model.frontFace = GL_CW;
      else
        Cache->List[lm].Data.Model.frontFace = -GL_CW;
    }
    else  // nothing to do, continue loop
      continue;
      
    // Update list pointers and set list node type
    Cache->List[lm].isViewport = isViewport;
    Cache->List[lm].next = NULL;  // current end of list
    if (Cache->ListHead[i] == NULL)
    {
      Cache->ListHead[i] = &(Cache->List[lm]);
      Cache->ListTail[i] = Cache->ListHead[i];
    }
    else
    {
      Cache->ListTail[i]->next = &(Cache->List[lm]);
      Cache->ListTail[i] = &(Cache->List[lm]);
    }
  }
    
  return OKAY;
}

// Clears the display list in preparation for a new frame
void CLegacy3D::ClearDisplayList(ModelCache *Cache)
{
  Cache->listSize = 0;
  for (size_t i = 0; i < 2; i++)
  {
    Cache->ListHead[i] = NULL;
    Cache->ListTail[i] = NULL;
  }
}


/******************************************************************************
 Model Caching
 
 Note that as vertices are inserted into the appropriate local vertex buffer
 (sorted by polygon state -- alpha and normal), the VBO index is advanced to 
 reserve space and does not correspond to the actual position of each vertex.
 Vertices are copied in batches sorted by state when the model is complete.
******************************************************************************/

int CLegacy3D::GetTextureBaseX(const Poly *P) const
{
  int x = 32 * (((P->header[4] & 0x7F) << 1) | ((P->header[5] >> 7) & 1));
  return (x + m_textureOffset.x) & 2047;
}

int CLegacy3D::GetTextureBaseY(const Poly *P) const
{
  int y = 32 * (P->header[5] & 0x7F);
  int bank = (P->header[4] & 0x40) << 4;
  return ((y + m_textureOffset.y) & 1023) + (bank ^ m_textureOffset.switchBank);
}

// Inserts a vertex into the local vertex buffer, incrementing both the local and VBO pointers. The normal is scaled by normFlip.
void CLegacy3D::InsertVertex(ModelCache *Cache, const Vertex *V, const Poly *P, float normFlip)
{
  // Texture selection
  unsigned  texEnable = P->header[6]&0x400;
  unsigned  texFormat = (P->header[6]>>7)&7;
  GLfloat   texWidth  = (GLfloat) (32<<((P->header[3]>>3)&7));
  GLfloat   texHeight = (GLfloat) (32<<((P->header[3]>>0)&7));
  TexSheet  *texSheet = fmtToTexSheet[texFormat]; // get X, Y offset of texture sheet within texture map
  GLfloat   texBaseX = (GLfloat)(texSheet->xOffset + GetTextureBaseX(P));
  GLfloat   texBaseY = (GLfloat)(texSheet->yOffset + GetTextureBaseY(P));

  /*
   * Lighting and Color Modulation:
   *
   * It appears that there is a modulate bit which causes the polygon color
   * to be multiplied by texel colors. However, if polygons are luminous,
   * this appears to be disabled (not quite correct yet, though).
   *
   * Color Table
   * -----------
   * 1. Color table base is definitely at 0x400 for most games.
   * 2. There are two color indexes in header[4]. One between bits 31-20 and
   *    the other between bits 19-8. Sometimes they are set the same, sometimes
   *    they differ by 1. They must either be selectable or apply to different
   *    sides of the polygon. Indexed colors appear to be enabled by
   *    !(header[1]&2).
   * 3. Bits 19-8 are needed to make Daytona 2 lights blink. They also seem to
   *    work well for Scud Race.
   * 4. Two bits, header[4]&0x80 and header[3]&0x80, seem to affect color
   *    modulation (multiplication of RGB or indexed color value by texels).
   *    header[4] works best in Sega Rally 2 but header[3] works a bit better
   *    elsewhere.
   * 5. !(header[4]&0x80) is sufficient to get blinking lights to work in 
   *    Daytona and also fixes shadows under the overpass (spiral turn) on the
   *    expert course. But, it makes the waterfalls on Scud's medium course too
   *    dark. The waterfalls have !(header[1]&2), which seems to indicate they 
   *    use indexed colors, but they are too dark when used. header[3]&0x80 is
   *    0, which if interpreted as modulation off, makes waterfalls appear
   *    correctly. If !(header[4]&0x80) is used instead, it is enabled, and
   *    modulation fails. Blinking lights in Scud Race (medium, expert courses)
   *    seem to work with both.
   * 6. Forcing modulation to be enabled in color index mode does not seem to
   *    work because of the Scud Race waterfalls (they seem to dislike being
   *    modulated).
   * 7. A possibly important test case, in addition to waterfalls, are the red
   *    traffic cones at the start of the Desert course in Sega Rally 2's 
   *    championship mode. When !header[4]&0x80 is used, colors are mostly 
   *    correct, but cones are too dark. Need to investigate further.
   */
   
  unsigned lightEnable  = !(P->header[6]&0x00010000);
  
  // Material color
  GLfloat r = 1.0;
  GLfloat g = 1.0;
  GLfloat b = 1.0;
  if ((P->header[1]&2) == 0)
  {
    //size_t sensorColorIdx = ((P->header[4]>>20)&0xFFF);
    size_t colorIdx = ((P->header[4]>>8)&0xFFF);
    b = (GLfloat) (polyRAM[m_colorTableAddr+colorIdx]&0xFF) * (1.0f/255.0f);
    g = (GLfloat) ((polyRAM[m_colorTableAddr+colorIdx]>>8)&0xFF) * (1.0f/255.0f);
    r = (GLfloat) ((polyRAM[m_colorTableAddr+colorIdx]>>16)&0xFF) * (1.0f/255.0f);
  }
  else
  {
    r = (GLfloat) (P->header[4]>>24) * (1.0f/255.0f);
    g = (GLfloat) ((P->header[4]>>16)&0xFF) * (1.0f/255.0f);
    b = (GLfloat) ((P->header[4]>>8)&0xFF) * (1.0f/255.0f);
  }

  /*
   * These observations are somewhat out of date now that we know the polygon
   * header layout from the Pro-1000 SDK. However, there are some important
   * regressions to keep an eye out for.
   *
   * Color Modulation Observations
   * -----------------------------
   *
   * Scud Race:
   *  - Skybox, airport terminal, aquarium tunnel: lighting disabled, textures enabled, RGB colors.
   *  - Waterfalls: lighting disabled, textures enabled, palettized colors.
   *  - Traffic lights: lighting disabled, textures enabled, palettized colors.
   *
   * Daytona 2:
   *  - Skybox, right (but not left) barrier under overpass on freeway spiral 
   *    ramp: lighting disabled, textures enabled, RGB colors.
   *  - Traffic lights (course start): lighting disabled, textures enabled, palettized colors.
   *  - Traffic lights (expert course in city): lighting disabled, textures enabled, RGB colors.
   *  - MODULATION FOR TEXTURED POLYGONS ALWAYS WORKS (no discernable case where it should be 
   *    disabled).
   *
   * Sega Rally 2:
   *  - Selection menu: lighting disabled, textures enabled, RGB colors -- MODULATION REQUIRED.
   *  - Skybox, trees: lighting disabled, textures enabled, RGB colors.
   *  - Cones: lighting enabled, textures disabled, RGB colors.
   *  - MODULATE=!(header[4]&0x80) WORKS
   *
   * Star Wars Trilogy:
   *  - HUD elements, lightsabers: lighting disabled, textures disabled, RGB colors.
   *
   * LA Machineguns:
   *  - Some (but not all!) scenery: lighting disabled.
   *  - Some scenergy: textures disabled.
   *  - Street (Last Vegas): palettized colors.
   *  - STREET MUST BE MODULATED, MOST SCENERY DOES NOT (MODULATE=!(header[4]&0x80) WORKS)
   *
   * Evidence seems to support the existence of a modulation setting that can
   * enable/disable modulation. No single bit that works for all games has been
   * identified but the best candidate is:
   *
   *  modulate = !(header[4]&0x80)  Step 2.x
   *  modulate = header[3]&0x80     Step 1.x
   *
   * But unfortunately, this still fails on most Scud Race geometry. It only
   * works for waterfalls. Totem poles and other items which lack lighting,
   * fail. Perhaps on Step 1.x, disabling lighting automatically disables 
   * modulation? For now, we use this as a hack.
   *
   * BUGS:
   *  - Fighting Vipers 2 shadows are not black anymore.
   *  - More to follow...
   */

  // Shading bits
  int fixedShading = (P->header[1] & 0x20);
  int smoothShading = (P->header[1] & 0x08);
    
  // Color modulation here means multiplication of texel by polygon color
  int modulate = true;
  
  // Source of normal vector depends on shading mode
  float nx = 0.0f;
  float ny = 0.0f;
  float nz = 0.0f;
  
  float intensity = 1.0f;
  
  // Lighting/shading modes
  if (lightEnable)
  {
    if (smoothShading)
    {
      // Vertex normals present: smooth shading only. Fixed shading never
      // observed to be enabled with light and smooth shading in any game.
      nx = V->n[0];
      ny = V->n[1];
      nz = V->n[2];
      modulate = true;
    }
    else
    {
      // Vertex normals absent: fixed or flat shading only
      if (fixedShading)
      {
        // LA Machineguns Vegas street pulsating color effect requires modulation
        intensity = V->intensity;
        lightEnable = 0;  // this breaks Yosemite level of LA Machineguns but fixes Scud castle
        modulate = true;
      }
      else
      {
        // Flat shading occurs rarely. It can be observed in LA Machineguns (LA
        // mission, parking lot). A fixed shading intensity seems to be provided
        // but is presumably unused.
        nx = P->n[0];
        ny = P->n[1];
        nz = P->n[2];
        modulate = true;
      }
    }
  }
  else
  {
    // Here things get tricky...
    if (smoothShading)
    {
      /*
       * Vertex normals don't matter because lighting is disabled.
       * So does the smooth shading bit convey any meaning?
       *
       * Cases of interest:
       *
       *  - Sega Rally 2: menu has lightEnable = 0, smoothShading = 1,
       *    fixedShading = 0. Color modulation must occur.
       *  - LA Machineguns: this setting occurs for enemy blue flames and
       *    flashing missile warheads. Only the latter require modulation. Blue
       *    flames look wrong if either fixed shading or modulation is applied.
       *    However, this is inconclusive because these and most other polygons
       *    in the game have the "translator map" feature enabled. Perhaps this
       *    compensates somehow?
       *  - LA Machineguns is the primary reason for disabling modulation
       *    whenever tranlator map is enabled.
       */
      modulate = true;
    }
    else
    {
      if (fixedShading)
      {
        /*
         * Fixed intensities are sometimes observed in the normal area (e.g., 
         * normal=0x3f,0,0, which cannot be a vertex normal because magnitude <
         * 1) but often, all three components are just 0, indicating that fixed
         * shading is indeed unused here.
         *
         * This is one of the most problematic settings:
         *
         *  - Scud Race waterfalls, totem poles (near waterfalls at check-
         *    point), orange guard rail lamps, sky, and beginner level tunnels
         *    all look correct with color modulation disabled. Fixed shading
         *    and polygon color values are simply too dark and there does not
         *    appear to be a plausible combination of the two that works.
         *  - Scud Race blinking traffic signals, blue arrow signs at entrance
         *    to Colosseum, all expect modulation to be on. Palette vs. RGB
         *    polygon color seems to make no difference.
         */
        modulate = true;
      }
      else
      {
        /*
         * No vertex normals or intensity. Almost never happens. Only a single,
         * untextured HUD triangle uses this mode in Scud Race.
        */
        modulate = true;
      }
    }
  }

  // Translator map probably affects shading. Used extensively in LA 
  // Machineguns. Here, we disable it to fix that game, otherwise polygon
  // colors and modulation make everything too dark.
  // TODO: search for translator map by looking for 128*4 byte block xfers? May
  // be in VROM, though...
  if ((P->header[4] & 0x80))
    modulate = false;
  
  // Untextured polygons always use polygon color. Make sure we pass it to
  // shader!
  if (!texEnable)
    modulate = true;

  // This removes the effect of polygon color
  if (!modulate)
  {
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
  }
  
  // Assemble final shading color and intensity
  r *= intensity;
  g *= intensity;
  b *= intensity;
    
  // Specular shininess
  GLfloat specularCoefficient = (GLfloat) ((P->header[0]>>26) & 0x3F) * (1.0f/63.0f);
  int shinyBits = (P->header[6] >> 5) & 3;
  float shininess = std::exp2f(1.0f + shinyBits);
  if (!(P->header[0]&0x80)) //|| (shininess == 0)) // bit 0x80 seems to enable specular lighting
  {
    specularCoefficient = 0.; // disable
    shininess = -1;
  }

  // Determine whether polygon is translucent
  GLfloat translucence = (GLfloat) ((P->header[6]>>18)&0x1F) * (1.0f/31.0f);
  if ((P->header[6]&0x00800000))  // if set, polygon is opaque
    translucence = 1.0f;
    
  // Fog intensity (apparently used for both luminous and shaded polygons)
  GLfloat fogIntensity = (GLfloat) ((P->header[6]>>11)&0x1F) * (1.0f/15.0f);  // only 4 bits seem to actually be used?

  /*
   * Contour processing. Any alpha value sufficiently close to 0 seems to
   * cause pixels to be discarded entirely on Model 3 (no modification of the
   * depth buffer). Strictly speaking, only T1RGB5 format textures are
   * "contour textures" (in Real3D lingo), we enable contour processing for
   * alpha blended texture formats as well in order to discard fully
   * transparent pixels.
   */
  GLfloat contourProcessing = -1.0f;
  if ((P->header[6]&0x80000000) || (texFormat==7) ||  // contour processing enabled or RGBA4 texture
    ((texFormat==1) && (P->header[6]&2)) ||     // A4L4 interleaved (these formats are not being interpreted correctly, see Scud Race clock tower)
    ((texFormat==3) && (P->header[6]&4)))     // A4L4 interleaved
    contourProcessing = 1.0f;

#ifdef DEBUG
  if (m_debugHighlightPolyHeaderIdx >= 0 || m_debugHighlightAll)
  {
    if ((P->header[m_debugHighlightPolyHeaderIdx] & m_debugHighlightPolyHeaderMask) || m_debugHighlightAll)
    {
      r = 0.;
      g = 1.;
      b = 0.;
      lightEnable = 0;
      texEnable = 0;
      contourProcessing = 0.;
      fogIntensity = 0.;
      translucence = 1.;
      shininess = -1;
    }
  }
#endif

  // Store to local vertex buffer
  size_t s = P->state;
  size_t baseIdx = Cache->curVertIdx[s]*VBO_VERTEX_SIZE;

  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_X] = V->x;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_Y] = V->y;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_Z] = V->z;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_R] = r;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_G] = g;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_B] = b;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TRANSLUCENCE] = translucence;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_LIGHTENABLE] = lightEnable ? 1.0f : 0.0f;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_SPECULAR] = specularCoefficient;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_SHININESS] = (GLfloat) shininess;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_FOGINTENSITY] = fogIntensity;
  
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_NX] = fixedShading ? 0.f : nx*normFlip;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_NY] = fixedShading ? 0.f : ny*normFlip;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_NZ] = fixedShading ? 0.f : nz*normFlip; 
  
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_U] = V->u;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_V] = V->v;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXTURE_X] = texBaseX;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXTURE_Y] = texBaseY;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXTURE_W] = texWidth;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXTURE_H] = texHeight;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXPARAMS_EN] = texEnable ? 1.0f : 0.0f;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXPARAMS_TRANS] = contourProcessing;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXPARAMS_UWRAP] = (P->header[2]&2) ? 1.0f : 0.0f;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXPARAMS_VWRAP] = (P->header[2]&1) ? 1.0f : 0.0f;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXFORMAT] = (float)texFormat;
  Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXMAP] = (float)texSheet->mapNum;

  Cache->curVertIdx[s]++;
  Cache->vboCurOffset += VBO_VERTEX_SIZE*sizeof(GLfloat);
}

bool CLegacy3D::InsertPolygon(ModelCache *Cache, const Poly *P)
{
  // Bounds testing: up to 12 triangles will be inserted (worst case: double sided quad is 6 triangles)
  if ((Cache->curVertIdx[P->state]+6*2) >= Cache->maxVertIdx)
    return ErrorLocalVertexOverflow();  // local buffers are not expected to overflow
  if ((Cache->vboCurOffset+6*2*VBO_VERTEX_SIZE*sizeof(GLfloat)) >= Cache->vboMaxOffset)
    return FAIL;  // this just indicates we may need to re-cache
    
  // Is the polygon double sided?
  bool doubleSided = (P->header[1]&0x10) ? true : false;
  
  /*
   * Determine polygon winding by taking cross product of vectors formed from
   * 3 polygon vertices (the middle one being the origin). In reality, back-
   * face culling is determined by the polygon normal and two-sided polygons
   * exist. This is just a temporary hack.
   *
   * If the cross product points the same way as the normal, the winding is
   * clockwise and can be kept, otherwise it must be reversed.
   *
   * NOTE: This assumes that the Model 3 base coordinate system's Z axis
   * (into the screen) is -1, like OpenGL. For some games (eg., Lost World),
   * this is not the case. Assuming games consistently use the same type of
   * coordinate system matrix, it seems that inverting the whole dot product
   * when Z is positive helps. I don't understand exactly why... but it has
   * to do with using the correct Z convention to identify a vector pointing
   * toward or away from the screen.
   */
  GLfloat v1[3];
  GLfloat v2[3];
  GLfloat n[3];
  v1[0] = P->Vert[0].x-P->Vert[1].x;
  v1[1] = P->Vert[0].y-P->Vert[1].y;
  v1[2] = P->Vert[0].z-P->Vert[1].z;
  v2[0] = P->Vert[2].x-P->Vert[1].x;
  v2[1] = P->Vert[2].y-P->Vert[1].y;
  v2[2] = P->Vert[2].z-P->Vert[1].z;
  CrossProd(n,v1,v2);
  
  GLfloat normZFlip = -1.0f*matrixBasePtr[0x5]; // coordinate system m13 component
  
  if (normZFlip*(n[0]*P->n[0]+n[1]*P->n[1]+n[2]*P->n[2]) >= 0.0)  // clockwise winding confirmed
  {
    // Store the first triangle
    for (int i = 0; i < 3; i++)
    {
      InsertVertex(Cache, &(P->Vert[i]), P, 1.0f);
    }
    
    if (doubleSided)  // store backside as counter-clockwise
    {
      for (int i = 2; i >=0; i--)
      {
        InsertVertex(Cache, &(P->Vert[i]), P, -1.0f);
      }
    }
  
    // If quad, second triangle will just be vertices 1, 3, 4
    if (P->numVerts == 4)
    {
      InsertVertex(Cache, &(P->Vert[0]), P, 1.0f);
      InsertVertex(Cache, &(P->Vert[2]), P, 1.0f);
      InsertVertex(Cache, &(P->Vert[3]), P, 1.0f);
      
      if (doubleSided)
      {
        InsertVertex(Cache, &(P->Vert[0]), P, -1.0f);
        InsertVertex(Cache, &(P->Vert[3]), P, -1.0f);
        InsertVertex(Cache, &(P->Vert[2]), P, -1.0f);
      }
    }
  }
  else  // counterclockwise winding, reverse it
  {
    for (int i = 2; i >=0; i--)
    {
      InsertVertex(Cache, &(P->Vert[i]), P, 1.0f);
    }
    
    if (doubleSided)  // store backside as clockwise
    {
      for (int i = 0; i < 3; i++)
      {
        InsertVertex(Cache, &(P->Vert[i]), P, -1.0f);
      }
    }
    
    if (P->numVerts == 4)
    {
      InsertVertex(Cache, &(P->Vert[0]), P, 1.0f);
      InsertVertex(Cache, &(P->Vert[3]), P, 1.0f);
      InsertVertex(Cache, &(P->Vert[2]), P, 1.0f);
      
      if (doubleSided)
      {
        InsertVertex(Cache, &(P->Vert[0]), P, -1.0f);
        InsertVertex(Cache, &(P->Vert[2]), P, -1.0f);
        InsertVertex(Cache, &(P->Vert[3]), P, -1.0f);
      }
    }
  }
  
  return OKAY;
}

// Begins caching a new model by resetting to the start of the local vertex buffer
struct VBORef *CLegacy3D::BeginModel(ModelCache *Cache)
{
  size_t  m = Cache->numModels;
  
  // Determine whether we've exceeded the model cache limits (caller will have to recache)
  if (m >= Cache->maxModels)
  {
    //ErrorLog("Too many %s models.", Cache->dynamic?"dynamic":"static");
    return NULL;  
  }
  
  struct VBORef *Model = &(Cache->Models[m]);
  
  // Reset to the beginning of the local vertex buffer
  for (size_t i = 0; i < 2; i++)
    Cache->curVertIdx[i] = 0;
  
  // Clear the VBO reference to 0 and clear texture references
  Model->Clear();
  
  // Record starting index of first opaque polygon in VBO (alpha poly index will be re-set in EndModel())
  Model->index[POLY_STATE_NORMAL] = Cache->vboCurOffset/(VBO_VERTEX_SIZE*sizeof(GLfloat));
  Model->index[POLY_STATE_ALPHA] = Model->index[POLY_STATE_NORMAL];
  
  return Model;
}

// Uploads all vertices from the local vertex buffer to the VBO, sets up the VBO reference, updates the LUT
void CLegacy3D::EndModel(ModelCache *Cache, struct VBORef *Model, int lutIdx, UINT16 textureOffsetState, bool useStencil)
{
  int m = Cache->numModels++;

  // Record the number of vertices, completing the VBORef
  for (size_t i = 0; i < 2; i++)
    Model->numVerts[i] = Cache->curVertIdx[i];

  // First alpha polygon immediately follows the normal polygons
  Model->index[POLY_STATE_ALPHA] = Model->index[POLY_STATE_NORMAL] + Model->numVerts[POLY_STATE_NORMAL];

  // Upload from local vertex buffer to real VBO
  glBindBuffer(GL_ARRAY_BUFFER, Cache->vboID);
  if (Model->numVerts[POLY_STATE_NORMAL] > 0)
    glBufferSubData(GL_ARRAY_BUFFER, Model->index[POLY_STATE_NORMAL]*VBO_VERTEX_SIZE*sizeof(GLfloat), Cache->curVertIdx[POLY_STATE_NORMAL]*VBO_VERTEX_SIZE*sizeof(GLfloat), Cache->verts[POLY_STATE_NORMAL]);
  if (Model->numVerts[POLY_STATE_ALPHA] > 0)
    glBufferSubData(GL_ARRAY_BUFFER, Model->index[POLY_STATE_ALPHA]*VBO_VERTEX_SIZE*sizeof(GLfloat), Cache->curVertIdx[POLY_STATE_ALPHA]*VBO_VERTEX_SIZE*sizeof(GLfloat), Cache->verts[POLY_STATE_ALPHA]);
    
  // Record LUT index in the model VBORef
  Model->lutIdx = lutIdx;
  
  // Texture offset of this model state
  Model->textureOffsetState = textureOffsetState;
  
  // Should we use stencil?
  Model->useStencil = useStencil;
  
  // Update the LUT and link up to any existing model that already exists here
  if (Cache->lut[lutIdx] >= 0)  // another texture offset state already cached
    Model->nextTextureOffsetState = &(Cache->Models[Cache->lut[lutIdx]]);
  Cache->lut[lutIdx] = m;
}

/*
 * CacheModel():
 *
 * Decodes and caches a complete model. Returns NULL if any sort of overflow in
 * the cache occurred. In this case, the model cache should be cleared before
 * being used again because an incomplete model will be stored, wasting vertex
 * buffer space.
 *
 * A pointer to the VBO reference for the cached model is returned when
 * successful.
 */

struct VBORef *CLegacy3D::CacheModel(ModelCache *Cache, int lutIdx, UINT16 textureOffsetState, const UINT32 *data)
{
  if (data == NULL)
    return NULL;
    
  // Start constructing a new model
  struct VBORef *Model = BeginModel(Cache);
  if (NULL == Model)
    return NULL;  // too many models!
  
  // Cache all polygons
  Vertex    Prev[4];  // previous vertices
  int       numPolys = 0;
  bool      useStencil = true;
  bool      done = false;
  while (!done)
  {    
    // Set current header pointer (header is 7 words)
    Poly P;     // current polygon
    P.header = data;
    data += 7;  // data will now point to first vertex
    if (P.header[6]==0)
      break;

    // Sega Rally 2: dust trails often have polygons with seemingly invalid
    // vertices (very large values or 0). Ignoring polygons with these bits set
    // seems to fix the problem. Perhaps these polygons exist for alignment
    // purposes or are another type of entity altogether?
    bool validPoly = (P.header[0] & 0x300) != 0x300;
    
    // Obtain basic polygon parameters
	  done = (P.header[1] & 4) > 0; // last polygon?
    P.numVerts = (P.header[0]&0x40)?4:3;

    // Texture data
    int texEnable = P.header[6]&0x400;
    int texFormat = (P.header[6]>>7)&7;
    int texWidth  = (32<<((P.header[3]>>3)&7));
    int texHeight = (32<<((P.header[3]>>0)&7));
    int texBaseX = GetTextureBaseX(&P);
    int texBaseY = GetTextureBaseY(&P);    
    GLfloat uvScale   = (P.header[1]&0x40)?1.0f:(1.0f/8.0f);
    
    // Determine whether this is an alpha polygon (TODO: when testing textures, test if texturing enabled? Might not matter)
    int isTranslucent = (P.header[6] & 0x00800000) == 0;
    if (isTranslucent ||
      (texFormat==7) ||   // RGBA4 texture
      (texFormat==4))     // A4L4 texture
      P.state = POLY_STATE_ALPHA;
    else
      P.state = POLY_STATE_NORMAL;
    if (texFormat==1)             // A4L4 interleaved
    {
      if ((P.header[6]&2))
        P.state = POLY_STATE_ALPHA;
      else
        P.state = POLY_STATE_NORMAL;
    }
    if (texFormat==3)           // A4L4 interleaved
    {
      if ((P.header[6]&4))
        P.state = POLY_STATE_ALPHA;
      else
        P.state = POLY_STATE_NORMAL;
    } 
      
    // Layered polygons are implemented with a stencil buffer. Here, we also
    // include a hack to detect likely shadow polygons. When not implemented as
    // layered polygons, games use translucent polygons (which on the actual
    // hardware are implemented with stipple) without texturing or lighting.
    // Usually they are also black with the annoying exception of Spikeout.
    // TODO: If this hack is too permissive and breaks anything, we should make
    // it a config option.
    int isLayered = P.header[6] & 0x8;
    int isLightDisabled = P.header[6] & 0x00010000;
    bool isProbablyShadow = isLightDisabled && isTranslucent && !texEnable;
    useStencil &= (isLayered || isProbablyShadow);
    
    // Decode the texture
    if (texEnable)
    {
      // If model cache is static, record texture reference in model cache entry for later decoding.
      // If cache is dynamic, or if it's not possible to record the texture reference (due to lack of
      // memory) then decode the texture now.
      if (Cache->dynamic || !Model->texRefs.AddRef(texFormat, texBaseX, texBaseY, texWidth, texHeight))
        DecodeTexture(texFormat, texBaseX, texBaseY, texWidth, texHeight);
    }
    
    // Polygon normal is in upper 24 bits: sign + 1.22 fixed point
    P.n[0] = (GLfloat) (((INT32)P.header[1])>>8) * (1.0f/4194304.0f);
    P.n[1] = (GLfloat) (((INT32)P.header[2])>>8) * (1.0f/4194304.0f);
    P.n[2] = (GLfloat) (((INT32)P.header[3])>>8) * (1.0f/4194304.0f);
    
    // Fetch reused vertices according to bitfield, then new verts
    size_t j = 0;
    size_t vmask = 1;
    for (size_t i = 0; i < 4; i++)  // up to 4 reused vertices
    {
      if ((P.header[0x00]&vmask))
      {
        P.Vert[j] = Prev[i];
        ++j;
      } 
      vmask <<= 1;
    }
    
    for (; j < P.numVerts; j++) // remaining vertices are new and defined here
    {
      // Fetch vertices
      UINT32 ix = data[0];
      UINT32 iy = data[1];
      UINT32 iz = data[2];
      UINT32 it = data[3];
      
      // Decode vertices
      P.Vert[j].x = (GLfloat) (((INT32)ix)>>8) * vertexFactor;
      P.Vert[j].y = (GLfloat) (((INT32)iy)>>8) * vertexFactor;
      P.Vert[j].z = (GLfloat) (((INT32)iz)>>8) * vertexFactor;
      P.Vert[j].n[0] = (GLfloat)(INT8)(ix&0xFF);
      P.Vert[j].n[1] = (GLfloat)(INT8)(iy&0xFF);
      P.Vert[j].n[2] = (GLfloat)(INT8)(iz&0xFF);
      P.Vert[j].u = (GLfloat) ((UINT16)(it>>16)) * uvScale;
      P.Vert[j].v = (GLfloat) ((UINT16)(it&0xFFFF)) * uvScale;
      P.Vert[j].intensity = GLfloat((ix + 128) & 0xFF) / 255.0f; // signed (-0.5 -> black, +0.5 -> white)
      //P.Vert[j].intensity_7u = GLfloat(ix & 0x7F) / 127.0f;
      //if ((P.header[1] & 0x20) && j == 0)
      //if (!(P.header[1] & 0x20) && !(P.header[1] & 0x08) && !(P.header[6] & 0x10000))
      //{
      //  printf("%06X: le=%d fx=%d sm=%d %02x %02x %02x\tcolor=%06x (%s)\tambient=%1.2f\n", lutIdx, !!!(P.header[6] & 0x10000), !!(P.header[1] & 0x20), !!(P.header[1] & 0x08), ix&0xff, iy&0xff, iz&0xff, P.header[4]>>8, (P.header[1]&2) ? "rgb" : "pal", lightingParams[4]);
      //}
      data += 4;
      
      // Normalize the vertex normal
      GLfloat mag = sqrt(P.Vert[j].n[0]*P.Vert[j].n[0]+P.Vert[j].n[1]*P.Vert[j].n[1]+P.Vert[j].n[2]*P.Vert[j].n[2]);
      P.Vert[j].n[0] /= mag;
      P.Vert[j].n[1] /= mag;
      P.Vert[j].n[2] /= mag;
    }
    
    if (validPoly)
    {
      // Copy current vertices into previous vertex array
      for (size_t i = 0; i < 4; i++)
        Prev[i] = P.Vert[i];
      
      // Copy this polygon into the model buffer
      if (OKAY != InsertPolygon(Cache,&P))
        return NULL;
      ++numPolys;
    }
  }
  
  // Finish model and enter it into the LUT
  EndModel(Cache, Model, lutIdx, textureOffsetState, useStencil);
  return Model;
}


/******************************************************************************
 Cache Management
******************************************************************************/

/*
 * Look up a model. Use this to determine if a model needs to be cached
 * (returns NULL if so).
 */
struct VBORef *CLegacy3D::LookUpModel(ModelCache *Cache, int lutIdx, UINT16 textureOffsetState)
{
  int m = Cache->lut[lutIdx];
  
  // Has any state associated with this model LUT index been cached at all?
  if (m < 0)
    return NULL;
  
  // Has the specified texture offset been cached?
  for (struct VBORef *Model = &(Cache->Models[m]); Model != NULL; Model = Model->nextTextureOffsetState)
  {
    if (Model->textureOffsetState == textureOffsetState)
      return Model;
  }
  
  return NULL;  // no match found, we must cache this new model state
}

// Discard all models in the cache and the display list
void CLegacy3D::ClearModelCache(ModelCache *Cache)
{
  Cache->vboCurOffset = 0;
  for (size_t i = 0; i < 2; i++)
    Cache->curVertIdx[i] = 0;
  for (size_t i = 0; i < Cache->numModels; i++)
    Cache->lut[Cache->Models[i].lutIdx] = -1;

  Cache->numModels = 0;
  ClearDisplayList(Cache);
}

bool CLegacy3D::CreateModelCache(ModelCache *Cache, unsigned vboMaxVerts, 
                 unsigned localMaxVerts, unsigned maxNumModels, unsigned numLUTEntries, 
                 unsigned displayListSize, bool isDynamic)
{
  Cache->dynamic = isDynamic;
  
  /*
   * VBO allocation:
   *
   * Progressively smaller VBOs, in steps of localMaxVerts are allocated
   * until successful. If the size dips below localMaxVerts, localMaxVerts is
   * attempted as the final try.
   */
   
  glGetError(); // clear error flag
  glGenBuffers(1, &(Cache->vboID));
  glBindBuffer(GL_ARRAY_BUFFER, Cache->vboID);
  
  size_t vboBytes = vboMaxVerts*VBO_VERTEX_SIZE*sizeof(GLfloat);
  size_t localBytes = localMaxVerts*VBO_VERTEX_SIZE*sizeof(GLfloat);
  
  // Try allocating until size is 
  bool success = false;
  while (vboBytes >= localBytes)
  {
    glBufferData(GL_ARRAY_BUFFER, vboBytes, 0, isDynamic?GL_STREAM_DRAW:GL_STATIC_DRAW);
    if (glGetError() == GL_NO_ERROR)
    {
      success = true;
      break;
    }
    
    vboBytes -= localBytes;
  }
  
  if (!success)
  {
    // Last ditch attempt: try the local buffer size
    vboBytes = localBytes;
    glBufferData(GL_ARRAY_BUFFER, vboBytes, nullptr, isDynamic?GL_STREAM_DRAW:GL_STATIC_DRAW);
    if (glGetError() != GL_NO_ERROR)
      return ErrorLog("OpenGL was unable to provide a %s vertex buffer.", isDynamic?"dynamic":"static");
  }
  
  DebugLog("%s vertex buffer size: %1.2f MB", isDynamic?"Dynamic":"Static", (float)vboBytes/(float)0x100000);
  InfoLog("%s vertex buffer size: %1.2f MB", isDynamic?"Dynamic":"Static", (float)vboBytes/(float)0x100000);
  
  // Set the VBO to the size we obtained
  Cache->vboMaxOffset = vboBytes;
  Cache->vboCurOffset = 0;
  
  // Attempt to allocate space for local VBO
  for (size_t i = 0; i < 2; i++)
  {
    Cache->verts[i] = new(std::nothrow) GLfloat[localMaxVerts*VBO_VERTEX_SIZE];
    Cache->curVertIdx[i] = 0;
  }
  Cache->maxVertIdx = localMaxVerts;
  
  // ... model array
  Cache->Models = new(std::nothrow) VBORef[maxNumModels];
  Cache->maxModels = maxNumModels;
  Cache->numModels = 0;
  
  // ... LUT
  Cache->lut = new(std::nothrow) INT16[numLUTEntries];
  Cache->lutSize = numLUTEntries;
  
  // ... display list
  Cache->List = new(std::nothrow) DisplayList[displayListSize];
  ClearDisplayList(Cache);
  Cache->maxListSize = displayListSize;
  
  // Check if memory allocation succeeded
  if ((Cache->verts[0]==NULL) || (Cache->verts[1]==NULL) || (Cache->Models==NULL) || (Cache->lut==NULL) || (Cache->List==NULL))
  {
    DestroyModelCache(Cache);
    return ErrorLog("Insufficient memory for model cache.");
  }

  // Clear LUT (MUST be done here because ClearModelCache() won't do it for dynamic models)
  for (size_t i = 0; i < numLUTEntries; i++)
    Cache->lut[i] = -1;
    
  // All good!
  return OKAY;
}

void CLegacy3D::DestroyModelCache(ModelCache *Cache)
{
  glDeleteBuffers(1, &(Cache->vboID));

  for (size_t i = 0; i < 2; i++)
  {
    if (Cache->verts[i] != NULL)
      delete [] Cache->verts[i];
  }
  if (Cache->Models != NULL)
    delete [] Cache->Models;
  if (Cache->lut != NULL)
    delete [] Cache->lut;
  if (Cache->List != NULL)
    delete [] Cache->List;
  
  memset(Cache, 0, sizeof(ModelCache));
}

} // Legacy3D
