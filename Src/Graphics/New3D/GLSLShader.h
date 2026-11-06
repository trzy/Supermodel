/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

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