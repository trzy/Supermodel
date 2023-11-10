#include "R3DShader.h"
#include "R3DShaderQuads.h"
#include "R3DShaderTriangles.h"
#include "R3DShaderCommon.h"

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
	m_noLosReturn		= false;
	m_textureInverted	= false;
	m_fixedShading		= false;
	m_translatorMap		= false;
	m_modelScale		= 1.0f;
	m_nodeAlpha			= 1.0f;
	m_shininess			= 0;
	m_specularValue		= 0;
	m_microTexScale		= 0;
	m_microTexID		= -1;

	m_baseTexInfo[0]	= -1;
	m_baseTexInfo[1]	= -1;
	m_baseTexInfo[2]	= -1;
	m_baseTexInfo[3]	= -1;

	m_baseTexType		= -1;

	m_transX			= -1;
	m_transY			= -1;
	m_transPage			= -1;

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

	const char* shaderArray[] = { fShader, fragmentShaderR3DCommon };

	glShaderSource(m_vertexShader, 1, (const GLchar **)&vShader, nullptr);
	glShaderSource(m_fragmentShader, (GLsizei)std::size(shaderArray), shaderArray, nullptr);

	glCompileShader(m_vertexShader);
	glCompileShader(m_fragmentShader);

	if (quads) {
		m_geoShader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(m_geoShader, 1, (const GLchar **)&gShader, nullptr);
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
	m_locTexture1Enabled	= glGetUniformLocation(m_shaderProgram, "textureEnabled");
	m_locTexture2Enabled	= glGetUniformLocation(m_shaderProgram, "microTexture");
	m_locTextureAlpha		= glGetUniformLocation(m_shaderProgram, "textureAlpha");
	m_locAlphaTest			= glGetUniformLocation(m_shaderProgram, "alphaTest");
	m_locMicroTexScale		= glGetUniformLocation(m_shaderProgram, "microTextureScale");
	m_locMicroTexID			= glGetUniformLocation(m_shaderProgram, "microTextureID");
	m_locBaseTexInfo		= glGetUniformLocation(m_shaderProgram, "baseTexInfo");
	m_locBaseTexType		= glGetUniformLocation(m_shaderProgram, "baseTexType");
	m_locTextureInverted	= glGetUniformLocation(m_shaderProgram, "textureInverted");
	m_locTexWrapMode		= glGetUniformLocation(m_shaderProgram, "textureWrapMode");
	m_locColourLayer		= glGetUniformLocation(m_shaderProgram, "colourLayer");

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
	m_locNodeAlpha			= glGetUniformLocation(m_shaderProgram, "nodeAlpha");

	m_locProjMat			= glGetUniformLocation(m_shaderProgram, "projMat");
	m_locModelMat			= glGetUniformLocation(m_shaderProgram, "modelMat");

	m_locHardwareStep		= glGetUniformLocation(m_shaderProgram, "hardwareStep");
	m_locDiscardAlpha		= glGetUniformLocation(m_shaderProgram, "discardAlpha");

	return true;
}

void R3DShader::UnloadShader()
{
	// make sure no shader is bound
	glUseProgram(0);

	if (m_vertexShader) {
		glDeleteShader(m_vertexShader);
		m_vertexShader = 0;
	}

	if (m_geoShader) {
		glDeleteShader(m_geoShader);
		m_geoShader = 0;
	}

	if (m_fragmentShader) {
		glDeleteShader(m_fragmentShader);
		m_fragmentShader = 0;
	}

	if (m_shaderProgram) {
		glDeleteProgram(m_shaderProgram);
		m_shaderProgram = 0;
	}
}

GLint R3DShader::GetVertexAttribPos(const std::string& attrib)
{
	if (m_vertexLocCache.count(attrib)==0) {
		auto pos = glGetAttribLocation(m_shaderProgram, attrib.c_str());
		m_vertexLocCache[attrib] = pos;
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

	if (m_dirtyMesh || m->microTextureID != m_microTexID) {
		glUniform1i(m_locMicroTexID, m->microTextureID);
		m_microTexID = m->microTextureID;
	}

	if (m_dirtyMesh || (m_baseTexInfo[0] != m->x || m_baseTexInfo[1] != m->y) || m_baseTexInfo[2] != m->width || m_baseTexInfo[3] != m->height) {

		m_baseTexInfo[0] = m->x;
		m_baseTexInfo[1] = m->y;
		m_baseTexInfo[2] = m->width;
		m_baseTexInfo[3] = m->height;

		int translatedX, translatedY;
		CalcTexOffset(m_transX, m_transY, m_transPage, m->x, m->y, translatedX, translatedY);	// need to apply model translation

		glUniform4i(m_locBaseTexInfo, translatedX, translatedY, m->width, m->height);
	}

	if (m_dirtyMesh || m_baseTexType != m->format) {
		m_baseTexType = m->format;
		glUniform1i(m_locBaseTexType,  m_baseTexType);
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

	if (m_dirtyMesh || m->noLosReturn != m_noLosReturn) {
		m_noLosReturn = m->noLosReturn;
		glStencilFunc(GL_ALWAYS, m_noLosReturn << 7, 0b10000000);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0b10000000);
	}

	if (m_dirtyMesh || m->layered != m_layered) {
		m_layered = m->layered;
		// i think it should just disable z write, but the polys I think must be written first
		if (m_layered) {
			glStencilFunc(GL_EQUAL, 0, 0b01111111);			// basically stencil test passes if the value is zero
			glStencilOp(GL_KEEP, GL_INCR, GL_INCR);			// if the stencil test passes, we increment the value
			glStencilMask(0b01111111);
		}
		else {
			glStencilFunc(GL_ALWAYS, m_noLosReturn << 7, 0b10000000);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
			glStencilMask(0b10000000);
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

	if (m_dirtyModel || model->alpha != m_nodeAlpha) {
		glUniform1f(m_locNodeAlpha, model->alpha);
		m_nodeAlpha = model->alpha;
	}

	m_transX = model->textureOffsetX;
	m_transY = model->textureOffsetY;
	m_transPage = model->page;

	// reset texture values
	for (auto& i : m_baseTexInfo) { i = -1; }

	glUniformMatrix4fv(m_locModelMat, 1, GL_FALSE, model->modelMat);

	m_dirtyModel = false;
}

void R3DShader::DiscardAlpha(bool discard)
{
	glUniform1i(m_locDiscardAlpha, discard);
}

void R3DShader::SetLayer(Layer layer)
{
	glUniform1i(m_locColourLayer, (GLint)layer);
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

void R3DShader::CalcTexOffset(int offX, int offY, int page, int x, int y, int& newX, int& newY)
{
	newX = (x + offX) & 2047;	// wrap around 2048, shouldn't be required

	int oldPage = y / 1024;

	y -= (oldPage * 1024);	// remove page from tex y

	// calc newY with wrap around, wraps around in the same sheet, not into another memory sheet

	newY = (y + offY) & 1023;

	// add page to Y

	newY += ((oldPage + page) & 1) * 1024;		// max page 0-1
}

} // New3D
