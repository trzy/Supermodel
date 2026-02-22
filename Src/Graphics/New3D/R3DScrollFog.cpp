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

#include "R3DScrollFog.h"
#include "Graphics/Shader.h"

namespace New3D {

	static const char* vertexShaderFog = R"glsl(

#version 410 core

void main(void)
{
	const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
									vec4(-1.0,  1.0, 0.0, 1.0),
									vec4( 1.0, -1.0, 0.0, 1.0),
									vec4( 1.0,  1.0, 0.0, 1.0));

	gl_Position = vertices[gl_VertexID % 4];
}

)glsl";

	static const char* fragmentShaderFog = R"glsl(

#version 410 core

uniform float	fogAttenuation;
uniform float	fogAmbient;
uniform vec4	fogColour;
uniform vec3	spotFogColor;
uniform vec4	spotEllipse;

// Spotlight on fog
float	ellipse;
vec2	position, size;
vec3	lSpotFogColor;

// Scroll fog
float	lfogAttenuation;
vec3	lFogColor;
vec4	scrollFog;

// outputs
layout(location = 0) out vec4 out0;		// opaque
layout(location = 1) out vec4 out1;		// trans layer 1
layout(location = 2) out vec4 out2;		// trans layer 2

void WriteOutputs(vec4 colour)
{
	vec4 blank = vec4(0.0);
	
	out0 = colour;
	out1 = blank;
	out2 = blank;	
}

void main()
{
	// Scroll fog base color
	lFogColor = fogColour.rgb * fogAmbient;

	// Spotlight on fog (area) 
	position = spotEllipse.xy;
	size = spotEllipse.zw;
	ellipse = length((gl_FragCoord.xy - position) / size);
	ellipse = ellipse * ellipse;			// decay rate = square of distance from center
	ellipse = 1.0 - ellipse;				// invert
	ellipse = max(0.0, ellipse);			// clamp

	// Spotlight on fog (color)
	lSpotFogColor = mix(vec3(0.0), spotFogColor * ellipse * fogColour.rgb, fogAttenuation);

	// Scroll fog density
	scrollFog = vec4(lFogColor + lSpotFogColor, fogColour.a);

	// Final Color
	WriteOutputs(scrollFog);
}

)glsl";


	R3DScrollFog::R3DScrollFog()
		: m_vao(0)
	{
		m_shaderProgram		= 0;
		m_vertexShader		= 0;
		m_fragmentShader	= 0;

		AllocResources();

		glGenVertexArrays(1, &m_vao);
		glBindVertexArray(m_vao);
		// no states needed since we do it in the shader
		glBindVertexArray(0);
	}

	R3DScrollFog::~R3DScrollFog()
	{
		DeallocResources();

		if (m_vao) {
			glDeleteVertexArrays(1, &m_vao);
			m_vao = 0;
		}
	}

	void R3DScrollFog::DrawScrollFog(float rgba[4], float attenuation, float ambient, float spotRGB[3], float spotEllipse[4])
	{
		// some ogl states
		glDepthMask			(GL_FALSE);			// disable z writes
		glDisable			(GL_DEPTH_TEST);	// disable depth testing

		glBindVertexArray	(m_vao);
		glUseProgram		(m_shaderProgram);
		glUniform4fv		(m_locFogColour, 1, rgba);
		glUniform1f			(m_locFogAttenuation, attenuation);
		glUniform1f			(m_locFogAmbient, ambient);
		glUniform3fv		(m_locSpotFogColor, 1, spotRGB);
		glUniform4fv		(m_locSpotEllipse, 1, spotEllipse);

		glDrawArrays		(GL_TRIANGLE_STRIP, 0, 4);

		glUseProgram		(0);
		glBindVertexArray	(0);

		glDisable			(GL_BLEND);
		glDepthMask			(GL_TRUE);
	}

	void R3DScrollFog::AllocResources()
	{
		LoadShaderProgram(&m_shaderProgram, &m_vertexShader, &m_fragmentShader, "", "", vertexShaderFog, fragmentShaderFog);

		m_locFogColour		= glGetUniformLocation(m_shaderProgram, "fogColour");
		m_locFogAttenuation	= glGetUniformLocation(m_shaderProgram, "fogAttenuation");
		m_locFogAmbient		= glGetUniformLocation(m_shaderProgram, "fogAmbient");
		m_locSpotFogColor	= glGetUniformLocation(m_shaderProgram, "spotFogColor");
		m_locSpotEllipse	= glGetUniformLocation(m_shaderProgram, "spotEllipse");
	}

	void R3DScrollFog::DeallocResources()
	{
		if (m_shaderProgram) {
			DestroyShaderProgram(m_shaderProgram, m_vertexShader, m_fragmentShader);
		}

		m_shaderProgram		= 0;
		m_vertexShader		= 0;
		m_fragmentShader	= 0;
	}

}
