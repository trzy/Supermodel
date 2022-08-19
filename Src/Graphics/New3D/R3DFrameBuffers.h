#ifndef FBO_H
#define FBO_H

#include <GL/glew.h>
#include "VBO.h"
#include "GLSLShader.h"
#include "Model.h"

namespace New3D {

class R3DFrameBuffers {

public:
	R3DFrameBuffers();
	~R3DFrameBuffers();

	void	Draw();					// draw and composite the transparent layers
	void	CompositeBaseLayer();
	void	CompositeAlphaLayer();
	void	DrawOverTransLayers();	// opaque pixels in next priority layer need to wipe trans pixels
	
	bool	CreateFBO(int width, int height);
	void	DestroyFBO();

	void	BindTexture(Layer layer);
	void	SetFBO(Layer layer);
	void	StoreDepth();
	void	RestoreDepth();

private:

	struct FBVertex
	{
		void Set(float x, float y, float s, float t)
		{
			texCoords[0] = s;
			texCoords[1] = t;
			verts[0] = x;
			verts[1] = y;
			verts[2] = 0.f;	// z = 0
		}

		float texCoords[2];
		float verts[3];
	};

	bool	CreateFBODepthCopy(int width, int height);
	GLuint	CreateTexture(int width, int height);
	void	AllocShaderTrans();
	void	AllocShaderBase();
	void	AllocShaderWipe();

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
	GLSLShader m_shaderWipe;

	// vertices for fbo
	VBO m_vbo;
};

}

#endif
