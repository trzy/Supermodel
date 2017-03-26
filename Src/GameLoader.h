#ifndef INCLUDED_GAMELOADER_H
#define INCLUDED_GAMELOADER_H

#include "Util/NewConfig.h"
#include "Pkgs/unzip.h"
#include "Game.h"
#include "ROMSet.h"
#include <map>
#include <set>

class GameLoader
{
private:
  // Describes a file node in the game XML
  struct File
  {
    typedef std::shared_ptr<File> ptr_t;
    uint32_t offset;
    std::string filename;
    uint32_t crc32;
    bool has_crc32;
    static ptr_t Create(const GameLoader &loader, const Util::Config::Node &file_node);
    bool Matches(const std::string &filename, uint32_t crc32) const;
  };

  // Describes a region node in the game XML
  struct Region
  {
    typedef std::shared_ptr<Region> ptr_t;
    std::string region_name;
    size_t stride;
    size_t chunk_size;
    bool byte_swap;
    typedef std::map<uint32_t, File::ptr_t> FilesByOffset_t;
    FilesByOffset_t files_by_offset;
    static ptr_t Create(const GameLoader &loader, const Util::Config::Node &region_node);
  };
  
  std::map<std::string, Game> m_game_info_by_game;  // ROMs not loaded here
  
  // Parsed XML
  typedef std::map<std::string, Region::ptr_t> RegionsByName_t;
  std::map<std::string, RegionsByName_t> m_regions_by_game;
  std::string m_xml_filename;
  
  // Zip file info
  std::string m_zipfilename;
  unzFile m_zf;
  std::map<uint32_t, std::string> m_filename_by_crc32;
  std::map<std::string, unz_file_info> m_zip_info_by_filename;
  std::map<uint32_t, unz_file_info> m_zip_info_by_crc32;

  static bool MissingAttrib(const GameLoader &loader, const Util::Config::Node &node, const std::string &attribute);
  bool ParseXML(const Util::Config::Node &xml);
  std::set<std::string> IdentifyGamesFileBelongsTo(const std::string &filename, uint32_t crc32) const;
  const unz_file_info *LookupZippedFile(const File::ptr_t &file) const;
  bool ComputeRegionSize(uint32_t *region_size, const Region::ptr_t &region) const;
  bool LoadZippedFile(std::shared_ptr<uint8_t> *buffer, size_t *file_size, const GameLoader::File::ptr_t &file);
  bool LoadRegion(ROM *buffer, const GameLoader::Region::ptr_t &region);
  bool LoadROMs(ROMSet *rom_set, const std::string &game_name);
  bool LoadDefinitionXML(const std::string &filename);

public:
  GameLoader(const std::string &xml_file);
  bool Load(Game *game, ROMSet *rom_set, const std::string &zipfilename);
  const std::map<std::string, Game> &GetGames() const
  {
    return m_game_info_by_game;
  }
};

#endif  // INCLUDED_GAMELOADER_H
