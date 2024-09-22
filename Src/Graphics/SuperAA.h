#pragma once

#include "Supermodel.h"
#include "FBO.h"
#include "New3D/GLSLShader.h"

// This class just implements super sampling. Super sampling looks fantastic but is quite expensive.
// 8x and beyond values can start to eat ridiculous amounts of memory / gpu time, for less and less noticable returns
// 4x works and looks great
// values such as 3 are also possible, that works out 9 samples per pixel
// The algorithm is super simple, just add up all samples and divide by the number

class SuperAA
{
public:
	SuperAA(int aaValue, CRTcolor CRTcolors);
	~SuperAA();

	void Init(int width, int height);		// width & height are real window dimensions
	void Draw();							// this is a no-op if AA is 1 and CRTcolors 0, since we'll be drawing straight on the back buffer anyway

	GLuint GetTargetID();

private:
	FBO m_fbo;
	GLSLShader m_shader;
	const int m_aa;
	const CRTcolor m_crtcolors;
	GLuint m_vao;
	int m_width;
	int m_height;
};
