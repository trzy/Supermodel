#ifndef INCLUDED_BMPFILE_HPP
#define INCLUDED_BMPFILE_HPP

#include <string>
#include <cstdint>

namespace Util
{
  extern bool WriteRGBA8SurfaceToBMP(const std::string &file_name, const uint8_t *pixels, int32_t width, int32_t height, bool flip_vertical);
} // Util

#endif  // INCLUDED_BMPFILE_HPP