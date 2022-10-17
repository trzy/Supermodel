#ifndef _GLSLSHADER_H_
#define _GLSLSHADER_H_

#include <GL/glew.h>
#include <map>
#include <cstring>

class GLSLShader {

public:
	GLSLShader();
	~GLSLShader();

	GLSLShader(GLSLShader&& other) noexcept;
	GLSLShader(const GLSLShader&) = delete;
	GLSLShader& operator=(const GLSLShader&) = delete;
	GLSLShader& operator=(GLSLShader&& other) noexcept;

	bool LoadShaders(const char* vertexShader, const char* fragmentShader);
	void UnloadShaders();

	void EnableShader();
	void DisableShader();

	int GetUniformLocation(const char* str);
	void GetUniformLocationMap(const char* str);
	int GetAttributeLocation(const char* str);
	void GetAttributeLocationMap(const char* str);

	int uniformLoc[64];
	int attribLoc[16];

private:

	struct cmp_str
	{
		bool operator()(char const* a, char const* b) const
		{
			return std::strcmp(a, b) < 0;
		}
	};

public:

	std::map<const char*, int, cmp_str> uniformLocMap;
	std::map<const char*, int, cmp_str> attribLocMap;

private:

	void Reset();

	void PrintShaderInfoLog(GLuint obj);
	void PrintProgramInfoLog(GLuint obj);

	GLuint m_program;
	GLuint m_vShader;
	GLuint m_fShader;
};


#endif