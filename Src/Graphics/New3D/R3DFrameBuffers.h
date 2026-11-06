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

#ifndef FBO_H
#define FBO_H

#include <GL/glew.h>
#include "GLSLShader.h"
#include "Model.h"

namespace New3D {

class R3DFrameBuffers {

public:
	R3DFrameBuffers();
	~R3DFrameBuffers();

	void	Draw();					// draw and composite the transparent layers
	
	Result	CreateFBO(int width, int height);
	void	DestroyFBO();

	void	BindTexture(Layer layer);
	void	SetFBO(Layer layer);
	void	StoreDepth();
	void	RestoreDepth();

private:

	Result	CreateFBODepthCopy(int width, int height);
	GLuint	CreateTexture(int width, int height);
	void	AllocShaderTrans();
	void	AllocShaderBase();

	void	DrawBaseLayer();
	void	DrawAlphaLayer();

	GLuint m_frameBufferID;
	GLuint m_renderBufferID;
	GLuint m_texIDs[3];
	GLuint m_frameBufferIDCopy;
	GLuint m_renderBufferIDCopy;
	Layer m_lastLayer;
	int m_width;
	int m_height;

	// shaders
	GLSLShader m_shaderBase;
	GLSLShader m_shaderTrans;

	// vao
	GLuint m_vao;	// this really needed if we don't actually use vertex attribs?
};

}

#endif
