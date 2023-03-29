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

#include "WhiteBorder.h"
#include "Supermodel.h"
#include "Graphics/New3D/New3D.h"
#include "OSD/FileSystemPath.h"
#include "SDLIncludes.h"
#include <GL/glew.h>
#include <vector>
#include "Inputs/Inputs.h"
#include "Util/Format.h"


bool CWhiteBorder::Init()
{
    //whiteBorders must change during runtime when resolution is chnaging
    if (m_drawWhiteBorder)
    {
       m_vertexShader = R"glsl(

            #version 410 core

            uniform mat4 mvp;
            layout(location = 0) in vec3 inVertices;

            void main(void)
            {
	            gl_Position = mvp * vec4(inVertices,1.0);
            }
            )glsl";

       m_fragmentShader = R"glsl(

            #version 410 core

            uniform vec4 colour;
            out vec4 fragColour;

            void main(void)
            {
	            fragColour = colour;
            }
            )glsl";

        m_shader.LoadShaders(m_vertexShader, m_fragmentShader);
        m_shader.GetUniformLocationMap("mvp");
        m_shader.GetUniformLocationMap("colour");
        m_vbo.Create(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, sizeof(BasicVertex) * (MaxVerts));
	    m_vbo.Bind(true);


        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        m_vbo.Bind(true);
	    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(BasicVertex), 0);
	    m_vbo.Bind(false);

        glEnableVertexAttribArray(0);
	    glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
        m_drawWhiteBorder = true;
	}
    return OKAY;
}

void CWhiteBorder::GenQuad(std::vector<CBasicDrawable::BasicVertex> &verts, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    verts.emplace_back(float(x),        float(y));
    verts.emplace_back(float(x),        float(y + h));
    verts.emplace_back(float(x + w),    float(y + h));
    verts.emplace_back(float(x),        float(y));
    verts.emplace_back(float(x + w),    float(y + h));
    verts.emplace_back(float(x + w),    float(y));
}


void CWhiteBorder::Update(unsigned int xOffset, unsigned int yOffset, unsigned int xRes, unsigned int yRes, unsigned int totalXRes, unsigned int totalYRes)
{
    //white border
    if (m_drawWhiteBorder)
    {
        if(m_xRes 		!= xRes 	|| 
           m_yRes 		!= yRes 	|| 
           m_xOffset	!= xOffset  ||
           m_yOffset	!= yOffset	)
        {
            m_xRes 		= xRes 	 ;
            m_yRes 		= yRes 	 ;
            m_xOffset	= xOffset;
            m_yOffset	= yOffset;
            int whiteBorder = m_width;
            int internalWhiteBorder = m_width / 2;
            //generate border geometry
            m_verts.clear();
            GenQuad(m_verts, m_xOffset - internalWhiteBorder, m_yOffset - internalWhiteBorder, internalWhiteBorder, m_yRes + internalWhiteBorder * 2);
            GenQuad(m_verts, m_xOffset + m_xRes, m_yOffset - internalWhiteBorder, internalWhiteBorder, m_yRes + internalWhiteBorder * 2);
            GenQuad(m_verts, m_xOffset, m_yOffset - internalWhiteBorder, m_xRes , internalWhiteBorder);
            GenQuad(m_verts, m_xOffset, m_yOffset + m_yRes, m_xRes, internalWhiteBorder);
            
            // update vbo mem
            m_vbo.Bind(true);
            m_vbo.BufferSubData(0, m_verts.size() * sizeof(BasicVertex), m_verts.data());			
        }

        glUseProgram(0); 
        glViewport(0, 0, totalXRes, totalYRes);
        glDisable(GL_BLEND);   
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        
        New3D::Mat4 mvpMatrix;
        mvpMatrix.Ortho(0.0, float(totalXRes), float(totalYRes), 0.0, -1.0f, 1.0f);
        
        m_shader.EnableShader();
        
        // update uniform memory
        glUniformMatrix4fv(m_shader.uniformLocMap["mvp"], 1, GL_FALSE, mvpMatrix);
        glUniform4f(m_shader.uniformLocMap["colour"], 1.0f, 1.0f, 1.0f, 1.0f);
        
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(m_verts.size()));
        glBindVertexArray(0);
        
        m_shader.DisableShader();
        
        glEnable(GL_SCISSOR_TEST);
    }
}

bool CWhiteBorder::IsEnabled()
{
	return m_drawWhiteBorder;	
}

unsigned int CWhiteBorder::Width()
{
    return m_width;
}


CWhiteBorder::CWhiteBorder(const Util::Config::Node& config) : m_config(config)
{
    m_drawWhiteBorder = m_config["DrawWhiteBorder"].ValueAs<bool>();
    m_vertexShader = nullptr;
    m_fragmentShader = nullptr;
    m_xRes 		= 0;
    m_yRes 		= 0;
    m_xOffset	= 0;
    m_yOffset	= 0;	
    m_width     = 50;
}

CWhiteBorder::~CWhiteBorder()
{
}
