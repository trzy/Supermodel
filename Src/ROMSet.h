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
