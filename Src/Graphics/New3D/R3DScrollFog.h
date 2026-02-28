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

#ifndef _R3DSCROLLFOG_H_
#define _R3DSCROLLFOG_H_

#include <GL/glew.h>

namespace New3D {

	class R3DScrollFog
	{
	public:

		R3DScrollFog();
		~R3DScrollFog();

		void DrawScrollFog(float rbga[4], float attenuation, float ambient, float spotRGB[3], float spotEllipse[4]);

	private:

		void AllocResources();
		void DeallocResources();

		GLuint m_shaderProgram;
		GLuint m_vertexShader;
		GLuint m_fragmentShader;

		GLint m_locFogColour;
		GLint m_locFogAttenuation;
		GLint m_locFogAmbient;
		GLint m_locSpotFogColor;
		GLint m_locSpotEllipse;

		GLuint m_vao;
	};

}

#endif