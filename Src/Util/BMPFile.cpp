#include "Util/BMPFile.h"
#include "OSD/Logger.h"
#include <memory>
#include <cstdio>

namespace Util
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
      : id{ 'B', 'M' },
        file_size(_file_size),
        reserved1(0),
        reserved2(0),
        bitmap_offset(_bitmap_offset)
    {}  
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
  
  bool WriteRGBA8SurfaceToBMP(const std::string &file_name, const uint8_t *pixels, int32_t width, int32_t height, bool flip_vertical)
  {
    size_t file_size = sizeof(FileHeader) + width*height*3;
    std::shared_ptr<uint8_t> file(new uint8_t[file_size], std::default_delete<uint8_t[]>());
    FileHeader *header = new (file.get()) FileHeader(width, height);
    uint8_t *bmp = file.get() + sizeof(*header);
    if (!flip_vertical)
    {
      for (int32_t y = height - 1; y >= 0; y--)
      {
        const uint8_t *src = &pixels[y*width*4];
        for (int32_t x = 0; x < width; x++)
        {
          *bmp++ = src[2];  // b
          *bmp++ = src[1];  // g
          *bmp++ = src[0];  // r
          src += 4;
        }
      }
    }
    else
    {
      for (int32_t y = 0; y < height; y++)
      {
        const uint8_t *src = &pixels[y*width*4];
        for (int32_t x = 0; x < width; x++)
        {
          *bmp++ = src[2];  // b
          *bmp++ = src[1];  // g
          *bmp++ = src[0];  // r
          src += 4;
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
