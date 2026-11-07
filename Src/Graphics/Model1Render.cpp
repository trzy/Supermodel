/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * Model1Render.cpp
 *
 * The frame is drawn in the order of MAME's screen_update_model1():
 *
 *  1. The tilemap layers below the 3D (opaque, includes the backdrop pen).
 *  2. The 3D quads of the below-HUD pass, batch by batch, each batch clipped
 *     to its viewport with the scissor rectangle.
 *  3. The HUD tilemap layers (alpha blended: alpha is 0 where they're empty).
 *  4. The 3D quads of the above-HUD pass. The hardware lets them show only
 *     through dark HUD pixels; MAME snapshots the image, draws the pass and
 *     puts back the snapshot's bright pixels (any channel above 8). The same
 *     is done here with a copy of the frame buffer.
 *
 * Quads are flat colored, so splitting them into two triangles changes
 * nothing. Moire quads (the hardware's translucency) draw every other pixel
 * of the native 496x384 grid, so the checkerboard looks the same at any
 * resolution. Wireframe quads are drawn as lines one native pixel wide.
 *
 * The Model 2 fragment shader is based on original work by R. Belmont, Olivier
 * Galibert, ElSemi, Angelo Salese and Matthew Daniels in MAME (model2rd.ipp),
 * and its log2 table on original work by Aaron Giles in MAME
 * (voodoo_render.cpp). Portions Copyright (c) R. Belmont, Olivier Galibert,
 * ElSemi, Angelo Salese, Matthew Daniels and Aaron Giles, used under the BSD
 * 3-Clause License: see Docs/README.txt.
 */

#include "Model1Render.h"
#include "OSD/Logger.h"
#include "Util/BMPFile.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <cstddef>
#include <string>

using Model1::SCREEN_WIDTH;
using Model1::SCREEN_HEIGHT;

static const char s_quadVertexShader[] = R"glsl(
#version 410 core

layout(location = 0) in vec2  inPos;      // native screen coordinates (y down)
layout(location = 1) in vec4  inColor;
layout(location = 2) in float inMoire;

uniform vec2 screenSize;                  // 496, 384
uniform vec2 xMap;                        // native x to clip space x: scale, offset (wide screen: the whole width)

out vec4 fsColor;
flat out float fsMoire;

void main(void)
{
  gl_Position = vec4(inPos.x * xMap.x + xMap.y, 1.0 - inPos.y / screenSize.y * 2.0, 0.0, 1.0);
  fsColor = inColor;
  fsMoire = inMoire;
}
)glsl";

static const char s_quadFragmentShader[] = R"glsl(
#version 410 core

uniform vec2 screenSize;                  // 496, 384
uniform vec4 viewport;                    // x, y, width, height of the viewable area in the frame buffer

in vec4 fsColor;
flat in float fsMoire;

out vec4 fragColor;

void main(void)
{
  if (fsMoire > 0.5)
  {
    // checkerboard on the native pixel grid: MAME draws pixels where (x ^ y) & 1 == 0, y down
    ivec2 p = ivec2(floor((gl_FragCoord.xy - viewport.xy) * screenSize / viewport.zw));
    int x = p.x;
    int y = int(screenSize.y) - 1 - p.y;
    if (((x ^ y) & 1) != 0)
      discard;
  }
  fragColor = vec4(fsColor.rgb, 1.0);
}
)glsl";

static const char s_imageVertexShader[] = R"glsl(
#version 410 core

out vec2 fsTexCoord;

void main(void)
{
  const vec2 corners[4] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
  vec2 p = corners[gl_VertexID % 4];
  gl_Position = vec4(p, 0.0, 1.0);
  fsTexCoord = vec2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);  // row 0 of the image is the top
}
)glsl";

// Preceded by "#version" and "#define UPSCALEMODE n" (UpscaleMode: 0 nearest,
// 1 biquintic, 2 bilinear, 3 bicubic). The tilemap layers are scaled with the
// same filters as the Model 3 2D layers (Shaders2D.h).
static const char s_imageFragmentShader[] = R"glsl(
uniform sampler2D tex;
uniform sampler2D snapshot;               // copy of the frame buffer (mode 2)
uniform int mode;                         // 0 = opaque, 1 = alpha blended, 2 = bright snapshot pixels only, 3 = opaque, unfiltered

in vec2 fsTexCoord;

out vec4 fragColor;

#if (UPSCALEMODE == 1)
// smoothstep instead of linear, remapping to a bilinear lookup
vec4 biquintic(sampler2D sampler, vec2 uv)
{
  vec2 ts = vec2(textureSize(sampler,0));
  uv = uv*ts + 0.5;

  vec2 i = floor(uv);
  vec2 f = uv - i;
  f = f*f*f*(f*(f*6.0-15.0)+10.0) - 0.5;

  uv = (i + f)/ts;
  return texture(sampler, uv);
}
#endif

#if (UPSCALEMODE == 3)
// 4x4 bicubic filter using 4 bilinear texture lookups
vec4 cubic(float v) {
  vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
  vec4 s = n * n * n;
  float x = s.x;
  float y = s.y - 4.0 * s.x;
  float z = s.z - 4.0 * s.y + 6.0 * s.x;
  float w = 6.0 - x - y - z;
  return vec4(x, y, z, w) * (1.0 / 6.0);
}
vec4 bicubic(sampler2D sampler, vec2 uv) {
  vec2 invTexSize = 1.0 / vec2(textureSize(sampler,0));
  uv = uv * vec2(textureSize(sampler,0)) - 0.5;
  vec2 uvi = floor(uv);
  vec2 fxy = uv-uvi;
  vec4 xcubic = cubic(fxy.x);
  vec4 ycubic = cubic(fxy.y);
  vec4 c = uvi.xxyy + vec2(-0.5, 1.5).xyxy;
  vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
  vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;
  offset *= invTexSize.xxyy;
  vec4 sample0 = texture(sampler, offset.xz);
  vec4 sample1 = texture(sampler, offset.yz);
  vec4 sample2 = texture(sampler, offset.xw);
  vec4 sample3 = texture(sampler, offset.yw);
  float sx = s.x / (s.x + s.y);
  float sy = s.z / (s.z + s.w);
  return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}
#endif

vec4 Sample(vec2 uv)
{
#if (UPSCALEMODE == 3)
  return bicubic(tex, uv);
#elif (UPSCALEMODE == 1)
  return vec4(biquintic(tex, uv).rgb, texture(tex, uv).a);
#else
  return texture(tex, uv);
#endif
}

void main(void)
{
  if (mode == 2)
  {
    vec4 c = texelFetch(snapshot, ivec2(gl_FragCoord.xy), 0);
    if (all(lessThanEqual(c.rgb, vec3(8.5 / 255.0))))
      discard;
    fragColor = vec4(c.rgb, 1.0);
    return;
  }
  if (mode == 3)
  {
    fragColor = vec4(texture(tex, fsTexCoord).rgb, 1.0);
    return;
  }
  vec4 c = Sample(fsTexCoord);
  if (mode == 1)
  {
    // As the Model 3 layers: blended by alpha, which the filters smooth at the edges
    if (c.a <= 0.0)
      discard;
    fragColor = c;
  }
  else
    fragColor = vec4(c.rgb, 1.0);
}
)glsl";



/******************************************************************************
 Model 2 3D pass
******************************************************************************/

/*
 * MAME's Model 2 rasterizer (model2rd.ipp) as a fragment shader: the same
 * integer texel addressing, mipmap selection, bilinear filtering,
 * microtextures, translucency, luma lookup and colour tables, per pixel of the
 * output. Pixel tests (viewport, checkerboard) are on the native 496x384 grid.
 * The polygons come back to front (see Model2::CVideo::BuildGPU()).
 */

static const char s_m2VertexShader[] = R"glsl(
#version 410 core

layout(location = 0) in vec3  inPos;      // homogeneous native screen coordinates (y down), w = z
layout(location = 1) in vec2  inUV;       // texture coordinates
layout(location = 2) in vec4  inClip;     // viewport x1, y1, x2, y2 (native, inclusive)
layout(location = 3) in uvec3 inData;     // texture header 0|1, 2|3, luma|lod

uniform vec2 screenSize;                  // 496, 384
uniform vec2 xMap;                        // native x to clip space x: scale, offset (wide screen: the whole width)
const float nearW = 1.0 / 1024.0;         // the near plane (the board's w is >= 0.5 or so)

out vec2       fsUV;                      // perspective correct
flat out vec4  fsClip;
flat out uvec3 fsData;

void main(void)
{
  // z = w - 2 near: the clipping in z keeps w >= near (in front of the eye;
  // a plane at w = 0 would put the clipped vertices at infinity)
  gl_Position = vec4(inPos.x * xMap.x + xMap.y * inPos.z, inPos.z - inPos.y / (screenSize.y * 0.5), inPos.z - 2.0 * nearW, inPos.z);
  fsUV = inUV;
  fsClip = inClip;
  fsData = inData;
}
)glsl";

static const char s_m2FragmentShader[] = R"glsl(
#version 410 core

uniform vec2 screenSize;                  // 496, 384
uniform vec4 viewport;                    // x, y, width, height of the viewable area in the frame buffer
uniform usampler2D     texRAM0;           // texture RAM 0: 2x2 texel blocks, a mip level per hardware level
uniform usampler2D     texRAM1;           // texture RAM 1
uniform usamplerBuffer lumaRAM;           // 8-bit
uniform usamplerBuffer colorXlat;         // 16-bit
uniform usamplerBuffer palRAM;            // 16-bit
uniform usamplerBuffer gammaTable;        // 8-bit

in vec2       fsUV;
flat in vec4  fsClip;
flat in uvec3 fsData;
vec3 fsP;                                 // 1/z, u/z, v/z

out vec4 fragColor;

// voodoo_render.cpp's fast_log2 (as MAME's rasterizer). The table is a
// uniform (s_m2Log2Table, set after linking): a constant array indexed per
// pixel is copied to scratch memory by some drivers (AMD's), a uniform array
// is read from the constant buffer
uniform uint log2Table[128];

int fastLog2(float value)
{
  if (value < 0.0)
    return 0;
  uint ival = floatBitsToUint(value) >> 16;
  int e = int(ival >> 7) - 127;
  return e * 256 + int(log2Table[ival & 127u]);
}

uint LERP(uint x, uint y, uint a)
{
  return (x + (((y - x) * a) >> 8)) & 0x00ff00ffu;
}

// (a branch, not ?:, so that only one of the fetches is made)
uint sheetBlock(uint sheet, ivec2 p, int level)
{
  if (sheet == 0u)
    return texelFetch(texRAM0, p, level).r;
  return texelFetch(texRAM1, p, level).r;
}

// A texel of mip level 'level' (0 for microtextures) at x, y of the texture
// at baseX, baseY (the level's position in the logical 2048x1024 sheet)
uint getTexel(uint baseX, uint baseY, int x, int y, uint sheet, int level)
{
  int x2 = int(baseX) + x;
  int y2 = int(baseY) + y;
  uint texel = 0u;
  // the level's image: the bottom right (2048 >> level) x (1024 >> level)
  int lx = 2048 - (2048 >> level);
  int ly = 1024 - (1024 >> level);
  if (level <= 9 && x2 >= lx && x2 < 2048 && y2 >= ly && y2 < 1024)
    texel = sheetBlock(sheet, ivec2((x2 - lx) >> 1, (y2 - ly) >> 1), level);
  else
  {
    // past the sheet's edges: the RAM address as MAME (the sheet is stored
    // folded, 1024x2048) in level 0 (the first half of the RAM, as uploaded)
    if (x2 >= 1024)
    {
      x2 -= 1024;
      y2 ^= 1024;
    }
    int offset = (y2 / 2) * 512 + (x2 / 2);
    if (offset >= 0 && offset < 512 * 1024)
    {
      int bx = offset & 511, by = offset >> 9;
      texel = sheetBlock(sheet, by < 512 ? ivec2(bx, by) : ivec2(bx + 512, by - 512), 0);
    }
  }
  if ((y & 1) == 0)
    texel >>= 8;
  if ((x & 1) == 0)
    texel >>= 4;
  return texel & 0x0fu;
}

// fetch_bilinear_texel
uint fetchTexel(bool translucent, uint th0, uint th2, int miplevel, int u, int v)
{
  uint mirrX = (th0 >> 8) & 1u;
  uint mirrY = (th0 >> 9) & 1u;
  uint wrapX = (th0 >> 6) & 1u & ~mirrX;
  uint wrapY = (th0 >> 7) & 1u & ~mirrY;
  uint texSwap = (th2 >> 12) & 1u;        // texsheet[0] is texture RAM 1 if set
  uint texW, texH, texX, texY, sheet;

  if (miplevel == -1)
  {
    // microtexture
    uint minlod = (th0 >> 10) & 3u;
    texW = 128u;
    texH = 128u;
    texX = ((th2 >> 13) & 1u) * 128u;
    texY = ((th2 >> 14) & 3u) * 128u;
    sheet = texSwap ^ 1u;
    u <<= (1 << minlod);
    v <<= (1 << minlod);
  }
  else
  {
    texW = (32u << (th0 & 7u)) >> uint(miplevel);
    texH = (32u << ((th0 >> 3) & 7u)) >> uint(miplevel);
    texX = ((32u * (th2 & 0x3fu) - 2048u) >> uint(miplevel)) & 2047u;
    texY = ((32u * ((th2 >> 6) & 0x1fu) - 1024u) >> uint(miplevel)) & 1023u;
    sheet = texSwap ^ uint(miplevel & 1);
    u >>= miplevel;
    v >>= miplevel;
  }

  if (mirrX != 0u && (uint(u) & (texW << 8)) != 0u)
    u = ~u;
  if (mirrY != 0u && (uint(v) & (texH << 8)) != 0u)
    v = ~v;

  // subtract 1/2 texel
  u -= 0x80;
  v -= 0x80;

  uint ufrac = uint(u) & 0xffu;
  uint vfrac = uint(v) & 0xffu;

  uint u0 = uint(u >> 8) & (texW - 1u);
  uint u1 = (u0 + 1u) & (texW - 1u);
  uint v0 = uint(v >> 8) & (texH - 1u);
  uint v1 = (v0 + 1u) & (texH - 1u);

  // clamp the texture coordinates if smooth wrapping is not enabled
  if (wrapX == 0u && u1 == 0u)
  {
    if (ufrac >= 0x80u) { u0 = u1; u1++; ufrac = 0u; }
    else                { u1 = u0; u0--; ufrac = 0x100u; }
  }
  if (wrapY == 0u && v1 == 0u)
  {
    if (vfrac >= 0x80u) { v0 = 0u; v1++; vfrac = 0u; }
    else                { v1 = v0; v0--; vfrac = 0x100u; }
  }

  uint tex00 = getTexel(texX, texY, int(u0), int(v0), sheet, max(miplevel, 0)) << 4;
  uint tex01 = getTexel(texX, texY, int(u1), int(v0), sheet, max(miplevel, 0)) << 4;
  uint tex10 = getTexel(texX, texY, int(u0), int(v1), sheet, max(miplevel, 0)) << 4;
  uint tex11 = getTexel(texX, texY, int(u1), int(v1), sheet, max(miplevel, 0)) << 4;

  if (translucent)
  {
    // pack the alpha components into the upper 16 bits
    if (tex00 != 0xf0u) tex00 |= 0x00800000u;
    if (tex01 != 0xf0u) tex01 |= 0x00800000u;
    if (tex10 != 0xf0u) tex10 |= 0x00800000u;
    if (tex11 != 0xf0u) tex11 |= 0x00800000u;

    // if a texel is transparent, it takes the luma value of the neighboring texel
    if (tex00 == 0x000000f0u) tex00 = tex01 & 0xffu;
    if (tex01 == 0x000000f0u) tex01 = tex00 & 0xffu;
    if (tex10 == 0x000000f0u) tex10 = tex11 & 0xffu;
    if (tex11 == 0x000000f0u) tex11 = tex10 & 0xffu;
  }

  uint tex0x = LERP(tex00, tex01, ufrac);
  uint tex1x = LERP(tex10, tex11, ufrac);

  if (translucent)
  {
    if (tex0x == 0x000000f0u) tex0x = tex1x & 0xffu;
    if (tex1x == 0x000000f0u) tex1x = tex0x & 0xffu;
  }

  return LERP(tex0x, tex1x, vfrac);
}

uint xlat(uint i)   { return texelFetch(colorXlat, int(i)).r & 0xffu; }
uint gammaOf(uint i) { return texelFetch(gammaTable, int(i)).r; }

vec4 shade(uint color, uint luma)
{
  // 6 bits of luma and 5 bits per colour component index the colour tables
  uint r = gammaOf(xlat(0x0000u + (((color >>  0) & 0x1fu) << 8) + luma));
  uint g = gammaOf(xlat(0x2000u + (((color >>  5) & 0x1fu) << 8) + luma));
  uint b = gammaOf(xlat(0x4000u + (((color >> 10) & 0x1fu) << 8) + luma));
  return vec4(float(r), float(g), float(b), 255.0) / 255.0;
}

void main(void)
{
  fsP = vec3(gl_FragCoord.w, fsUV * gl_FragCoord.w);   // gl_FragCoord.w = 1/w = 1/z
  ivec2 p = ivec2(floor((gl_FragCoord.xy - viewport.xy) * screenSize / viewport.zw));
  int x = p.x;
  int y = int(screenSize.y) - 1 - p.y;
  if (float(x) < fsClip.x || float(x) > fsClip.z || float(y) < fsClip.y || float(y) > fsClip.w)
    discard;

  uint th0 = fsData.x & 0xffffu;
  uint th1 = fsData.x >> 16;
  uint th2 = fsData.y & 0xffffu;
  uint th3 = fsData.y >> 16;
  uint lumaValue = fsData.z & 0xffu;
  int  texlod = int(fsData.z) >> 16;
  uint renderer = (th0 >> 13) & 3u;       // bit 1 = textured, bit 0 = translucent
  bool translucent = (renderer & 1u) != 0u;

  // checkerboard: every other pixel of the native grid is skipped
  if (((th0 >> 15) & 1u) != 0u && ((x ^ y) & 1) == 0)
    discard;

  uint colorbase = (th3 >> 6) & 0x3ffu;
  if ((renderer & 2u) == 0u)
  {
    // flat: translucent draws nothing
    if (translucent)
      discard;
    uint color = texelFetch(palRAM, int(colorbase + 0x1000u)).r & 0xffffu;
    fragColor = shade(color, lumaValue >> 2);
    return;
  }

  uint lumabase = (th1 & 0xffu) << 7;
  uint texW = 32u << (th0 & 7u), texH = 32u << ((th0 >> 3) & 7u);
  int maxLevel = findMSB(min(texW, texH)) - 1;   // down to 2x2
  bool utex = ((th0 >> 12) & 1u) != 0u;
  int utexminlod = int((th0 >> 10) & 3u);

  float z = 1.0 / fsP.x;
  int mml = -texlod + fastLog2(z);
  int level = clamp(mml >> 7, 0, maxLevel);
  int u = int(fsP.y * z * 256.0);
  int v = int(fsP.z * z * 256.0);

  // The second level (or the microtexture) is only read when it is blended
  // in: LERP(t, x, 0) = t
  uint t = fetchTexel(translucent, th0, th2, level, u, v);
  if (mml > 0 && level < maxLevel)
  {
    uint w = uint((mml & 127) << 1);
    if (w != 0u)
      t = LERP(t, fetchTexel(translucent, th0, th2, level + 1, u, v), w);
  }
  else if (utex && mml < 0)
  {
    uint w = uint(min(-mml >> utexminlod, 127));
    if (w != 0u)
      t = LERP(t, fetchTexel(translucent, th0, th2, -1, u, v), w);
  }

  if (translucent)
  {
    // if alpha is less than 50%, discard
    if (t < 0x00400000u)
      discard;
    t &= 0xffu;
  }

  uint luma = (texelFetch(lumaRAM, int(lumabase + (t >> 1))).r * lumaValue / 256u) & 0xffu;
  luma = min(luma, 0x3fu);
  uint color = texelFetch(palRAM, int(colorbase + 0x1000u)).r & 0x7fffu;
  fragColor = shade(color, luma);
}
)glsl";


/*
 * Quads: the Model 3 renderer's 4 point interpolation (New3D's
 * R3DShaderQuads.h). The geometry shader takes a quad (GL_LINES_ADJACENCY)
 * and emits it as a strip of 2 triangles, with what the fragment shader needs
 * to weigh the 4 corners at each pixel (generalized barycentric coordinates
 * for quads, with 1/w as R3DShaderQuads.h, so corners behind the eye work);
 * 1/z, u/z and v/z are interpolated with them instead of across each
 * triangle, so the 2 triangles of a quad that isn't flat in texture space
 * don't bend along the diagonal. The quads come whole (BuildGPU() doesn't
 * clip them), so this holds at the screen edges too. The weights are computed
 * in native pixels: they are not unchanged by a stretch in one direction. A
 * triangle (vertex 3 = vertex 2) and pixels on the edges use the triangle
 * interpolation, which is the same there.
 */

static const char s_m2QuadVertexShader[] = R"glsl(
#version 410 core

layout(location = 0) in vec3  inPos;      // homogeneous native screen coordinates (y down), w = z
layout(location = 1) in vec2  inUV;       // texture coordinates
layout(location = 2) in vec4  inClip;     // viewport x1, y1, x2, y2 (native, inclusive)
layout(location = 3) in uvec3 inData;     // texture header 0|1, 2|3, luma|lod

uniform vec2 screenSize;                  // 496, 384
uniform vec2 xMap;                        // native x to clip space x, as the triangles'
const float nearW = 1.0 / 1024.0;         // the near plane, as the triangles'

out VS_OUT
{
  vec2       uv;
  vec4       clip;
  flat uvec3 data;
} vs_out;

void main(void)
{
  gl_Position = vec4(inPos.x * xMap.x + xMap.y * inPos.z, inPos.z - inPos.y / (screenSize.y * 0.5), inPos.z - 2.0 * nearW, inPos.z);
  vs_out.uv = inUV;
  vs_out.clip = inClip;
  vs_out.data = inData;
}
)glsl";

static const char s_m2QuadGeometryShader[] = R"glsl(
#version 410 core

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 4) out;

uniform vec2 screenSize;                  // 496, 384
uniform vec2 xMap;                        // native x to clip space x (the vertex shader's)

in VS_OUT
{
  vec2       uv;
  vec4       clip;
  flat uvec3 data;
} gs_in[4];

out GS_OUT
{
  flat vec2          corner[4];   // the corners (native pixels from the centre, y up)
  flat vec4          cornerCross; // cross(corner[i], corner[i + 1])
  flat vec3          P[4];        // the corners' 1/w, u/w, v/w
  vec4               bary;        // w = 1: a triangle
  vec2               triUV;       // interpolated across the emitted triangle (triangles, edges)
  flat vec4          clip;
  flat uvec3         data;
} gs_out;

// a*b - c*d, computed in a stable fashion (Kahan)
float DifferenceOfProducts(float a, float b, float c, float d)
{
  precise float cd = c * d;
  precise float err = fma(-c, d, cd);
  precise float dop = fma(a, b, -cd);
  return dop + err;
}

void main(void)
{
  // clip space x, y in native pixels from the screen's centre (the weights
  // change with a stretch in one direction): x back through xMap
  vec2 scale = screenSize * 0.5;
  vec4 h[4];
  for (int i = 0; i < 4; i++)
  {
    vec4 c = gl_in[i].gl_Position;
    h[i] = vec4((c.x - xMap.y * c.w) / xMap.x - scale.x * c.w, c.y * scale.y, c.zw);
    float oneOverW = 1.0 / h[i].w;
    gs_out.P[i] = vec3(oneOverW, gs_in[i].uv * oneOverW);
  }
  gs_out.clip = gs_in[0].clip;
  gs_out.data = gs_in[0].data;

  if (gl_in[3].gl_Position == gl_in[2].gl_Position)
  {
    // a triangle
    for (int i = 0; i < 3; i++)
    {
      for (int j = 0; j < 4; j++)
        gs_out.corner[j] = vec2(0.0);
      gs_out.cornerCross = vec4(0.0);
      gs_out.bary = vec4(i == 0, i == 1, i == 2, 1.0);
      gs_out.triUV = gs_in[i].uv;
      gl_Position = gl_in[i].gl_Position;
      EmitVertex();
    }
    return;
  }

  vec2 v[4];
  for (int i = 0; i < 4; i++)
    v[i] = h[i].xy / h[i].w;

  // the cross products of all the corners, divided by w afterwards (precision)
  precise float cross[16];
  for (int i = 0; i < 4; i++)
  {
    cross[i * 4 + i] = 0.0;
    for (int j = i + 1; j < 4; j++)
      cross[i * 4 + j] = DifferenceOfProducts(h[i].x, h[j].y, h[j].x, h[i].y) / (h[i].w * h[j].w);
  }
  for (int i = 1; i < 4; i++)
    for (int j = 0; j < i; j++)
      cross[i * 4 + j] = -cross[j * 4 + i];

  // the quad 0-1-2-3 as the strip 1, 0, 2, 3
  const int reorder[4] = int[4](1, 0, 2, 3);
  const vec3 baryOf[4] = vec3[4](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, -1.0, 0.0));
  for (int i = 0; i < 4; i++)
  {
    int ii = reorder[i];
    for (int j = 0; j < 4; j++)
    {
      gs_out.corner[j] = v[j];
      gs_out.cornerCross[j] = cross[j * 4 + (j + 1) % 4];
    }
    gs_out.bary = vec4(baryOf[ii], 0.0);
    gs_out.triUV = gs_in[ii].uv;
    gl_Position = gl_in[ii].gl_Position;
    EmitVertex();
  }
}
)glsl";

// The fragment shader's inputs for quads, in place of the triangles' (see
// QuadFragmentShader())
static const char s_m2QuadFragmentInputs[] = R"glsl(
in GS_OUT
{
  flat vec2          corner[4];
  flat vec4          cornerCross;
  flat vec3          P[4];
  vec4               bary;
  vec2               triUV;
  flat vec4          clip;
  flat uvec3         data;
} fs_in;

#define fsClip fs_in.clip
#define fsData fs_in.data
vec3 fsP;

// 1/z, u/z, v/z: the 4 corners weighed (R3DShaderQuads.h's QuadraticInterpolation)
vec3 QuadP()
{
  const float hpFMin = 0.00006103515625;    // half precision FLT_MIN
  if (fs_in.bary.w > 0.5 || fs_in.bary.x <= hpFMin || fs_in.bary.z <= hpFMin)
    return vec3(gl_FragCoord.w, fs_in.triUV * gl_FragCoord.w);   // a triangle, or the edge: linear there

  // The corners relative to the pixel, and the areas of the triangles they
  // make with it, from the flat corners and the pixel's position: not
  // interpolated (noperspective values across triangles clipped at the near
  // plane come out wrong on some GPUs). area[i] = cross(v[i], v[i + 1]).
  vec2 pixel = (gl_FragCoord.xy - viewport.xy) / viewport.zw * screenSize - screenSize * 0.5;
  vec2 v[4];
  for (int i = 0; i < 4; i++)
    v[i] = fs_in.corner[i] - pixel;
  float area[4];
  for (int i = 0; i < 4; i++)
  {
    vec2 edge = fs_in.corner[i] - fs_in.corner[(i + 1) % 4];
    area[i] = fs_in.cornerCross[i] + (pixel.x * edge.y - pixel.y * edge.x);
  }

  float u[4];
  for (int i = 0; i < 4; i++)
    u[i] = length(v[i]) * sign(fs_in.P[i].x);   // behind the eye (w < 0)?

  // precise each, not the array: a precise array made this shader about ten
  // times slower per pixel on NVIDIA
  float t[4];
  for (int i = 0; i < 4; i++)
  {
    int iNext = (i + 1) % 4;
    precise float ti = area[i] == 0.0 ? 0.0 : fma(u[i], u[iNext], -dot(v[i], v[iNext])) / area[i];
    t[i] = ti;
  }

  // weights neither all positive nor all negative: outside the quad
  int signCount = 0;
  for (int i = 0; i < 4; i++)
  {
    int iPrev = (i + 3) % 4;
    u[i] = (t[iPrev] + t[i]) / u[i];
    signCount += (t[iPrev] < -t[i]) ? -1 : 1;
  }
  if (signCount == 0)
    discard;

  vec3 p = vec3(0.0);
  float sum = 0.0;
  for (int i = 0; i < 4; i++)
  {
    p = fma(fs_in.P[i], vec3(u[i]), p);
    sum += u[i];
  }
  return p / sum;
}
)glsl";

// The fragment shader's log2Table (voodoo_render.cpp's fast_log2 table)
static const GLuint s_m2Log2Table[128] =
{
    0,   2,   5,   8,  11,  14,  16,  19,  22,  25,  27,  30,  33,  35,  38,  40,
   43,  46,  48,  51,  53,  56,  58,  61,  63,  65,  68,  70,  73,  75,  77,  80,
   82,  84,  87,  89,  91,  93,  96,  98, 100, 102, 104, 106, 109, 111, 113, 115,
  117, 119, 121, 123, 125, 127, 129, 132, 134, 136, 138, 140, 141, 143, 145, 147,
  149, 151, 153, 155, 157, 159, 161, 162, 164, 166, 168, 170, 172, 173, 175, 177,
  179, 181, 182, 184, 186, 188, 189, 191, 193, 194, 196, 198, 200, 201, 203, 205,
  206, 208, 209, 211, 213, 214, 216, 218, 219, 221, 222, 224, 225, 227, 229, 230,
  232, 233, 235, 236, 238, 239, 241, 242, 244, 245, 247, 248, 250, 251, 253, 254
};

// The triangles' fragment shader with the quad inputs: fsP from QuadP()
static std::string QuadFragmentShader()
{
  std::string s = s_m2FragmentShader;
  const std::string inputs = "in vec2       fsUV;\nflat in vec4  fsClip;\nflat in uvec3 fsData;\nvec3 fsP;                                 // 1/z, u/z, v/z\n";
  const std::string body = "void main(void)\n{\n  fsP = vec3(gl_FragCoord.w, fsUV * gl_FragCoord.w);   // gl_FragCoord.w = 1/w = 1/z\n";
  size_t i = s.find(inputs);
  size_t j = s.find(body);
  if (i == std::string::npos || j == std::string::npos)
    return std::string();
  s.replace(j, body.size(), "void main(void)\n{\n  fsP = QuadP();\n");
  s.replace(i, inputs.size(), s_m2QuadFragmentInputs);
  return s;
}

static GLuint CompileShader(GLenum type, const char *source)
{
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok)
  {
    char log[4096] = { 0 };
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    ErrorLog("Model 2 quad shader: %s", log);
  }
  return shader;
}

// A program of vertex, geometry and fragment shaders (0 if it doesn't build)
static GLuint BuildQuadProgram()
{
  std::string fragment = QuadFragmentShader();
  if (fragment.empty())
    return 0;
  GLuint vs = CompileShader(GL_VERTEX_SHADER, s_m2QuadVertexShader);
  GLuint gs = CompileShader(GL_GEOMETRY_SHADER, s_m2QuadGeometryShader);
  GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragment.c_str());
  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, gs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glDeleteShader(vs);
  glDeleteShader(gs);
  glDeleteShader(fs);
  GLint ok = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok)
  {
    char log[4096] = { 0 };
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    ErrorLog("Model 2 quad shaders: %s", log);
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

bool CModel1Render::InitModel2()
{
  if (!m_m2Shader.LoadShaders(s_m2VertexShader, s_m2FragmentShader))
  {
    ErrorLog("Model 1/2 renderer: unable to compile the Model 2 shaders.");
    return false;
  }
  m_m2ScreenSizeLoc = m_m2Shader.GetUniformLocation("screenSize");
  m_m2ViewportLoc = m_m2Shader.GetUniformLocation("viewport");
  m_m2XMapLoc = m_m2Shader.GetUniformLocation("xMap");

  glGenVertexArrays(1, &m_m2VAO);
  glGenBuffers(1, &m_m2VBO);
  glBindVertexArray(m_m2VAO);
  glBindBuffer(GL_ARRAY_BUFFER, m_m2VBO);
  typedef Model2::GPUVertex V;
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (const void *) offsetof(V, x));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V), (const void *) offsetof(V, u));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(V), (const void *) offsetof(V, clip));
  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(3, 3, GL_UNSIGNED_INT, sizeof(V), (const void *) offsetof(V, header01));
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Texture RAM 0/1: 2D textures of 2x2 texel blocks (16 bits each), the
  // logical 2048x1024 sheet as 1024x512 blocks, with mip levels 1-9 (see
  // UploadModel2Sheet())
  glGenBuffers(M2_NUM_BUFFERS, m_m2Buffers);
  glGenTextures(M2_NUM_BUFFERS, m_m2Textures);
  for (int i = M2_TEXRAM0; i <= M2_TEXRAM1; i++)
  {
    glBindTexture(GL_TEXTURE_2D, m_m2Textures[i]);
    for (int level = 0; level <= M2_SHEET_LEVELS; level++)
      glTexImage2D(GL_TEXTURE_2D, level, GL_R16UI, 1024 >> level, std::max(1, 512 >> level), 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, M2_SHEET_LEVELS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  m_m2SheetValid[0] = false;
  m_m2SheetValid[1] = false;

  // The rest: buffer textures, sized for the board's memory
  static const GLenum formats[M2_NUM_BUFFERS] = { GL_R32UI, GL_R32UI, GL_R8UI, GL_R16UI, GL_R16UI, GL_R8UI };
  static const size_t sizes[M2_NUM_BUFFERS] = { 0x80000 * 4, 0x80000 * 4, 0x8000, 0x6000 * 2, 0x2000 * 2, 256 };
  for (int i = M2_LUMA; i < M2_NUM_BUFFERS; i++)
  {
    glBindBuffer(GL_TEXTURE_BUFFER, m_m2Buffers[i]);
    glBufferData(GL_TEXTURE_BUFFER, sizes[i], nullptr, GL_DYNAMIC_DRAW);
    glBindTexture(GL_TEXTURE_BUFFER, m_m2Textures[i]);
    glTexBuffer(GL_TEXTURE_BUFFER, formats[i], m_m2Buffers[i]);
  }
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  m_m2Shader.EnableShader();
  static const char *names[M2_NUM_BUFFERS] = { "texRAM0", "texRAM1", "lumaRAM", "colorXlat", "palRAM", "gammaTable" };
  for (int i = 0; i < M2_NUM_BUFFERS; i++)
    glUniform1i(m_m2Shader.GetUniformLocation(names[i]), 2 + i);   // units 0 and 1 are the image shader's
  glUniform1uiv(m_m2Shader.GetUniformLocation("log2Table"), 128, s_m2Log2Table);
  m_m2Shader.DisableShader();

  // Quads (if they don't build, the triangles are drawn instead)
  m_m2QuadProgram = BuildQuadProgram();
  if (m_m2QuadProgram)
  {
    glUseProgram(m_m2QuadProgram);
    for (int i = 0; i < M2_NUM_BUFFERS; i++)
      glUniform1i(glGetUniformLocation(m_m2QuadProgram, names[i]), 2 + i);
    glUniform1uiv(glGetUniformLocation(m_m2QuadProgram, "log2Table"), 128, s_m2Log2Table);
    m_m2QuadScreenSizeLoc = glGetUniformLocation(m_m2QuadProgram, "screenSize");
    m_m2QuadViewportLoc = glGetUniformLocation(m_m2QuadProgram, "viewport");
    m_m2QuadXMapLoc = glGetUniformLocation(m_m2QuadProgram, "xMap");
    glUseProgram(0);
  }
  else
    ErrorLog("Model 1/2 renderer: unable to build the Model 2 quad shaders; drawing triangles.");
  m_m2Ready = true;
  return true;
}

// Texture RAM (0x80000 words; the rasterizer reads the first half): 16-bit
// words of 2x2 texel blocks, 512 words a row, the logical 2048x1024 sheet
// folded to 1024x2048 (its right half below the left). Mip level n (n >= 1)
// is always the bottom right (2048 >> n) x (1024 >> n) of the logical sheet
// (fetch_bilinear_texel's texture x, y for the level), so each is uploaded as
// that GL mip level, as the Model 3 renderer's mipmaps (a fetch stays within
// one level's image instead of reading the levels from the one sheet).
void CModel1Render::UploadModel2Sheet(GLuint texture, const uint32_t *ram)
{
  const uint16_t *ram16 = reinterpret_cast<const uint16_t *>(ram);
  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 512);
  // level 0: the left half is block rows 0-511, the right half rows 512-1023
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 512, 512, GL_RED_INTEGER, GL_UNSIGNED_SHORT, ram16);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 512, 0, 512, 512, GL_RED_INTEGER, GL_UNSIGNED_SHORT, ram16 + 512 * 512);
  // level n: in the right half, its bottom right
  for (int level = 1; level <= M2_SHEET_LEVELS; level++)
  {
    int w = 1024 >> level, h = std::max(1, 512 >> level);
    glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, w, h, GL_RED_INTEGER, GL_UNSIGNED_SHORT, ram16 + (1024 - h) * 512 + (512 - w));
  }
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void CModel1Render::DrawModel2(const std::vector<Model2::GPUVertex> &vertices, const Model2::CVideo::Memory &memory, bool quads)
{
  if (!m_m2Ready || vertices.empty())
    return;

  // The board's memory as it is now. Texture RAM (2 MB a sheet with its mip
  // levels) only when the board wrote to it since the last upload: most
  // frames don't, and the integer-format uploads are slow on some drivers
  // (AMD's). The board sets textureDirty[i] when it writes sheet i, and we
  // clear it once it's uploaded.
  const uint32_t *sheets[2] = { memory.textureRAM0, memory.textureRAM1 };
  for (int i = 0; i < 2; i++)
  {
    bool dirty = !memory.textureDirty || memory.textureDirty[i];
    if (dirty || !m_m2SheetValid[i] || sheets[i] != m_m2SheetRAM[i])
    {
      UploadModel2Sheet(m_m2Textures[M2_TEXRAM0 + i], sheets[i]);
      m_m2SheetValid[i] = true;
      m_m2SheetRAM[i] = sheets[i];
      if (memory.textureDirty)
        memory.textureDirty[i] = false;
    }
  }
  for (int i = M2_TEXRAM0; i <= M2_TEXRAM1; i++)
  {
    glActiveTexture(GL_TEXTURE2 + i);
    glBindTexture(GL_TEXTURE_2D, m_m2Textures[i]);
  }
  const void *data[M2_NUM_BUFFERS] = { nullptr, nullptr, memory.lumaRAM, memory.colorXlat, memory.paletteRAM, memory.gamma };
  static const size_t sizes[M2_NUM_BUFFERS] = { 0, 0, 0x8000, 0x6000 * 2, 0x2000 * 2, 256 };
  for (int i = M2_LUMA; i < M2_NUM_BUFFERS; i++)
  {
    glBindBuffer(GL_TEXTURE_BUFFER, m_m2Buffers[i]);
    glBufferSubData(GL_TEXTURE_BUFFER, 0, sizes[i], data[i]);
    glActiveTexture(GL_TEXTURE2 + i);
    glBindTexture(GL_TEXTURE_BUFFER, m_m2Textures[i]);
  }
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
  glActiveTexture(GL_TEXTURE0);

  glBindVertexArray(m_m2VAO);
  glBindBuffer(GL_ARRAY_BUFFER, m_m2VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Model2::GPUVertex), vertices.data(), GL_STREAM_DRAW);
  if (quads && m_m2QuadProgram)
  {
    glUseProgram(m_m2QuadProgram);
    glUniform2f(m_m2QuadScreenSizeLoc, float(SCREEN_WIDTH), float(SCREEN_HEIGHT));
    glUniform4f(m_m2QuadViewportLoc, float(m_xOffset), float(m_yOffset), float(m_xPixels), float(m_yPixels));
    glUniform2f(m_m2QuadXMapLoc, m_xMap[0], m_xMap[1]);
    glDrawArrays(GL_LINES_ADJACENCY, 0, (GLsizei) vertices.size());
    glUseProgram(0);
  }
  else if (!quads)  // (quads without the program: nothing, they aren't triangles)
  {
    m_m2Shader.EnableShader();
    glUniform2f(m_m2ScreenSizeLoc, float(SCREEN_WIDTH), float(SCREEN_HEIGHT));
    glUniform4f(m_m2ViewportLoc, float(m_xOffset), float(m_yOffset), float(m_xPixels), float(m_yPixels));
    glUniform2f(m_m2XMapLoc, m_xMap[0], m_xMap[1]);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei) vertices.size());
    m_m2Shader.DisableShader();
  }
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/******************************************************************************
 Drawing
******************************************************************************/

// Native rectangle (inclusive) to a scissor rectangle in the frame buffer
void CModel1Render::SetScissor(int x1, int y1, int x2, int y2)
{
  const double sx = double(m_xPixels) / SCREEN_WIDTH;
  const double sy = double(m_yPixels) / SCREEN_HEIGHT;
  int left   = int(std::lround(x1 * sx));
  int right  = int(std::lround((x2 + 1) * sx));
  int top    = int(std::lround(y1 * sy));
  int bottom = int(std::lround((y2 + 1) * sy));
  if (right < left)
    right = left;
  if (bottom < top)
    bottom = top;
  glScissor(m_xOffset + left, m_yOffset + (m_yPixels - bottom), right - left, bottom - top);
}

void CModel1Render::AddQuad(const Model1::ScreenQuad &q, float expand)
{
  // 0x00RRGGBB to bytes R, G, B, A
  const uint32_t c = q.color;
  const uint32_t rgba = ((c >> 16) & 0xFF) | (c & 0xFF00) | ((c & 0xFF) << 16) | 0xFF000000;
  const float moire = (q.color & Model1::MOIRE) ? 1.0f : 0.0f;

  if (q.line)
  {
    // A line one native pixel wide, centered on the pixel centers of its end points
    float ax = q.x[q.lineA] + 0.5f, ay = q.y[q.lineA] + 0.5f;
    float bx = q.x[q.lineB] + 0.5f, by = q.y[q.lineB] + 0.5f;
    float dx = bx - ax, dy = by - ay;
    float len = std::sqrt(dx * dx + dy * dy);
    if (!(len > 0.0f) || !std::isfinite(len))
      return;
    dx = dx / len * 0.5f;
    dy = dy / len * 0.5f;
    const Vertex v0 { ax - dx - dy, ay - dy + dx, rgba, moire };
    const Vertex v1 { ax - dx + dy, ay - dy - dx, rgba, moire };
    const Vertex v2 { bx + dx + dy, by + dy - dx, rgba, moire };
    const Vertex v3 { bx + dx - dy, by + dy + dx, rgba, moire };
    m_vertices.insert(m_vertices.end(), { v0, v1, v2, v0, v2, v3 });
    return;
  }

  // The corners, without repeats (a triangle is a quad with two equal corners)
  float px[4], py[4];
  int n = 0;
  for (int i = 0; i < 4; i++)
  {
    // Projections of degenerate points can be infinite or NaN; MAME's integer
    // conversion turns them into large values, which the clip rectangle removes
    if (!std::isfinite(q.x[i]) || !std::isfinite(q.y[i]))
      return;
    if (n && std::fabs(q.x[i] - px[n - 1]) < 1e-4f && std::fabs(q.y[i] - py[n - 1]) < 1e-4f)
      continue;
    px[n] = q.x[i];
    py[n] = q.y[i];
    n++;
  }
  if (n > 1 && std::fabs(px[n - 1] - px[0]) < 1e-4f && std::fabs(py[n - 1] - py[0]) < 1e-4f)
    n--;
  if (n < 3)
    return;

  // The hardware's rasterizer truncates the corners to whole pixels and fills
  // each span inclusively, so every polygon covers up to about one native
  // pixel more than its exact shape. The models rely on it: separate strips
  // that only nearly touch, and slivers thinner than a pixel. The exact shape
  // drawn at a high resolution shows cracks there, so each polygon is grown
  // outward by m_edgeExpand native pixels (mitred corners, limited at sharp
  // ones). Being in native pixels, it closes the same gaps at any resolution.
  float area = 0.0f;
  for (int i = 0; i < n; i++)
  {
    int j = (i + 1) % n;
    area += px[i] * py[j] - px[j] * py[i];
  }
  if (expand > 0.0f && std::fabs(area) > 1e-6f)
  {
    const float s = area > 0.0f ? 1.0f : -1.0f;  // outward normal of edge (dx, dy) is s * (dy, -dx)
    float nx[4], ny[4];
    for (int i = 0; i < n; i++)
    {
      int j = (i + 1) % n;
      float dx = px[j] - px[i], dy = py[j] - py[i];
      float len = std::sqrt(dx * dx + dy * dy);
      nx[i] = len > 0.0f ? s * dy / len : 0.0f;
      ny[i] = len > 0.0f ? -s * dx / len : 0.0f;
    }
    // Each corner moves along its bisector, as far as a miter would but at
    // most 1.5x the expansion: sharp corners (thin slivers such as eyebrow
    // strokes) would otherwise grow long spikes
    const float maxLength = 1.5f * expand;
    float ox[4], oy[4];
    for (int i = 0; i < n; i++)
    {
      int prev = (i + n - 1) % n;
      float d = std::max(1.0f + nx[prev] * nx[i] + ny[prev] * ny[i], 1e-3f);
      float mx = expand * (nx[prev] + nx[i]) / d;
      float my = expand * (ny[prev] + ny[i]) / d;
      float len = std::sqrt(mx * mx + my * my);
      if (len > maxLength)
      {
        mx *= maxLength / len;
        my *= maxLength / len;
      }
      ox[i] = px[i] + mx;
      oy[i] = py[i] + my;
    }
    for (int i = 0; i < n; i++)
    {
      px[i] = ox[i];
      py[i] = oy[i];
    }
  }

  Vertex v[4];
  for (int i = 0; i < n; i++)
    v[i] = Vertex { px[i], py[i], rgba, moire };
  m_vertices.insert(m_vertices.end(), { v[0], v[1], v[2] });
  if (n == 4)
    m_vertices.insert(m_vertices.end(), { v[0], v[2], v[3] });
}

void CModel1Render::DrawBatches(const Model1::Frame &frame, const std::vector<Model1::Batch> &batches)
{
  if (batches.empty())
    return;

  // All the vertices in one buffer, then one draw per batch (its scissor
  // rectangle). Primitives are drawn in submission order.
  //
  // With crack filling, each batch is drawn twice: first every quad grown by
  // m_edgeExpand native pixels (covering like the hardware's rasterizer, so
  // the cracks between strips of a model are filled), then every quad again
  // with its exact shape. Where a quad really covers a pixel, the exact pass
  // gives it its exact shape (small details such as eyes and eyebrows stay
  // clean); the grown quads only show where nothing covers the pixel.
  m_vertices.clear();
  std::vector<std::pair<GLint, GLsizei>> ranges;
  ranges.reserve(batches.size());
  for (const Model1::Batch &b : batches)
  {
    GLint first = (GLint) m_vertices.size();
    if (m_crackFill)
    {
      for (uint32_t i = 0; i < b.count; i++)
        if (!frame.quads[b.first + i].line)
          AddQuad(frame.quads[b.first + i], m_edgeExpand);
      for (uint32_t i = 0; i < b.count; i++)
        AddQuad(frame.quads[b.first + i], 0.0f);
    }
    else
    {
      for (uint32_t i = 0; i < b.count; i++)
        AddQuad(frame.quads[b.first + i], m_edgeExpand);
    }
    ranges.emplace_back(first, (GLsizei) (m_vertices.size() - first));
  }
  if (m_vertices.empty())
    return;

  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STREAM_DRAW);

  m_quadShader.EnableShader();
  glUniform2f(m_quadScreenSizeLoc, float(SCREEN_WIDTH), float(SCREEN_HEIGHT));
  glUniform4f(m_quadViewportLoc, float(m_xOffset), float(m_yOffset), float(m_xPixels), float(m_yPixels));
  glUniform2f(m_quadXMapLoc, m_xMap[0], m_xMap[1]);

  glEnable(GL_SCISSOR_TEST);
  for (size_t i = 0; i < batches.size(); i++)
  {
    if (!ranges[i].second)
      continue;
    const Model1::Batch &b = batches[i];
    SetScissor(b.x1, b.y1, b.x2, b.y2);
    glDrawArrays(GL_TRIANGLES, ranges[i].first, ranges[i].second);
  }

  m_quadShader.DisableShader();
  glBindVertexArray(0);
}

void CModel1Render::DrawImage(GLuint texture, const uint32_t *pixels, int mode)
{
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  if (pixels)
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_snapshotTexture);
  glActiveTexture(GL_TEXTURE0);

  m_imageShader.EnableShader();
  glUniform1i(m_imageTexLoc, 0);
  glUniform1i(m_imageSnapshotLoc, 1);
  glUniform1i(m_imageModeLoc, mode);
  if (mode == 1)
  {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
  glBindVertexArray(m_imageVAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
  glDisable(GL_BLEND);
  m_imageShader.DisableShader();
}

void CModel1Render::SnapshotFrameBuffer()
{
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_snapshotTexture);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_totalXPixels, m_totalYPixels);
  glActiveTexture(GL_TEXTURE0);
}

void CModel1Render::DrawFrame(const VideoFrame &frame)
{
  if (!m_ready)
    return;

  if (m_aaTarget)
    glBindFramebuffer(GL_FRAMEBUFFER, m_aaTarget);

  GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  GLint scissorBox[4];
  glGetIntegerv(GL_SCISSOR_BOX, scissorBox);

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDepthMask(GL_FALSE);
  glViewport(0, 0, m_totalXPixels, m_totalYPixels);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // The 4:3 area, and with wide screen the whole width: the 3D is drawn over
  // it (xMap maps native x there; the fragment shaders' native pixels still
  // come from the 4:3 area, beyond it x < 0 or > 495). The tilemap layers stay
  // 4:3, except the bottom one with WideBackground, stretched.
  const bool wide = m_wideScreen && !frame.software;
  const int wideX = wide ? 0 : int(m_xOffset);
  const int wideW = wide ? int(m_totalXPixels) : int(m_xPixels);
  m_xMap[0] = 2.0f * float(m_xPixels) / (float(SCREEN_WIDTH) * float(wideW));
  m_xMap[1] = 2.0f * float(int(m_xOffset) - wideX) / float(wideW) - 1.0f;
  auto viewport43 = [&]() { glViewport(m_xOffset, m_yOffset, m_xPixels, m_yPixels); };
  auto viewportWide = [&]() { glViewport(wideX, m_yOffset, wideW, m_yPixels); };

  if (frame.valid)
  {
    if (frame.software)
    {
      viewport43();
      DrawImage(m_softwareTexture, frame.softwareImage.data(), 3);
    }
    else
    {
      if (wide && m_wideBackground)
        viewportWide();
      else
        viewport43();
      DrawImage(m_backTexture, frame.back.data(), 0);
      viewportWide();
      if (frame.model2 && frame.model2Memory)
        DrawModel2(*frame.model2, *frame.model2Memory, frame.model2Quads);
      DrawBatches(frame.geometry, frame.geometry.below);
      glDisable(GL_SCISSOR_TEST);
      viewport43();
      DrawImage(m_hudTexture, frame.hud.data(), 1);
      if (!frame.geometry.above.empty())
      {
        SnapshotFrameBuffer();
        viewportWide();
        DrawBatches(frame.geometry, frame.geometry.above);
        glDisable(GL_SCISSOR_TEST);
        viewport43();
        DrawImage(m_hudTexture, nullptr, 2);
      }
    }
  }

  if (!m_dumpFile.empty())
  {
    // the viewable area (with wide screen, the whole width)
    const unsigned dumpX = wide ? 0 : m_xOffset, dumpW = wide ? m_totalXPixels : m_xPixels;
    std::shared_ptr<uint8_t> pixels(new uint8_t[size_t(dumpW) * m_yPixels * 4], std::default_delete<uint8_t[]>());
    glReadPixels(dumpX, m_yOffset, dumpW, m_yPixels, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
    Util::WriteSurfaceToBMP<Util::RGBA8>(m_dumpFile, pixels.get(), dumpW, m_yPixels, true);
    m_dumpFile.clear();
  }

  // Leave the state as the OSD layer expects it
  glViewport(0, 0, m_totalXPixels, m_totalYPixels);
  glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
  if (scissorEnabled)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);
  glDepthMask(GL_TRUE);

  if (m_aaTarget)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CModel1Render::BeginFrame(void)
{
}

void CModel1Render::PreRenderFrame(void)
{
}

void CModel1Render::RenderFrameBottom(void)
{
}

void CModel1Render::RenderFrameTop(void)
{
}

void CModel1Render::EndFrame(void)
{
}


/******************************************************************************
 Configuration and Initialization
******************************************************************************/

static GLuint CreateTexture(GLsizei width, GLsizei height, GLint filter)
{
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texture;
}

Result CModel1Render::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget, UpscaleMode upscaleMode)
{
  m_xPixels = xRes;
  m_yPixels = yRes;
  m_xOffset = xOffset;
  m_yOffset = yOffset;
  m_totalXPixels = totalXRes;
  m_totalYPixels = totalYRes;
  m_aaTarget = aaTarget;

  // Tilemap layer scaling as CRender2D: nearest at the native resolution
  m_upscaleMode = (m_xPixels == SCREEN_WIDTH && m_yPixels == SCREEN_HEIGHT) ? UpscaleMode::Nearest : upscaleMode;
  const std::string imageShader = std::string("#version 410 core\n#define UPSCALEMODE ") + std::to_string((int) m_upscaleMode) + "\n" + s_imageFragmentShader;

  if (!m_quadShader.LoadShaders(s_quadVertexShader, s_quadFragmentShader) ||
      !m_imageShader.LoadShaders(s_imageVertexShader, imageShader.c_str()))
  {
    ErrorLog("Model 1 renderer: unable to compile shaders.");
    return Result::FAIL;
  }
  m_quadScreenSizeLoc = m_quadShader.GetUniformLocation("screenSize");
  m_quadViewportLoc = m_quadShader.GetUniformLocation("viewport");
  m_quadXMapLoc = m_quadShader.GetUniformLocation("xMap");
  m_imageModeLoc = m_imageShader.GetUniformLocation("mode");
  m_imageTexLoc = m_imageShader.GetUniformLocation("tex");
  m_imageSnapshotLoc = m_imageShader.GetUniformLocation("snapshot");

  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void *) offsetof(Vertex, x));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (const void *) offsetof(Vertex, color));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void *) offsetof(Vertex, moire));
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenVertexArrays(1, &m_imageVAO);

  const GLint filter = m_upscaleMode == UpscaleMode::Nearest ? GL_NEAREST : GL_LINEAR;
  m_backTexture = CreateTexture(SCREEN_WIDTH, SCREEN_HEIGHT, filter);
  m_hudTexture = CreateTexture(SCREEN_WIDTH, SCREEN_HEIGHT, filter);
  m_softwareTexture = CreateTexture(SCREEN_WIDTH, SCREEN_HEIGHT, GL_NEAREST);
  m_snapshotTexture = CreateTexture(totalXRes, totalYRes, GL_NEAREST);

  InitModel2();   // without it, Model 2 has no 3D (unless its software renderer is used)

  m_ready = true;
  return Result::OKAY;
}

CModel1Render::~CModel1Render()
{
  if (m_vbo)
    glDeleteBuffers(1, &m_vbo);
  if (m_vao)
    glDeleteVertexArrays(1, &m_vao);
  if (m_imageVAO)
    glDeleteVertexArrays(1, &m_imageVAO);
  GLuint textures[] = { m_backTexture, m_hudTexture, m_softwareTexture, m_snapshotTexture };
  for (GLuint t : textures)
    if (t)
      glDeleteTextures(1, &t);
  if (m_m2VBO)
    glDeleteBuffers(1, &m_m2VBO);
  if (m_m2VAO)
    glDeleteVertexArrays(1, &m_m2VAO);
  if (m_m2Buffers[0])
    glDeleteBuffers(M2_NUM_BUFFERS, m_m2Buffers);
    if (m_m2QuadProgram)
      glDeleteProgram(m_m2QuadProgram);
  if (m_m2Textures[0])
    glDeleteTextures(M2_NUM_BUFFERS, m_m2Textures);
}
