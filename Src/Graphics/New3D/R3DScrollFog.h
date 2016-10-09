#ifndef _R3DSCROLLFOG_H_
#define _R3DSCROLLFOG_H_

#include "VBO.h"

namespace New3D {

class R3DScrollFog
{
public:

	R3DScrollFog();
	~R3DScrollFog();

	void DrawScrollFog(float r, float g, float b, float a);

private:

	void AllocResources();
	void DeallocResources();

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

	GLuint m_locFogColour;
	GLuint m_locMVP;

	VBO m_vbo;
};

}

#endif