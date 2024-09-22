#include "SuperAA.h"
#include <string>

SuperAA::SuperAA(int aaValue, CRTcolor CRTcolors) :
	m_aa(aaValue),
	m_crtcolors(CRTcolors),
	m_vao(0),
	m_width(0),
	m_height(0)
{
	if ((m_aa > 1) || (m_crtcolors != CRTcolor::None)) {

		static const char* vertexShader = R"glsl(

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


		static const std::string fragmentShaderVersion = R"glsl(
			#version 410 core

		)glsl";

		std::string aaString = "const int aa = ";
		aaString += std::to_string(m_aa);
		aaString += ";\n";
		std::string ccString = "#define CRTCOLORS ";
		ccString += std::to_string((int)m_crtcolors);
		ccString += '\n';

		static const std::string fragmentShader = R"glsl(

		// inputs
		uniform sampler2D tex1;			// base tex

		// outputs
		out vec4 fragColor;

		#if (CRTCOLORS == 1)
		const float cgamma = 2.5; // what is the 'real' gamma/EOTF of a japanese arcade CRT of that time? 2.4 or 2.5 is what the net so far agrees on
		#elif (CRTCOLORS == 2)
		const float cgamma = 2.25;
		#elif (CRTCOLORS > 2)
		const float cgamma = -1.0;
		#endif

		// see https://www.retrorgb.com/colour-malarkey.html
		// and accompanying matrices https://github.com/danmons/colour_matrix_adaptations/tree/main/csv
		#if (CRTCOLORS == 1)
		const mat3 colmatrix = mat3(1.3272128334714093, // ARI(D93/9300K) (JP) -> sRGB(D65/6500K) // what can be found in some of the sega or corresponding CRT manuals of the time is a range of ~9700K-10600K, so even cooler than D93
									-0.3802108879412366,
									-0.1003607696463202,
									-0.0241848600268417,
									0.9544506228550125,
									0.0807358585208939,
									-0.0239585810555497,
									-0.0409736005706461,
									1.4076563728858242);
		#elif (CRTCOLORS == 2)
		const mat3 colmatrix = mat3(0.9241392201737613, // PVM_20M2U(D93/9300K) (JP) -> sRGB(D65/6500K), gamma 2.25
									-0.0690701363506526,
									-0.0084279079392561,
									0.0231962420140721,
									0.9729221778325590,
									0.0148832015024337,
									-0.0054875731998806,
									0.0098220960740544,
									1.3383896683854550);
		#elif (CRTCOLORS == 3)
		const mat3 colmatrix = mat3(0.7822490684754086, // BT601_525_D93 (JP) -> sRGB(D65/6500K), gamma sRGB
									0.0506168576073398,
									0.0137752498011040,
									0.0147968947158740,
									0.9741745313046067,
									0.0220301953285839,
									-0.0013501205469208,
									-0.0044076726925543,
									1.3484819844991036);
		#elif (CRTCOLORS == 4)
		const mat3 colmatrix = mat3(0.9395420637732393, // BT601_525 (US) -> sRGB(D65/6500K), gamma sRGB // extremely subtle
									0.0501813568598678,
									0.0102765793668928,
									0.0177722231435608,
									0.9657928624969044,
									0.0164349143595347,
									-0.0016215999431855,
									-0.0043697496597356,
									1.0059913496029214);
		#elif (CRTCOLORS == 5)
		const mat3 colmatrix = mat3(1.0440432087628346, // BT601_625 (EUR,AUS) -> sRGB(D65/6500K), gamma sRGB // barely noticeable at all
									-0.0440432087628348,
									0.0000000000000001,
									0.0000000000000000,
									1.0000000000000002,
									0.0000000000000000,
									0.0000000000000000,
									0.0117933782840052,
									0.9882066217159947);
		#endif

		vec3 GetTextureValue(sampler2D s)
		{
			ivec2 texPos	= ivec2(gl_FragCoord.xy /*-vec2(0.5)*/) * aa;
			vec3 texColour	= vec3(0.0);

			for(int i=0; i < aa; i++) {
				for(int j=0; j < aa; j++) {
					texColour += texelFetch(s,ivec2(texPos.x+i,texPos.y+j),0).rgb;
				}
			}

			texColour *= 1.0/float(aa * aa);

			return texColour;
		}

		float sRGB(float color)
		{
			if (color <= 0.0031308)
				return 12.92 * color;
			else
				return 1.055 * pow(color, 1.0/2.4) - 0.055;
		}

		float invsRGB(float color)
		{
			if (color <= 0.04045) // 0.03928 ?
				return color * (1.0/12.92);
			else
				return pow(color * (1.0/1.055) + (0.055/1.055), 2.4);
		}

		void main()
		{
			vec3 finalColor = GetTextureValue(tex1);

			#if (CRTCOLORS != 0)
			{
				// transform input space to linear space
				if (cgamma != -1.0)
					finalColor = pow(finalColor, vec3(cgamma));
				else
					finalColor = vec3(invsRGB(finalColor.r),invsRGB(finalColor.g),invsRGB(finalColor.b));
				// do color transformation
				finalColor *= colmatrix;
				// transform from linear to sRGB output space
				finalColor = vec3(sRGB(finalColor.r),sRGB(finalColor.g),sRGB(finalColor.b));
			}
			#endif

			fragColor = vec4(finalColor, 1.0);
		}

		)glsl";

		std::string fragmentShaderString = fragmentShaderVersion + aaString + ccString + fragmentShader;

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
	if ((m_aa > 1) || (m_crtcolors != CRTcolor::None)) {
		m_fbo.Destroy();
		m_fbo.Create(width * m_aa, height * m_aa);

		m_width = width;
		m_height = height;
	}
}

void SuperAA::Draw()
{
	if ((m_aa > 1) || (m_crtcolors != CRTcolor::None)) {
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
