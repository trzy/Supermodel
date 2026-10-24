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
 * IRender2D.h
 *
 * Interface for 2D layer renderers. The 2D layers are drawn behind (bottom)
 * and in front of (top) the 3D image.
 *
 * Implementations:
 *
 *  - CRender2D: the Model 3 tile generator layers.
 *  - CPro1000Render2D: the Real3D Pro-1000 scroll layer (one solid color).
 *
 * The emulator that uses the renderer knows its type and casts to it for the
 * functions that are not in this interface.
 */

#ifndef INCLUDED_IRENDER2D_H
#define INCLUDED_IRENDER2D_H

#include "Supermodel.h"

class IRender2D
{
public:
  /*
   * Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes, aaTarget,
   *      upscaleMode):
   *
   * One-time initialization. Must be called before any other members.
   *
   * Parameters:
   *    xOffset     X offset of the viewable area within OpenGL display
   *                surface, in pixels.
   *    yOffset     Y offset.
   *    xRes        Horizontal resolution of the viewable area.
   *    yRes        Vertical resolution.
   *    totalXRes   Horizontal resolution of the complete display area.
   *    totalYRes   Vertical resolution.
   *    aaTarget    Frame buffer to draw to (0 for the back buffer).
   *    upscaleMode Upscale filter for layers that are images.
   *
   * Returns:
   *    OKAY if successful, otherwise FAIL. Prints own error messages.
   */
  virtual Result Init(unsigned xOffset, unsigned yOffset, unsigned xRes, unsigned yRes, unsigned totalXRes, unsigned totalYRes, unsigned aaTarget, UpscaleMode upscaleMode) = 0;

  // Prepares to render a new frame. Call it one time for each frame before
  // anything is drawn.
  virtual void BeginFrame(void) = 0;

  // Prepares the layers for drawing (after BeginFrame()).
  virtual void PreRenderFrame(void) = 0;

  // Draws the bottom layer. Call it before the 3D image is drawn.
  virtual void RenderFrameBottom(void) = 0;

  // Draws the top layer. Call it after the 3D image is drawn.
  virtual void RenderFrameTop(void) = 0;

  // Ends the frame.
  virtual void EndFrame(void) = 0;

  virtual ~IRender2D()
  {
  }
};

#endif  // INCLUDED_IRENDER2D_H
