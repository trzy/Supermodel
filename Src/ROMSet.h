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

#ifndef INCLUDED_ROMSET_H
#define INCLUDED_ROMSET_H

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <cstdint>

// Holds a single ROM region
struct ROM
{
  struct BigEndianPatch
  {
    uint32_t offset;
    uint64_t value;
    unsigned bits;
    
    BigEndianPatch(uint32_t o, uint64_t v, unsigned b)
      : offset(o),
        value(v),
        bits(b)
    {}
  };
  
  std::shared_ptr<uint8_t> data;
  std::vector<BigEndianPatch> patches;
  size_t size = 0;
  
  void CopyTo(uint8_t *dest, size_t dest_size, bool apply_patches = true) const;
};
  
struct ROMSet
{
  std::map<std::string, ROM> rom_by_region;
  
  ROM get_rom(const std::string &region) const;
};

#endif  // INCLUDED_ROMSET_H
