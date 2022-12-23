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

bool CCrosshair::init()
{
    const std::string P1CrosshairFile = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Media) << "p1crosshair.bmp";
    const std::string P2CrosshairFile = Util::Format() << FileSystemPath::GetPath(FileSystemPath::Media) << "p2crosshair.bmp";

	IsBitmapCrosshair = m_config["BitmapCrosshair"].ValueAs<bool>();
    xRes = m_config["XResolution"].ValueAs<unsigned>();
    yRes = m_config["YResolution"].ValueAs<unsigned>();
    a = (float)xRes / (float)yRes;

	SDL_Surface* SurfaceCrosshairP1 = SDL_LoadBMP(P1CrosshairFile.c_str());
	SDL_Surface* SurfaceCrosshairP2 = SDL_LoadBMP(P2CrosshairFile.c_str());
    if (SurfaceCrosshairP1 == NULL || SurfaceCrosshairP2 == NULL)
        return FAIL;

    P1CrosshairW = SurfaceCrosshairP1->w;
    P1CrosshairH = SurfaceCrosshairP1->h;
    P2CrosshairW = SurfaceCrosshairP2->w;
    P2CrosshairH = SurfaceCrosshairP2->h;

    glGenTextures(2, CrosshairtxID);

    glBindTexture(GL_TEXTURE_2D, CrosshairtxID[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, P1CrosshairW, P1CrosshairH, 0, GL_BGRA, GL_UNSIGNED_BYTE, SurfaceCrosshairP1->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, CrosshairtxID[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, P1CrosshairW, P1CrosshairH, 0, GL_BGRA, GL_UNSIGNED_BYTE, SurfaceCrosshairP2->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_FreeSurface(SurfaceCrosshairP1);
    SDL_FreeSurface(SurfaceCrosshairP2);

    // Get DPI
    SDL_GetDisplayDPI(0, &diagdpi, &hdpi, &vdpi);
    dpiMultiplicator = hdpi / standardDpi;  // note : on linux VM diagdpi returns 0
    
    // 3d obj
    UVCoord.emplace_back(0.0f, 0.0f);
    UVCoord.emplace_back(1.0f, 0.0f);
    UVCoord.emplace_back(1.0f, 1.0f);
    UVCoord.emplace_back(0.0f, 0.0f);
    UVCoord.emplace_back(1.0f, 1.0f);
    UVCoord.emplace_back(0.0f, 1.0f);

    if (!IsBitmapCrosshair)
    {
        verts.emplace_back(0.0f, dist);  // bottom triangle
        verts.emplace_back(base / 2.0f, (dist + height) * a);
        verts.emplace_back(-base / 2.0f, (dist + height) * a);
        verts.emplace_back(0.0f, -dist);  // top triangle
        verts.emplace_back(-base / 2.0f, -(dist + height) * a);
        verts.emplace_back(base / 2.0f, -(dist + height) * a);
        verts.emplace_back(-dist, 0.0f);  // left triangle
        verts.emplace_back(-dist - height, (base / 2.0f) * a);
        verts.emplace_back(-dist - height, -(base / 2.0f) * a);
        verts.emplace_back(dist, 0.0f);  // right triangle
        verts.emplace_back(dist + height, -(base / 2.0f) * a);
        verts.emplace_back(dist + height, (base / 2.0f) * a);
    }
    else
    {
        verts.emplace_back(-squareSize / 2.0f, -squareSize / 2.0f * a);
        verts.emplace_back(squareSize / 2.0f, -squareSize / 2.0f * a);
        verts.emplace_back(squareSize / 2.0f, squareSize / 2.0f * a);
        verts.emplace_back(-squareSize / 2.0f, -squareSize / 2.0f * a);
        verts.emplace_back(squareSize / 2.0f, squareSize / 2.0f * a);
        verts.emplace_back(-squareSize / 2.0f, squareSize / 2.0f * a);
    }

    vertexShader = R"glsl(

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

    fragmentShader = R"glsl(

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

    m_shader.LoadShaders(vertexShader, fragmentShader);
    m_shader.GetUniformLocationMap("mvp");
    m_shader.GetUniformLocationMap("CrosshairTexture");
    m_shader.GetUniformLocationMap("colour");
    m_shader.GetUniformLocationMap("isBitmap");

    m_vbo.Create(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, sizeof(BasicVertex) * (MaxVerts));
    m_vbo.Bind(true);
    m_textvbo.Create(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, sizeof(UVCoords) * (int)UVCoord.size());
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

void CCrosshair::DrawCrosshair(New3D::Mat4 matrix, float x, float y, float r, float g, float b)
{
    int count = (int)verts.size();
    if (count > MaxVerts) {
        count = MaxVerts;       // maybe we could error out somehow
    }


    m_shader.EnableShader();

    matrix.Translate(x, y, 0);
    matrix.Scale(dpiMultiplicator, dpiMultiplicator, 0);

    // update uniform memory
    glUniformMatrix4fv(m_shader.uniformLocMap["mvp"], 1, GL_FALSE, matrix);
    glUniform4f(m_shader.uniformLocMap["colour"], r, g, b, 1.0f);
    glUniform1i(m_shader.uniformLocMap["isBitmap"], false);

    // update vbo mem
    m_vbo.Bind(true);
    m_vbo.BufferSubData(0, count * sizeof(BasicVertex), verts.data());

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glBindVertexArray(0);

    m_shader.DisableShader();
}

void CCrosshair::DrawBitmapCrosshair(New3D::Mat4 matrix, float x, float y, int TextureNum)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, CrosshairtxID[TextureNum]);

    
    int count = (int)verts.size();
    if (count > MaxVerts) {
        count = MaxVerts;       // maybe we could error out somehow
    }
    m_TextureCoordsCount = (int)UVCoord.size();

    m_shader.EnableShader();

    matrix.Translate(x, y, 0);
    matrix.Scale(dpiMultiplicator * ScaleBitmap, dpiMultiplicator * ScaleBitmap, 0);

    // update uniform memory
    glUniformMatrix4fv(m_shader.uniformLocMap["mvp"], 1, GL_FALSE, matrix);
    glUniform1i(m_shader.uniformLocMap["CrosshairTexture"], 0); // 0 or 1 or GL_TEXTURE0 GL_TEXTURE1
    glUniform4f(m_shader.uniformLocMap["colour"], 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1i(m_shader.uniformLocMap["isBitmap"], true);

    // update vbo mem
    m_vbo.Bind(true);
    m_vbo.BufferSubData(0, count * sizeof(BasicVertex), verts.data());
    m_vbo.Bind(false);
    m_textvbo.Bind(true);
    m_textvbo.BufferSubData(0, m_TextureCoordsCount * sizeof(UVCoords), UVCoord.data());
    m_textvbo.Bind(false);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glBindVertexArray(0);

    m_shader.DisableShader();
}

CCrosshair::CCrosshair(const Util::Config::Node& config) : m_config(config),xRes(0),yRes(0)
{
    vertexShader = nullptr;
    fragmentShader = nullptr;
}

CCrosshair::~CCrosshair()
{
}
