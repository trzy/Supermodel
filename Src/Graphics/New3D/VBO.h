#ifndef _VBO_H_
#define _VBO_H_

#include "Pkgs/glew.h"

namespace New3D {

class VBO
{
public:
	VBO();

	void Create			(GLenum target, GLenum usage, GLsizeiptr size, const void* data=nullptr);
	void BufferSubData	(GLintptr offset, GLsizeiptr size, const GLvoid* data);
	void Destroy		();
	void Bind			(bool enable);

private:
	GLuint m_id;
	GLenum m_target;
};

} // New3D

#endif
