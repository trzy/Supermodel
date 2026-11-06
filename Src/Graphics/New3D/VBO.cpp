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

#include "VBO.h"

VBO::VBO()
{
	m_id		= 0;
	m_target	= 0;
	m_capacity	= 0;
	m_size		= 0;
}

void VBO::Create(GLenum target, GLenum usage, GLsizeiptr size, const void* data)
{
	glGenBuffers(1, &m_id);							// create a vbo
	glBindBuffer(target, m_id);						// activate vbo id to use
	glBufferData(target, size, data, usage);		// upload data to video card

	m_target	= target;
	m_capacity	= (int)size;
	m_size		= 0;

	Bind(false);		// unbind
}

void VBO::BufferSubData(GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
	glBufferSubData(m_target, offset, size, data);
}

bool VBO::AppendData(GLsizeiptr size, const GLvoid* data)
{
	if (size == 0 || !data) {
		return true;	// nothing to do
	}

	if (m_size + size >= m_capacity) {
		return false;
	}

	BufferSubData(m_size, size, data);

	m_size += (int)size;

	return true;
}

void VBO::Reset()
{
	m_size = 0;
}

void VBO::Destroy()
{
	if (m_id) {
		glDeleteBuffers(1, &m_id);
		m_id		= 0;
		m_target	= 0;
		m_capacity	= 0;
		m_size		= 0;
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

int VBO::GetSize() const
{
	return m_size;
}

int VBO::GetCapacity() const
{
	return m_capacity;
}
