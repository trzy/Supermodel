#include "GLSLShader.h"
#include <cstdio>

GLSLShader::GLSLShader() {

	m_vShader	= 0;
	m_fShader	= 0;
	m_program	= 0;

	for (auto &i : uniformLoc) {
		i = 0;
	}

	for (auto &i : attribLoc) {
		i = 0;
	}
}

GLSLShader::~GLSLShader() {

	UnloadShaders();
}

bool GLSLShader::LoadShaders(const char *vertexShader, const char *fragmentShader) {

	m_program = glCreateProgram();
	m_vShader = glCreateShader(GL_VERTEX_SHADER);
	m_fShader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(m_vShader, 1, &vertexShader, NULL);
	glShaderSource(m_fShader, 1, &fragmentShader, NULL);

	glCompileShader(m_vShader);
	glCompileShader(m_fShader);

	glAttachShader(m_program, m_vShader);
	glAttachShader(m_program, m_fShader);

	glLinkProgram(m_program);

	PrintShaderInfoLog(m_vShader);
	PrintShaderInfoLog(m_fShader);
	PrintProgramInfoLog(m_program);

	return true;
}

void GLSLShader::UnloadShaders()
{
	if (m_program) {
		glDeleteShader(m_vShader);
		glDeleteShader(m_fShader);
		glDeleteProgram(m_program);
	}

	m_vShader = 0;
	m_fShader = 0;
	m_program = 0;
}

void GLSLShader::EnableShader() {

	glUseProgram(m_program);
}

void GLSLShader::DisableShader() {

	glUseProgram(0);
}

void GLSLShader::PrintShaderInfoLog(GLuint obj) {

	int infologLength = 0;
	int charsWritten = 0;
	
	glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

	if (infologLength > 0) {
		char *infoLog = new char[infologLength];
		glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
		printf("%s\n", infoLog);
		delete[] infoLog;
	}
}

void GLSLShader::PrintProgramInfoLog(GLuint obj) {

	int infologLength = 0;
	int charsWritten = 0;

	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

	if (infologLength > 0) {
		char *infoLog = new char[infologLength];
		glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
		printf("%s\n", infoLog);
		delete[] infoLog;
	}
}

int GLSLShader::GetUniformLocation(const char *str)
{
	return glGetUniformLocation(m_program, str);
}

int GLSLShader::GetAttributeLocation(const char *str)
{
	return glGetAttribLocation(m_program, str);
}
