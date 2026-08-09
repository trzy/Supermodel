#ifndef INCLUDED_GRAPHICS_GLSL_H
#define INCLUDED_GRAPHICS_GLSL_H

#include "Graphics/GL.h"
#include <string>

namespace GLSL
{

enum class ShaderStage
{
  Vertex,
  Geometry,
  Fragment
};

inline const char *VersionDirective()
{
#ifdef SUPERMODEL_GLES
  // The vendored ImGui backend selects its ES shader variant specifically for 300.
  // GLSL ES 3.00 shaders are valid on every supported OpenGL ES context.
  return "#version 300 es";
#else
  return "#version 410";
#endif
}

inline std::string PrepareSource(const char *source, ShaderStage stage)
{
  std::string shaderSource(source == nullptr ? "" : source);

#ifdef SUPERMODEL_GLES
  const size_t versionPos = shaderSource.find("#version");
  if (versionPos != std::string::npos)
  {
    const size_t lineEnd = shaderSource.find('\n', versionPos);
    shaderSource.erase(versionPos, lineEnd == std::string::npos ? std::string::npos : lineEnd - versionPos + 1);
  }

  GLint majorVersion = 0;
  GLint minorVersion = 0;
  glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
  glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

  const int version = majorVersion * 100 + minorVersion * 10;
  std::string header = version >= 320 ? "#version 320 es\n" : "#version 310 es\n";

  header += "#define SUPERMODEL_GLES 1\nprecision highp float;\nprecision highp int;\n";
  if (stage == ShaderStage::Fragment)
  {
    header += "precision highp sampler2D;\nprecision highp usampler2D;\n";
  }
  return header + shaderSource;
#else
  (void)stage;
  return shaderSource;
#endif
}

} // namespace GLSL

#endif // INCLUDED_GRAPHICS_GLSL_H
