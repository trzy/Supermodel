#ifndef _GLSLSHADER_H_
#define _GLSLSHADER_H_

#include <GL/glew.h>

class GLSLShader {

public:
	GLSLShader();
   ~GLSLShader();

	bool LoadShaders(const char *vertexShader, const char *fragmentShader);
	void UnloadShaders();

	void EnableShader();
	void DisableShader();

	int GetUniformLocation(const char *str);
	int GetAttributeLocation(const char *str);

	int uniformLoc[64];
	int attribLoc[16];

private:

	void PrintShaderInfoLog(GLuint obj);
	void PrintProgramInfoLog(GLuint obj);

	GLuint m_program;
	GLuint m_vShader;
	GLuint m_fShader;
};

#endif