#include "Util/Format.h"

namespace Util
{
  static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

  const std::string Hex(uint32_t n, size_t num_digits)
  {
    Util::Format f;
    f << "0x";
    for (size_t b = num_digits * 4; b; )
    {
      b -= 4;
      f << hex_digits[(n >> b) & 0xf];
    }
    return f;
  }

  const std::string Hex(uint32_t n)
  {
    return Hex(n, 8);
  }
  
  const std::string Hex(uint16_t n)
  {
    return Hex(n, 4);
  }
  
  const std::string Hex(uint8_t n)
  {
    return Hex(n, 2);
  }
} // Util
