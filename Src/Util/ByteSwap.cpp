#include "Util/ByteSwap.h"

namespace Util
{
  void FlipEndian16(uint8_t *buffer, size_t size)
  {
    for (size_t i = 0; i < (size & ~1); i += 2)
    {
      uint8_t tmp = buffer[i + 0];
      buffer[i + 0] = buffer[i + 1];
      buffer[i + 1] = tmp;
    }
  }
  
  void FlipEndian32(uint8_t *buffer, size_t size)
  {
    for (size_t i = 0; i < (size & ~3); i += 4)
    {
      uint8_t tmp1 = buffer[i+0];
      uint8_t tmp2 = buffer[i+1];
      buffer[i+0] = buffer[i+3];
      buffer[i+1] = buffer[i+2];
      buffer[i+2] = tmp2;
      buffer[i+3] = tmp1;
    }
  }
} // Util
