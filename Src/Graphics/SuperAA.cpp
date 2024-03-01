#include "SuperAA.h"
#include <string>

SuperAA::SuperAA(int aaValue) :
	m_aa(aaValue),
	m_vao(0),
	m_width(0),
	m_height(0)
{
	if (aaValue > 1) {

		const char* vertexShader = R"glsl(

			#version 410 core

			// outputs
			out vec2 fsTexCoord;

			void main(void)
			{
				const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
												vec4(-1.0,  1.0, 0.0, 1.0),
												vec4( 1.0, -1.0, 0.0, 1.0),
												vec4( 1.0,  1.0, 0.0, 1.0));

				fsTexCoord = (vertices[gl_VertexID % 4].xy + 1.0) / 2.0;
				gl_Position = vertices[gl_VertexID % 4];	
			}

			)glsl";


		std::string fragmentShaderVersion = R"glsl(
			#version 410 core

		)glsl";

		std::string aaString = "const int aa = ";
		aaString += std::to_string(aaValue);
		aaString += ";\n";

		std::string fragmentShader = R"glsl(

		// inputs
		uniform sampler2D tex1;			// base tex
		in vec2 fsTexCoord;

		// outputs
		out vec4 fragColor;

		vec4 GetTextureValue(sampler2D s, vec2 texCoord)
		{
			vec2 size		= vec2(textureSize(s,0));        // want the values as floats
			ivec2 texPos	= ivec2(size * texCoord);
			vec4 texColour	= vec4(0.0);

			for(int i=0; i < aa; i++) {
				for(int j=0; j < aa; j++) {
					texColour += texelFetch(s,ivec2(texPos.x+i,texPos.y+j),0);
				}
			}

			texColour /= float(aa * aa);

			return texColour;
		}

		void main()
		{
			fragColor = GetTextureValue(tex1, fsTexCoord);
		}

		)glsl";

		std::string fragmentShaderString = fragmentShaderVersion + aaString + fragmentShader;
		
		// load shaders
		m_shader.LoadShaders(vertexShader, fragmentShaderString.c_str());
		m_shader.GetUniformLocationMap("tex1");

		// setup uniform memory
		m_shader.EnableShader();
		glUniform1i(m_shader.attribLocMap["tex1"], 0);		// texture will be bound to unit zero
		m_shader.DisableShader();

		glGenVertexArrays(1, &m_vao);
		glBindVertexArray(m_vao);
		// no states needed since we do it in the shader
		glBindVertexArray(0);
	}
}

// need an active context bound to the current thread to destroy our objects
SuperAA::~SuperAA()
{
	m_shader.UnloadShaders();
	m_fbo.Destroy();

	if (m_vao) {
		glDeleteVertexArrays(1, &m_vao);
		m_vao = 0;
	}
}

void SuperAA::Init(int width, int height)
{
	if (m_aa > 1) {
		m_fbo.Destroy();
		m_fbo.Create(width * m_aa, height * m_aa);

		m_width = width;
		m_height = height;
	}
}

void SuperAA::Draw()
{
	if (m_aa > 1) {
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_BLEND);

		glBindTexture(GL_TEXTURE_2D, m_fbo.GetTextureID());
		glBindVertexArray(m_vao);
		glViewport(0, 0, m_width, m_height);
		m_shader.EnableShader();
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		m_shader.DisableShader();
		glBindVertexArray(0);
	}
}

GLuint SuperAA::GetTargetID()
{
	return m_fbo.GetFBOID();	// will return 0 if no render target which will be our default frame buffer (back buffer)
}
