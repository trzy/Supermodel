#ifndef INCLUDED_ROMSET_H
#define INCLUDED_ROMSET_H

#include <memory>
#include <string>
#include <map>

// Holds a single ROM region
struct ROM
{
  std::shared_ptr<uint8_t> data;
  size_t size = 0;
  
  void CopyTo(uint8_t *dest, size_t dest_size) const
  {
    if (!data || !size || !dest || !dest_size)
      return;
    size_t bytes_to_copy = std::min(size, dest_size);
    const uint8_t *src = data.get();
    memcpy(dest, src, bytes_to_copy);
  }
};
  
struct ROMSet
{
  std::map<std::string, ROM> rom_by_region;
  
  ROM get_rom(const std::string &region) const
  {
    auto it = rom_by_region.find(region);
    return it == rom_by_region.end() ? ROM() : it->second;
  }
};

#endif  // INCLUDED_ROMSET_H
