#ifndef INCLUDED_GAME_H
#define INCLUDED_GAME_H

#include "Util/NewConfig.h"
#include "Pkgs/unzip.h"
#include <map>
#include <set>

//TODO: separate Game and ROM structs
struct Game
{
  std::string name;
  std::string title;
  std::string version;
  std::string manufacturer;
  int year = 0;
  std::string stepping;
  std::string mpeg_board;
  uint32_t encryption_key = 0;
  enum Inputs
  {
    INPUT_UI              = 0,          // special code reserved for Supermodel UI inputs
    INPUT_COMMON          = 0x00000001, // common controls (coins, service, test)
    INPUT_VEHICLE         = 0x00000002, // vehicle controls
    INPUT_JOYSTICK1       = 0x00000004, // joystick 1 
    INPUT_JOYSTICK2       = 0x00000008, // joystick 2
    INPUT_FIGHTING        = 0x00000010, // fighting game controls
    INPUT_VR4             = 0x00000020, // four VR view buttons
    INPUT_VIEWCHANGE      = 0x00000040, // single view change button
    INPUT_SHIFT4          = 0x00000080, // 4-speed shifter
    INPUT_SHIFTUPDOWN     = 0x00000100, // up/down shifter
    INPUT_HANDBRAKE       = 0x00000200, // handbrake
    INPUT_HARLEY          = 0x00000400, // Harley Davidson controls
    INPUT_GUN1            = 0x00000800, // light gun 1
    INPUT_GUN2            = 0x00001000, // light gun 2
    INPUT_ANALOG_JOYSTICK = 0x00002000, // analog joystick
    INPUT_TWIN_JOYSTICKS  = 0x00004000, // twin joysticks
    INPUT_SOCCER          = 0x00008000, // soccer controls
    INPUT_SPIKEOUT        = 0x00010000, // Spikeout buttons
    INPUT_ANALOG_GUN1     = 0x00020000, // analog gun 1
    INPUT_ANALOG_GUN2     = 0x00040000, // analog gun 2
    INPUT_SKI             = 0x00080000, // ski controls
    INPUT_MAGTRUCK        = 0x00100000, // magical truck controls
    INPUT_FISHING         = 0x00200000, // fishing controls
    INPUT_ALL             = 0x003FFFFF
  };
  uint32_t inputs = 0;
  // Holds a ROM region
  struct ROM
  {
    std::shared_ptr<uint8_t> data;
    size_t size = 0;
    void CopyTo(uint8_t *dest, size_t dest_size) const;
  };
  std::map<std::string, ROM> roms;
  ROM get_rom(const std::string &region) const
  {
    auto it = roms.find(region);
    return it == roms.end() ? ROM() : it->second;
  }
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
  
  // Parsed XML
  typedef std::map<std::string, Region::Ptr_t> RegionsByName_t;
  std::map<std::string, RegionsByName_t> m_regions_by_game;
  std::string m_xml_filename;
  
  // Zip file info
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
  bool LoadDefinitionXML(const std::string &filename);

public:
  GameLoader(const Util::Config::Node &config);
  bool Load(Game *game, const std::string &zipfilename);
};

#endif  // INCLUDED_GAME_H
