#include "VBO.h"

namespace New3D {

VBO::VBO()
{
	m_id = 0;
	m_target = 0;
}

void VBO::Create(GLenum target, GLenum usage, GLsizeiptr size, const void* data)
{
	glGenBuffers(1, &m_id);							// create a vbo
	glBindBuffer(target, m_id);						// activate vbo id to use
	glBufferData(target, size, data, usage);		// upload data to video card

	m_target = target;

	Bind(false);		// unbind
}

void VBO::BufferSubData(GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
	glBufferSubData(m_target, offset, size, data);
}

void VBO::Destroy()
{
	if (m_id) {
		glDeleteBuffers(1, &m_id);
		m_id = 0;
		m_target = 0;
	}
}

void VBO::Bind(bool enable)
{
	if (enable) {
		glBindBuffer(m_target, m_id);
	}
	else {
		glBindBuffer(m_target, 0);
	}
}

} // New3D
