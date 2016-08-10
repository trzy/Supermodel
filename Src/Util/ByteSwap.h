#ifndef INCLUDED_BYTESWAP_H
#define INCLUDED_BYTESWAP_H

#include <cstddef>
#include <cstdint>

namespace Util
{
  void ByteSwap(uint8_t *buffer, size_t size);
} // Util

#endif  // INCLUDED_BYTESWAP_H
