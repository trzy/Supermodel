#include "SuperAA.h"

#include <algorithm>
#include <string>

namespace
{

const char* s_fullscreenVertexShader = R"glsl(
  #version 410 core

  void main(void)
  {
    const vec4 vertices[] = vec4[](
      vec4(-1.0, -1.0, 0.0, 1.0),
      vec4(-1.0,  1.0, 0.0, 1.0),
      vec4( 1.0, -1.0, 0.0, 1.0),
      vec4( 1.0,  1.0, 0.0, 1.0));
    gl_Position = vertices[gl_VertexID % 4];
  }
)glsl";

const char* s_blurFragmentShader = R"glsl(
  #version 410 core

  uniform sampler2D sourceTexture;
  uniform vec2 outputSize;
  uniform vec2 direction;
  out vec4 fragColor;

  void main()
  {
    vec2 uv = gl_FragCoord.xy / outputSize;
    vec3 color = pow(texture(sourceTexture, uv).rgb, vec3(2.2)) * 0.227027;
    color += pow(texture(sourceTexture, uv + direction * 1.384615).rgb, vec3(2.2)) * 0.316216;
    color += pow(texture(sourceTexture, uv - direction * 1.384615).rgb, vec3(2.2)) * 0.316216;
    color += pow(texture(sourceTexture, uv + direction * 3.230769).rgb, vec3(2.2)) * 0.070270;
    color += pow(texture(sourceTexture, uv - direction * 3.230769).rgb, vec3(2.2)) * 0.070270;
    fragColor = vec4(pow(max(color, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
  }
)glsl";

std::string BuildSourceFragmentShader(int aa, CRTcolor crtColors, int crtMode)
{
  std::string defines = "#version 410 core\nconst int aa = " + std::to_string(aa) + ";\n";
  defines += "#define CRTCOLORS " + std::to_string(static_cast<int>(crtColors)) + "\n";
  defines += "#define CRTMODE " + std::to_string(crtMode) + "\n";

  static const char* source = R"glsl(
    uniform sampler2D tex1;
    uniform sampler2D previousFrame;
    uniform vec2 outputSize;
    uniform float phosphorAmplitude;
    out vec4 fragColor;

    #if (CRTCOLORS == 1)
    const float cgamma = 2.5;
    const mat3 colmatrix = mat3(
       1.3272128334714093, -0.3802108879412366, -0.1003607696463202,
      -0.0241848600268417,  0.9544506228550125,  0.0807358585208939,
      -0.0239585810555497, -0.0409736005706461,  1.4076563728858242);
    #elif (CRTCOLORS == 2)
    const float cgamma = 2.25;
    const mat3 colmatrix = mat3(
       0.9241392201737613, -0.0690701363506526, -0.0084279079392561,
       0.0231962420140721,  0.9729221778325590,  0.0148832015024337,
      -0.0054875731998806,  0.0098220960740544,  1.3383896683854550);
    #elif (CRTCOLORS == 3)
    const float cgamma = -1.0;
    const mat3 colmatrix = mat3(
      0.7822490684754086,  0.0506168576073398, 0.0137752498011040,
      0.0147968947158740,  0.9741745313046067, 0.0220301953285839,
     -0.0013501205469208, -0.0044076726925543, 1.3484819844991036);
    #elif (CRTCOLORS == 4)
    const float cgamma = -1.0;
    const mat3 colmatrix = mat3(
       0.9395420637732393,  0.0501813568598678, 0.0102765793668928,
       0.0177722231435608,  0.9657928624969044, 0.0164349143595347,
      -0.0016215999431855, -0.0043697496597356, 1.0059913496029214);
    #elif (CRTCOLORS == 5)
    const float cgamma = -1.0;
    const mat3 colmatrix = mat3(
      1.0440432087628346, -0.0440432087628348, 0.0000000000000001,
      0.0000000000000000,  1.0000000000000002, 0.0000000000000000,
      0.0000000000000000,  0.0117933782840052, 0.9882066217159947);
    #endif

    vec3 GetTextureValue(sampler2D sourceTexture)
    {
      ivec2 texPos = ivec2(gl_FragCoord.xy) * aa;
      vec3 color = vec3(0.0);
      for (int y = 0; y < aa; ++y)
        for (int x = 0; x < aa; ++x)
          color += texelFetch(sourceTexture, texPos + ivec2(x, y), 0).rgb;
      return color / float(aa * aa);
    }

    float ToSRGB(float color)
    {
      return color <= 0.0031308 ? 12.92 * color : 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    }

    float FromSRGB(float color)
    {
      return color <= 0.04045 ? color / 12.92 : pow((color + 0.055) / 1.055, 2.4);
    }

    void main()
    {
      vec3 color = GetTextureValue(tex1);

      #if (CRTCOLORS != 0)
      if (cgamma != -1.0)
        color = pow(max(color, vec3(0.0)), vec3(cgamma));
      else
        color = vec3(FromSRGB(color.r), FromSRGB(color.g), FromSRGB(color.b));
      color *= colmatrix;
      color = vec3(ToSRGB(color.r), ToSRGB(color.g), ToSRGB(color.b));
      #endif

      #if (CRTMODE != 0)
      vec2 uv = gl_FragCoord.xy / outputSize;
      vec3 previous = texture(previousFrame, uv).rgb;
      vec3 linearColor = pow(max(color, vec3(0.0)), vec3(2.2));
      vec3 linearPrevious = pow(max(previous, vec3(0.0)), vec3(2.2));
      color = pow(linearColor + linearPrevious * phosphorAmplitude, vec3(1.0 / 2.2));
      #endif

      fragColor = vec4(color, 1.0);
    }
  )glsl";

  return defines + source;
}

std::string BuildCRTFragmentShader()
{
  std::string defines = "#version 410 core\n";
  static const char* source = R"glsl(
    uniform sampler2D baseTexture;
    uniform sampler2D blurTexture;
    uniform vec2 outputSize;
    uniform vec4 crtViewport;
    uniform float crtStrength;
    uniform float crtCurvature;
    out vec4 fragColor;

    const float inputGamma = 2.4;
    const float outputGamma = 2.2;
    const float apertureStrength = 0.14;
    const float spotSize = 0.30;
    const float spotGrowth = 0.10;
    const float spotGrowthPower = 3.0;
    const float halation = 0.10;
    const float rasterBloom = 0.01;
    const float brightness = 0.10;

    vec2 Curve(vec2 position)
    {
      if (crtCurvature < 0.5)
        return position;
      vec2 centered = position * 2.0 - 1.0;
      float radius2 = dot(centered, centered);
      // A healthy 29-inch arcade tube has visible geometry, but substantially
      // less bowing than a small consumer television.
      centered *= 1.0 + vec2(0.030, 0.040) * radius2;
      return centered * 0.5 + 0.5;
    }

    float RoundedCornerMask(vec2 position)
    {
      if (crtCurvature < 0.5)
        return 1.0;
      const float radius = 0.015;
      vec2 distanceToEdge = min(position, 1.0 - position);
      float nearestEdge = min(distanceToEdge.x, distanceToEdge.y);
      return smoothstep(0.0, radius, nearestEdge);
    }

    vec3 ApertureSample(float pixelPosition)
    {
      float phase = mod(floor(pixelPosition), 3.0);
      return phase < 1.0 ? vec3(1.0, 1.0 - apertureStrength, 1.0 - apertureStrength) :
             phase < 2.0 ? vec3(1.0 - apertureStrength, 1.0, 1.0 - apertureStrength) :
                           vec3(1.0 - apertureStrength, 1.0 - apertureStrength, 1.0);
    }

    void main()
    {
      vec2 outputUV = gl_FragCoord.xy / outputSize;
      vec3 untouched = texture(baseTexture, outputUV).rgb;
      vec2 viewportMin = crtViewport.xy;
      vec2 viewportMax = viewportMin + crtViewport.zw;
      bool insideViewport = all(greaterThanEqual(gl_FragCoord.xy, viewportMin)) &&
                            all(lessThan(gl_FragCoord.xy, viewportMax));
      if (!insideViewport || crtViewport.z <= 0.0 || crtViewport.w <= 0.0)
      {
        fragColor = vec4(untouched, 1.0);
        return;
      }

      vec2 localPosition = (gl_FragCoord.xy - crtViewport.xy) / crtViewport.zw;
      vec2 curvedPosition = Curve(localPosition);
      if (any(lessThan(curvedPosition, vec2(0.0))) || any(greaterThan(curvedPosition, vec2(1.0))))
      {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
      }

      vec2 samplePixel = crtViewport.xy + curvedPosition * crtViewport.zw;
      vec2 sampleUV = samplePixel / outputSize;
      vec3 baseColor = texture(baseTexture, sampleUV).rgb;
      vec3 blurColor = texture(blurTexture, sampleUV).rgb;
      vec3 linearColor = pow(max(baseColor, vec3(0.0)), vec3(inputGamma));

      float luminance = dot(linearColor, vec3(0.299, 0.587, 0.114));
      float beamWidth = spotSize + spotGrowth * pow(clamp(luminance, 0.0, 1.0), spotGrowthPower);
      // ArcCabView computes the beam in warped source space. Integrate across
      // the local derivative so the curved grid remains stable on LCD panels.
      float nativeLine = curvedPosition.y * 384.0;
      float nativeLinesPerPixel = max(fwidth(nativeLine), 0.0001);
      float beam = 0.0;
      const float sampleOffsets[4] = float[](-0.375, -0.125, 0.125, 0.375);
      for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex)
      {
        float sampledLine = nativeLine + sampleOffsets[sampleIndex] * nativeLinesPerPixel;
        float distanceFromBeam = abs(fract(sampledLine) - 0.5) * 2.0;
        beam += exp(-0.5 * pow(distanceFromBeam / max(beamWidth, 0.001), 2.0));
      }
      beam *= 0.25;
      float pixelsPerNativeLine = 1.0 / nativeLinesPerPixel;
      float scanlineVisibility = smoothstep(1.25, 2.25, pixelsPerNativeLine);
      linearColor *= mix(1.0, 0.72 + beam * 0.58, scanlineVisibility);

      // The aperture grille follows the same warped coordinates. Approximate
      // filtered texture sampling by averaging across the transformed pixel.
      float maskPixelWidth = max(fwidth(samplePixel.x), 0.0001);
      vec3 aperture = vec3(0.0);
      for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex)
        aperture += ApertureSample(samplePixel.x + sampleOffsets[sampleIndex] * maskPixelWidth);
      aperture *= 0.25;
      // Preserve average luminance while reducing the prominence of individual
      // RGB stripes to match a large, normally viewed arcade monitor.
      aperture /= 1.0 - apertureStrength * (2.0 / 3.0);
      linearColor *= aperture;

      vec3 linearBlur = pow(max(blurColor, vec3(0.0)), vec3(outputGamma));
      linearColor += linearBlur * (halation + rasterBloom * max(luminance - 0.5, 0.0));
      linearColor *= 1.0 + brightness;

      vec2 centered = curvedPosition * 2.0 - 1.0;
      linearColor *= 1.0 - 0.12 * dot(centered, centered);
      vec3 effected = pow(max(linearColor, vec3(0.0)), vec3(1.0 / outputGamma));
      effected *= RoundedCornerMask(curvedPosition);
      fragColor = vec4(mix(baseColor, effected, crtStrength), 1.0);
    }
  )glsl";
  return defines + source;
}

} // anonymous namespace

SuperAA::SuperAA(int aaValue, CRTcolor crtColors, int crtMode, float crtStrength) :
  m_aa(aaValue),
  m_crtcolors(crtColors),
  m_crtMode(crtMode),
  m_crtStrength(std::clamp(crtStrength, 0.0f, 1.0f)),
  m_vao(0),
  m_width(0),
  m_height(0),
  m_viewportX(0),
  m_viewportY(0),
  m_viewportWidth(0),
  m_viewportHeight(0),
  m_historyIndex(0),
  m_resetHistory(true),
  m_curvatureEnabled(crtMode == 2)
{
  if (!IsEnabled())
    return;

  const std::string sourceFragmentShader = BuildSourceFragmentShader(m_aa, m_crtcolors, m_crtMode);
  m_sourceShader.LoadShaders(s_fullscreenVertexShader, sourceFragmentShader.c_str());
  m_sourceShader.GetUniformLocationMap("tex1");
  m_sourceShader.GetUniformLocationMap("previousFrame");
  m_sourceShader.GetUniformLocationMap("outputSize");
  m_sourceShader.GetUniformLocationMap("phosphorAmplitude");

  if (m_crtMode != 0)
  {
    m_blurShader.LoadShaders(s_fullscreenVertexShader, s_blurFragmentShader);
    m_blurShader.GetUniformLocationMap("sourceTexture");
    m_blurShader.GetUniformLocationMap("outputSize");
    m_blurShader.GetUniformLocationMap("direction");

    const std::string crtFragmentShader = BuildCRTFragmentShader();
    m_crtShader.LoadShaders(s_fullscreenVertexShader, crtFragmentShader.c_str());
    m_crtShader.GetUniformLocationMap("baseTexture");
    m_crtShader.GetUniformLocationMap("blurTexture");
    m_crtShader.GetUniformLocationMap("outputSize");
    m_crtShader.GetUniformLocationMap("crtViewport");
    m_crtShader.GetUniformLocationMap("crtStrength");
    m_crtShader.GetUniformLocationMap("crtCurvature");
  }

  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);
  glBindVertexArray(0);
}

SuperAA::~SuperAA()
{
  m_sourceShader.UnloadShaders();
  m_blurShader.UnloadShaders();
  m_crtShader.UnloadShaders();
  m_fbo.Destroy();
  m_history[0].Destroy();
  m_history[1].Destroy();
  m_blurHorizontal.Destroy();
  m_blurVertical.Destroy();

  if (m_vao)
  {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
}

bool SuperAA::IsEnabled() const
{
  return m_aa > 1 || m_crtcolors != CRTcolor::None || m_crtMode != 0;
}

void SuperAA::ClearIntermediateTargets()
{
  if (m_crtMode == 0 || m_width <= 0 || m_height <= 0)
    return;

  glViewport(0, 0, m_width, m_height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  for (FBO* target : { &m_history[0], &m_history[1], &m_blurHorizontal, &m_blurVertical })
  {
    target->Set();
    glClear(GL_COLOR_BUFFER_BIT);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  m_historyIndex = 0;
  m_resetHistory = false;
}

void SuperAA::Init(int width, int height)
{
  if (!IsEnabled())
    return;

  m_fbo.Destroy();
  m_history[0].Destroy();
  m_history[1].Destroy();
  m_blurHorizontal.Destroy();
  m_blurVertical.Destroy();

  m_width = width;
  m_height = height;
  m_fbo.Create(width * m_aa, height * m_aa);
  if (m_crtMode != 0)
  {
    m_history[0].Create(width, height);
    m_history[1].Create(width, height);
    m_blurHorizontal.Create(width, height);
    m_blurVertical.Create(width, height);
    m_resetHistory = true;
    ClearIntermediateTargets();
  }
}

void SuperAA::Draw()
{
  if (!IsEnabled())
    return;

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glBindVertexArray(m_vao);
  glViewport(0, 0, m_width, m_height);

  if (m_crtMode == 0)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fbo.GetTextureID());
    m_sourceShader.EnableShader();
    glUniform1i(m_sourceShader.uniformLocMap["tex1"], 0);
    glUniform2f(m_sourceShader.uniformLocMap["outputSize"], static_cast<float>(m_width), static_cast<float>(m_height));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_sourceShader.DisableShader();
    glBindVertexArray(0);
    return;
  }

  if (m_resetHistory)
    ClearIntermediateTargets();

  const unsigned previousHistory = m_historyIndex;
  const unsigned currentHistory = 1 - previousHistory;
  m_history[currentHistory].Set();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_fbo.GetTextureID());
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_history[previousHistory].GetTextureID());
  m_sourceShader.EnableShader();
  glUniform1i(m_sourceShader.uniformLocMap["tex1"], 0);
  glUniform1i(m_sourceShader.uniformLocMap["previousFrame"], 1);
  glUniform2f(m_sourceShader.uniformLocMap["outputSize"], static_cast<float>(m_width), static_cast<float>(m_height));
  glUniform1f(m_sourceShader.uniformLocMap["phosphorAmplitude"], 0.05f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  m_sourceShader.DisableShader();
  m_historyIndex = currentHistory;

  m_blurHorizontal.Set();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_history[m_historyIndex].GetTextureID());
  m_blurShader.EnableShader();
  glUniform1i(m_blurShader.uniformLocMap["sourceTexture"], 0);
  glUniform2f(m_blurShader.uniformLocMap["outputSize"], static_cast<float>(m_width), static_cast<float>(m_height));
  glUniform2f(m_blurShader.uniformLocMap["direction"], 1.0f / static_cast<float>(m_width), 0.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  m_blurVertical.Set();
  glBindTexture(GL_TEXTURE_2D, m_blurHorizontal.GetTextureID());
  glUniform2f(m_blurShader.uniformLocMap["direction"], 0.0f, 1.0f / static_cast<float>(m_height));
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  m_blurShader.DisableShader();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_history[m_historyIndex].GetTextureID());
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_blurVertical.GetTextureID());
  m_crtShader.EnableShader();
  glUniform1i(m_crtShader.uniformLocMap["baseTexture"], 0);
  glUniform1i(m_crtShader.uniformLocMap["blurTexture"], 1);
  glUniform2f(m_crtShader.uniformLocMap["outputSize"], static_cast<float>(m_width), static_cast<float>(m_height));
  glUniform4f(m_crtShader.uniformLocMap["crtViewport"],
    static_cast<float>(m_viewportX), static_cast<float>(m_viewportY),
    static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight));
  glUniform1f(m_crtShader.uniformLocMap["crtStrength"], m_crtStrength);
  glUniform1f(m_crtShader.uniformLocMap["crtCurvature"], m_curvatureEnabled ? 1.0f : 0.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  m_crtShader.DisableShader();

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindVertexArray(0);
}

void SuperAA::SetViewport(unsigned x, unsigned y, unsigned width, unsigned height)
{
  if (x != m_viewportX || y != m_viewportY || width != m_viewportWidth || height != m_viewportHeight)
    m_resetHistory = true;
  m_viewportX = x;
  m_viewportY = y;
  m_viewportWidth = width;
  m_viewportHeight = height;
}

void SuperAA::SetCurvature(bool enable)
{
  if (m_curvatureEnabled != enable)
    m_resetHistory = true;
  m_curvatureEnabled = enable;
}

GLuint SuperAA::GetTargetID()
{
  return m_fbo.GetFBOID();
}
