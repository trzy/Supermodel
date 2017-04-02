#include "R3DScrollFog.h"
#include "Graphics/Shader.h"
#include "Mat4.h"

namespace New3D {

static const char *vertexShaderFog =

	"uniform mat4 mvp;\n"

	"void main(void)\n"
	"{\n"
	    "gl_Position = mvp * gl_Vertex;\n"
	"}\n";

static const char *fragmentShaderFog =

	"uniform float	fogAttenuation;\n"
	"uniform float	fogAmbient;\n"
	"uniform vec4	fogColour;\n"
	"uniform vec3	spotFogColor;\n"
	"uniform vec4	spotEllipse;\n"

	// Spotlight on fog
	"float	ellipse;\n"
	"vec2	position, size;\n"
	"vec3	lSpotFogColor;\n"

	// Scroll fog
	"float	lfogAttenuation;\n"
	"vec3	lFogColor;\n"
	"vec4	scrollFog;\n"

	"void main()\n"
	"{\n"
		// Scroll fog base color
		"lFogColor = fogColour.rgb * fogAmbient;\n"

		// Spotlight on fog (area) 
		"position = spotEllipse.xy;\n"
		"size = spotEllipse.zw;\n"
		"ellipse = length((gl_FragCoord.xy - position) / size);\n"
		"ellipse = pow(ellipse, 2.0);\n"			// decay rate = square of distance from center
		"ellipse = 1.0 - ellipse;\n"				// invert
		"ellipse = max(0.0, ellipse);\n"			// clamp

		// Spotlight on fog (color)
		"lSpotFogColor = mix(spotFogColor * ellipse * fogColour.rgb, vec3(0.0), fogAttenuation);\n"

		// Scroll fog density
		"scrollFog = vec4(lFogColor + lSpotFogColor, fogColour.a);\n"

		// Final Color
		"gl_FragColor = scrollFog;\n"
	"}\n";


R3DScrollFog::R3DScrollFog()
{
	//default coordinates are NDC -1,1 etc

	m_triangles[0].p1.Set(-1,-1, 0);
	m_triangles[0].p2.Set(-1, 1, 0);
	m_triangles[0].p3.Set( 1, 1, 0);

	m_triangles[1].p1.Set(-1,-1, 0);
	m_triangles[1].p2.Set( 1, 1, 0);
	m_triangles[1].p3.Set( 1,-1, 0);

	m_shaderProgram		= 0;
	m_vertexShader		= 0;
	m_fragmentShader	= 0;

	AllocResources();
}

R3DScrollFog::~R3DScrollFog()
{
	DeallocResources();
}

void R3DScrollFog::DrawScrollFog(float rgba[4], float attenuation, float ambient, float *spotRGB, float *spotEllipse)
{
	//=======
	Mat4 mvp;
	//=======

	// yeah this would have been much easier with immediate mode and fixed function ..  >_<

	// some ogl states
	glDepthMask			(GL_FALSE);			// disable z writes
	glDisable			(GL_DEPTH_TEST);	// disable depth testing
	glEnable			(GL_BLEND);
	glBlendFunc			(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_vbo.Bind			(true);
	glUseProgram		(m_shaderProgram);
	glUniform4f			(m_locFogColour, rgba[0], rgba[1], rgba[2], rgba[3]);
	glUniform1f			(m_locFogAttenuation, attenuation);
	glUniform1f			(m_locFogAmbient, ambient);
	glUniform3f			(m_locSpotFogColor, spotRGB[0], spotRGB[1], spotRGB[2]);
	glUniform4f			(m_locSpotEllipse, spotEllipse[0], spotEllipse[1], spotEllipse[2], spotEllipse[3]);
	glUniformMatrix4fv	(m_locMVP, 1, GL_FALSE, mvp);

	glEnableClientState	(GL_VERTEX_ARRAY);
	glVertexPointer		(3, GL_FLOAT, sizeof(SFVertex), 0);
	glDrawArrays		(GL_TRIANGLES, 0, 6);
	glDisableClientState(GL_VERTEX_ARRAY);

	glUseProgram		(0);
	m_vbo.Bind			(false);

	glDisable			(GL_BLEND);
	glDepthMask			(GL_TRUE);
}

void R3DScrollFog::AllocResources()
{
	bool success = LoadShaderProgram(&m_shaderProgram, &m_vertexShader, &m_fragmentShader, std::string(), std::string(), vertexShaderFog, fragmentShaderFog);

	m_locMVP			= glGetUniformLocation(m_shaderProgram, "mvp");
	m_locFogColour		= glGetUniformLocation(m_shaderProgram, "fogColour");
	m_locFogAttenuation	= glGetUniformLocation(m_shaderProgram, "fogAttenuation");
	m_locFogAmbient		= glGetUniformLocation(m_shaderProgram, "fogAmbient");
	m_locSpotFogColor	= glGetUniformLocation(m_shaderProgram, "spotFogColor");
	m_locSpotEllipse	= glGetUniformLocation(m_shaderProgram, "spotEllipse");

	m_vbo.Create(GL_ARRAY_BUFFER, GL_STATIC_DRAW, sizeof(SFTriangle) * (2), m_triangles);
}

void R3DScrollFog::DeallocResources()
{
	if (m_shaderProgram) {
		DestroyShaderProgram(m_shaderProgram, m_vertexShader, m_fragmentShader);
	}

	m_shaderProgram		= 0;
	m_vertexShader		= 0;
	m_fragmentShader	= 0;

	m_vbo.Destroy();
}

}