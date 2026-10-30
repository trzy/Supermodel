/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
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
 */

#include "Model1Render.h"
#include "OSD/Logger.h"
#include "Util/BMPFile.h"
#include <cmath>
#include <memory>
#include <cstddef>

using Model1::SCREEN_WIDTH;
using Model1::SCREEN_HEIGHT;

static const char s_quadVertexShader[] = R"glsl(
#version 410 core

layout(location = 0) in vec2  inPos;      // native screen coordinates (y down)
layout(location = 1) in vec4  inColor;
layout(location = 2) in float inMoire;

uniform vec2 screenSize;                  // 496, 384

out vec4 fsColor;
flat out float fsMoire;

void main(void)
{
  gl_Position = vec4(inPos.x / screenSize.x * 2.0 - 1.0, 1.0 - inPos.y / screenSize.y * 2.0, 0.0, 1.0);
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
  glViewport(m_xOffset, m_yOffset, m_xPixels, m_yPixels);

  if (frame.valid)
  {
    if (frame.software)
      DrawImage(m_softwareTexture, frame.softwareImage.data(), 3);
    else
    {
      DrawImage(m_backTexture, frame.back.data(), 0);
      DrawBatches(frame.geometry, frame.geometry.below);
      glDisable(GL_SCISSOR_TEST);
      DrawImage(m_hudTexture, frame.hud.data(), 1);
      if (!frame.geometry.above.empty())
      {
        SnapshotFrameBuffer();
        DrawBatches(frame.geometry, frame.geometry.above);
        glDisable(GL_SCISSOR_TEST);
        DrawImage(m_hudTexture, nullptr, 2);
      }
    }
  }

  if (!m_dumpFile.empty())
  {
    std::shared_ptr<uint8_t> pixels(new uint8_t[size_t(m_xPixels) * m_yPixels * 4], std::default_delete<uint8_t[]>());
    glReadPixels(m_xOffset, m_yOffset, m_xPixels, m_yPixels, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
    Util::WriteSurfaceToBMP<Util::RGBA8>(m_dumpFile, pixels.get(), m_xPixels, m_yPixels, true);
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
}
