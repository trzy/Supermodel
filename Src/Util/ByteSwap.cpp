#include "Util/ByteSwap.h"

namespace Util
{
  void ByteSwap(uint8_t *buffer, size_t size)
  {
    for (size_t i = 0; i < (size & ~1); i += 2)
    {
      uint8_t x = buffer[i + 0];
      buffer[i + 0] = buffer[i + 1];
      buffer[i + 1] = x;
    }
  }
} // Util
