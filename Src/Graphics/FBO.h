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