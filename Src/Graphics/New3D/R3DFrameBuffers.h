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

/*
 * The frame the 3D scene is drawn in, as the Real3D's frame buffer: the colour,
 * where translucent polygons are on half of a checkerboard (see WriteOutputs()
 * in R3DShaderCommon.h), and a marker of the translucent pixels. Draw()
 * blends them as the Real3D's anti-aliasing filter does.
 */
class R3DFrameBuffers {

public:
	R3DFrameBuffers();
	~R3DFrameBuffers();

	Result	CreateFBO(int width, int height);
	void	DestroyFBO();

	// Draws to our frame: the colour, and with marker the translucent marker
	// (the polygons; the fog layers only write the colour)
	void	Bind(bool marker);
	void	Unbind();

	// Draws the frame to the bound frame buffer, blending the translucent pixels
	void	Draw();

private:

	GLuint	CreateTexture(int width, int height, GLint format);
	void	AllocShader();

	GLuint m_frameBufferID;
	GLuint m_renderBufferID;
	GLuint m_colourTexID;		// the frame
	GLuint m_markTexID;			// r = 1: a translucent pixel
	int m_width;
	int m_height;

	GLSLShader m_shader;

	// vao
	GLuint m_vao;	// this really needed if we don't actually use vertex attribs?
};

}

#endif
