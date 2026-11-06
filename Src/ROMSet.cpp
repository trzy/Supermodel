/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
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

#include "ROMSet.h"
#include "OSD/Logger.h"
#include <cstring>
#include <algorithm>

void ROM::CopyTo(uint8_t *dest, size_t dest_size, bool apply_patches ) const
{
  if (!data || !size || !dest || !dest_size)
    return;
  
  size_t bytes_to_copy = std::min(size, dest_size);
  const uint8_t *src = data.get();
  memcpy(dest, src, bytes_to_copy);
  
  if (apply_patches)
  {
    for (auto &patch: patches)
    {
      unsigned bytes = patch.bits / 8;
      if (patch.offset + bytes > dest_size)
      {
        ErrorLog("Ignored ROM patch to offset 0x%x in region 0x%x bytes long.", patch.offset, dest_size);
        continue;
      }
      uint64_t value = patch.value;
      switch (patch.bits)
      {
      case 8:
      case 16:
      case 32:
      case 64:
        for (size_t i = 0; i < bytes; i++)
        {
          dest[patch.offset + bytes - 1 - i] = value & 0xff;
          value >>= 8;
        }
        break;
      default:
        break;
      }
    }
  }
}

ROM ROMSet::get_rom(const std::string &region) const
{
  auto it = rom_by_region.find(region);
  return it == rom_by_region.end() ? ROM() : it->second;
}
