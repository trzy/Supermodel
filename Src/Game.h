#ifndef INCLUDED_GAME_H
#define INCLUDED_GAME_H

#include "Util/NewConfig.h"
#include "Pkgs/unzip.h"
#include <map>
#include <set>

struct Game
{
  std::string title;
  std::string version;
  std::string manufacturer;
  int year;
  std::string stepping;
  // Holds a ROM region
  struct ROM
  {
    std::shared_ptr<uint8_t> data;
    size_t size = 0;
  };
  std::map<std::string, ROM> roms;
};

//TODO: move to separate GameLoader.cpp
class GameLoader
{
private:
  // Describes a file node in the game XML
  struct File
  {
    typedef std::shared_ptr<File> Ptr_t;
    uint32_t offset;
    std::string filename;
    uint32_t crc32;
    bool has_crc32;
    static Ptr_t Create(const GameLoader &loader, const Util::Config::Node &file_node);
    bool Matches(const std::string &filename, uint32_t crc32) const;
  };

  // Describes a region node in the game XML
  struct Region
  {
    typedef std::shared_ptr<Region> Ptr_t;
    std::string region_name;
    size_t stride;
    size_t chunk_size;
    bool byte_swap;
    typedef std::map<uint32_t, File::Ptr_t> FilesByOffset_t;
    FilesByOffset_t files_by_offset;
    static Ptr_t Create(const GameLoader &loader, const Util::Config::Node &region_node);
  };
  
  std::map<std::string, Game> m_game_info_by_game;  // ROMs not loaded here
  typedef std::map<std::string, Region::Ptr_t> RegionsByName_t;
  std::map<std::string, RegionsByName_t> m_regions_by_game;
  std::string m_xml_filename;
  std::string m_zipfilename;
  unzFile m_zf;
  std::map<uint32_t, std::string> m_filename_by_crc32;
  std::map<std::string, unz_file_info> m_zip_info_by_filename;
  std::map<uint32_t, unz_file_info> m_zip_info_by_crc32;

  static bool MissingAttrib(const GameLoader &loader, const Util::Config::Node &node, const std::string &attribute);
  bool ParseXML(const Util::Config::Node::ConstPtr_t &xml);
  std::set<std::string> IdentifyGamesFileBelongsTo(const std::string &filename, uint32_t crc32) const;
  const unz_file_info *LookupZippedFile(const File::Ptr_t &file) const;
  bool ComputeRegionSize(uint32_t *region_size, const Region::Ptr_t &region) const;
  bool LoadZippedFile(std::shared_ptr<uint8_t> *buffer, size_t *file_size, const GameLoader::File::Ptr_t &file);
  bool LoadRegion(Game::ROM *buffer, const GameLoader::Region::Ptr_t &region);
  bool LoadROMs(std::map<std::string, Game::ROM> *roms, const std::string &game_name);

public:
  GameLoader()
  {
  }

  bool LoadDefinitionXML(const std::string &filename);
  bool Load(Game *game, const std::string &zipfilename);
};

#endif  // INCLUDED_GAME_H
