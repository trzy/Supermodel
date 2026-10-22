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
 * GLU.cpp (libretro)
 *
 * The legacy 3D engine calls gluPerspective(). On Linux and other Unix
 * systems libGLU is a separate library that is not always installed, so the
 * libretro core gives its own gluPerspective() there. Windows (glu32) and
 * macOS (OpenGL framework) always have GLU, and the core links it.
 */

#if !defined(_WIN32) && !defined(__APPLE__)

#include <GL/glew.h>
#include <cmath>

extern "C" void GLAPIENTRY gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
  GLdouble ymax = zNear * std::tan(fovy * 3.14159265358979323846 / 360.0);
  glFrustum(-ymax * aspect, ymax * aspect, -ymax, ymax, zNear, zFar);
}

#endif
