#ifndef _R3DSCROLLFOG_H_
#define _R3DSCROLLFOG_H_

#include "Util/NewConfig.h"
#include "VBO.h"

namespace New3D {

class R3DScrollFog
{
public:

	R3DScrollFog(const Util::Config::Node &config);
	~R3DScrollFog();

	void DrawScrollFog(float rbga[4], float attenuation, float ambient, float *spotRGB, float *spotEllipse);

private:

	void AllocResources();
	void DeallocResources();

  const Util::Config::Node &m_config;

	struct SFVertex
	{
		void Set(float x, float y, float z) {
			v[0] = x;
			v[1] = y;
			v[2] = z;
		}

		float v[3];
	};

	struct SFTriangle
	{
		SFVertex p1;
		SFVertex p2;
		SFVertex p3;
	};

	SFTriangle m_triangles[2];

	GLuint m_shaderProgram;
	GLuint m_vertexShader;
	GLuint m_fragmentShader;

	GLint m_locFogColour;
	GLint m_locMVP;
	GLint m_locFogAttenuation;
	GLint m_locFogAmbient;
	GLint m_locSpotFogColor;
	GLint m_locSpotEllipse;

	// vertex attrib locs
	GLint m_locInVertex;

	VBO m_vbo;
};

}

#endif