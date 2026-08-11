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
	SuperAA(int aaValue, CRTcolor CRTcolors, int crtMode, float crtStrength);
	~SuperAA();

	void Init(int width, int height);		// width & height are real window dimensions
	void Draw();							// no-op when AA, CRT color correction, and CRT display effects are all disabled
	void SetViewport(unsigned x, unsigned y, unsigned width, unsigned height);
	void SetCurvature(bool enable);

	GLuint GetTargetID();

private:
	FBO m_fbo;
	FBO m_history[2];
	FBO m_blurHorizontal;
	FBO m_blurVertical;
	GLSLShader m_sourceShader;
	GLSLShader m_blurShader;
	GLSLShader m_crtShader;
	const int m_aa;
	const CRTcolor m_crtcolors;
	const int m_crtMode;
	const float m_crtStrength;
	GLuint m_vao;
	int m_width;
	int m_height;
	unsigned m_viewportX;
	unsigned m_viewportY;
	unsigned m_viewportWidth;
	unsigned m_viewportHeight;
	unsigned m_historyIndex;
	bool m_resetHistory;
	bool m_curvatureEnabled;

	bool IsEnabled() const;
	void ClearIntermediateTargets();
};
