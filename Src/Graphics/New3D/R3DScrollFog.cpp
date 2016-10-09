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

	"uniform vec4 fogColour;\n"

	"void main()\n"
	"{\n"
	    "gl_FragColor = fogColour;\n"
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

void R3DScrollFog::DrawScrollFog(float r, float g, float b, float a)
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
	glUniform4f			(m_locFogColour, r, g, b, a);
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
	bool success = LoadShaderProgram(&m_shaderProgram, &m_vertexShader, &m_fragmentShader, nullptr, nullptr, vertexShaderFog, fragmentShaderFog);

	m_locMVP		= glGetUniformLocation(m_shaderProgram, "mvp");
	m_locFogColour	= glGetUniformLocation(m_shaderProgram, "fogColour");

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