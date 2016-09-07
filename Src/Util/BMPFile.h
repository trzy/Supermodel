#ifndef INCLUDED_BMPFILE_HPP
#define INCLUDED_BMPFILE_HPP

#include "OSD/Logger.h"
#include <memory>
#include <string>
#include <cstdio>
#include <cstdint>

namespace Util
{
  namespace detail
  {
    #pragma pack(push, 1)

    struct BMPHeader
    {
      uint8_t id[2];
      uint32_t file_size;
      uint16_t reserved1;
      uint16_t reserved2;
      uint32_t bitmap_offset;
      BMPHeader(uint32_t _file_size, uint32_t _bitmap_offset)
        : file_size(_file_size),
          reserved1(0),
          reserved2(0),
          bitmap_offset(_bitmap_offset)
      {
  	  id[0] = 'B';
  	  id[1] = 'M';
  	}  
    };
    
    struct BMPInfoHeader
    {
      uint32_t size;
      int32_t width;
      int32_t height;
      uint16_t num_planes;
      uint16_t bits_per_pixel;
      uint32_t compression_method;
      uint32_t bitmap_size;
      int32_t horizontal_resolution;
      int32_t vertical_resolution;
      uint32_t num_palette_colors;
      uint32_t num_important_colors;
      BMPInfoHeader(int32_t _width, int32_t _height)
        : size(sizeof(BMPInfoHeader)),
          width(_width),
          height(_height),
          num_planes(1),
          bits_per_pixel(24),
          compression_method(0),
          bitmap_size(_width*_height*3),
          horizontal_resolution(2835),  // 72 dpi
          vertical_resolution(2835),
          num_palette_colors(0),
          num_important_colors(0)
      {}
    };
    
    struct FileHeader
    {
      BMPHeader bmp_header;
      BMPInfoHeader bmp_info_header;
      FileHeader(int32_t width, int32_t height)
        : bmp_header(sizeof(FileHeader) + width*height*3, sizeof(FileHeader)),
          bmp_info_header(width, height)
      {}
    };

    #pragma pack(pop)
  }

  struct RGBA8
  {
    static const unsigned bytes_per_pixel = 4;
    static inline uint8_t GetRed(const uint8_t *pixel)
    {
      return pixel[0];
    }
    static inline uint8_t GetGreen(const uint8_t *pixel)
    {
      return pixel[1];
    }
    static inline uint8_t GetBlue(const uint8_t *pixel)
    {
      return pixel[2];
    }
    static inline uint8_t GetAlpha(const uint8_t *pixel)
    {
      return pixel[3];
    }
  };

  struct A1RGB5
  {
    static const unsigned bytes_per_pixel = 2;
    static inline uint8_t GetRed(const uint8_t *pixel)
    {
      return uint8_t((255.0f / 31.0f) * float((*reinterpret_cast<const uint16_t *>(pixel) >> 10) & 0x1f));
    }
    static inline uint8_t GetGreen(const uint8_t *pixel)
    {
      return uint8_t((255.0f / 31.0f) * float((*reinterpret_cast<const uint16_t *>(pixel) >> 5) & 0x1f));
    }
    static inline uint8_t GetBlue(const uint8_t *pixel)
    {
      return uint8_t((255.0f / 31.0f) * float((*reinterpret_cast<const uint16_t *>(pixel) >> 0) & 0x1f));
    }
    static inline uint8_t GetAlpha(const uint8_t *pixel)
    {
      return uint8_t((255.0f / 1.0f) * float((*reinterpret_cast<const uint16_t *>(pixel) >> 15) & 0x1));
    }
  };

  struct RGBA4
  {
    static const unsigned bytes_per_pixel = 2;
    static inline uint8_t GetRed(const uint8_t *pixel)
    {
      return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t *>(pixel) >> 12) & 0xf));
    }
    static inline uint8_t GetGreen(const uint8_t *pixel)
    {
      return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t *>(pixel) >> 8) & 0xf));
    }
    static inline uint8_t GetBlue(const uint8_t *pixel)
    {
      return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t *>(pixel) >> 4) & 0xf));
    }
    static inline uint8_t GetAlpha(const uint8_t *pixel)
    {
      return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t *>(pixel) >> 0) & 0xf));
    }
  };

  template <class SurfaceFormat>
  static bool WriteSurfaceToBMP(const std::string &file_name, const uint8_t *pixels, int32_t width, int32_t height, bool flip_vertical)
  {
    using namespace detail;
    size_t file_size = sizeof(FileHeader) + width*height*3;
    std::shared_ptr<uint8_t> file(new uint8_t[file_size], std::default_delete<uint8_t[]>());
    FileHeader *header = new (file.get()) FileHeader(width, height);
    uint8_t *bmp = file.get() + sizeof(*header);
    if (!flip_vertical)
    {
      for (int32_t y = height - 1; y >= 0; y--)
      {
        const uint8_t *src = &pixels[y * width * SurfaceFormat::bytes_per_pixel];
        for (int32_t x = 0; x < width; x++)
        {
          *bmp++ = SurfaceFormat::GetBlue(src);
          *bmp++ = SurfaceFormat::GetGreen(src);
          *bmp++ = SurfaceFormat::GetRed(src);
          src += SurfaceFormat::bytes_per_pixel;
        }
      }
    }
    else
    {
      for (int32_t y = 0; y < height; y++)
      {
        const uint8_t *src = &pixels[y * width * SurfaceFormat::bytes_per_pixel];
        for (int32_t x = 0; x < width; x++)
        {
          *bmp++ = SurfaceFormat::GetBlue(src);
          *bmp++ = SurfaceFormat::GetGreen(src);
          *bmp++ = SurfaceFormat::GetRed(src);
          src += SurfaceFormat::bytes_per_pixel;
        }
      }
    }
    FILE *fp = fopen(file_name.c_str(), "wb");
    if (fp)
    {
      fwrite(file.get(), sizeof(uint8_t), file_size, fp);
      fclose(fp);
    }
    else
    {
      ErrorLog("Unable to open '%s' for writing.", file_name.c_str());
      return true;
    }
    return false;
  }
} // Util

#endif  // INCLUDED_BMPFILE_HPP