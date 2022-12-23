/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2022 The Supermodel Team
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

class CCrosshair
{
private:
	const Util::Config::Node& m_config;
	bool IsBitmapCrosshair=false;
	GLuint CrosshairtxID[2] = { 0 };
	int P1CrosshairW = 0, P1CrosshairH = 0, P2CrosshairW = 0, P2CrosshairH = 0;
	float diagdpi = 0.0f, hdpi = 0.0f, vdpi = 0.0f;
	unsigned int xRes=0;
	unsigned int yRes=0;
	const float base = 0.01f, height = 0.02f; // geometric parameters of each triangle
	const float dist = 0.004f;                // distance of triangle tip from center
	float a = 0.0f;                           // aspect ratio (to square the crosshair)
	const float squareSize = 1.0f;
	const float standardDpi = 96.0f;          // normal dpi for usual monitor (full hd)
	float dpiMultiplicator = 0.0f;
	const float ScaleBitmap = 0.1f;

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

	std::vector<BasicVertex> verts;
	std::vector<UVCoords> UVCoord;

	GLSLShader m_shader;
	VBO m_vbo;
	VBO m_textvbo;
	GLuint m_vao = 0;
	int m_TextureCoordsCount = 0;
	const char* vertexShader;
	const char* fragmentShader;

	const int MaxVerts = 1024;  // per draw call

public:
	CCrosshair(const Util::Config::Node& config);
	~CCrosshair();
	bool init();
	void DrawCrosshair(New3D::Mat4 matrix, float x, float y, float r, float g, float b);
	void DrawBitmapCrosshair(New3D::Mat4 matrix, float x, float y, int TextureNum);
};

#endif
