#include "GLSLShader.h"
#include "Graphics/GLSL.h"
#include <cstdio>

GLSLShader::GLSLShader() 
{
	Reset();
}

GLSLShader::~GLSLShader() 
{
	UnloadShaders();
}

GLSLShader::GLSLShader(GLSLShader&& other) noexcept
{
	*this = std::move(other);
}

GLSLShader& GLSLShader::operator=(GLSLShader&& other) noexcept
{
	if (this != &other) {
		m_program = other.m_program;
		m_vShader = other.m_vShader;
		m_fShader = other.m_fShader;
		uniformLocMap = other.uniformLocMap;
		attribLocMap = other.attribLocMap;

		std::copy(std::begin(other.uniformLoc), std::end(other.uniformLoc), std::begin(uniformLoc));
		std::copy(std::begin(other.attribLoc), std::end(other.attribLoc), std::begin(attribLoc));

		other.Reset();
	}

	return *this;
}

bool GLSLShader::LoadShaders(const char* vertexShader, const char* fragmentShader) 
{
	UnloadShaders();

	const std::string vertexSource = GLSL::PrepareSource(vertexShader, GLSL::ShaderStage::Vertex);
	const std::string fragmentSource = GLSL::PrepareSource(fragmentShader, GLSL::ShaderStage::Fragment);
	const char *vertexSourcePtr = vertexSource.c_str();
	const char *fragmentSourcePtr = fragmentSource.c_str();

	m_program = glCreateProgram();
	m_vShader = glCreateShader(GL_VERTEX_SHADER);
	m_fShader = glCreateShader(GL_FRAGMENT_SHADER);
	if (!m_program || !m_vShader || !m_fShader) {
		UnloadShaders();
		return false;
	}

	glShaderSource(m_vShader, 1, &vertexSourcePtr, NULL);
	glShaderSource(m_fShader, 1, &fragmentSourcePtr, NULL);
	glCompileShader(m_vShader);
	glCompileShader(m_fShader);

	PrintShaderInfoLog(m_vShader);
	PrintShaderInfoLog(m_fShader);

	GLint vertexCompiled = GL_FALSE;
	GLint fragmentCompiled = GL_FALSE;
	glGetShaderiv(m_vShader, GL_COMPILE_STATUS, &vertexCompiled);
	glGetShaderiv(m_fShader, GL_COMPILE_STATUS, &fragmentCompiled);
	if (vertexCompiled != GL_TRUE || fragmentCompiled != GL_TRUE) {
		UnloadShaders();
		return false;
	}

	glAttachShader(m_program, m_vShader);
	glAttachShader(m_program, m_fShader);
	glLinkProgram(m_program);
	PrintProgramInfoLog(m_program);

	GLint linked = GL_FALSE;
	glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE) {
		UnloadShaders();
		return false;
	}

	return true;
}

void GLSLShader::UnloadShaders()
{
	if (m_vShader) {
		glDeleteShader(m_vShader);
	}
	if (m_fShader) {
		glDeleteShader(m_fShader);
	}
	if (m_program) {
		glDeleteProgram(m_program);
	}

	m_vShader = 0;
	m_fShader = 0;
	m_program = 0;
}

void GLSLShader::EnableShader() 
{
	glUseProgram(m_program);
}

void GLSLShader::DisableShader() 
{
	glUseProgram(0);
}

void GLSLShader::Reset()
{
	m_vShader = 0;
	m_fShader = 0;
	m_program = 0;

	for (auto& i : uniformLoc) {
		i = -1;
	}

	for (auto& i : attribLoc) {
		i = -1;
	}

	uniformLocMap.clear();
	attribLocMap.clear();
}

void GLSLShader::PrintShaderInfoLog(GLuint obj) 
{
	int infologLength = 0;
	int charsWritten = 0;

	glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

	if (infologLength > 0) {
		char* infoLog = new char[infologLength];
		glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
		printf("%s\n", infoLog);
		delete[] infoLog;
	}
}

void GLSLShader::PrintProgramInfoLog(GLuint obj) 
{
	int infologLength = 0;
	int charsWritten = 0;

	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

	if (infologLength > 0) {
		char* infoLog = new char[infologLength];
		glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
		printf("%s\n", infoLog);
		delete[] infoLog;
	}
}

int GLSLShader::GetUniformLocation(const char* str)
{
	return glGetUniformLocation(m_program, str);
}

void GLSLShader::GetUniformLocationMap(const char* str)
{
	uniformLocMap[str] = GetUniformLocation(str);
}

int GLSLShader::GetAttributeLocation(const char* str)
{
	return glGetAttribLocation(m_program, str);
}

void GLSLShader::GetAttributeLocationMap(const char* str)
{
	attribLocMap[str] = GetAttributeLocation(str);
}
