#ifndef _R3DSHADER_H_
#define _R3DSHADER_H_

#include "Pkgs/glew.h"
#include "Util/NewConfig.h"
#include "Model.h"

namespace New3D {

class R3DShader
{
public:
	R3DShader(const Util::Config::Node &config);

	bool	LoadShader			(const char* vertexShader = nullptr, const char* fragmentShader = nullptr);
	void	SetMeshUniforms		(const Mesh* m);
	void	SetModelStates		(const Model* model);
	void	SetViewportUniforms	(const Viewport *vp);
	void	Start				();
	void	SetShader			(bool enable = true);
	GLint	GetVertexAttribPos	(const char* attrib);

private:

	// run-time config
	const Util::Config::Node &m_config;

	// shader IDs
	GLuint m_shaderProgram;
	GLuint m_vertexShader;
	GLuint m_fragmentShader;

	// mesh uniform locations
	GLint m_locTexture1;
	GLint m_locTexture2;
	GLint m_locTexture1Enabled;
	GLint m_locTexture2Enabled;
	GLint m_locTextureAlpha;
	GLint m_locAlphaTest;
	GLint m_locMicroTexScale;
	GLint m_locBaseTexSize;
	GLint m_locTextureInverted;

	// cached mesh values
	bool	m_textured1;
	bool	m_textured2;
	bool	m_textureAlpha;		// use alpha in texture
	bool	m_alphaTest;		// discard fragment based on alpha (ogl does this with fixed function)
	float	m_fogIntensity;
	bool	m_doubleSided;
	bool	m_lightEnabled;
	float	m_shininess;
	float	m_specularValue;
	bool	m_specularEnabled;

	bool	m_layered;
	float	m_microTexScale;
	float	m_baseTexSize[2];
	bool	m_textureInverted;
	
	// cached model values
	enum class MatDet { notset, negative, positive, zero };
	MatDet	m_matDet;
	float	m_modelScale;

	// are our cache values dirty
	bool	m_dirtyMesh;
	bool	m_dirtyModel;

	// viewport uniform locations
	GLint m_locFogIntensity;
	GLint m_locFogDensity;
	GLint m_locFogStart;
	GLint m_locFogColour;
	GLint m_locFogAttenuation;
	GLint m_locFogAmbient;

	// lighting / other
	GLint m_locLighting;
	GLint m_locLightEnabled;
	GLint m_locSunClamp;
	GLint m_locIntensityClamp;
	GLint m_locShininess;
	GLint m_locSpecularValue;
	GLint m_locSpecularEnabled;

	GLint m_locSpotEllipse;
	GLint m_locSpotRange;
	GLint m_locSpotColor;
	GLint m_locSpotFogColor;
	
	// model uniforms
	GLint m_locModelScale;
};

} // New3D

#endif