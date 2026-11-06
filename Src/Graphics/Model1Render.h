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
 * Model1Render.h
 *
 * The Sega Model 1 renderer: draws the 2D tilemap layers and the 3D quads
 * that the geometrizer simulation produces (Model1::CGeometry).
 *
 * The 3D quads are drawn as the hardware draws them: flat colored, in the
 * sorted order (painter's algorithm, no depth buffer), clipped to their
 * viewport. Only the rasterization is done by the GPU, at the output
 * resolution instead of 496x384. The tilemap layers are drawn by the CPU at
 * the native resolution and scaled.
 *
 * It implements IRender2D so that the OSD layer can create it like the other
 * 2D renderers; CModel1 calls DrawFrame() for the actual drawing.
 *
 * Model 2 uses it too: the same tilemap layers, and its own 3D pass, textured
 * polygons drawn as MAME's rasterizer draws them (see DrawModel2()).
 */

#ifndef INCLUDED_MODEL1RENDER_H
#define INCLUDED_MODEL1RENDER_H

#include "IRender2D.h"
#include "New3D/GLSLShader.h"
#include "Model1/Model1Geometry.h"
#include "Model2/Model2Video.h"
#include <GL/glew.h>
#include <string>
#include <vector>

class CModel1Render: public IRender2D
{
public:
  // A frame to draw. The images are 496x384 pixels, 0xAARRGGBB.
  struct VideoFrame
  {
    bool                  valid = false;
    bool                  software = false;   // draw softwareImage only (MAME's rasterizer)
    Model1::Frame         geometry;
    std::vector<uint32_t> back;           // tilemap layers below the 3D, opaque
    std::vector<uint32_t> hud;            // tilemap layers above the 3D, alpha 0 where nothing is drawn
    std::vector<uint32_t> softwareImage;  // the complete image, for software mode
    // Model 2: the 3D (drawn instead of geometry, between back and hud) and
    // the memory its textures and colours come from
    const std::vector<Model2::GPUVertex> *model2 = nullptr;
    const Model2::CVideo::Memory *model2Memory = nullptr;
    bool model2Quads = false;             // model2 is quads (GL_LINES_ADJACENCY), not triangles
  };

  void DrawFrame(const VideoFrame &frame);

  // How far polygons are grown outward, in native pixels, to cover like the
  // hardware's rasterizer (0 = exact shape). With crack filling, the grown
  // polygons are drawn first and the exact ones over them (see DrawBatches());
  // without, only the grown ones are drawn.
  void SetEdgeExpand(float nativePixels, bool crackFill) { m_edgeExpand = nativePixels; m_crackFill = crackFill; }

  // Wide screen (as Model 3's WideScreen/WideBackground): the 3D is drawn over
  // the whole width of the window instead of the 4:3 area, the tilemap layers
  // stay 4:3 (the bottom one stretched with wideBackground). The emulation
  // widens the viewports at the screen's left and right edges by
  // WideExtension() native pixels, so polygons there aren't clipped away.
  void SetWideScreen(bool wideScreen, bool wideBackground) { m_wideScreen = wideScreen; m_wideBackground = wideBackground; }
  float WideExtension() const { return (m_wideScreen && m_xPixels) ? float(m_xOffset) * float(Model1::SCREEN_WIDTH) / float(m_xPixels) : 0.0f; }

  // Debugging aid: writes the viewable area of the next frame drawn to a BMP file
  void RequestDump(const std::string &file) { m_dumpFile = file; }

  // IRender2D
  Result Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget, UpscaleMode upscaleMode) override;
  void BeginFrame(void) override;
  void PreRenderFrame(void) override;
  void RenderFrameBottom(void) override;
  void RenderFrameTop(void) override;
  void EndFrame(void) override;

  ~CModel1Render();

private:
  struct Vertex
  {
    float     x, y;
    uint32_t  color;    // 0x00BBGGRR (bytes R, G, B, A in memory)
    float     moire;
  };

  void DrawImage(GLuint texture, const uint32_t *pixels, int mode);
  void DrawBatches(const Model1::Frame &frame, const std::vector<Model1::Batch> &batches);
  void AddQuad(const Model1::ScreenQuad &q, float expand);
  void SetScissor(int x1, int y1, int x2, int y2);
  void SnapshotFrameBuffer();
  bool InitModel2();
  void DrawModel2(const std::vector<Model2::GPUVertex> &vertices, const Model2::CVideo::Memory &memory, bool quads);

  unsigned m_xOffset = 0, m_yOffset = 0, m_xPixels = 0, m_yPixels = 0;
  unsigned m_totalXPixels = 0, m_totalYPixels = 0;
  GLuint m_aaTarget = 0;

  GLSLShader  m_quadShader;
  GLSLShader  m_imageShader;
  GLint       m_quadScreenSizeLoc = -1, m_quadViewportLoc = -1, m_quadXMapLoc = -1;
  GLint       m_imageModeLoc = -1, m_imageTexLoc = -1, m_imageSnapshotLoc = -1;
  GLuint      m_vao = 0, m_vbo = 0, m_imageVAO = 0;
  GLuint      m_backTexture = 0, m_hudTexture = 0, m_softwareTexture = 0, m_snapshotTexture = 0;
  std::vector<Vertex> m_vertices;

  // Model 2 3D pass: shader, vertex buffer, and textures holding the board's
  // texture RAM 0/1 (2D, a mip level per hardware mip level: see
  // UploadModel2Sheet()), and luma RAM, colour tables, palette and gamma table
  // (buffer textures)
  enum { M2_TEXRAM0, M2_TEXRAM1, M2_LUMA, M2_XLAT, M2_PALETTE, M2_GAMMA, M2_NUM_BUFFERS };
  enum { M2_SHEET_LEVELS = 9 };   // mip levels 1-9 (level 9: 4x2 texels)
  void        UploadModel2Sheet(GLuint texture, const uint32_t *ram);
  GLSLShader  m_m2Shader;
  GLint       m_m2ScreenSizeLoc = -1, m_m2ViewportLoc = -1, m_m2XMapLoc = -1;
  GLuint      m_m2QuadProgram = 0;      // quads: vertex + geometry + fragment shaders
  GLint       m_m2QuadScreenSizeLoc = -1, m_m2QuadViewportLoc = -1, m_m2QuadXMapLoc = -1;
  GLuint      m_m2VAO = 0, m_m2VBO = 0;
  GLuint      m_m2Buffers[M2_NUM_BUFFERS] = { };
  GLuint      m_m2Textures[M2_NUM_BUFFERS] = { };
  bool        m_m2Ready = false;
  bool        m_ready = false;
  UpscaleMode m_upscaleMode = UpscaleMode::Nearest;   // tilemap layer filtering, as CRender2D
  float       m_edgeExpand = 0.5f;
  bool        m_crackFill = true;
  std::string m_dumpFile;
  bool        m_wideScreen = false, m_wideBackground = false;
  float       m_xMap[2] = { 2.0f / 496.0f, -1.0f };   // native x to clip space x (see DrawFrame())
};

#endif  // INCLUDED_MODEL1RENDER_H
