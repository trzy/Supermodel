/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2023 The Supermodel Team
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

#ifndef INCLUDED_CROSSHAIR_H
#define INCLUDED_CROSSHAIR_H

#include "Supermodel.h"
#include "Graphics/New3D/New3D.h"
#include "Inputs/Inputs.h"
#include "basicDrawable.h"

class CCrosshair : public CBasicDrawable
{
private:
  const Util::Config::Node& m_config;
  bool m_isBitmapCrosshair = false;
  std::string m_crosshairStyle = "";
  GLuint m_crosshairTexId[2] = { 0 };
  int m_p1CrosshairW = 0, m_p1CrosshairH = 0, m_p2CrosshairW = 0, m_p2CrosshairH = 0;
  float m_diagDpi = 0.0f, m_hDpi = 0.0f, m_vDpi = 0.0f;
  const float m_base = 0.01f, m_height = 0.02f; // geometric parameters of each triangle
  const float m_dist = 0.004f;                  // distance of triangle tip from center
  const float m_squareSize = 1.0f;
  const float m_standardDpi = 96.0f;            // normal dpi for usual monitor (full hd)
  float m_dpiMultiplicator = 0.0f;
  const float m_scaleBitmap = 0.1f;

  void BuildCrosshairVertices(unsigned int xRes, unsigned int yRes);
  void DrawCrosshair(New3D::Mat4 matrix, float x, float y, int player, unsigned int xRes, unsigned int yRes);
  void GunToViewCoords(float* x, float* y);

public:
  CCrosshair(const Util::Config::Node& config);
  ~CCrosshair();
  bool Init();
  void Update(uint32_t currentInputs, CInputs* Inputs, unsigned int xOffset, unsigned int yOffset, unsigned int xRes, unsigned int yRes);
};

#endif
