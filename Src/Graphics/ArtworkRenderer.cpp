/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **/

#include "ArtworkRenderer.h"

#include "Supermodel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <vector>
#include <zlib.h>

namespace Video
{

namespace
{

struct PngImage
{
  unsigned width = 0;
  unsigned height = 0;
  std::vector<uint8_t> rgba;
};

uint32_t ReadBigEndian32(const uint8_t* data)
{
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) |
          static_cast<uint32_t>(data[3]);
}

uint8_t Paeth(uint8_t left, uint8_t above, uint8_t upperLeft)
{
  const int p = static_cast<int>(left) + static_cast<int>(above) - static_cast<int>(upperLeft);
  const int pa = std::abs(p - static_cast<int>(left));
  const int pb = std::abs(p - static_cast<int>(above));
  const int pc = std::abs(p - static_cast<int>(upperLeft));
  return pa <= pb && pa <= pc ? left : (pb <= pc ? above : upperLeft);
}

bool ReadFile(const std::string& path, std::vector<uint8_t>& data)
{
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file)
    return false;

  const std::streamoff size = file.tellg();
  if (size <= 0 || static_cast<uint64_t>(size) > static_cast<uint64_t>((std::numeric_limits<std::size_t>::max)()))
    return false;

  data.resize(static_cast<std::size_t>(size));
  file.seekg(0, std::ios::beg);
  return static_cast<bool>(file.read(reinterpret_cast<char*>(data.data()), size));
}

bool DecodePng(const std::string& path, PngImage& image, std::string& error)
{
  static constexpr std::array<uint8_t, 8> signature = { 137, 80, 78, 71, 13, 10, 26, 10 };

  std::vector<uint8_t> fileData;
  if (!ReadFile(path, fileData))
  {
    error = "unable to read file";
    return false;
  }
  if (fileData.size() < signature.size() || !std::equal(signature.begin(), signature.end(), fileData.begin()))
  {
    error = "invalid PNG signature";
    return false;
  }

  unsigned bitDepth = 0;
  unsigned colorType = 0;
  unsigned interlace = 0;
  std::vector<uint8_t> compressed;
  std::vector<uint8_t> palette;
  std::vector<uint8_t> transparency;
  bool haveHeader = false;
  bool haveEnd = false;

  std::size_t offset = signature.size();
  while (offset + 12 <= fileData.size())
  {
    const uint32_t length = ReadBigEndian32(&fileData[offset]);
    offset += 4;
    if (length > fileData.size() - offset - 8)
    {
      error = "truncated PNG chunk";
      return false;
    }

    const uint8_t* type = &fileData[offset];
    const uint8_t* chunk = type + 4;
    const uint32_t storedCrc = ReadBigEndian32(chunk + length);
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, type, length + 4);
    if (static_cast<uint32_t>(crc) != storedCrc)
    {
      error = "PNG chunk checksum mismatch";
      return false;
    }

    if (type[0] == 'I' && type[1] == 'H' && type[2] == 'D' && type[3] == 'R')
    {
      if (length != 13 || haveHeader)
      {
        error = "invalid PNG header";
        return false;
      }
      image.width = ReadBigEndian32(chunk);
      image.height = ReadBigEndian32(chunk + 4);
      bitDepth = chunk[8];
      colorType = chunk[9];
      interlace = chunk[12];
      if (chunk[10] != 0 || chunk[11] != 0)
      {
        error = "unsupported PNG compression or filter method";
        return false;
      }
      haveHeader = true;
    }
    else if (type[0] == 'I' && type[1] == 'D' && type[2] == 'A' && type[3] == 'T')
    {
      compressed.insert(compressed.end(), chunk, chunk + length);
    }
    else if (type[0] == 'P' && type[1] == 'L' && type[2] == 'T' && type[3] == 'E')
    {
      palette.assign(chunk, chunk + length);
    }
    else if (type[0] == 't' && type[1] == 'R' && type[2] == 'N' && type[3] == 'S')
    {
      transparency.assign(chunk, chunk + length);
    }
    else if (type[0] == 'I' && type[1] == 'E' && type[2] == 'N' && type[3] == 'D')
    {
      haveEnd = true;
      break;
    }

    offset += static_cast<std::size_t>(length) + 8;
  }

  if (!haveHeader || !haveEnd || image.width == 0 || image.height == 0 || compressed.empty())
  {
    error = "incomplete PNG";
    return false;
  }
  if (bitDepth != 8 || interlace != 0)
  {
    error = "only non-interlaced 8-bit PNG files are supported";
    return false;
  }

  unsigned channels = 0;
  switch (colorType)
  {
  case 0: channels = 1; break;
  case 2: channels = 3; break;
  case 3: channels = 1; break;
  case 4: channels = 2; break;
  case 6: channels = 4; break;
  default:
    error = "unsupported PNG color type";
    return false;
  }
  if (colorType == 3 && (palette.empty() || palette.size() % 3 != 0))
  {
    error = "invalid PNG palette";
    return false;
  }

  const uint64_t rowBytes64 = static_cast<uint64_t>(image.width) * channels;
  const uint64_t filteredSize64 = (rowBytes64 + 1) * image.height;
  const uint64_t rgbaSize64 = static_cast<uint64_t>(image.width) * image.height * 4;
  if (rowBytes64 > (std::numeric_limits<std::size_t>::max)() ||
      filteredSize64 > (std::numeric_limits<std::size_t>::max)() ||
      rgbaSize64 > (std::numeric_limits<std::size_t>::max)() ||
      filteredSize64 > (std::numeric_limits<uLongf>::max)())
  {
    error = "PNG dimensions are too large";
    return false;
  }

  const std::size_t rowBytes = static_cast<std::size_t>(rowBytes64);
  std::vector<uint8_t> filtered(static_cast<std::size_t>(filteredSize64));
  uLongf filteredSize = static_cast<uLongf>(filtered.size());
  const int zResult = uncompress(filtered.data(), &filteredSize, compressed.data(), static_cast<uLong>(compressed.size()));
  if (zResult != Z_OK || filteredSize != filtered.size())
  {
    error = "unable to decompress PNG image data";
    return false;
  }

  std::vector<uint8_t> pixels(rowBytes * image.height);
  for (unsigned y = 0; y < image.height; ++y)
  {
    const uint8_t filter = filtered[y * (rowBytes + 1)];
    const uint8_t* source = &filtered[y * (rowBytes + 1) + 1];
    uint8_t* destination = &pixels[y * rowBytes];
    const uint8_t* previous = y == 0 ? nullptr : &pixels[(y - 1) * rowBytes];

    if (filter > 4)
    {
      error = "invalid PNG row filter";
      return false;
    }

    for (std::size_t x = 0; x < rowBytes; ++x)
    {
      const uint8_t left = x < channels ? 0 : destination[x - channels];
      const uint8_t above = previous == nullptr ? 0 : previous[x];
      const uint8_t upperLeft = previous == nullptr || x < channels ? 0 : previous[x - channels];
      switch (filter)
      {
      case 0: destination[x] = source[x]; break;
      case 1: destination[x] = static_cast<uint8_t>(source[x] + left); break;
      case 2: destination[x] = static_cast<uint8_t>(source[x] + above); break;
      case 3: destination[x] = static_cast<uint8_t>(source[x] + ((static_cast<unsigned>(left) + above) / 2)); break;
      case 4: destination[x] = static_cast<uint8_t>(source[x] + Paeth(left, above, upperLeft)); break;
      }
    }
  }

  image.rgba.resize(static_cast<std::size_t>(rgbaSize64));
  for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(image.width) * image.height; ++pixel)
  {
    const uint8_t* source = &pixels[pixel * channels];
    uint8_t* destination = &image.rgba[pixel * 4];
    switch (colorType)
    {
    case 0:
      destination[0] = destination[1] = destination[2] = source[0];
      destination[3] = transparency.size() >= 2 && source[0] == transparency[1] ? 0 : 255;
      break;
    case 2:
      destination[0] = source[0];
      destination[1] = source[1];
      destination[2] = source[2];
      destination[3] = transparency.size() >= 6 && source[0] == transparency[1] && source[1] == transparency[3] && source[2] == transparency[5] ? 0 : 255;
      break;
    case 3:
    {
      const std::size_t paletteOffset = static_cast<std::size_t>(source[0]) * 3;
      if (paletteOffset + 2 >= palette.size())
      {
        error = "PNG palette index is out of range";
        return false;
      }
      destination[0] = palette[paletteOffset];
      destination[1] = palette[paletteOffset + 1];
      destination[2] = palette[paletteOffset + 2];
      destination[3] = source[0] < transparency.size() ? transparency[source[0]] : 255;
      break;
    }
    case 4:
      destination[0] = destination[1] = destination[2] = source[0];
      destination[3] = source[1];
      break;
    case 6:
      std::copy(source, source + 4, destination);
      break;
    }
  }

  return true;
}

} // anonymous namespace

CArtworkRenderer::~CArtworkRenderer()
{
  Unload();
}

bool CArtworkRenderer::Load(const std::string& path)
{
  Unload();

  PngImage image;
  std::string error;
  if (!DecodePng(path, image, error))
  {
    ErrorLog("Unable to load artwork PNG '%s': %s.\n", path.c_str(), error.c_str());
    return false;
  }

  static constexpr char vertexShader[] = R"glsl(
    #version 410 core
    layout(location = 0) in vec2 inPosition;
    layout(location = 1) in vec2 inTexCoord;
    out vec2 texCoord;

    void main(void)
    {
      gl_Position = vec4(inPosition, 0.0, 1.0);
      texCoord = inTexCoord;
    }
  )glsl";

  static constexpr char fragmentShader[] = R"glsl(
    #version 410 core
    uniform sampler2D artworkTexture;
    in vec2 texCoord;
    out vec4 fragColor;

    void main(void)
    {
      fragColor = texture(artworkTexture, texCoord);
    }
  )glsl";

  if (!m_shader.LoadShaders(vertexShader, fragmentShader))
  {
    ErrorLog("Unable to create artwork renderer shaders.\n");
    return false;
  }
  m_shader.GetUniformLocationMap("artworkTexture");

  static constexpr float vertices[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 0.0f
  };

  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);
  glGenBuffers(1, &m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void*>(2 * sizeof(float)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  glGenTextures(1, &m_texture);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(image.width), static_cast<GLsizei>(image.height), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  m_loaded = true;
  InfoLog("Loaded artwork PNG '%s' (%ux%u).", path.c_str(), image.width, image.height);
  return true;
}

void CArtworkRenderer::Draw(unsigned outputWidth, unsigned outputHeight)
{
  if (!m_loaded)
    return;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, outputWidth, outputHeight);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_shader.EnableShader();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glUniform1i(m_shader.uniformLocMap["artworkTexture"], 0);
  glBindVertexArray(m_vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  m_shader.DisableShader();

  glDisable(GL_BLEND);
}

void CArtworkRenderer::Unload()
{
  m_loaded = false;
  if (m_texture != 0)
  {
    glDeleteTextures(1, &m_texture);
    m_texture = 0;
  }
  if (m_vbo != 0)
  {
    glDeleteBuffers(1, &m_vbo);
    m_vbo = 0;
  }
  if (m_vao != 0)
  {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
  m_shader.UnloadShaders();
}

} // namespace Video
