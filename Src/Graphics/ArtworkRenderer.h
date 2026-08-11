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
 **/

#ifndef INCLUDED_ARTWORKRENDERER_H
#define INCLUDED_ARTWORKRENDERER_H

#include "Graphics/New3D/GLSLShader.h"

#include <GL/glew.h>
#include <string>

namespace Video
{

class CArtworkRenderer
{
public:
  CArtworkRenderer() = default;
  ~CArtworkRenderer();

  CArtworkRenderer(const CArtworkRenderer&) = delete;
  CArtworkRenderer& operator=(const CArtworkRenderer&) = delete;

  bool Load(const std::string& path);
  void Unload();
  void Draw(unsigned outputWidth, unsigned outputHeight);

private:
  GLSLShader m_shader;
  GLuint m_texture = 0;
  GLuint m_vao = 0;
  GLuint m_vbo = 0;
  bool m_loaded = false;
};

} // namespace Video

#endif // INCLUDED_ARTWORKRENDERER_H
