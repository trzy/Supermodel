/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2012 Bart Trzynadlowski, Nik Henson 
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
 
/*
 * Shaders2D.h
 * 
 * Header file containing the 2D vertex and fragment shaders.
 */

#ifndef INCLUDED_SHADERS2D_H
#define INCLUDED_SHADERS2D_H

// Vertex shader
static const char s_vertexShaderSource[] = R"glsl(

	#version 410 core

	// outputs
	out vec2 fsTexCoord;

	void main(void)
	{
		const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
										vec4(-1.0,  1.0, 0.0, 1.0),
										vec4( 1.0, -1.0, 0.0, 1.0),
										vec4( 1.0,  1.0, 0.0, 1.0));

		fsTexCoord = ((vertices[gl_VertexID % 4].xy + 1.0) / 2.0);
		fsTexCoord.y = 1.0 - fsTexCoord.y;							// flip upside down
		gl_Position = vertices[gl_VertexID % 4];	
	}

	)glsl";


// Fragment shader
static const char s_fragmentShaderSource[] = R"glsl(

	#version 410 core

	// inputs
	uniform sampler2D tex1;			// texture
	in vec2 fsTexCoord;

	// outputs
	out vec4 fragColor;

	void main()
	{
		fragColor = texture(tex1, fsTexCoord);
	}

	)glsl";


#endif	// INCLUDED_SHADERS2D_H
