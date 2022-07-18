#include "R3DShader.h"
#include "R3DShaderQuads.h"
#include "R3DShaderTriangles.h"

// having 2 sets of shaders to maintain is really less than ideal
// but hopefully not too many breaking changes at this point

namespace New3D {

R3DShader::R3DShader(const Util::Config::Node &config)
	: m_config(config)
{
	m_shaderProgram		= 0;
	m_vertexShader		= 0;
	m_geoShader			= 0;
	m_fragmentShader	= 0;

	Start();	// reset attributes
}

void R3DShader::Start()
{
	m_textured1			= false;
	m_textured2			= false;
	m_textureAlpha		= false;		// use alpha in texture
	m_alphaTest			= false;		// discard fragment based on alpha (ogl does this with fixed function)
	m_lightEnabled		= false;
	m_specularEnabled	= false;
	m_layered			= false;
	m_textureInverted	= false;
	m_fixedShading		= false;
	m_translatorMap		= false;
	m_modelScale		= 1.0f;
	m_shininess			= 0;
	m_specularValue		= 0;
	m_microTexScale		= 0;

	m_baseTexSize[0]	= 0;
	m_baseTexSize[1]	= 0;

	m_texWrapMode[0]	= 0;
	m_texWrapMode[1]	= 0;

	m_dirtyMesh			= true;			// dirty means all the above are dirty, ie first run
	m_dirtyModel		= true;
}

bool R3DShader::LoadShader(const char* vertexShader, const char* fragmentShader)
{
	bool quads = m_config["QuadRendering"].ValueAs<bool>();

	const char* vShader = vertexShaderR3D;
	const char* gShader = "";
	const char* fShader = fragmentShaderR3D;

	if (quads) {
		vShader = vertexShaderR3DQuads;
		gShader = geometryShaderR3DQuads;
		fShader = fragmentShaderR3DQuads;
	}

	m_shaderProgram		= glCreateProgram();
	m_vertexShader		= glCreateShader(GL_VERTEX_SHADER);
	m_fragmentShader	= glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(m_vertexShader,		1, (const GLchar **)&vShader, NULL);
	glShaderSource(m_fragmentShader,	1, (const GLchar **)&fShader, NULL);

	glCompileShader(m_vertexShader);
	glCompileShader(m_fragmentShader);

	if (quads) {
		m_geoShader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(m_geoShader, 1, (const GLchar **)&gShader, NULL);
		glCompileShader(m_geoShader);
		glAttachShader(m_shaderProgram, m_geoShader);
		PrintShaderResult(m_geoShader);
	}

	PrintShaderResult(m_vertexShader);
	PrintShaderResult(m_fragmentShader);

	glAttachShader(m_shaderProgram, m_vertexShader);
	glAttachShader(m_shaderProgram, m_fragmentShader);
	glLinkProgram(m_shaderProgram);

	PrintProgramResult(m_shaderProgram);

	m_locTexture1			= glGetUniformLocation(m_shaderProgram, "tex1");
	m_locTexture2			= glGetUniformLocation(m_shaderProgram, "tex2");
	m_locTexture1Enabled	= glGetUniformLocation(m_shaderProgram, "textureEnabled");
	m_locTexture2Enabled	= glGetUniformLocation(m_shaderProgram, "microTexture");
	m_locTextureAlpha		= glGetUniformLocation(m_shaderProgram, "textureAlpha");
	m_locAlphaTest			= glGetUniformLocation(m_shaderProgram, "alphaTest");
	m_locMicroTexScale		= glGetUniformLocation(m_shaderProgram, "microTextureScale");
	m_locBaseTexSize		= glGetUniformLocation(m_shaderProgram, "baseTexSize");
	m_locTextureInverted	= glGetUniformLocation(m_shaderProgram, "textureInverted");
	m_locTexWrapMode		= glGetUniformLocation(m_shaderProgram, "textureWrapMode");

	m_locFogIntensity		= glGetUniformLocation(m_shaderProgram, "fogIntensity");
	m_locFogDensity			= glGetUniformLocation(m_shaderProgram, "fogDensity");
	m_locFogStart			= glGetUniformLocation(m_shaderProgram, "fogStart");
	m_locFogColour			= glGetUniformLocation(m_shaderProgram, "fogColour");
	m_locFogAttenuation		= glGetUniformLocation(m_shaderProgram, "fogAttenuation");
	m_locFogAmbient			= glGetUniformLocation(m_shaderProgram, "fogAmbient");

	m_locLighting			= glGetUniformLocation(m_shaderProgram, "lighting");
	m_locLightEnabled		= glGetUniformLocation(m_shaderProgram, "lightEnabled");
	m_locSunClamp			= glGetUniformLocation(m_shaderProgram, "sunClamp");
	m_locIntensityClamp		= glGetUniformLocation(m_shaderProgram, "intensityClamp");
	m_locShininess			= glGetUniformLocation(m_shaderProgram, "shininess");
	m_locSpecularValue		= glGetUniformLocation(m_shaderProgram, "specularValue");
	m_locSpecularEnabled	= glGetUniformLocation(m_shaderProgram, "specularEnabled");
	m_locFixedShading		= glGetUniformLocation(m_shaderProgram, "fixedShading");
	m_locTranslatorMap		= glGetUniformLocation(m_shaderProgram, "translatorMap");

	m_locSpotEllipse		= glGetUniformLocation(m_shaderProgram, "spotEllipse");
	m_locSpotRange			= glGetUniformLocation(m_shaderProgram, "spotRange");
	m_locSpotColor			= glGetUniformLocation(m_shaderProgram, "spotColor");
	m_locSpotFogColor		= glGetUniformLocation(m_shaderProgram, "spotFogColor");
	m_locModelScale			= glGetUniformLocation(m_shaderProgram, "modelScale");

	m_locProjMat			= glGetUniformLocation(m_shaderProgram, "projMat");
	m_locModelMat			= glGetUniformLocation(m_shaderProgram, "modelMat");

	m_locHardwareStep		= glGetUniformLocation(m_shaderProgram, "hardwareStep");
	m_locDiscardAlpha		= glGetUniformLocation(m_shaderProgram, "discardAlpha");

	return true;
}

GLint R3DShader::GetVertexAttribPos(const std::string& attrib)
{
	if (m_vertexLocCache.count(attrib)==0) {
		auto pos = glGetAttribLocation(m_shaderProgram, attrib.c_str());
		m_vertexLocCache[attrib] = pos;
		return pos;
	}

	return m_vertexLocCache[attrib];
}

void R3DShader::SetShader(bool enable)
{
	if (enable) {
		glUseProgram(m_shaderProgram);
		Start();
		DiscardAlpha(false);	// need some default
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
		glUniform1i(m_locTexture1, 0);
		glUniform1i(m_locTexture2, 1);
	}

	if (m_dirtyMesh || m->textured != m_textured1) {
		glUniform1i(m_locTexture1Enabled, m->textured);
		m_textured1 = m->textured;
	}

	if (m_dirtyMesh || m->microTexture != m_textured2) {
		glUniform1i(m_locTexture2Enabled, m->microTexture);
		m_textured2 = m->microTexture;
	}

	if (m_dirtyMesh || m->microTextureScale != m_microTexScale) {
		glUniform1f(m_locMicroTexScale, m->microTextureScale);
		m_microTexScale = m->microTextureScale;
	}

	if (m_dirtyMesh || (m_baseTexSize[0] != m->width || m_baseTexSize[1] != m->height)) {
		m_baseTexSize[0] = (float)m->width;
		m_baseTexSize[1] = (float)m->height;
		glUniform2fv(m_locBaseTexSize, 1, m_baseTexSize);
	}

	if (m_dirtyMesh || m->inverted != m_textureInverted) {
		glUniform1i(m_locTextureInverted, m->inverted);
		m_textureInverted = m->inverted;
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
		glUniform1i(m_locLightEnabled, m->lighting);
		m_lightEnabled = m->lighting;
	}

	if (m_dirtyMesh || m->shininess != m_shininess) {
		glUniform1f(m_locShininess, m->shininess);
		m_shininess = m->shininess;
	}

	if (m_dirtyMesh || m->specular != m_specularEnabled) {
		glUniform1i(m_locSpecularEnabled, m->specular);
		m_specularEnabled = m->specular;
	}

	if (m_dirtyMesh || m->specularValue != m_specularValue) {
		glUniform1f(m_locSpecularValue, m->specularValue);
		m_specularValue = m->specularValue;
	}

	if (m_dirtyMesh || m->fixedShading != m_fixedShading) {
		glUniform1i(m_locFixedShading, m->fixedShading);
		m_fixedShading = m->fixedShading;
	}

	if (m_dirtyMesh || m->translatorMap != m_translatorMap) {
		glUniform1i(m_locTranslatorMap, m->translatorMap);
		m_translatorMap = m->translatorMap;
	}

	if (m_dirtyMesh || m->wrapModeU != m_texWrapMode[0] || m->wrapModeV != m_texWrapMode[1]) {
		m_texWrapMode[0] = m->wrapModeU;
		m_texWrapMode[1] = m->wrapModeV;
		glUniform2iv(m_locTexWrapMode, 1, m_texWrapMode);
	}

	if (m_dirtyMesh || m->layered != m_layered) {
		m_layered = m->layered;
		if (m_layered) {
			glEnable(GL_STENCIL_TEST);
		}
		else {
			glDisable(GL_STENCIL_TEST);
		}
	}

	m_dirtyMesh = false;
}

void R3DShader::SetViewportUniforms(const Viewport *vp)
{
	//didn't bother caching these, they don't get frequently called anyway
	glUniform1f(m_locFogDensity, vp->fogParams[3]);
	glUniform1f(m_locFogStart, vp->fogParams[4]);
	glUniform3fv(m_locFogColour, 1, vp->fogParams);
	glUniform1f(m_locFogAttenuation, vp->fogParams[5]);
	glUniform1f(m_locFogAmbient, vp->fogParams[6]);

	glUniform3fv(m_locLighting, 2, vp->lightingParams);
	glUniform1i(m_locSunClamp, vp->sunClamp);
	glUniform1i(m_locIntensityClamp, vp->intensityClamp);
	glUniform4fv(m_locSpotEllipse, 1, vp->spotEllipse);
	glUniform2fv(m_locSpotRange, 1, vp->spotRange);
	glUniform3fv(m_locSpotColor, 1, vp->spotColor);
	glUniform3fv(m_locSpotFogColor, 1, vp->spotFogColor);

	glUniformMatrix4fv(m_locProjMat, 1, GL_FALSE, vp->projectionMatrix);

	glUniform1i(m_locHardwareStep, vp->hardwareStep);
}

void R3DShader::SetModelStates(const Model* model)
{
	if (m_dirtyModel || model->scale != m_modelScale) {
		glUniform1f(m_locModelScale, model->scale);
		m_modelScale = model->scale;
	}

	glUniformMatrix4fv(m_locModelMat, 1, GL_FALSE, model->modelMat);

	m_dirtyModel = false;
}

void R3DShader::DiscardAlpha(bool discard)
{
	glUniform1i(m_locDiscardAlpha, discard);
}

void R3DShader::PrintShaderResult(GLuint shader)
{
	//===========
	GLint result;
	GLint length;
	//===========

	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

	if (result == GL_FALSE) {

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

		if (length > 0) {
			std::vector<char> msg(length);
			glGetShaderInfoLog(shader, length, NULL, msg.data());
			printf("%s\n", msg.data());
		}
	}
}

void R3DShader::PrintProgramResult(GLuint program)
{
	//===========
	GLint result;
	//===========

	glGetProgramiv(program, GL_LINK_STATUS, &result);

	if (result == GL_FALSE) {

		GLint maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		//The maxLength includes the NULL character
		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
		printf("%s\n", infoLog.data());
	}
}
} // New3D
