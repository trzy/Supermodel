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
 * Pro1000Render2D.h
 *
 * Header file for the CPro1000Render2D class: the 2D layer of the Real3D
 * Pro-1000 (the scroll layer).
 */

#ifndef INCLUDED_PRO1000RENDER2D_H
#define INCLUDED_PRO1000RENDER2D_H

#include <GL/glew.h>
#include <cstdint>
#include "Supermodel.h"
#include "IRender2D.h"

/*
 * CPro1000Render2D:
 *
 * The Pro-1000 has no tile generator. Its 2D layer (the scroll layer) is one
 * solid color, set by the SCROLL register (0x62000000, see Pro1000.cpp). The
 * color is behind the 3D image (background) or in front of it (foreground).
 * This class fills the viewable area with the color. It uses the same render
 * target as CRender2D, thus the 3D image is put over the background in the
 * same way as on Model 3.
 *
 * Apart from the constructor, all members assume that a global GL device
 * context is available and that GL functions may be called.
 */
class CPro1000Render2D : public IRender2D
{
public:
  // IRender2D interface
  Result Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget, UpscaleMode upscaleMode) override;
  void BeginFrame(void) override;
  void PreRenderFrame(void) override;
  void RenderFrameBottom(void) override;
  void RenderFrameTop(void) override;
  void EndFrame(void) override;

  /*
   * SetScrollRegister(value):
   *
   * Sets the value of the SCROLL register that the next frames use. Call it
   * when the emulation and the renderer are synchronized.
   *
   * Parameters:
   *    value   SCROLL register value. Bits 23-0 are the color (RGB, 8 bits
   *            each, red in bits 23-16). Bit 26 is 1 for a background and
   *            0 for a foreground. If bits 25-24 (viewport select) are 0, there
   *            is no layer.
   */
  void SetScrollRegister(uint32_t value);

  // Init(): the upscale mode is not used (the layer is one color).
  // RenderFrameBottom(): clears the display surface and draws a background.
  // RenderFrameTop(): draws a foreground.
  // BeginFrame(), PreRenderFrame(), EndFrame(): nothing to do.

private:
  bool LayerEnabled(void) const;
  bool IsBackground(void) const;
  void FillLayer(void);

  // Display area
  unsigned  m_xPixels = 496;      // viewable area
  unsigned  m_yPixels = 384;
  unsigned  m_xOffset = 0;
  unsigned  m_yOffset = 0;
  unsigned  m_totalXPixels = 0;   // complete display surface
  unsigned  m_totalYPixels = 0;
  GLuint    m_aaTarget = 0;

  // SCROLL register
  uint32_t  m_scroll = 0;
};


#endif  // INCLUDED_PRO1000RENDER2D_H
