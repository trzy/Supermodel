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
static constexpr char s_vertexShader[] = R"glsl(

	#version 410 core

	// outputs
	out vec2 fsTexCoord;

	void main(void)
	{
		const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
										vec4(-1.0,  1.0, 0.0, 1.0),
										vec4( 1.0, -1.0, 0.0, 1.0),
										vec4( 1.0,  1.0, 0.0, 1.0));

		const vec2 texCoords[] = vec2[](vec2(0.0, 1.0), // flip upside down
										vec2(0.0, 0.0),
										vec2(1.0, 1.0),
										vec2(1.0, 0.0));

		fsTexCoord = texCoords[gl_VertexID % 4];
		gl_Position = vertices[gl_VertexID % 4];
	}

	)glsl";


// Fragment shader
static constexpr char s_fragmentShaderHeader[] = R"glsl(

	#version 410 core

	)glsl";

static constexpr char s_fragmentShader[] = R"glsl(

	// inputs
	uniform sampler2D tex1;			// texture
	in vec2 fsTexCoord;

	// outputs
	out vec4 fragColor;

	#if (UPSCALEMODE == 1)
	// smoothstep instead of linear, remapping to a bilinear lookup
	vec4 biquintic(sampler2D sampler, vec2 uv)
	{
		vec2 ts = vec2(textureSize(sampler,0));
		uv = uv*ts + 0.5;

		vec2 i = floor(uv);
		vec2 f = uv - i;
		f = f*f*f*(f*(f*6.0-15.0)+10.0) - 0.5;

		uv = (i + f)/ts;
		return texture(sampler, uv);
	}
	#endif

	#if (UPSCALEMODE == 3)
	// 4x4 bicubic filter using 4 bilinear texture lookups
	vec4 cubic(float v) {
		vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
		vec4 s = n * n * n;
		float x = s.x;
		float y = s.y - 4.0 * s.x;
		float z = s.z - 4.0 * s.y + 6.0 * s.x;
		float w = 6.0 - x - y - z;
		return vec4(x, y, z, w) * (1.0 / 6.0);
	}
	vec4 bicubic(sampler2D sampler, vec2 uv) {
		vec2 invTexSize = 1.0 / vec2(textureSize(sampler,0));
		uv = uv * vec2(textureSize(sampler,0)) - 0.5;
		vec2 uvi = floor(uv);
		vec2 fxy = uv-uvi;
		vec4 xcubic = cubic(fxy.x);
		vec4 ycubic = cubic(fxy.y);
		vec4 c = uvi.xxyy + vec2(-0.5, 1.5).xyxy;
		vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
		vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;
		offset *= invTexSize.xxyy;
		vec4 sample0 = texture(sampler, offset.xz);
		vec4 sample1 = texture(sampler, offset.yz);
		vec4 sample2 = texture(sampler, offset.xw);
		vec4 sample3 = texture(sampler, offset.yw);
		float sx = s.x / (s.x + s.y);
		float sy = s.z / (s.z + s.w);
		return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
	}
	#endif

	void main()
	{
		#if (UPSCALEMODE == 3)
		fragColor = bicubic(tex1, fsTexCoord);
		#elif (UPSCALEMODE == 1)
		fragColor = vec4(biquintic(tex1, fsTexCoord).rgb, texture(tex1, fsTexCoord).a);
		#else
		fragColor = texture(tex1, fsTexCoord);
		#endif
	}

	)glsl";

#endif	// INCLUDED_SHADERS2D_H
