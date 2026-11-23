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

#include "R3DFrameBuffers.h"
#include <initializer_list>
#include <iterator>

namespace New3D {

R3DFrameBuffers::R3DFrameBuffers()
{
	m_frameBufferID = 0;
	m_renderBufferID = 0;
	m_colourTexID = 0;
	m_markTexID = 0;
	m_width = 0;
	m_height = 0;
	m_vao = 0;

	AllocShader();

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	// no states needed since we do it in the shader
	glBindVertexArray(0);
}

R3DFrameBuffers::~R3DFrameBuffers()
{
	DestroyFBO();
	m_shader.UnloadShaders();

	if (m_vao) {
		glDeleteVertexArrays(1, &m_vao);
		m_vao = 0;
	}
}

Result R3DFrameBuffers::CreateFBO(int width, int height)
{
	m_width = width;
	m_height = height;

	m_colourTexID	= CreateTexture(width, height, GL_RGBA8);
	m_markTexID		= CreateTexture(width, height, GL_RGBA8);	// r: translucent, gb: the edge (EdgeInfo())

	glGenFramebuffers(1, &m_frameBufferID);
	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);

	// colour attachments: the shaders' outputs 0 and 1
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colourTexID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_markTexID, 0);

	// depth/stencil attachment
	glGenRenderbuffers(1, &m_renderBufferID);
	glBindRenderbuffer(GL_RENDERBUFFER, m_renderBufferID);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderBufferID);

	// check setup was successful (while bound)
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return (fboStatus == GL_FRAMEBUFFER_COMPLETE) ? Result::OKAY : Result::FAIL;
}

void R3DFrameBuffers::DestroyFBO()
{
	if (m_frameBufferID) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteRenderbuffers(1, &m_renderBufferID);
		glDeleteFramebuffers(1, &m_frameBufferID);
	}

	for (GLuint *tex : { &m_colourTexID, &m_markTexID }) {
		if (*tex) {
			glDeleteTextures(1, tex);
			*tex = 0;
		}
	}

	m_frameBufferID = 0;
	m_renderBufferID = 0;
	m_width = 0;
	m_height = 0;
}

GLuint R3DFrameBuffers::CreateTexture(int width, int height, GLint format)
{
	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format == GL_R8 ? GL_RED : GL_RGBA, GL_UNSIGNED_BYTE, 0);
	return texId;
}

void R3DFrameBuffers::Bind(bool marker)
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
	GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_NONE };
	if (marker) {
		buffers[1] = GL_COLOR_ATTACHMENT1;
	}
	glDrawBuffers((GLsizei)std::size(buffers), buffers);
}

void R3DFrameBuffers::Unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDrawBuffer(GL_BACK);
}

// The Real3D draws the translucent pixels on half of a checkerboard (layer 0
// where x+y is even, layer 1 where it is odd; see WriteOutputs() in
// R3DShaderCommon.h), and its anti-aliasing filter blends each with what is
// next to it, which is what is behind it. In a checkerboard a pixel's four
// neighbours (left, right, up, down) are all on the other half, so here:
//   - a translucent pixel (marked) is drawn over the average of its neighbours,
//     which are what is behind it
//   - but next to another translucent pixel (where layers 0 and 1 overlap) the
//     two are mixed, opaque, as the hardware does
//   - a pixel that isn't translucent with two or more translucent neighbours is
//     behind them (inside their polygon, even at its edge): their average is
//     drawn over it. With only one it is beside the polygon, and left alone.
// Otherwise the pixel is drawn as it is.
void R3DFrameBuffers::AllocShader()
{
	static const char *vertexShader = R"glsl(

	#version 410 core

	void main(void)
	{
		const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
										vec4(-1.0,  1.0, 0.0, 1.0),
										vec4( 1.0, -1.0, 0.0, 1.0),
										vec4( 1.0,  1.0, 0.0, 1.0));

		gl_Position = vertices[gl_VertexID % 4];
	}

	)glsl";

	static const char *fragmentShader = R"glsl(

	#version 410 core

	uniform sampler2D colourTex;	// the frame
	uniform sampler2D markTex;		// r = 1: a translucent pixel; g, b: its polygon's edge (EdgeInfo() in R3DShaderQuads.h)

	out vec4 fragColor;

	// f over b (not premultiplied)
	vec4 Over(vec4 f, vec4 b)
	{
		float a = f.a + b.a * (1.0 - f.a);
		if (a <= 0.0) {
			return vec4(0.0);
		}
		return vec4((f.rgb * f.a + b.rgb * b.a * (1.0 - f.a)) / a, a);
	}

	// a and b mixed (b by w), weighted by their alpha: an edge against nothing
	// fades out instead of towards black
	vec4 Mix(vec4 a, vec4 b, float w)
	{
		float alpha = mix(a.a, b.a, w);
		if (alpha <= 0.0) {
			return vec4(0.0);
		}
		return vec4((a.rgb * a.a * (1.0 - w) + b.rgb * b.a * w) / alpha, alpha);
	}

	// A pixel's edge: the neighbour it points to (dir) and its distance (in pixels), or dir 0
	void Edge(vec4 mark, out ivec2 dir, out float dist)
	{
		dir = ivec2(0);
		dist = 1.0;
		if (mark.r > 0.5 || mark.g < 0.25) {
			return;					// translucent, or no edge near
		}
		float off = mark.b * 2.0 - 1.0;
		int sgn = off < 0.0 ? -1 : 1;
		dir = mark.g < 0.75 ? ivec2(sgn, 0) : ivec2(0, sgn);
		dist = abs(off);
	}

	void main()
	{
		ivec2 p = ivec2(gl_FragCoord.xy);
		ivec2 size = textureSize(colourTex, 0);
		vec4 cp = texelFetch(colourTex, p, 0);
		bool tp = texelFetch(markTex, p, 0).r > 0.5;

		// the neighbours on the screen: the translucent ones and the others
		const ivec2 offsets[4] = ivec2[](ivec2(-1, 0), ivec2(1, 0), ivec2(0, -1), ivec2(0, 1));
		vec4 translucent = vec4(0.0), other = vec4(0.0);
		int numTranslucent = 0, numOther = 0;
		for (int i = 0; i < 4; i++) {
			ivec2 n = p + offsets[i];
			if (any(lessThan(n, ivec2(0))) || any(greaterThanEqual(n, size))) {
				continue;
			}
			vec4 c = texelFetch(colourTex, n, 0);
			if (texelFetch(markTex, n, 0).r > 0.5) {
				translucent += c;
				numTranslucent++;
			}
			else {
				other += c;
				numOther++;
			}
		}

		vec4 res;
		if (tp) {
			if (numTranslucent == 0) {
				res = Over(cp, other / float(max(numOther, 1)));		// over what is behind it
			}
			else {
				// next to a translucent pixel (layers 0 and 1 overlap): opaque, mixed
				vec4 n = translucent / float(numTranslucent);
				bool pIsLayer0 = ((p.x + p.y) & 1) == 0;
				vec4 l0 = pIsLayer0 ? cp : n;
				vec4 l1 = pIsLayer0 ? n : cp;
				res = vec4(mix(l0.rgb, l1.rgb, (l1.a + (1.0 - l0.a)) / 2.0), 1.0);
			}
		}
		else if (numTranslucent >= 2) {
			res = Over(translucent / float(numTranslucent), cp);		// behind translucent pixels
		}
		else {
			res = cp;												// nothing translucent here
		}

		// a translucent pixel on a diagonal: the checkerboard's other half, so with
		// one of the four neighbours it says translucency is next to this pixel
		bool diagTranslucent = false;
		const ivec2 diagonals[4] = ivec2[](ivec2(-1, -1), ivec2(1, -1), ivec2(-1, 1), ivec2(1, 1));
		for (int i = 0; i < 4; i++) {
			ivec2 n = p + diagonals[i];
			if (all(greaterThanEqual(n, ivec2(0))) && all(lessThan(n, size)) && texelFetch(markTex, n, 0).r > 0.5) {
				diagTranslucent = true;
			}
		}

		// Edge anti-aliasing, as the Real3D: a polygon's pixel whose edge is
		// under half a pixel away (dist d) only covers 0.5 + d of it, the rest is
		// the neighbour across the edge; and an edge between half a pixel and one
		// pixel from a neighbour's centre covers d - 0.5 of this pixel.
		// None next to translucency (any of the 8 pixels around translucent): on
		// the hardware an opaque polygon's edge in front of a translucent one
		// isn't anti-aliased, and else the surface behind a translucent polygon
		// would show its edges through it
		if (!tp && numTranslucent == 0 && !diagTranslucent) {
			ivec2 dir;
			float d;
			Edge(texelFetch(markTex, p, 0), dir, d);
			ivec2 n = p + dir;
			if (dir != ivec2(0) && d < 0.5 && all(greaterThanEqual(n, ivec2(0))) && all(lessThan(n, size)) && texelFetch(markTex, n, 0).r < 0.5) {
				res = Mix(res, texelFetch(colourTex, n, 0), 0.5 - d);
			}
			for (int i = 0; i < 4; i++) {
				ivec2 m = p + offsets[i];
				if (any(lessThan(m, ivec2(0))) || any(greaterThanEqual(m, size))) {
					continue;
				}
				ivec2 mdir;
				float md;
				Edge(texelFetch(markTex, m, 0), mdir, md);
				if (m + mdir == p && md >= 0.5) {
					res = Mix(res, texelFetch(colourTex, m, 0), md - 0.5);
				}
			}
		}

		fragColor = res;
	}

	)glsl";

	m_shader.LoadShaders(vertexShader, fragmentShader);
	m_shader.uniformLoc[0] = m_shader.GetUniformLocation("colourTex");
	m_shader.uniformLoc[1] = m_shader.GetUniformLocation("markTex");
}

void R3DFrameBuffers::Draw()
{
	glViewport	(0, 0, m_width, m_height);			// cover the entire screen
	glDisable	(GL_DEPTH_TEST);
	glDisable	(GL_CULL_FACE);
	glBlendFunc	(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	// over the 2D layers behind
	glEnable	(GL_BLEND);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_colourTexID);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_markTexID);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(m_vao);

	m_shader.EnableShader();
	glUniform1i(m_shader.uniformLoc[0], 0);
	glUniform1i(m_shader.uniformLoc[1], 1);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	m_shader.DisableShader();

	glDisable			(GL_BLEND);
	glBindVertexArray	(0);
}

}
