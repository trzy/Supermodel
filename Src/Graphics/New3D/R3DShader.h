#ifndef _R3DSHADER_H_
#define _R3DSHADER_H_

#include "Pkgs/glew.h"
#include "Model.h"

namespace New3D {

class R3DShader
{
public:
	R3DShader();

	bool LoadShader(const char* vertexShader = nullptr, const char* fragmentShader = nullptr);
	void SetMeshUniforms(const Mesh* m);
	void SetViewportUniforms(const Viewport *vp);
	void Start();
	void SetShader(bool enable = true);

private:

	GLuint m_shaderProgram;
	GLuint m_vertexShader;
	GLuint m_fragmentShader;

	// mesh uniform data
	GLint m_locTexture;
	GLint m_locTextureEnabled;
	GLint m_locTextureAlpha;
	GLint m_locAlphaTest;

	bool m_textured;
	bool m_textureAlpha;	// use alpha in texture
	bool m_alphaTest;		// discard fragment based on alpha (ogl does this with fixed function)
	float m_fogIntensity;
	bool m_doubleSided;

	bool m_dirty;

	// viewport uniform data
	GLint m_locFogIntensity;
	GLint m_locFogDensity;
	GLint m_locFogStart;
	GLint m_locFogColour;

	// lighting
	GLint m_locLighting;
	GLint m_locLightEnable;
	GLint m_locShininess;
	GLint m_locSpotEllipse;
	GLint m_locSpotRange;
	GLint m_locSpotColor;
};

} // New3D

#endif