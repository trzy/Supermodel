#ifndef INCLUDED_BYTESWAP_H
#define INCLUDED_BYTESWAP_H

#include <cstddef>
#include <cstdint>

namespace Util
{
  void FlipEndian16(uint8_t *buffer, size_t size);
  void FlipEndian32(uint8_t *buffer, size_t size);
} // Util

#endif  // INCLUDED_BYTESWAP_H
