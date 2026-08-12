#ifndef FBO_H
#define FBO_H

#include "Graphics/GL.h"
#include "GLSLShader.h"
#include "Model.h"

namespace New3D {

class R3DFrameBuffers {

public:
	explicit R3DFrameBuffers(bool scaledShadersRequested = false);
	~R3DFrameBuffers();

	void	Draw();					// draw and composite the transparent layers
	void	DrawScaled(int x, int y, int width, int height);

	Result	CreateFBO(int width, int height);
	void	DestroyFBO();

	bool	IsReady() const { return m_shaderReady; }
	void	BindTexture(Layer layer);
	void	SetFBO(Layer layer);
	void	StoreDepth();
	void	RestoreDepth();
#ifdef SUPERMODEL_GLES
	void	PrepareLos();
	void	SetLosFlagWrite(bool enable);
	void	ReadLos(int x, int y, float& depth, bool& noLosReturn);
#endif

private:

	Result	CreateFBODepthCopy(int width, int height);
	GLuint	CreateTexture(int width, int height);
	bool	AllocShaderTrans();
	bool	AllocShaderBase();
	bool	AllocShaderTransScaled();
	bool	AllocShaderBaseScaled();

	void	DrawBaseLayer();
	void	DrawAlphaLayer();
	void	DrawBaseLayerScaled();
	void	DrawAlphaLayerScaled();

	GLuint m_frameBufferID;
	GLuint m_renderBufferID;
	GLuint m_texIDs[3];
#ifdef SUPERMODEL_GLES
	GLuint m_losTextureID;
#endif
	GLuint m_frameBufferIDCopy;
	GLuint m_renderBufferIDCopy;
	Layer m_lastLayer;
	bool m_shaderReady;
#ifdef SUPERMODEL_GLES
	bool m_losFlagWriteValid;
	bool m_losFlagWriteEnabled;
#endif
	int m_width;
	int m_height;

	// shaders
	GLSLShader m_shaderBase;
	GLSLShader m_shaderTrans;
	GLSLShader m_shaderBaseScaled;
	GLSLShader m_shaderTransScaled;

	// vao
	GLuint m_vao;	// this really needed if we don't actually use vertex attribs?
};

}

#endif
