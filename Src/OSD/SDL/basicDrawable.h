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

#ifndef INCLUDED_BASICDRAWABLE_H
#define INCLUDED_BASICDRAWABLE_H

class CBasicDrawable
{
protected:
	struct BasicVertex
	{
		BasicVertex(float x, float y, float z) : x(x), y(y), z(z) {}
		BasicVertex(float x, float y) : x(x), y(y), z(0.0f) {}
		float x, y, z;
	};

	struct UVCoords
	{
		UVCoords(float x, float y) : x(x), y(y) {}
		float x, y;
	};
	
	std::vector<BasicVertex> m_verts;
	std::vector<UVCoords> m_uvCoord;

	GLSLShader m_shader;
	VBO m_vbo;
	VBO m_textvbo;
	GLuint m_vao = 0;
	int m_textureCoordsCount = 0;
	const char* m_vertexShader;
	const char* m_fragmentShader;

	const int MaxVerts = 1024;  // per draw call


};

#endif

