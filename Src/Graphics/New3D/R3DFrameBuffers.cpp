#include "R3DFrameBuffers.h"
#include "OSD/Logger.h"
#include <cstring>

#ifdef SUPERMODEL_GLES
namespace {

const char *GetFramebufferStatusName(GLenum status)
{
	switch (status) {
	case GL_FRAMEBUFFER_COMPLETE: return "complete";
	case GL_FRAMEBUFFER_UNDEFINED: return "undefined";
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "incomplete attachment";
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "missing attachment";
	case GL_FRAMEBUFFER_UNSUPPORTED: return "unsupported";
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: return "incomplete multisample";
	default: return "unknown";
	}
}

const char *GetGLErrorName(GLenum error)
{
	switch (error) {
	case GL_NO_ERROR: return "no error";
	case GL_INVALID_ENUM: return "invalid enum";
	case GL_INVALID_VALUE: return "invalid value";
	case GL_INVALID_OPERATION: return "invalid operation";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "invalid framebuffer operation";
	case GL_OUT_OF_MEMORY: return "out of memory";
	default: return "unknown";
	}
}

void LogFramebufferSetupError(const char *name, GLenum status, GLenum error, int width, int height)
{
	if (status == GL_FRAMEBUFFER_COMPLETE && error == GL_NO_ERROR) {
		return;
	}
	ErrorLog("New3D %s framebuffer setup failed: status=0x%04X (%s), GL error=0x%04X (%s), size=%dx%d.",
		name, static_cast<unsigned>(status), GetFramebufferStatusName(status),
		static_cast<unsigned>(error), GetGLErrorName(error), width, height);
}

} // anonymous namespace
#endif

namespace New3D {

R3DFrameBuffers::R3DFrameBuffers(bool scaledShadersRequested)
{
	m_frameBufferID = 0;
	m_renderBufferID = 0;
	m_frameBufferIDCopy = 0;
	m_renderBufferIDCopy = 0;
	m_width = 0;
	m_height = 0;
	m_vao = 0;
#ifdef SUPERMODEL_GLES
	m_losTextureID = 0;
#endif

	for (auto &i : m_texIDs) {
		i = 0;
	}

	m_lastLayer = Layer::none;
#ifdef SUPERMODEL_GLES
	m_losFlagWriteValid = false;
	m_losFlagWriteEnabled = false;
#endif

	const bool transShaderReady = AllocShaderTrans();
	const bool baseShaderReady = AllocShaderBase();
	const bool scaledTransShaderReady = !scaledShadersRequested || AllocShaderTransScaled();
	const bool scaledBaseShaderReady = !scaledShadersRequested || AllocShaderBaseScaled();
	m_shaderReady = transShaderReady && baseShaderReady && scaledTransShaderReady && scaledBaseShaderReady;
	if (!m_shaderReady) {
		return;
	}

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	// no states needed since we do it in the shader
	glBindVertexArray(0);
}

R3DFrameBuffers::~R3DFrameBuffers()
{
	DestroyFBO();
	m_shaderTrans.UnloadShaders();
	m_shaderBase.UnloadShaders();
	m_shaderTransScaled.UnloadShaders();
	m_shaderBaseScaled.UnloadShaders();

	if (m_vao) {
		glDeleteVertexArrays(1, &m_vao);
		m_vao = 0;
	}
}

Result R3DFrameBuffers::CreateFBO(int width, int height)
{
	if (!m_shaderReady) {
		return Result::FAIL;
	}

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
#ifdef SUPERMODEL_GLES
	glGenTextures(1, &m_losTextureID);
	glBindTexture(GL_TEXTURE_2D, m_losTextureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, width, height, 0, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_losTextureID, 0);
#endif

	// depth/stencil attachment
	glGenRenderbuffers(1, &m_renderBufferID);
	glBindRenderbuffer(GL_RENDERBUFFER, m_renderBufferID);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderBufferID);

	// check setup was successful
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
#ifdef SUPERMODEL_GLES
	const GLenum glError = glGetError();
	LogFramebufferSetupError("primary", fboStatus, glError, width, height);
#endif

	glBindFramebuffer(GL_FRAMEBUFFER, 0);	//created R3DFrameBuffers now disable it

	return ((CreateFBODepthCopy(width, height) == Result::OKAY) && (fboStatus == GL_FRAMEBUFFER_COMPLETE)) ? Result::OKAY : Result::FAIL;
}

Result R3DFrameBuffers::CreateFBODepthCopy(int width, int height)
{
	glGenFramebuffers(1, &m_frameBufferIDCopy);
	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferIDCopy);

	glGenRenderbuffers(1, &m_renderBufferIDCopy);
	glBindRenderbuffer(GL_RENDERBUFFER, m_renderBufferIDCopy);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderBufferIDCopy);

	const GLenum noDrawBuffer = GL_NONE;
	glDrawBuffers(1, &noDrawBuffer);
	glReadBuffer(GL_NONE);

	// check setup was successful
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
#ifdef SUPERMODEL_GLES
	const GLenum glError = glGetError();
	LogFramebufferSetupError("depth-copy", fboStatus, glError, width, height);
#endif
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return (fboStatus == GL_FRAMEBUFFER_COMPLETE) ? Result::OKAY : Result::FAIL;
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
#ifdef SUPERMODEL_GLES
	if (m_losTextureID) {
		glDeleteTextures(1, &m_losTextureID);
		m_losTextureID = 0;
	}
#endif

	m_frameBufferID = 0;
	m_renderBufferID = 0;
	m_frameBufferIDCopy = 0;
	m_renderBufferIDCopy = 0;
	m_width = 0;
	m_height = 0;
	m_lastLayer = Layer::none;
#ifdef SUPERMODEL_GLES
	m_losFlagWriteValid = false;
#endif
}

GLuint R3DFrameBuffers::CreateTexture(int width, int height)
{
	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
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
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
#ifdef SUPERMODEL_GLES
		GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
#else
		GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
#endif
		glDrawBuffers((GLsizei)std::size(buffers), buffers);
		break;
	}
	case Layer::trans1:
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
		GLenum buffers[] = { GL_NONE, GL_COLOR_ATTACHMENT1, GL_NONE };
		glDrawBuffers((GLsizei)std::size(buffers), buffers);
		break;
	}
	case Layer::trans2:
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
		GLenum buffers[] = { GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT2 };
		glDrawBuffers((GLsizei)std::size(buffers), buffers);
		break;
	}
	case Layer::none:
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
#ifdef SUPERMODEL_GLES
		const GLenum buffer = GL_BACK;
		glDrawBuffers(1, &buffer);
#else
		glDrawBuffer(GL_BACK);
#endif
		break;
	}
	default:
	{
		break;
	}
	}

#ifdef SUPERMODEL_GLES
	m_losFlagWriteValid = false;
#endif
	m_lastLayer = layer;
}

#ifdef SUPERMODEL_GLES
void R3DFrameBuffers::PrepareLos()
{
	const GLuint clearValue[4] = { 0, 0, 0, 0 };
	glColorMaski(3, GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE);
	glClearBufferuiv(GL_COLOR, 3, clearValue);
	m_losFlagWriteEnabled = true;
	m_losFlagWriteValid = true;
}

void R3DFrameBuffers::SetLosFlagWrite(bool enable)
{
	if (m_losFlagWriteValid && m_losFlagWriteEnabled == enable) {
		return;
	}

	// Layered polygons must still mirror their depth into R, but preserve the
	// no-LOS-return flag in G just as the desktop stencil mask does.
	glColorMaski(3, GL_TRUE, enable ? GL_TRUE : GL_FALSE, GL_FALSE, GL_FALSE);
	m_losFlagWriteEnabled = enable;
	m_losFlagWriteValid = true;
}

void R3DFrameBuffers::ReadLos(int x, int y, float& depth, bool& noLosReturn)
{
	GLuint losData[4] = { 0, 0, 0, 0 };
	glReadBuffer(GL_COLOR_ATTACHMENT3);
	glReadPixels(x, y, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, losData);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	static_assert(sizeof(depth) == sizeof(losData[0]), "LOS depth storage must match float size");
	std::memcpy(&depth, &losData[0], sizeof(depth));
	noLosReturn = losData[1] != 0;
}
#endif

bool R3DFrameBuffers::AllocShaderBase()
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

	// inputs
	uniform sampler2D tex1;			// base tex

	// outputs
	out vec4 fragColor;

	void main()
	{
		ivec2 tc = ivec2(gl_FragCoord.xy /*-vec2(0.5)*/);
		fragColor = texelFetch(tex1, tc, 0);
	}

	)glsl";

	if (!m_shaderBase.LoadShaders(vertexShader, fragmentShader)) {
		return false;
	}
	m_shaderBase.uniformLoc[0] = m_shaderBase.GetUniformLocation("tex1");
	return true;
}

bool R3DFrameBuffers::AllocShaderTrans()
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

	uniform sampler2D tex1;			// trans layer 1
	uniform sampler2D tex2;			// trans layer 2

	// outputs
	out vec4 fragColor;

	void main()
	{
		ivec2 tc = ivec2(gl_FragCoord.xy /*-vec2(0.5)*/);
		vec4 colTrans1 = texelFetch(tex1, tc, 0);
		vec4 colTrans2 = texelFetch(tex2, tc, 0);

		// if both transparency layers overlap, the result is opaque
		if (colTrans1.a * colTrans2.a > 0.0) {
			vec3 mixCol = mix(colTrans1.rgb, colTrans2.rgb, (colTrans2.a + (1.0 - colTrans1.a)) / 2.0);
			fragColor = vec4(mixCol, 1.0);
		}
		else if (colTrans1.a > 0.0) {
			fragColor = colTrans1;
		}
		else {
			fragColor = colTrans2;		// if alpha is zero it will have no effect anyway
		}
	}

	)glsl";

	if (!m_shaderTrans.LoadShaders(vertexShader, fragmentShader)) {
		return false;
	}

	m_shaderTrans.uniformLoc[0] = m_shaderTrans.GetUniformLocation("tex1");
	m_shaderTrans.uniformLoc[1] = m_shaderTrans.GetUniformLocation("tex2");
	return true;
}

bool R3DFrameBuffers::AllocShaderBaseScaled()
{
	static const char *vertexShader = R"glsl(

	#version 410 core

	out vec2 texCoord;

	void main(void)
	{
		const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
										vec4(-1.0,  1.0, 0.0, 1.0),
										vec4( 1.0, -1.0, 0.0, 1.0),
										vec4( 1.0,  1.0, 0.0, 1.0));
		const vec2 texCoords[] = vec2[](vec2(0.0, 0.0),
										 vec2(0.0, 1.0),
										 vec2(1.0, 0.0),
										 vec2(1.0, 1.0));

		gl_Position = vertices[gl_VertexID % 4];
		texCoord = texCoords[gl_VertexID % 4];
	}

	)glsl";

	static const char *fragmentShader = R"glsl(

	#version 410 core

	in vec2 texCoord;
	uniform sampler2D tex1;
	uniform ivec2 sourceSize;
	out vec4 fragColor;

	void main()
	{
		ivec2 tc = clamp(ivec2(texCoord * vec2(sourceSize)), ivec2(0), sourceSize - ivec2(1));
		fragColor = texelFetch(tex1, tc, 0);
	}

	)glsl";

	if (!m_shaderBaseScaled.LoadShaders(vertexShader, fragmentShader)) {
		return false;
	}
	m_shaderBaseScaled.uniformLoc[0] = m_shaderBaseScaled.GetUniformLocation("tex1");
	m_shaderBaseScaled.uniformLoc[1] = m_shaderBaseScaled.GetUniformLocation("sourceSize");
	return true;
}

bool R3DFrameBuffers::AllocShaderTransScaled()
{
	static const char *vertexShader = R"glsl(

	#version 410 core

	out vec2 texCoord;

	void main(void)
	{
		const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
										vec4(-1.0,  1.0, 0.0, 1.0),
										vec4( 1.0, -1.0, 0.0, 1.0),
										vec4( 1.0,  1.0, 0.0, 1.0));
		const vec2 texCoords[] = vec2[](vec2(0.0, 0.0),
										 vec2(0.0, 1.0),
										 vec2(1.0, 0.0),
										 vec2(1.0, 1.0));

		gl_Position = vertices[gl_VertexID % 4];
		texCoord = texCoords[gl_VertexID % 4];
	}

	)glsl";

	static const char *fragmentShader = R"glsl(

	#version 410 core

	in vec2 texCoord;
	uniform sampler2D tex1;
	uniform sampler2D tex2;
	uniform ivec2 sourceSize;
	out vec4 fragColor;

	void main()
	{
		ivec2 tc = clamp(ivec2(texCoord * vec2(sourceSize)), ivec2(0), sourceSize - ivec2(1));
		vec4 colTrans1 = texelFetch(tex1, tc, 0);
		vec4 colTrans2 = texelFetch(tex2, tc, 0);

		if (colTrans1.a * colTrans2.a > 0.0) {
			vec3 mixCol = mix(colTrans1.rgb, colTrans2.rgb, (colTrans2.a + (1.0 - colTrans1.a)) / 2.0);
			fragColor = vec4(mixCol, 1.0);
		}
		else if (colTrans1.a > 0.0) {
			fragColor = colTrans1;
		}
		else {
			fragColor = colTrans2;
		}
	}

	)glsl";

	if (!m_shaderTransScaled.LoadShaders(vertexShader, fragmentShader)) {
		return false;
	}
	m_shaderTransScaled.uniformLoc[0] = m_shaderTransScaled.GetUniformLocation("tex1");
	m_shaderTransScaled.uniformLoc[1] = m_shaderTransScaled.GetUniformLocation("tex2");
	m_shaderTransScaled.uniformLoc[2] = m_shaderTransScaled.GetUniformLocation("sourceSize");
	return true;
}

void R3DFrameBuffers::Draw()
{
	glViewport	(0, 0, m_width, m_height);			// cover the entire screen
	glDisable	(GL_DEPTH_TEST);					// disable depth testing / writing
	glDisable	(GL_CULL_FACE);
	glBlendFunc	(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable	(GL_BLEND);

	for (int i = 0; i < (int)std::size(m_texIDs); i++) {	// bind our textures to correct texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_texIDs[i]);
	}

	glActiveTexture		(GL_TEXTURE0);
	glBindVertexArray	(m_vao);

	DrawBaseLayer		();
	DrawAlphaLayer		();

	glDisable			(GL_BLEND);
	glBindVertexArray	(0);
}

void R3DFrameBuffers::DrawBaseLayer()
{
	m_shaderBase.EnableShader();
	glUniform1i(m_shaderBase.uniformLoc[0], 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	m_shaderBase.DisableShader();
}

void R3DFrameBuffers::DrawAlphaLayer()
{
	m_shaderTrans.EnableShader();
	glUniform1i(m_shaderTrans.uniformLoc[0], 1);		// tex unit 1
	glUniform1i(m_shaderTrans.uniformLoc[1], 2);		// tex unit 2

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	m_shaderTrans.DisableShader();
}

void R3DFrameBuffers::DrawScaled(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	for (int i = 0; i < (int)std::size(m_texIDs); i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_texIDs[i]);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(m_vao);

	DrawBaseLayerScaled();
	DrawAlphaLayerScaled();

	glDisable(GL_BLEND);
	glBindVertexArray(0);
}

void R3DFrameBuffers::DrawBaseLayerScaled()
{
	m_shaderBaseScaled.EnableShader();
	glUniform1i(m_shaderBaseScaled.uniformLoc[0], 0);
	glUniform2i(m_shaderBaseScaled.uniformLoc[1], m_width, m_height);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	m_shaderBaseScaled.DisableShader();
}

void R3DFrameBuffers::DrawAlphaLayerScaled()
{
	m_shaderTransScaled.EnableShader();
	glUniform1i(m_shaderTransScaled.uniformLoc[0], 1);
	glUniform1i(m_shaderTransScaled.uniformLoc[1], 2);
	glUniform2i(m_shaderTransScaled.uniformLoc[2], m_width, m_height);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	m_shaderTransScaled.DisableShader();
}

}
