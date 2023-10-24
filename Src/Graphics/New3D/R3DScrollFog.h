#ifndef _R3DSCROLLFOG_H_
#define _R3DSCROLLFOG_H_

#include "Util/NewConfig.h"
#include <GL/glew.h>

namespace New3D {

class R3DScrollFog
{
public:

	R3DScrollFog(const Util::Config::Node &config);
	~R3DScrollFog();

	void DrawScrollFog(float rbga[4], float attenuation, float ambient);

private:

	void AllocResources();
	void DeallocResources();

  const Util::Config::Node &m_config;

	GLuint m_shaderProgram;
	GLuint m_vertexShader;
	GLuint m_fragmentShader;

	GLint m_locFogColour;
	GLint m_locFogAttenuation;
	GLint m_locFogAmbient;

	GLuint m_vao;
};

}

#endif