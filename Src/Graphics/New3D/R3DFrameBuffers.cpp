#include "R3DFrameBuffers.h"
#include "Mat4.h"

#define countof(a) (sizeof(a)/sizeof(*(a)))

namespace New3D {

R3DFrameBuffers::R3DFrameBuffers()
{
	m_frameBufferID = 0;
	m_renderBufferID = 0;
	m_frameBufferIDCopy = 0;
	m_renderBufferIDCopy = 0;
	m_width = 0;
	m_height = 0;

	for (auto &i : m_texIDs) {
		i = 0;
	}

	m_lastLayer = Layer::none;

	AllocShaderTrans();
	AllocShaderBase();
	AllocShaderWipe();

	FBVertex vertices[4];
	vertices[0].Set(-1,-1, 0, 0);
	vertices[1].Set(-1, 1, 0, 1);
	vertices[2].Set( 1,-1, 1, 0);
	vertices[3].Set( 1, 1, 1, 1);

	m_vbo.Create(GL_ARRAY_BUFFER, GL_STATIC_DRAW, sizeof(vertices), vertices);
}

R3DFrameBuffers::~R3DFrameBuffers()
{
	DestroyFBO();
	m_shaderTrans.UnloadShaders();
	m_shaderBase.UnloadShaders();
	m_shaderWipe.UnloadShaders();
	m_vbo.Destroy();
}

bool R3DFrameBuffers::CreateFBO(int width, int height)
{
	m_width = width;
	m_height = height;

	m_texIDs[0] = CreateTexture(width, height);		// colour buffer
	m_texIDs[1] = CreateTexture(width, height);		// trans layer1
	m_texIDs[2] = CreateTexture(width, height);		// trans layer2

	glGenFramebuffers(1, &m_frameBufferID);
	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);

	// colour attachments
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texIDs[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_texIDs[1], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_texIDs[2], 0);

	// depth/stencil attachment
	glGenRenderbuffers(1, &m_renderBufferID);
	glBindRenderbuffer(GL_RENDERBUFFER, m_renderBufferID);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_renderBufferID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderBufferID);

	// check setup was successful
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);	//created R3DFrameBuffers now disable it

	CreateFBODepthCopy(width, height);

	return (fboStatus == GL_FRAMEBUFFER_COMPLETE);
}

bool R3DFrameBuffers::CreateFBODepthCopy(int width, int height)
{
	glGenFramebuffers(1, &m_frameBufferIDCopy);
	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferIDCopy);

	glGenRenderbuffers(1, &m_renderBufferIDCopy);
	glBindRenderbuffer(GL_RENDERBUFFER, m_renderBufferIDCopy);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_renderBufferIDCopy);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderBufferIDCopy);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// check setup was successful
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	return (fboStatus == GL_FRAMEBUFFER_COMPLETE);
}

void R3DFrameBuffers::StoreDepth()
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_frameBufferID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_frameBufferIDCopy);
	glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
}

void R3DFrameBuffers::RestoreDepth()
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_frameBufferIDCopy);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_frameBufferID);
	glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
}

void R3DFrameBuffers::DestroyFBO()
{
	if (m_frameBufferID) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteRenderbuffers(1, &m_renderBufferID);
		glDeleteFramebuffers(1, &m_frameBufferID);
	}

	if (m_frameBufferIDCopy) {
		glDeleteRenderbuffers(1, &m_renderBufferIDCopy);
		glDeleteFramebuffers(1, &m_frameBufferIDCopy);
	}

	for (auto &i : m_texIDs) {
		if (i) {
			glDeleteTextures(1, &i);
			i = 0;
		}
	}

	m_frameBufferID = 0;
	m_renderBufferID = 0;
	m_frameBufferIDCopy = 0;
	m_renderBufferIDCopy = 0;
	m_width = 0;
	m_height = 0;
}

GLuint R3DFrameBuffers::CreateTexture(int width, int height)
{
	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	return texId;
}

void R3DFrameBuffers::BindTexture(Layer layer)
{
	glBindTexture(GL_TEXTURE_2D, m_texIDs[(int)layer]);
}

void R3DFrameBuffers::SetFBO(Layer layer)
{
	if (m_lastLayer == layer) {
		return;
	}

	switch (layer)
	{
	case Layer::colour:
	case Layer::trans1:
	case Layer::trans2:
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
		GLenum buffers[] = { GL_COLOR_ATTACHMENT0 + (GLenum)layer };
		glDrawBuffers(countof(buffers), buffers);
		break;
	}
	case Layer::trans12:
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
		GLenum buffers[] = { GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		glDrawBuffers(countof(buffers), buffers);
		break;
	}
	case Layer::all:
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
		GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		glDrawBuffers(countof(buffers), buffers);
		break;
	}
	case Layer::none:
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDrawBuffer(GL_BACK);
		break;
	}
	}

	m_lastLayer = layer;
}

void R3DFrameBuffers::AllocShaderBase()
{
	const char *vertexShader = R"glsl(

	// inputs
	attribute vec3 inVertex; 
	attribute vec2 inTexCoord;

	// outputs
	varying vec2 fsTexCoord;

	void main(void)
	{
		fsTexCoord = inTexCoord;
		gl_Position = vec4(inVertex,1.0);
	}

	)glsl";

	const char *fragmentShader = R"glsl(

	uniform sampler2D tex1;			// base tex

	varying vec2 fsTexCoord;

	void main()
	{
		vec4 colBase = texture2D( tex1, fsTexCoord);
		if(colBase.a < 1.0) discard;
		gl_FragColor = colBase;
	}

	)glsl";

	m_shaderBase.LoadShaders(vertexShader, fragmentShader);
	m_shaderBase.uniformLoc[0] = m_shaderTrans.GetUniformLocation("tex1");
	m_shaderBase.attribLoc[0] = m_shaderTrans.GetAttributeLocation("inVertex");
	m_shaderBase.attribLoc[1] = m_shaderTrans.GetAttributeLocation("inTexCoord");
}

void R3DFrameBuffers::AllocShaderTrans()
{
	const char *vertexShader = R"glsl(

	// inputs
	attribute vec3 inVertex; 
	attribute vec2 inTexCoord;

	// outputs
	varying vec2 fsTexCoord;

	void main(void)
	{
		fsTexCoord = inTexCoord;
		gl_Position = vec4(inVertex,1.0);
	}

	)glsl";

	const char *fragmentShader = R"glsl(

	uniform sampler2D tex1;			// trans layer 1
	uniform sampler2D tex2;			// trans layer 2

	varying vec2 fsTexCoord;

	void main()
	{
		vec4 colTrans1 = texture2D( tex1, fsTexCoord);
		vec4 colTrans2 = texture2D( tex2, fsTexCoord);

		if(colTrans1.a+colTrans2.a > 0.0) {
			vec3 col1 = (colTrans1.rgb * colTrans1.a) / ( colTrans1.a + colTrans2.a);		// this is my best guess at the blending between the layers
			vec3 col2 = (colTrans2.rgb * colTrans2.a) / ( colTrans1.a + colTrans2.a);

			colTrans1 = vec4(col1+col2,colTrans1.a+colTrans2.a);
		}
		
		gl_FragColor = colTrans1;
	}

	)glsl";

	m_shaderTrans.LoadShaders(vertexShader, fragmentShader);

	m_shaderTrans.uniformLoc[0] = m_shaderTrans.GetUniformLocation("tex1");
	m_shaderTrans.uniformLoc[1] = m_shaderTrans.GetUniformLocation("tex2");

	m_shaderTrans.attribLoc[0] = m_shaderTrans.GetAttributeLocation("inVertex");
	m_shaderTrans.attribLoc[1] = m_shaderTrans.GetAttributeLocation("inTexCoord");
}

void R3DFrameBuffers::AllocShaderWipe()
{
	const char *vertexShader = R"glsl(

	// inputs
	attribute vec3 inVertex; 
	attribute vec2 inTexCoord;

	// outputs
	varying vec2 fsTexCoord;

	void main(void)
	{
		fsTexCoord = inTexCoord;
		gl_Position = vec4(inVertex,1.0);
	}

	)glsl";

	const char *fragmentShader = R"glsl(

	uniform sampler2D texColor;				// base colour layer
	varying vec2 fsTexCoord;

	void main()
	{
		vec4 colBase = texture2D( texColor, fsTexCoord);

		if(colBase.a == 0.0) {
			discard;						// no colour pixels have been written
		}

		gl_FragData[0] = vec4(0.0);			// wipe these parts of the alpha buffer
		gl_FragData[1] = vec4(0.0);			// since they have been overwritten by the next priority layer
	}

	)glsl";

	m_shaderWipe.LoadShaders(vertexShader, fragmentShader);

	m_shaderWipe.uniformLoc[0] = m_shaderTrans.GetUniformLocation("texColor");

	m_shaderWipe.attribLoc[0] = m_shaderTrans.GetAttributeLocation("inVertex");
	m_shaderWipe.attribLoc[1] = m_shaderTrans.GetAttributeLocation("inTexCoord");
}

void R3DFrameBuffers::Draw()
{
	SetFBO		(Layer::none);						// make sure to draw on the back buffer
	glViewport	(0, 0, m_width, m_height);			// cover the entire screen
	glDisable	(GL_DEPTH_TEST);					// disable depth testing / writing
	glDisable	(GL_CULL_FACE);
	glDisable	(GL_BLEND);

	for (int i = 0; i < countof(m_texIDs); i++) {	// bind our textures to correct texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_texIDs[i]);
	}

	glActiveTexture	(GL_TEXTURE0);
	m_vbo.Bind		(true);

	DrawBaseLayer	();

	glBlendFunc		(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable		(GL_BLEND);

	DrawAlphaLayer	();

	glDisable		(GL_BLEND);
	m_vbo.Bind		(false);
}

void R3DFrameBuffers::CompositeBaseLayer()
{
	SetFBO(Layer::none);							// make sure to draw on the back buffer
	glViewport(0, 0, m_width, m_height);			// cover the entire screen
	glDisable(GL_DEPTH_TEST);						// disable depth testing / writing
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	for (int i = 0; i < countof(m_texIDs); i++) {	// bind our textures to correct texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_texIDs[i]);
	}

	glActiveTexture(GL_TEXTURE0);
	m_vbo.Bind(true);

	DrawBaseLayer();

	m_vbo.Bind(false);
}

void R3DFrameBuffers::CompositeAlphaLayer()
{
	SetFBO(Layer::none);							// make sure to draw on the back buffer
	glViewport(0, 0, m_width, m_height);			// cover the entire screen
	glDisable(GL_DEPTH_TEST);						// disable depth testing / writing
	glDisable(GL_CULL_FACE);

	for (int i = 0; i < countof(m_texIDs); i++) {	// bind our textures to correct texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_texIDs[i]);
	}

	glActiveTexture(GL_TEXTURE0);
	m_vbo.Bind(true);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	DrawAlphaLayer();

	glDisable(GL_BLEND);
	m_vbo.Bind(false);
}

void R3DFrameBuffers::DrawOverTransLayers()
{
	SetFBO(Layer::trans12);							// need to write to both layers

	glViewport	(0, 0, m_width, m_height);			// cover the entire screen
	glDisable	(GL_DEPTH_TEST);					// disable depth testing / writing
	glDisable	(GL_CULL_FACE);
	glDisable	(GL_BLEND);

	glActiveTexture	(GL_TEXTURE0);
	glBindTexture	(GL_TEXTURE_2D, m_texIDs[0]);

	m_vbo.Bind(true);

	m_shaderWipe.EnableShader();
	glUniform1i(m_shaderWipe.uniformLoc[0], 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(m_shaderWipe.attribLoc[0], 3, GL_FLOAT, GL_FALSE, sizeof(FBVertex), (void*)offsetof(FBVertex, verts));
		glVertexAttribPointer(m_shaderWipe.attribLoc[1], 2, GL_FLOAT, GL_FALSE, sizeof(FBVertex), (void*)offsetof(FBVertex, texCoords));

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);

	m_shaderWipe.DisableShader();

	m_vbo.Bind(false);
}

void R3DFrameBuffers::DrawBaseLayer()
{
	m_shaderBase.EnableShader();
	glUniform1i(m_shaderTrans.uniformLoc[0], 0);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(m_shaderTrans.attribLoc[0], 3, GL_FLOAT, GL_FALSE, sizeof(FBVertex), (void*)offsetof(FBVertex, verts));
	glVertexAttribPointer(m_shaderTrans.attribLoc[1], 2, GL_FLOAT, GL_FALSE, sizeof(FBVertex), (void*)offsetof(FBVertex, texCoords));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	m_shaderBase.DisableShader();
}

void R3DFrameBuffers::DrawAlphaLayer()
{
	m_shaderTrans.EnableShader();
	glUniform1i(m_shaderTrans.uniformLoc[0], 1);		// tex unit 1
	glUniform1i(m_shaderTrans.uniformLoc[1], 2);		// tex unit 2

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(m_shaderTrans.attribLoc[0], 3, GL_FLOAT, GL_FALSE, sizeof(FBVertex), (void*)offsetof(FBVertex, verts));
	glVertexAttribPointer(m_shaderTrans.attribLoc[1], 2, GL_FLOAT, GL_FALSE, sizeof(FBVertex), (void*)offsetof(FBVertex, texCoords));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	m_shaderTrans.DisableShader();
}

}