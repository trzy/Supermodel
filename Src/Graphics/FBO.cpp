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

#include "FBO.h"

FBO::FBO() :
	m_frameBufferID(0),
	m_textureID(0)
{
}

bool FBO::Create(int width, int height)
{
	CreateTexture(width, height);

	glGenFramebuffers(1, &m_frameBufferID);
	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureID, 0);
	
	auto frameBufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);	//created FBO now disable it

	return frameBufferStatus == GL_FRAMEBUFFER_COMPLETE;
}

void FBO::Destroy()
{
	if (m_frameBufferID) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &m_frameBufferID);
	}

	if (m_textureID) {
		glDeleteTextures(1, &m_textureID);
	}

	m_frameBufferID = 0;
	m_textureID = 0;
}

void FBO::BindTexture()
{
	glBindTexture(GL_TEXTURE_2D, m_textureID);
}

void FBO::Set()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
}

void FBO::Disable()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint FBO::GetFBOID()
{
	return m_frameBufferID;
}

GLuint FBO::GetTextureID()
{
	return m_textureID;
}

void FBO::CreateTexture(int width, int height)
{
	glGenTextures	(1, &m_textureID);
	glBindTexture	(GL_TEXTURE_2D, m_textureID);
	glTexParameteri	(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri	(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri	(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri	(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D	(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
}
