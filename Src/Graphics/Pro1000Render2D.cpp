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
 * Pro1000Render2D.cpp
 *
 * Implementation of the CPro1000Render2D class: the 2D layer (scroll layer)
 * of the Real3D Pro-1000.
 *
 * The layer is one solid color. The renderer fills the complete viewable
 * area with glClear() and a scissor rectangle. It does not use textures or
 * shaders.
 *
 * Drawing operates as CRender2D:
 *
 *  - The bottom layer clears the complete display surface to transparent
 *    black, and then fills the viewable area with the color if the layer is
 *    a background.
 *  - The 3D image is drawn over it. The 3D renderer leaves alpha at 0 where
 *    it draws nothing, thus the background shows there.
 *  - The top layer fills the viewable area with the color if the layer is a
 *    foreground. This covers the complete 3D image. No known program uses a
 *    foreground, thus this is a guess (the same as the proof of concept in
 *    pro1000_aspi).
 *
 * The 3D renderer clears its frame buffers with the current clear color, and
 * it uses the scissor rectangle that the OSD layer sets. Thus the renderer
 * sets the clear color to transparent black and restores the scissor
 * rectangle after it draws.
 */

#include "Pro1000Render2D.h"


/******************************************************************************
 Drawing
******************************************************************************/

bool CPro1000Render2D::LayerEnabled(void) const
{
  return (m_scroll & 0x03000000) != 0;  // viewport select
}

bool CPro1000Render2D::IsBackground(void) const
{
  return (m_scroll & 0x04000000) != 0;
}

void CPro1000Render2D::FillLayer(void)
{
  GLint scissorBox[4];
  glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
  GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);

  const float r = float((m_scroll >> 16) & 0xFF) / 255.0f;
  const float g = float((m_scroll >> 8) & 0xFF) / 255.0f;
  const float b = float((m_scroll >> 0) & 0xFF) / 255.0f;
  glEnable(GL_SCISSOR_TEST);
  glScissor(m_xOffset, m_yOffset, m_xPixels, m_yPixels);
  glClearColor(r, g, b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

  glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
  if (!scissorEnabled)
    glDisable(GL_SCISSOR_TEST);
}

void CPro1000Render2D::BeginFrame(void)
{
}

void CPro1000Render2D::PreRenderFrame(void)
{
}

void CPro1000Render2D::EndFrame(void)
{
}

void CPro1000Render2D::RenderFrameBottom(void)
{
  if (m_aaTarget)
    glBindFramebuffer(GL_FRAMEBUFFER, m_aaTarget);

  // Clear everything, also outside the viewable area (as CRender2D)
  GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  glViewport(0, 0, m_totalXPixels, m_totalYPixels);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (scissorEnabled)
    glEnable(GL_SCISSOR_TEST);

  if (LayerEnabled() && IsBackground())
    FillLayer();

  if (m_aaTarget)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CPro1000Render2D::RenderFrameTop(void)
{
  if (!LayerEnabled() || IsBackground())
    return;

  if (m_aaTarget)
    glBindFramebuffer(GL_FRAMEBUFFER, m_aaTarget);
  FillLayer();
  if (m_aaTarget)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


/******************************************************************************
 Configuration and Initialization
******************************************************************************/

void CPro1000Render2D::SetScrollRegister(uint32_t value)
{
  m_scroll = value;
}

Result CPro1000Render2D::Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget, UpscaleMode upscaleMode)
{
  m_xPixels = xRes;
  m_yPixels = yRes;
  m_xOffset = xOffset;
  m_yOffset = yOffset;
  m_totalXPixels = totalXRes;
  m_totalYPixels = totalYRes;
  m_aaTarget = aaTarget;
  return Result::OKAY;
}
