#include "R3DShader.h"
#include "Graphics/Shader.h"

namespace New3D {

static char *vertexShaderBasic =

// uniforms
"uniform float	fogIntensity;\n"
"uniform float	fogDensity;\n"
"uniform float	fogStart;\n"

//outputs to fragment shader
"varying float	fsFogFactor;\n"
"varying float	fsSpecularTerm;\n"		// specular light term (additive)
"varying float	fsViewZ;\n"
"varying vec3	fsViewNormal;\n"		// per vertex normal vector

"void main(void)\n"
"{\n"
	"vec3	viewVertex;\n"

	"viewVertex		= vec3(gl_ModelViewMatrix * gl_Vertex);\n"
	"fsViewNormal	= normalize(gl_NormalMatrix*gl_Normal);\n"
	"float z		= length(viewVertex);\n"
	"fsFogFactor	= clamp(1.0 - fogIntensity*(fogStart + z*fogDensity), 0.0, 1.0);\n"
	"fsViewZ		= -viewVertex.z;\n"	// convert Z from GL->Real3D convention (want +Z to be further into screen)

	"gl_FrontColor	= gl_Color;\n"
	"gl_TexCoord[0]	= gl_MultiTexCoord0;\n"
	"gl_Position	= gl_ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n";

static char *fragmentShaderBasic =

"uniform sampler2D tex;\n"
"uniform int	textureEnabled;\n"
"uniform int	alphaTest;\n"
"uniform int	textureAlpha;\n"
"uniform vec3	fogColour;\n"
"uniform vec4	spotEllipse;\n"		// spotlight ellipse position: .x=X position (screen coordinates), .y=Y position, .z=half-width, .w=half-height)
"uniform vec2	spotRange;\n"			// spotlight Z range: .x=start (viewspace coordinates), .y=limit
"uniform vec3	spotColor;\n"			// spotlight RGB color
"uniform vec3	lighting[2];\n"		// lighting state (lighting[0] = sun direction, lighting[1].x,y = diffuse, ambient intensities from 0-1.0)
"uniform int	lightEnable;\n"		// lighting enabled (1.0) or luminous (0.0), drawn at full intensity
"uniform float	shininess;\n"		// specular shininess (if >= 0.0) or disable specular lighting (negative)

//interpolated inputs from vertex shader
"varying float	fsFogFactor;\n"
"varying float	fsSpecularTerm;\n"		// specular light term (additive)
"varying float	fsViewZ;\n"
"varying vec3	fsViewNormal;\n"		// per vertex normal vector

"void main()\n"
"{\n"
	"vec4 texData;\n"
	"vec4 colData;\n"
	"vec4 finalData;\n"

	"texData = vec4(1.0, 1.0, 1.0, 1.0);\n"

	"if(textureEnabled==1) {\n"
		"texData = texture2D( tex, gl_TexCoord[0].st);\n"

		"if (alphaTest==1) {\n"			// does it make any sense to do this later?
			"if (texData.a < (8.0/16.0)) {\n"
				"discard;\n"
			"}\n"
		"}\n"

		"if (textureAlpha == 0) {\n"
			"texData.a = 1.0;\n"
		"}\n"
	"}\n"

	"colData =  gl_Color;\n"

	"finalData = texData * colData;\n"
	"if (finalData.a < (1.0/16.0)) {\n"		// basically chuck out any totally transparent pixels value = 1/16 the smallest transparency level h/w supports
		"discard;\n"
	"}\n"

	"vec3 lightIntensity;\n"

	"if (lightEnable==1)\n"
	"{\n"
		"vec3	sunVector;\n"		// sun lighting vector (as reflecting away from vertex)
		"float	sunFactor;\n"		// sun light projection along vertex normal (0.0 to 1.0)

		// Real3D -> OpenGL view space convention (TO-DO: do this outside of shader)
		"sunVector = lighting[0] * vec3(1.0, -1.0, -1.0);\n"

		// Compute diffuse factor for sunlight
		"sunFactor = max(dot(sunVector, fsViewNormal), 0.0);\n"

		// Total light intensity: sum of all components
		"lightIntensity = vec3(sunFactor*lighting[1].x + lighting[1].y);\n"
		"lightIntensity = clamp(lightIntensity,0.0,1.0);\n"
	"}\n"
	"else {\n"
			"lightIntensity = vec3(1.0,1.0,1.0);\n"
	"}\n"

	"finalData.rgb *= lightIntensity;\n"

	/*
	"vec2	ellipse;\n"
	"vec3	lightIntensity;\n"
	"float	insideSpot;\n"

	// Compute spotlight and apply lighting
	"ellipse	= (gl_FragCoord.xy - spotEllipse.xy) / spotEllipse.zw;\n"
	"insideSpot = dot(ellipse, ellipse);\n"

	"if ((insideSpot <= 1.0) && (fsViewZ >= spotRange.x) && (fsViewZ<spotRange.y)) {\n"
		"lightIntensity = fsLightIntensity + (1.0 - insideSpot)*spotColor;\n"
	"}\n"
	"else {\n"
		"lightIntensity = fsLightIntensity;\n"
	"}\n"

	"finalData.rgb *= lightIntensity;\n"
	"finalData.rgb += vec3(fsSpecularTerm, fsSpecularTerm, fsSpecularTerm);\n"
	*/

	"finalData.rgb = mix(fogColour, finalData.rgb, fsFogFactor);\n"

	"gl_FragColor = finalData;\n"
"}\n";

R3DShader::R3DShader()
{
	m_shaderProgram		= 0;
	m_vertexShader		= 0;
	m_fragmentShader	= 0;

	Start();	// reset attributes
}

void R3DShader::Start()
{
	m_textured		= false;
	m_textureAlpha	= false;		// use alpha in texture
	m_alphaTest		= false;		// discard fragment based on alpha (ogl does this with fixed function)
	m_doubleSided	= false;
	m_lightEnabled	= false;

	m_matDet = MatDet::notset;

	m_dirtyMesh		= true;			// dirty means all the above are dirty, ie first run
	m_dirtyModel	= true;
}

bool R3DShader::LoadShader(const char* vertexShader, const char* fragmentShader)
{
	const char* vShader;
	const char* fShader;
	bool success;

	if (vertexShader) {
		vShader = vertexShader;
	}
	else {
		vShader = vertexShaderBasic;
	}

	if (fragmentShader) {
		fShader = fragmentShader;
	}
	else {
		fShader = fragmentShaderBasic;
	}

	success = LoadShaderProgram(&m_shaderProgram, &m_vertexShader, &m_fragmentShader, nullptr, nullptr, vShader, fShader);

	m_locTexture		= glGetUniformLocation(m_shaderProgram, "tex");
	m_locTextureEnabled	= glGetUniformLocation(m_shaderProgram, "textureEnabled");
	m_locTextureAlpha	= glGetUniformLocation(m_shaderProgram, "textureAlpha");
	m_locAlphaTest		= glGetUniformLocation(m_shaderProgram, "alphaTest");

	m_locFogIntensity	= glGetUniformLocation(m_shaderProgram, "fogIntensity");
	m_locFogDensity		= glGetUniformLocation(m_shaderProgram, "fogDensity");
	m_locFogStart		= glGetUniformLocation(m_shaderProgram, "fogStart");
	m_locFogColour		= glGetUniformLocation(m_shaderProgram, "fogColour");

	m_locLighting		= glGetUniformLocation(m_shaderProgram, "lighting");
	m_locLightEnable	= glGetUniformLocation(m_shaderProgram, "lightEnable");
	m_locShininess		= glGetUniformLocation(m_shaderProgram, "shininess");
	m_locSpotEllipse	= glGetUniformLocation(m_shaderProgram, "spotEllipse");
	m_locSpotRange		= glGetUniformLocation(m_shaderProgram, "spotRange");
	m_locSpotColor		= glGetUniformLocation(m_shaderProgram, "spotColor");

	return success;
}

void R3DShader::SetShader(bool enable)
{
	if (enable) {
		glUseProgram(m_shaderProgram);
		Start();
	}
	else {
		glUseProgram(0);
	}
}

void R3DShader::SetMeshUniforms(const Mesh* m)
{
	if (m == nullptr) {
		return;			// sanity check
	}

	if (m_dirtyMesh) {
		glUniform1i(m_locTexture, 0);
	}

	if (m_dirtyMesh || m->textured != m_textured) {
		glUniform1i(m_locTextureEnabled, m->textured);
		m_textured = m->textured;
	}

	if (m_dirtyMesh || m->alphaTest != m_alphaTest) {
		glUniform1i(m_locAlphaTest, m->alphaTest);
		m_alphaTest = m->alphaTest;
	}

	if (m_dirtyMesh || m->textureAlpha != m_textureAlpha) {
		glUniform1i(m_locTextureAlpha, m->textureAlpha);
		m_textureAlpha = m->textureAlpha;
	}

	if (m_dirtyMesh || m->fogIntensity != m_fogIntensity) {
		glUniform1f(m_locFogIntensity, m->fogIntensity);
		m_fogIntensity = m->fogIntensity;
	}

	if (m_dirtyMesh || m->lighting != m_lightEnabled) {
		glUniform1i(m_locLightEnable, m->lighting);
		m_lightEnabled = m->lighting;
	}

	//glUniform1f(m_locShininess, 1);

	if (m_matDet!=MatDet::zero) {

		if (m_dirtyMesh || m->doubleSided != m_doubleSided) {

			m_doubleSided = m->doubleSided;

			if (m_doubleSided) {
				glDisable(GL_CULL_FACE);
			}
			else {
				glEnable(GL_CULL_FACE);
			}
		}
	}


	m_dirtyMesh = false;
}

void R3DShader::SetViewportUniforms(const Viewport *vp)
{	
	//didn't bother caching these, they don't get frequently called anyway
	glUniform1f	(m_locFogDensity, vp->fogParams[3]);
	glUniform1f	(m_locFogStart, vp->fogParams[4]);
	glUniform3fv(m_locFogColour, 1, vp->fogParams);

	glUniform3fv(m_locLighting, 2, vp->lightingParams);
	glUniform4fv(m_locSpotEllipse, 1, vp->spotEllipse);
	glUniform2fv(m_locSpotRange, 1, vp->spotRange);
	glUniform3fv(m_locSpotColor, 1, vp->spotColor);
}

void R3DShader::SetModelStates(const Model* model)
{
	//==========
	MatDet test;
	//==========

	test = MatDet::notset;		// happens for bad matrices with NaN

	if (model->determinant < 0)			{ test = MatDet::negative; }
	else if (model->determinant > 0)	{ test = MatDet::positive; }
	else if (model->determinant == 0)	{ test = MatDet::zero; }

	if (m_dirtyModel || m_matDet!=test) {

		switch (test) {
		case MatDet::negative:
			glCullFace(GL_FRONT);
			glEnable(GL_CULL_FACE);
			m_doubleSided = false;
			break;
		case MatDet::positive:
			glCullFace(GL_BACK);
			glEnable(GL_CULL_FACE);
			m_doubleSided = false;
			break;
		default:
			glDisable(GL_CULL_FACE);
			m_doubleSided = true;		// basically drawing on both sides now
		}
	}

	m_matDet		= test;
	m_dirtyModel	= false;
}

} // New3D
