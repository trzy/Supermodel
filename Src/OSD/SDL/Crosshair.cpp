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

#include "Crosshair.h"
#include "Supermodel.h"
#include "Graphics/New3D/New3D.h"
#include "OSD/FileSystemPath.h"
#include "SDLIncludes.h"
#include <GL/glew.h>
#include <vector>
#include "Inputs/Inputs.h"
#include "Util/Format.h"

bool CCrosshair::Init()
{
  const std::string p1CrosshairFile = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Assets) << "p1crosshair.bmp";
  const std::string p2CrosshairFile = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Assets) << "p2crosshair.bmp";

  m_crosshairStyle = Util::ToLower(m_config["CrosshairStyle"].ValueAs<std::string>());
  if (m_crosshairStyle == "bmp")
    m_isBitmapCrosshair = true;
  else if (m_crosshairStyle == "vector")
    m_isBitmapCrosshair = false;
  else
  {
    ErrorLog("Invalid crosshair style '%s', must be 'vector' or 'bmp'. Reverting to 'vector'.\n", m_crosshairStyle.c_str());
    m_isBitmapCrosshair = false;
  }

  SDL_Surface* surfaceCrosshairP1 = SDL_LoadBMP(p1CrosshairFile.c_str());
  SDL_Surface* surfaceCrosshairP2 = SDL_LoadBMP(p2CrosshairFile.c_str());
  if (surfaceCrosshairP1 == NULL || surfaceCrosshairP2 == NULL)
      return FAIL;

  m_p1CrosshairW = surfaceCrosshairP1->w;
  m_p1CrosshairH = surfaceCrosshairP1->h;
  m_p2CrosshairW = surfaceCrosshairP2->w;
  m_p2CrosshairH = surfaceCrosshairP2->h;

  glGenTextures(2, m_crosshairTexId);

  glBindTexture(GL_TEXTURE_2D, m_crosshairTexId[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_p1CrosshairW, m_p1CrosshairH, 0, GL_BGRA, GL_UNSIGNED_BYTE, surfaceCrosshairP1->pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, m_crosshairTexId[1]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_p1CrosshairW, m_p1CrosshairH, 0, GL_BGRA, GL_UNSIGNED_BYTE, surfaceCrosshairP2->pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);

  SDL_FreeSurface(surfaceCrosshairP1);
  SDL_FreeSurface(surfaceCrosshairP2);

  // Get DPI
  SDL_GetDisplayDPI(0, &m_diagDpi, &m_hDpi, &m_vDpi);
  m_dpiMultiplicator = m_hDpi / m_standardDpi;  // note : on linux VM diagdpi returns 0

  // 3d obj
  m_uvCoord.emplace_back(0.0f, 0.0f);
  m_uvCoord.emplace_back(1.0f, 0.0f);
  m_uvCoord.emplace_back(1.0f, 1.0f);
  m_uvCoord.emplace_back(0.0f, 0.0f);
  m_uvCoord.emplace_back(1.0f, 1.0f);
  m_uvCoord.emplace_back(0.0f, 1.0f);

  BuildCrosshairVertices();

  m_vertexShader = R"glsl(

    #version 410 core

    uniform mat4 mvp;
    layout(location = 0) in vec3 inVertices;
    layout(location = 1) in vec2 vertexUV;
    out vec2 UV;

    void main(void)
    {
      gl_Position = mvp * vec4(inVertices,1.0);
        UV = vertexUV;
    }
    )glsl";

  m_fragmentShader = R"glsl(

    #version 410 core

    uniform vec4 colour;
    uniform sampler2D CrosshairTexture;
    uniform bool isBitmap;
    out vec4 fragColour;
    in vec2 UV;

    void main(void)
    {
    if (!isBitmap)
      fragColour = colour;
    else
        fragColour = colour * texture(CrosshairTexture, UV);
    }
    )glsl";

  m_shader.LoadShaders(m_vertexShader, m_fragmentShader);
  m_shader.GetUniformLocationMap("mvp");
  m_shader.GetUniformLocationMap("CrosshairTexture");
  m_shader.GetUniformLocationMap("colour");
  m_shader.GetUniformLocationMap("isBitmap");

  m_vbo.Create(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, sizeof(BasicVertex) * (MaxVerts));
  m_vbo.Bind(true);
  m_textvbo.Create(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, sizeof(UVCoords) * (int)m_uvCoord.size());
  m_textvbo.Bind(true);

  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);

  m_vbo.Bind(true);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(BasicVertex), 0);
  m_vbo.Bind(false);

  m_textvbo.Bind(true);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UVCoords), 0);
  m_textvbo.Bind(false);

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  return OKAY;
}

void CCrosshair::BuildCrosshairVertices()
{
  m_verts.clear();
  if (!m_isBitmapCrosshair)
  {
    m_verts.emplace_back(0.0f, m_dist);  // bottom triangle
    m_verts.emplace_back(m_base / 2.0f, (m_dist + m_height));
    m_verts.emplace_back(-m_base / 2.0f, (m_dist + m_height));
    m_verts.emplace_back(0.0f, -m_dist);  // top triangle
    m_verts.emplace_back(-m_base / 2.0f, -(m_dist + m_height));
    m_verts.emplace_back(m_base / 2.0f, -(m_dist + m_height));
    m_verts.emplace_back(-m_dist, 0.0f);  // left triangle
    m_verts.emplace_back(-m_dist - m_height, (m_base / 2.0f));
    m_verts.emplace_back(-m_dist - m_height, -(m_base / 2.0f));
    m_verts.emplace_back(m_dist, 0.0f);  // right triangle
    m_verts.emplace_back(m_dist + m_height, -(m_base / 2.0f));
    m_verts.emplace_back(m_dist + m_height, (m_base / 2.0f));
  }
  else
  {
    m_verts.emplace_back(-m_squareSize / 2.0f, -m_squareSize / 2.0f);
    m_verts.emplace_back(m_squareSize / 2.0f, -m_squareSize / 2.0f);
    m_verts.emplace_back(m_squareSize / 2.0f, m_squareSize / 2.0f);
    m_verts.emplace_back(-m_squareSize / 2.0f, -m_squareSize / 2.0f);
    m_verts.emplace_back(m_squareSize / 2.0f, m_squareSize / 2.0f);
    m_verts.emplace_back(-m_squareSize / 2.0f, m_squareSize / 2.0f);
  }
}

void CCrosshair::DrawCrosshair(New3D::Mat4 matrix, float x, float y, int player, unsigned int xRes, unsigned int yRes)
{
  float aspect = (float)xRes / (float)yRes;
  float r=0.0f, g=0.0f, b=0.0f;
  int count = (int)m_verts.size();

  if (count > MaxVerts)
  {
    count = MaxVerts;       // maybe we could error out somehow
  }

  m_shader.EnableShader();
  matrix.Translate(x, y, 0);

  if (!m_isBitmapCrosshair)
  {
    switch (player)
    {
    case 0: // P1 red color
      r = 1.0f;
      g = 0.0f;
      b = 0.0f;
      break;
    case 1: // P2 green color
      r = 0.0f;
      g = 1.0f;
      b = 0.0f;
      break;
    }

    matrix.Scale(m_dpiMultiplicator, m_dpiMultiplicator * aspect, 0);

    // update uniform memory
    glUniformMatrix4fv(m_shader.uniformLocMap["mvp"], 1, GL_FALSE, matrix);
    glUniform4f(m_shader.uniformLocMap["colour"], r, g, b, 1.0f);
    glUniform1i(m_shader.uniformLocMap["isBitmap"], false);

    // update vbo mem
    m_vbo.Bind(true);
    m_vbo.BufferSubData(0, count * sizeof(BasicVertex), m_verts.data());
  }
  else
  {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_crosshairTexId[player]);

    m_textureCoordsCount = (int)m_uvCoord.size();

    matrix.Scale(m_dpiMultiplicator * m_scaleBitmap, m_dpiMultiplicator * m_scaleBitmap * aspect, 0);

    // update uniform memory
    glUniformMatrix4fv(m_shader.uniformLocMap["mvp"], 1, GL_FALSE, matrix);
    glUniform1i(m_shader.uniformLocMap["CrosshairTexture"], 0); // 0 or 1 or GL_TEXTURE0 GL_TEXTURE1
    glUniform4f(m_shader.uniformLocMap["colour"], 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1i(m_shader.uniformLocMap["isBitmap"], true);

    // update vbo mem
    m_vbo.Bind(true);
    m_vbo.BufferSubData(0, count * sizeof(BasicVertex), m_verts.data());
    m_vbo.Bind(false);
    m_textvbo.Bind(true);
    m_textvbo.BufferSubData(0, m_textureCoordsCount * sizeof(UVCoords), m_uvCoord.data());
    m_textvbo.Bind(false);
  }

  glBindVertexArray(m_vao);
  glDrawArrays(GL_TRIANGLES, 0, count);
  glBindVertexArray(0);

  m_shader.DisableShader();
}

void CCrosshair::Update(uint32_t currentInputs, CInputs* Inputs, unsigned int xOffset, unsigned int yOffset, unsigned int xRes, unsigned int yRes)
{
  bool offscreenTrigger[2]{false};
  float x[2]{ 0.0f }, y[2]{ 0.0f };

  // Crosshairs can be enabled/disabled at run-tim
  unsigned crosshairs = m_config["Crosshairs"].ValueAs<unsigned>();
  crosshairs &= 3;
  if (!crosshairs)
    return;

  // Set up the viewport and orthogonal projection
  glUseProgram(0);    // no shaders
  glViewport(xOffset, yOffset, xRes, yRes);
  glDisable(GL_DEPTH_TEST); // no Z-buffering needed

  if (!m_isBitmapCrosshair)
  {
    glDisable(GL_BLEND);    // no blending
  }
  else
  {
    glEnable(GL_TEXTURE_2D); // enable texture mapping, blending and alpha chanel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  New3D::Mat4 m;
  m.Ortho(0.0, 1.0, 1.0, 0.0, -1.0f, 1.0f);

  // Convert gun coordinates to viewspace coordinates
  if (currentInputs & Game::INPUT_ANALOG_GUN1)
  {
    x[0] = ((float)Inputs->analogGunX[0]->value / 255.0f);
    y[0] = ((255.0f - (float)Inputs->analogGunY[0]->value) / 255.0f);
    offscreenTrigger[0] = Inputs->analogTriggerLeft[0]->value || Inputs->analogTriggerRight[0]->value;
  }
  else if (currentInputs & Game::INPUT_GUN1)
  {
    x[0] = (float)Inputs->gunX[0]->value;
    y[0] = (float)Inputs->gunY[0]->value;
    GunToViewCoords(&x[0], &y[0]);
    offscreenTrigger[0] = (Inputs->trigger[0]->offscreenValue) > 0;
  }
  if (currentInputs & Game::INPUT_ANALOG_GUN2)
  {
    x[1] = ((float)Inputs->analogGunX[1]->value / 255.0f);
    y[1] = ((255.0f - (float)Inputs->analogGunY[1]->value) / 255.0f);
    offscreenTrigger[1] = Inputs->analogTriggerLeft[1]->value || Inputs->analogTriggerRight[1]->value;
  }
  else if (currentInputs & Game::INPUT_GUN2)
  {
    x[1] = (float)Inputs->gunX[1]->value;
    y[1] = (float)Inputs->gunY[1]->value;
    GunToViewCoords(&x[1], &y[1]);
    offscreenTrigger[1] = (Inputs->trigger[1]->offscreenValue) > 0;
  }

  // Draw visible crosshairs

  if ((crosshairs & 1) && !offscreenTrigger[0]) // Player 1
  {
    DrawCrosshair(m, x[0], y[0], 0, xRes, yRes);
  }
  if ((crosshairs & 2) && !offscreenTrigger[1]) // Player 2
  {
    DrawCrosshair(m, x[1], y[1], 1, xRes, yRes);
  }

  //PrintGLError(glGetError());
}

void CCrosshair::GunToViewCoords(float* x, float* y)
{
  *x = (*x - 150.0f) / (651.0f - 150.0f); // Scale [150,651] -> [0.0,1.0]
  *y = (*y - 80.0f) / (465.0f - 80.0f);   // Scale [80,465] -> [0.0,1.0]
}

CCrosshair::CCrosshair(const Util::Config::Node& config)
  : m_config(config),
    m_vertexShader(nullptr),
    m_fragmentShader(nullptr)
{
}

CCrosshair::~CCrosshair()
{
}
