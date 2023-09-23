#ifndef _FBO_H_
#define _FBO_H_

#include <GL/glew.h>

class FBO
{
public:

	FBO();

	bool	Create(int width, int height);
	void	Destroy();
	void	BindTexture();
	void	Set();
	void	Disable();
	GLuint	GetFBOID();
	GLuint	GetTextureID();

private:

	void	CreateTexture(int width, int height);

	GLuint m_frameBufferID;
	GLuint m_textureID;
};

#endif