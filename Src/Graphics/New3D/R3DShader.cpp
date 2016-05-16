#include "R3DShader.h"
#include "Graphics/Shader.h"

namespace New3D {

static const char *vertexShaderBasic =

// uniforms
"uniform float	fogIntensity;\n"
"uniform float	fogDensity;\n"
"uniform float	fogStart;\n"

//outputs to fragment shader
"varying float	fsFogFactor;\n"
"varying float	fsSpecularTerm;\n"		// specular light term (additive)
"varying vec3	fsViewVertex;\n"
"varying vec3	fsViewNormal;\n"		// per vertex normal vector

"void main(void)\n"
"{\n"
	"fsViewVertex	= vec3(gl_ModelViewMatrix * gl_Vertex);\n"
	"fsViewNormal	= normalize(gl_NormalMatrix  *gl_Normal);\n"
	"float z		= length(fsViewVertex);\n"
	"fsFogFactor	= fogIntensity * clamp(fogStart + z * fogDensity, 0.0, 1.0);\n"

	"gl_FrontColor	= gl_Color;\n"
	"gl_TexCoord[0]	= gl_MultiTexCoord0;\n"
	"gl_Position	= gl_ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n";

static const char *fragmentShaderBasic =

"uniform sampler2D tex1;\n"			// base tex
"uniform sampler2D tex2;\n"			// micro tex (optional)

"uniform int	textureEnabled;\n"
"uniform int	microTexture;\n"
"uniform int	alphaTest;\n"
"uniform int	textureAlpha;\n"
"uniform vec3	fogColour;\n"
"uniform vec4	spotEllipse;\n"			// spotlight ellipse position: .x=X position (screen coordinates), .y=Y position, .z=half-width, .w=half-height)
"uniform vec2	spotRange;\n"			// spotlight Z range: .x=start (viewspace coordinates), .y=limit
"uniform vec3	spotColor;\n"			// spotlight RGB color
"uniform vec3	lighting[2];\n"			// lighting state (lighting[0] = sun direction, lighting[1].x,y = diffuse, ambient intensities from 0-1.0)
"uniform int	lightEnable;\n"			// lighting enabled (1.0) or luminous (0.0), drawn at full intensity
"uniform float	specularCoefficient;\n"	// specular coefficient
"uniform float	shininess;\n"			// specular shininess

//interpolated inputs from vertex shader
"varying float	fsFogFactor;\n"
"varying float	fsSpecularTerm;\n"		// specular light term (additive)
"varying vec3	fsViewVertex;\n"
"varying vec3	fsViewNormal;\n"		// per vertex normal vector

"void main()\n"
"{\n"
	"vec4 tex1Data;\n"
	"vec4 colData;\n"
	"vec4 finalData;\n"

	"tex1Data = vec4(1.0, 1.0, 1.0, 1.0);\n"

	"if(textureEnabled==1) {\n"

		"tex1Data = texture2D( tex1, gl_TexCoord[0].st);\n"

		"if (microTexture==1) {\n"
			"vec4 tex2Data = texture2D( tex2, gl_TexCoord[0].st * 4.0);\n"
			"tex1Data = (tex1Data+tex2Data)/2.0;\n"
		"}\n"

		"if (alphaTest==1) {\n"			// does it make any sense to do this later?
			"if (tex1Data.a < (8.0/16.0)) {\n"
				"discard;\n"
			"}\n"
		"}\n"

		"if (textureAlpha == 0) {\n"
			"tex1Data.a = 1.0;\n"
		"}\n"
	"}\n"

	"colData = gl_Color;\n"

	"finalData = tex1Data * colData;\n"
	"if (finalData.a < (1.0/16.0)) {\n"		// basically chuck out any totally transparent pixels value = 1/16 the smallest transparency level h/w supports
		"discard;\n"
	"}\n"

	
	"if (lightEnable==1)\n"
	"{\n"
		"vec3	lightIntensity;\n"
		"vec3	sunVector;\n"		// sun lighting vector (as reflecting away from vertex)
		"float	sunFactor;\n"		// sun light projection along vertex normal (0.0 to 1.0)

		// Real3D -> OpenGL view space convention (TO-DO: do this outside of shader)
		"sunVector = lighting[0] * vec3(1.0, -1.0, -1.0);\n"

		// Compute diffuse factor for sunlight
		"sunFactor = max(dot(sunVector, fsViewNormal), 0.0);\n"

		// Total light intensity: sum of all components 
		"lightIntensity = vec3(sunFactor*lighting[1].x + lighting[1].y);\n"	// ambient + diffuse
		"lightIntensity = clamp(lightIntensity,0.0,1.0);\n"

		"finalData.rgb *= lightIntensity;\n"

		"if (sunFactor > 0.0 && specularCoefficient > 0.0) {\n"

			"vec3 v = normalize(-fsViewVertex);\n"
			"vec3 h = normalize(sunVector + v);\n"	// halfway vector

			"float NdotHV = max(dot(fsViewNormal,h),0.0);\n"

			"finalData.rgb += vec3(specularCoefficient * pow(NdotHV,shininess));\n"
		"}\n"
	"}\n"

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

	*/

	"finalData.rgb = mix(finalData.rgb, fogColour, fsFogFactor);\n"

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
	m_textured1		= false;
	m_textured2		= false;
	m_textureAlpha	= false;		// use alpha in texture
	m_alphaTest		= false;		// discard fragment based on alpha (ogl does this with fixed function)
	m_doubleSided	= false;
	m_lightEnabled	= false;

	m_shininess = 0;
	m_specularCoefficient = 0;

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

	m_locTexture1		= glGetUniformLocation(m_shaderProgram, "tex1");
	m_locTexture2		= glGetUniformLocation(m_shaderProgram, "tex2");
	m_locTexture1Enabled= glGetUniformLocation(m_shaderProgram, "textureEnabled");
	m_locTexture2Enabled= glGetUniformLocation(m_shaderProgram, "microTexture");
	m_locTextureAlpha	= glGetUniformLocation(m_shaderProgram, "textureAlpha");
	m_locAlphaTest		= glGetUniformLocation(m_shaderProgram, "alphaTest");

	m_locFogIntensity	= glGetUniformLocation(m_shaderProgram, "fogIntensity");
	m_locFogDensity		= glGetUniformLocation(m_shaderProgram, "fogDensity");
	m_locFogStart		= glGetUniformLocation(m_shaderProgram, "fogStart");
	m_locFogColour		= glGetUniformLocation(m_shaderProgram, "fogColour");

	m_locLighting		= glGetUniformLocation(m_shaderProgram, "lighting");
	m_locLightEnable	= glGetUniformLocation(m_shaderProgram, "lightEnable");
	m_locShininess		= glGetUniformLocation(m_shaderProgram, "shininess");
	m_locSpecCoefficient= glGetUniformLocation(m_shaderProgram, "specularCoefficient");
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

	if (m_dirtyMesh || m->shininess != m_shininess) {
		glUniform1f(m_locShininess, (m->shininess + 1) * 4);
		m_shininess = m->shininess;
	}

	if (m_dirtyMesh || m->specularCoefficient != m_specularCoefficient) {
		glUniform1f(m_locSpecCoefficient, m->specularCoefficient);
		m_specularCoefficient = m->specularCoefficient;
	}

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

	printf("fog %f %f %f %f %f\n", vp->fogParams[0], vp->fogParams[1], vp->fogParams[2], vp->fogParams[3], vp->fogParams[4]);
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
