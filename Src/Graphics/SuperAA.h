/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
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

#pragma once

#include "Supermodel.h"
#include "FBO.h"
#include "New3D/GLSLShader.h"

// This class just implements super sampling. Super sampling looks fantastic but is quite expensive.
// 8x and beyond values can start to eat ridiculous amounts of memory / gpu time, for less and less noticable returns
// 4x works and looks great
// values such as 3 are also possible, that works out 9 samples per pixel
// The algorithm is super simple, just add up all samples and divide by the number

class SuperAA
{
public:
	SuperAA(int aaValue, CRTcolor CRTcolors);
	~SuperAA();

	void Init(int width, int height);		// width & height are real window dimensions
	void Draw();							// this is a no-op if AA is 1 and CRTcolors 0, since we'll be drawing straight on the back buffer anyway

	GLuint GetTargetID();

private:
	FBO m_fbo;
	GLSLShader m_shader;
	const int m_aa;
	const CRTcolor m_crtcolors;
	GLuint m_vao;
	int m_width;
	int m_height;
};
