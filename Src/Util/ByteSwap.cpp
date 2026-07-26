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
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "Util/ByteSwap.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace Util
{
  void FlipEndian16(uint8_t * const buffer, const size_t size)
  {
#ifdef _MSC_VER
    uint16_t * const buffer16 = (uint16_t*)buffer;
    for (size_t i = 0; i < size/2; i++)
      buffer16[i] = _byteswap_ushort(buffer16[i]);
#elif defined(__GNUC__)
    uint16_t * const buffer16 = (uint16_t*)buffer;
    for (size_t i = 0; i < size/2; i++)
      buffer16[i] = __builtin_bswap16(buffer16[i]);
#else
    for (size_t i = 0; i < (size & ~1); i += 2)
    {
      uint8_t tmp = buffer[i + 0];
      buffer[i + 0] = buffer[i + 1];
      buffer[i + 1] = tmp;
    }
#endif
  }

  void FlipEndian32(uint8_t * const buffer, const size_t size)
  {
#ifdef _MSC_VER
    uint32_t * const buffer32 = (uint32_t*)buffer;
    for (size_t i = 0; i < size/4; i++)
      buffer32[i] = _byteswap_ulong(buffer32[i]);
#elif defined(__GNUC__)
    uint32_t * const buffer32 = (uint32_t*)buffer;
    for (size_t i = 0; i < size/4; i++)
      buffer32[i] = __builtin_bswap32(buffer32[i]);
#else
    for (size_t i = 0; i < (size & ~3); i += 4)
    {
      uint8_t tmp1 = buffer[i+0];
      uint8_t tmp2 = buffer[i+1];
      buffer[i+0] = buffer[i+3];
      buffer[i+1] = buffer[i+2];
      buffer[i+2] = tmp2;
      buffer[i+3] = tmp1;
    }
#endif
  }
} // Util
