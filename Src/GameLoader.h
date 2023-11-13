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
    bool operator==(const File &rhs) const;
  };

  // Describes a region node in the game XML
  struct Region
  {
    typedef std::shared_ptr<Region> ptr_t;
    std::string region_name;
    size_t stride;
    size_t chunk_size;
    std::string byte_layout;
    bool required;
    std::vector<File::ptr_t> files;
    static ptr_t Create(const GameLoader &loader, const Util::Config::Node &region_node);
    bool AttribsMatch(const ptr_t &other) const;
    bool FindFileIndexByOffset(size_t *idx, uint32_t offset) const;
  };

  // Game information from XML
  std::map<std::string, Game> m_game_info_by_game;

  // ROM patches by game and by region. Cannot place into Region because child
  // sets do not inherit parent patches, which may complicate merging.
  typedef std::map<std::string, std::vector<ROM::BigEndianPatch>> PatchesByRegion_t;
  std::map<std::string, PatchesByRegion_t> m_patches_by_game;

  // Parsed XML
  typedef std::map<std::string, Region::ptr_t> RegionsByName_t;
  std::map<std::string, RegionsByName_t> m_regions_by_game;         // all games as defined in XML
  std::map<std::string, RegionsByName_t> m_regions_by_merged_game;  // only child sets merged w/ parents
  std::string m_xml_filename;

  // Single compressed file inside of a zip archive
  struct ZippedFile
  {
    unzFile zf = nullptr;
    std::string zipfilename;  // zip archive
    std::string filename;     // file inside the zip archive
    size_t uncompressed_size = 0;
    uint32_t crc32 = 0;
  };

  // Multiple zip archives
  struct ZipArchive
  {
    std::vector<std::string> zipfilenames;
    std::vector<unzFile> zfs;
    std::map<uint32_t, ZippedFile> files_by_crc;

    ~ZipArchive()
    {
      for (auto &zf: zfs)
      {
        unzClose(zf);
      }
    }
  };

  bool LoadZipArchive(ZipArchive *zip, const std::string &zipfilename) const;
  const ZippedFile *LookupFile(const File::ptr_t &file, const ZipArchive &zip) const;
  bool FileExistsInZipArchive(const File::ptr_t &file, const ZipArchive &zip) const;
  bool LoadZippedFile(std::shared_ptr<uint8_t> *buffer, size_t *file_size, const GameLoader::File::ptr_t &file, const ZipArchive &zip) const;
  static bool MissingAttrib(const GameLoader &loader, const Util::Config::Node &node, const std::string &attribute);
  bool LoadGamesFromXML(const Util::Config::Node &xml);
  bool MergeChildrenWithParents();
  void LogROMDefinition(const std::string &game_name, const RegionsByName_t &regions_by_name) const;
  bool ParseXML(const Util::Config::Node &xml);
  bool LoadDefinitionXML(const std::string &filename);
  static void FindEquivalentFiles(std::set<File::ptr_t> *equivalent_files, const std::set<File::ptr_t> &a, const std::set<File::ptr_t> &b);
  void IdentifyGamesInZipArchive(
    std::set<std::string> *complete_games,
    std::map<std::string, std::set<File::ptr_t>> *files_missing_by_game,
    const ZipArchive &zip,
    const std::map<std::string, RegionsByName_t> &regions_by_game) const;
  bool ComputeRegionSize(uint32_t *region_size, const Region::ptr_t &region, const ZipArchive &zip) const;
  void ChooseGameInZipArchive(std::string *chosen_game, bool *missing_parent_roms, const ZipArchive &zip, const std::string &zipfilename) const;
  bool LoadRegion(ROM *buffer, const GameLoader::Region::ptr_t &region, const ZipArchive &zip) const;
  bool LoadROMs(ROMSet *rom_set, const std::string &game_name, const ZipArchive &zip) const;
  std::string ChooseGame(const std::set<std::string> &games_found, const std::string &zipfilename) const;
  static bool CompareFilesByName(const File::ptr_t &a,const File::ptr_t &b);

public:
  GameLoader(const std::string &xml_file);
  bool Load(Game *game, ROMSet *rom_set, const std::string &zipfilename) const;
  const std::map<std::string, Game> &GetGames() const
  {
    return m_game_info_by_game;
  }
};

#endif  // INCLUDED_GAMELOADER_H
