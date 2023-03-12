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

#include "Crosshair.h"
#include "Supermodel.h"
#include "Graphics/New3D/New3D.h"
#include "OSD/FileSystemPath.h"
#include "SDLIncludes.h"
#include <GL/glew.h>
#include <vector>

bool CCrosshair::Init()
{
    const std::string p1CrosshairFile = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Assets) << "p1crosshair.bmp";
    const std::string p2CrosshairFile = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Assets) << "p2crosshair.bmp";

    m_isBitmapCrosshair = m_config["CrosshairStyle"].ValueAs<std::string>();
    m_xRes = m_config["XResolution"].ValueAs<unsigned>();
    m_yRes = m_config["YResolution"].ValueAs<unsigned>();
    m_a = (float)m_xRes / (float)m_yRes;

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

    if (m_isBitmapCrosshair != "bmp")
    {
        m_verts.emplace_back(0.0f, m_dist);  // bottom triangle
        m_verts.emplace_back(m_base / 2.0f, (m_dist + m_height) * m_a);
        m_verts.emplace_back(-m_base / 2.0f, (m_dist + m_height) * m_a);
        m_verts.emplace_back(0.0f, -m_dist);  // top triangle
        m_verts.emplace_back(-m_base / 2.0f, -(m_dist + m_height) * m_a);
        m_verts.emplace_back(m_base / 2.0f, -(m_dist + m_height) * m_a);
        m_verts.emplace_back(-m_dist, 0.0f);  // left triangle
        m_verts.emplace_back(-m_dist - m_height, (m_base / 2.0f) * m_a);
        m_verts.emplace_back(-m_dist - m_height, -(m_base / 2.0f) * m_a);
        m_verts.emplace_back(m_dist, 0.0f);  // right triangle
        m_verts.emplace_back(m_dist + m_height, -(m_base / 2.0f) * m_a);
        m_verts.emplace_back(m_dist + m_height, (m_base / 2.0f) * m_a);
    }
    else
    {
        m_verts.emplace_back(-m_squareSize / 2.0f, -m_squareSize / 2.0f * m_a);
        m_verts.emplace_back(m_squareSize / 2.0f, -m_squareSize / 2.0f * m_a);
        m_verts.emplace_back(m_squareSize / 2.0f, m_squareSize / 2.0f * m_a);
        m_verts.emplace_back(-m_squareSize / 2.0f, -m_squareSize / 2.0f * m_a);
        m_verts.emplace_back(m_squareSize / 2.0f, m_squareSize / 2.0f * m_a);
        m_verts.emplace_back(-m_squareSize / 2.0f, m_squareSize / 2.0f * m_a);
    }

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

void CCrosshair::DrawCrosshair(New3D::Mat4 matrix, float x, float y, int player)
{
    float r=0.0f, g=0.0f, b=0.0f;
    int count = (int)m_verts.size();

    if (count > MaxVerts) {
        count = MaxVerts;       // maybe we could error out somehow
    }

    m_shader.EnableShader();
    matrix.Translate(x, y, 0);

    if (m_isBitmapCrosshair != "bmp")
    {
        switch (player)
        {
        case 0:
            r = 1.0f;
            g = 0.0f;
            b = 0.0f;
            break;
        case 1:
            r = 0.0f;
            g = 1.0f;
            b = 0.0f;
            break;
        }

        matrix.Scale(m_dpiMultiplicator, m_dpiMultiplicator, 0);

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

        matrix.Scale(m_dpiMultiplicator * m_scaleBitmap, m_dpiMultiplicator * m_scaleBitmap, 0);

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

CCrosshair::CCrosshair(const Util::Config::Node& config) : m_config(config),m_xRes(0),m_yRes(0)
{
    m_vertexShader = nullptr;
    m_fragmentShader = nullptr;
}

CCrosshair::~CCrosshair()
{
}
