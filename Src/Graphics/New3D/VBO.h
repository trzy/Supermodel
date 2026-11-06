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

#ifndef _VBO_H_
#define _VBO_H_

#include <GL/glew.h>

class VBO
{
public:
	VBO();

	void Create			(GLenum target, GLenum usage, GLsizeiptr size, const void* data=nullptr);
	void BufferSubData	(GLintptr offset, GLsizeiptr size, const GLvoid* data);
	bool AppendData		(GLsizeiptr size, const GLvoid* data);
	void Reset			();		// don't delete data, just go back to start
	void Destroy		();
	void Bind			(bool enable);
	int  GetSize		() const;
	int  GetCapacity	() const;

private:
	GLuint		m_id;
	GLenum		m_target;
	int			m_capacity;
	int			m_size;
};

#endif
