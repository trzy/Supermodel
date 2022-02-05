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

    // BITMAPV4HEADER
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
      uint32_t red_mask;
      uint32_t green_mask;
      uint32_t blue_mask;
      uint32_t alpha_mask;
      uint32_t color_space;
      uint32_t endpoints_red_x;
      uint32_t endpoints_red_y;
      uint32_t endpoints_red_z;
      uint32_t endpoints_green_x;
      uint32_t endpoints_green_y;
      uint32_t endpoints_green_z;
      uint32_t endpoints_blue_x;
      uint32_t endpoints_blue_y;
      uint32_t endpoints_blue_z;
      uint32_t gamma_red;
      uint32_t gamma_green;
      uint32_t gamma_blue;
      BMPInfoHeader(int32_t _width, int32_t _height)
        : size(sizeof(BMPInfoHeader)),
          width(_width),
          height(_height),
          num_planes(1),
          bits_per_pixel(32),
          compression_method(3),        // BI_BITFIELDS
          bitmap_size(_width*_height*3),
          horizontal_resolution(2835),  // 72 dpi
          vertical_resolution(2835),
          num_palette_colors(0),
          num_important_colors(0),
          red_mask(0x00ff0000),
          green_mask(0x0000ff00),
          blue_mask(0x000000ff),
          alpha_mask(0xff000000),
          color_space(1),               // LCS_DEVICE_RGB
          endpoints_red_x(0),
          endpoints_red_y(0),
          endpoints_red_z(0),
          endpoints_green_x(0),
          endpoints_green_y(0),
          endpoints_green_z(0),
          endpoints_blue_x(0),
          endpoints_blue_y(0),
          endpoints_blue_z(0),
          gamma_red(0),
          gamma_green(0),
          gamma_blue(0)
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

  // Texture format 0: TRRR RRGG GGGB BBBB, T = contour bit
  template <bool EnableContour>
  struct T1RGB5
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
      if (EnableContour)
      {
        bool t = (*reinterpret_cast<const uint16_t*>(pixel) >> 15) & 0x1;
        return t ? uint8_t(0x00) : uint8_t(0xff); // T-bit indicates transparency
      }
      else
      {
        return 0xff;  // force opaque
      }
    }
  };

  using T1RGB5ContourEnabled = struct T1RGB5<true>;
  using T1RGB5ContourIgnored = struct T1RGB5<false>;

  // Texture format 1: xxxx xxxx AAAA LLLL
  struct A4L4Low
  {
      static const unsigned bytes_per_pixel = 2;
      static inline uint8_t GetRed(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 0) & 0xf));
      }
      static inline uint8_t GetGreen(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 0) & 0xf));
      }
      static inline uint8_t GetBlue(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 0) & 0xf));
      }
      static inline uint8_t GetAlpha(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 4) & 0xf));
      }
  };

  // Texture format 2: xxxx xxxx LLLL AAAA
  struct L4A4Low
  {
      static const unsigned bytes_per_pixel = 2;
      static inline uint8_t GetRed(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 4) & 0xf));
      }
      static inline uint8_t GetGreen(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 4) & 0xf));
      }
      static inline uint8_t GetBlue(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 4) & 0xf));
      }
      static inline uint8_t GetAlpha(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 0) & 0xf));
      }
  };

  // Texture format 3: AAAA LLLL xxxx xxxx
  struct A4L4High
  {
      static const unsigned bytes_per_pixel = 2;
      static inline uint8_t GetRed(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 8) & 0xf));
      }
      static inline uint8_t GetGreen(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 8) & 0xf));
      }
      static inline uint8_t GetBlue(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 8) & 0xf));
      }
      static inline uint8_t GetAlpha(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 12) & 0xf));
      }
  };

  // Texture format 4: LLLL AAAA xxxx xxxx
  struct L4A4High
  {
      static const unsigned bytes_per_pixel = 2;
      static inline uint8_t GetRed(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 12) & 0xf));
      }
      static inline uint8_t GetGreen(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 12) & 0xf));
      }
      static inline uint8_t GetBlue(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 12) & 0xf));
      }
      static inline uint8_t GetAlpha(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> 8) & 0xf));
      }
  };

  // Texture format 5: xxxx xxxx LLLL LLLL, where L=0xff is transparent and L!=0xff is opaque
  struct L8Low
  {
      static const unsigned bytes_per_pixel = 2;
      static inline uint8_t GetRed(const uint8_t* pixel)
      {
          return uint8_t((*reinterpret_cast<const uint16_t*>(pixel) >> 0) & 0xff);
      }
      static inline uint8_t GetGreen(const uint8_t* pixel)
      {
          return uint8_t((*reinterpret_cast<const uint16_t*>(pixel) >> 0) & 0xff);
      }
      static inline uint8_t GetBlue(const uint8_t* pixel)
      {
          return uint8_t((*reinterpret_cast<const uint16_t*>(pixel) >> 0) & 0xff);
      }
      static inline uint8_t GetAlpha(const uint8_t* pixel)
      {
          uint8_t l = uint8_t((*reinterpret_cast<const uint16_t*>(pixel) >> 0) & 0xff);
          return l == 0xff ? 0 : 0xff;
      }
  };

  // Texture format 6: LLLL LLLL xxxx xxxx, where L=0xff is transparent and L!=0xff is opaque
  struct L8High
  {
      static const unsigned bytes_per_pixel = 2;
      static inline uint8_t GetRed(const uint8_t* pixel)
      {
          return uint8_t((*reinterpret_cast<const uint16_t*>(pixel) >> 8) & 0xff);
      }
      static inline uint8_t GetGreen(const uint8_t* pixel)
      {
          return uint8_t((*reinterpret_cast<const uint16_t*>(pixel) >> 8) & 0xff);
      }
      static inline uint8_t GetBlue(const uint8_t* pixel)
      {
          return uint8_t((*reinterpret_cast<const uint16_t*>(pixel) >> 8) & 0xff);
      }
      static inline uint8_t GetAlpha(const uint8_t* pixel)
      {
          uint8_t l = uint8_t((*reinterpret_cast<const uint16_t*>(pixel) >> 8) & 0xff);
          return l == 0xff ? 0 : 0xff;
      }
  };

  // Texture format 7: RRRR GGGG BBBB AAAA
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

  // Texture format 8:  xxxx xxxx xxxx LLLL
  // Texture format 9:  xxxx xxxx LLLL xxxx
  // Texture format 10: xxxx LLLL xxxx xxxx
  // Texture format 11: LLLL xxxx xxxx xxxx
  template <int Channel>
  struct L4
  {
      static const unsigned bytes_per_pixel = 2;
      static inline uint8_t GetRed(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> (Channel * 4)) & 0xf));
      }
      static inline uint8_t GetGreen(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> (Channel * 4)) & 0xf));
      }
      static inline uint8_t GetBlue(const uint8_t* pixel)
      {
          return uint8_t((255.0f / 15.0f) * float((*reinterpret_cast<const uint16_t*>(pixel) >> (Channel * 4)) & 0xf));
      }
      static inline uint8_t GetAlpha(const uint8_t* pixel)
      {
          uint8_t l = (*reinterpret_cast<const uint16_t*>(pixel) >> (Channel * 4)) & 0xf;
          return l == 0x0f ? 0x00 : 0xff;
      }
  };

  using L4Channel0 = struct L4<0>;
  using L4Channel1 = struct L4<1>;
  using L4Channel2 = struct L4<2>;
  using L4Channel3 = struct L4<3>;

  template <class SurfaceFormat>
  static bool WriteSurfaceToBMP(const std::string &file_name, const uint8_t *pixels, int32_t width, int32_t height, bool flip_vertical)
  {
    using namespace detail;
    size_t file_size = sizeof(FileHeader) + width*height*4;
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
          *bmp++ = SurfaceFormat::GetAlpha(src);
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
          *bmp++ = SurfaceFormat::GetAlpha(src);
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