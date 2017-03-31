/*
 * TODO: 
 * -----
 * - Special case parent and child in same zip (when multiple complete games, 
     and selected is parent, look for child, else if child is chosen, look for
     parent
 */

#include "GameLoader.h"
#include "OSD/Logger.h"
#include "Util/NewConfig.h"
#include "Util/ConfigBuilders.h"
#include "Util/ByteSwap.h"
#include <algorithm>

#include <iostream>

bool GameLoader::LoadZipArchive(ZipArchive *zip, const std::string &zipfilename) const
{
  zip->zipfilename = zipfilename;
  zip->zf = unzOpen(zipfilename.c_str());
  if (NULL == zip->zf)
  {
    ErrorLog("Could not open '%s'.", zipfilename.c_str());
    return true;
  }
  
  // Identify all files in zip archive
  int err = UNZ_OK;
  for (err = unzGoToFirstFile(zip->zf); err == UNZ_OK; err = unzGoToNextFile(zip->zf))
  {
    unz_file_info file_info;
    char filename_buffer[256];
    if (UNZ_OK != unzGetCurrentFileInfo(zip->zf, &file_info, filename_buffer, sizeof(filename_buffer), NULL, 0, NULL, 0))
      continue;
    zip->files_by_crc[file_info.crc].filename = filename_buffer;
    zip->files_by_crc[file_info.crc].uncompressed_size = file_info.uncompressed_size;
    zip->files_by_crc[file_info.crc].crc32 = file_info.crc;
  }

  if (err != UNZ_END_OF_LIST_OF_FILE)
  {
    ErrorLog("Unable to read the contents of '%s' (code 0x%x).", zipfilename.c_str(), err);
    return true;
  }
  return false;
}

bool GameLoader::LoadZippedFile(std::shared_ptr<uint8_t> *buffer, size_t *file_size, const GameLoader::File::ptr_t &file, const ZipArchive &zip)
{
  // Locate file
  const ZippedFile *zipped_file = LookupFile(file, zip);
  if (!zipped_file)
    return true;
  if (UNZ_OK != unzLocateFile(zip.zf, zipped_file->filename.c_str(), 2))
  {
    ErrorLog("Unable to locate '%s' in '%s'. Is zip file corrupt?", zipped_file->filename.c_str(), zip.zipfilename.c_str());
    return true;
  }
  
  // Read it in
  if (UNZ_OK != unzOpenCurrentFile(zip.zf))
  {
    ErrorLog("Unable to read '%s' from '%s'. Is zip file corrupt?", zipped_file->filename.c_str(), zip.zipfilename.c_str());
    return true;
  }
  *file_size = zipped_file->uncompressed_size;
  buffer->reset(new uint8_t[*file_size], std::default_delete<uint8_t[]>());
  ZPOS64_T bytes_read = unzReadCurrentFile(zip.zf, buffer->get(), *file_size);
  if (bytes_read != *file_size)
  {
    ErrorLog("Unable to read '%s' from '%s'. Is zip file corrupt?", zipped_file->filename.c_str(), zip.zipfilename.c_str());
    unzCloseCurrentFile(zip.zf);
    return true;
  }
  
  // And close it
  if (UNZ_CRCERROR == unzCloseCurrentFile(zip.zf))
    ErrorLog("CRC error reading '%s' from '%s'. File may be corrupt.", zipped_file->filename.c_str(), zip.zipfilename.c_str());
  return false;
}

const GameLoader::ZippedFile *GameLoader::LookupFile(const File::ptr_t &file, const ZipArchive &zip) const
{
  if (file->has_crc32)
  {
    auto it = zip.files_by_crc.find(file->crc32);
    if (it == zip.files_by_crc.end())
    {
      ErrorLog("'%s' with CRC32 0x%08x not found in '%s'.", file->filename.c_str(), file->crc32, zip.zipfilename.c_str());
      return nullptr;
    }
    return &it->second;
  }
  
  // Try to lookup by name
  for (auto &v: zip.files_by_crc)
  {
    if (Util::ToLower(v.second.filename) == file->filename)
      return &v.second;
  }
  ErrorLog("'%s' not found in '%s'.", file->filename.c_str(), zip.zipfilename.c_str());
  return nullptr;
}

bool GameLoader::MissingAttrib(const GameLoader &loader, const Util::Config::Node &node, const std::string &attribute)
{
  if (node[attribute].Empty())
  {
    ErrorLog("%s: <%s> tag is missing required attribute '%s'.", loader.m_xml_filename.c_str(), node.Key().c_str(), attribute.c_str());
    return true;
  }
  return false;
}

GameLoader::File::ptr_t GameLoader::File::Create(const GameLoader &loader, const Util::Config::Node &file_node)
{
  if (GameLoader::MissingAttrib(loader, file_node, "name") | GameLoader::MissingAttrib(loader, file_node, "offset"))
    return ptr_t();
  ptr_t file = std::make_shared<File>();
  file->offset = file_node["offset"].ValueAs<uint32_t>();
  file->filename = Util::ToLower(file_node["name"].ValueAs<std::string>());
  file->has_crc32 = file_node["crc32"].Exists();
  file->crc32 = file->has_crc32 ? file_node["crc32"].ValueAs<uint32_t>() : 0;
  return file;
}

bool GameLoader::File::Matches(const std::string &filename_to_match, uint32_t crc32_to_match) const
{
  if (has_crc32)
    return crc32_to_match == crc32;
  return Util::ToLower(filename_to_match) == filename;
}

GameLoader::Region::ptr_t GameLoader::Region::Create(const GameLoader &loader, const Util::Config::Node &region_node)
{
  if (GameLoader::MissingAttrib(loader, region_node, "name") | MissingAttrib(loader, region_node, "stride") | GameLoader::MissingAttrib(loader, region_node, "chunk_size"))
    return ptr_t();
  ptr_t region = std::make_shared<Region>();
  region->region_name = region_node["name"].Value<std::string>();
  region->stride = region_node["stride"].ValueAs<size_t>();
  region->chunk_size = region_node["chunk_size"].ValueAs<size_t>();
  region->byte_swap = region_node["byte_swap"].ValueAsDefault<bool>(false);
  return region;
}

static void PopulateGameInfo(Game *game, const Util::Config::Node &game_node)
{
  game->name = game_node["name"].ValueAs<std::string>();
  game->parent = game_node["parent"].ValueAsDefault<std::string>(std::string());
  game->title = game_node["identity/title"].ValueAsDefault<std::string>("Unknown");
  game->version = game_node["identity/version"].ValueAsDefault<std::string>("");
  game->manufacturer = game_node["identity/manufacturer"].ValueAsDefault<std::string>("Unknown");
  game->year = game_node["identity/year"].ValueAsDefault<unsigned>(0);
  game->stepping = game_node["hardware/stepping"].ValueAsDefault<std::string>("");
  game->mpeg_board = game_node["hardware/mpeg_board"].ValueAsDefault<std::string>("");
  game->encryption_key = game_node["hardware/encryption_key"].ValueAsDefault<uint32_t>(0);
  std::map<std::string, uint32_t> input_flags
  {
    { "common",           Game::INPUT_COMMON },
    { "vehicle",          Game::INPUT_VEHICLE },
    { "joystick1",        Game::INPUT_JOYSTICK1 },
    { "joystick2",        Game::INPUT_JOYSTICK2 },
    { "fighting",         Game::INPUT_FIGHTING },
    { "vr4",              Game::INPUT_VR4 },
    { "viewchange",       Game::INPUT_VIEWCHANGE },
    { "shift4",           Game::INPUT_SHIFT4 },
    { "shiftupdown",      Game::INPUT_SHIFTUPDOWN },
    { "handbrake",        Game::INPUT_HANDBRAKE },
    { "harley",           Game::INPUT_HARLEY },
    { "gun1",             Game::INPUT_GUN1 },
    { "gun2",             Game::INPUT_GUN2 },
    { "analog_joystick",  Game::INPUT_ANALOG_JOYSTICK },
    { "twin_joysticks",   Game::INPUT_TWIN_JOYSTICKS },
    { "soccer",           Game::INPUT_SOCCER },
    { "spikeout",         Game::INPUT_SPIKEOUT },
    { "analog_gun1",      Game::INPUT_ANALOG_GUN1 },
    { "analog_gun2",      Game::INPUT_ANALOG_GUN2 },
    { "ski",              Game::INPUT_SKI },
    { "magtruck",         Game::INPUT_MAGTRUCK },
    { "fishing",          Game::INPUT_FISHING }
  };
  for (auto &node: game_node["hardware/inputs"])
  {
    if (node.Key() == "input" && node["type"].Exists())
    {
      const std::string input_type = node["type"].ValueAs<std::string>();
      game->inputs |= input_flags[input_type];
    }
  }
}

bool GameLoader::ParseXML(const Util::Config::Node &xml)
{
  for (auto it = xml.begin(); it != xml.end(); ++it)
  {
    // Game node
    auto &game_node = *it;
    if (game_node.Key() != "game")
      continue;
    if (game_node["name"].Empty())
    {
      //TODO: associate line numbers in config
      //ErrorLog("%s: Ignoring <game> tag with missing 'name' attribute.", m_xml_filename.c_str());
      continue;
    }
    std::string game_name = game_node["name"].ValueAs<std::string>();
    if (m_regions_by_game.find(game_name) != m_regions_by_game.end())
    {
      ErrorLog("%s: Ignoring redefinition of game '%s'.", m_xml_filename.c_str(), game_name.c_str());
      continue;
    }
    RegionsByName_t &regions_by_name = m_regions_by_game[game_name];
    PopulateGameInfo(&m_game_info_by_game[game_name], game_node);

    for (auto &roms_node: game_node)
    {
      if (roms_node.Key() != "roms")
        continue;

      /*
       * Regions define contiguous memory areas that individual ROM files are
       * loaded into. It is possible to have multiple region tags identifying
       * the same region. They will be aggregated. This is useful for parent
       * and child ROM sets, which each may need to define the same region,
       * with the child set loading in different files to overwrite the parent
       * set.
       */
      for (auto &region_node: roms_node)
      {
        if (region_node.Key() != "region")
          continue;
        
        // Look up region structure or create new one if needed
        std::string region_name = region_node["name"].Value<std::string>();
        auto it = regions_by_name.find(region_name);
        Region::ptr_t region = (it != regions_by_name.end()) ? it->second : Region::Create(*this, region_node);
        if (!region)
          continue;

        /*
         * Files are defined by the offset they are loaded at. Normally, there
         * should be one file per offset but parent/child ROM sets will violate
         * this, and so it is allowed.
         */
        std::vector<File::ptr_t> &files = region->files;
        for (auto &file_node: region_node)
        {
          if (file_node.Key() != "file")
            continue;
          File::ptr_t file = File::Create(*this, file_node);
          if (!file)
            continue;
          files.push_back(file);
        }
        
        // Check to ensure that some files were defined in the region
        if (files.empty())
          ErrorLog("%s: No files defined in region '%s' of '%s'.", m_xml_filename.c_str(), region->region_name.c_str(), game_name.c_str());
        else
          regions_by_name[region->region_name] = region;
      }
    }
    
    // Check to ensure that some ROM regions were defined for the game
    if (regions_by_name.empty())
      ErrorLog("%s: No ROM regions defined for '%s'.", m_xml_filename.c_str(), game_name.c_str());
  }
  
  // Check to ensure some games were defined
  if (m_regions_by_game.empty())
  {
    ErrorLog("%s: No games defined.", m_xml_filename.c_str());
    return true;
  }
  return false;
}

std::set<std::string> GameLoader::IdentifyCompleteGamesInZipArchive(const ZipArchive &zip) const
{
  std::set<std::string> complete_games;
  std::map<std::string, std::set<File::ptr_t>> files_required_per_game;
  std::map<std::string, std::set<File::ptr_t>> files_found_per_game;
  std::map<std::string, std::set<File::ptr_t>> files_missing_per_game; // only for those games which are at least partially present

  // Determine which files each game requires and which files are present in
  // the zip archive for each game
  for (auto &v1: m_regions_by_game)
  {
    const std::string &game_name = v1.first;
    auto &regions_by_name = v1.second;
    for (auto &v2: regions_by_name)
    {
      Region::ptr_t region = v2.second;
      for (auto file: region->files)
      {
        // Add each file to the set of required files per game
        files_required_per_game[game_name].insert(file);
        // Check file in ROM definition against all files in zip
        for (auto &v3: zip.files_by_crc)
        {
          const std::string &filename = v3.second.filename;
          uint32_t crc32 = v3.first;
          if (file->Matches(filename, crc32))
            files_found_per_game[game_name].insert(file);
        }
      }
    }
  }

  // Of those games for which any files were found, find the missing files
  for (auto &v: files_found_per_game)
  {
      auto &files_found = v.second;
      auto &files_required = files_required_per_game[v.first];
      auto &files_missing = files_missing_per_game[v.first];
      std::set_difference(
        files_required.begin(), files_required.end(),
        files_found.begin(), files_found.end(),
        std::inserter(files_missing, files_missing.end()),
        [](File::ptr_t a, File::ptr_t b) { return a->filename < b->filename; });
  }

  // Print missing files
  for (auto &v: files_missing_per_game)
  {
    for (auto &file: v.second)
    {
      ErrorLog("'%s' (CRC32 0x%08x) not found in '%s' for game '%s'.", file->filename.c_str(), file->crc32, zip.zipfilename.c_str(), v.first.c_str());
    }
  }

  // Determine whether we have any complete ROM sets in this zip archive
  for (auto &v: files_found_per_game)
  {
    if (v.second == files_required_per_game[v.first])
      complete_games.insert(v.first);
  }
  return complete_games;
}

bool GameLoader::ComputeRegionSize(uint32_t *region_size, const GameLoader::Region::ptr_t &region, const ZipArchive &zip) const
{
  // Files in region need not be loaded contiguously. To find region size,
  // use maximum end_addr = offset + stride * (num_chunks - 1) + chunk_size.
  std::vector<uint32_t> end_addr;
  bool error = false;
  for (auto file: region->files)
  {
    const ZippedFile *zipped_file = LookupFile(file, zip);
    if (zipped_file)
    {
      if (zipped_file->uncompressed_size % region->chunk_size != 0)
      {
        ErrorLog("File '%s' in '%s' is not sized in %d-byte chunks.", zipped_file->filename.c_str(), zip.zipfilename.c_str(), region->chunk_size);
        error = true;
      }
	    uint32_t num_chunks = (uint32_t)(zipped_file->uncompressed_size / region->chunk_size);
      end_addr.push_back(file->offset + region->stride * (num_chunks - 1) + region->chunk_size);
    }
    else
      error = true;
  }
  if (!error)
    *region_size = *std::max_element(end_addr.begin(), end_addr.end());
  return error;
}

// We need to preserve the absolute offsets in order for byte swapping to work
// properly when chunk size is 1
static void CopyBytes(uint8_t *dest_base, size_t dest_offset, const uint8_t *src_base, size_t src_offset, size_t size, bool byte_swap)
{
  size_t swap = byte_swap ? 1 : 0;
  for (size_t i = 0; i < size; i++)
  {
    dest_base[(dest_offset + i) ^ swap] = src_base[src_offset + i];
  }
}

bool GameLoader::LoadRegion(ROM *rom, const GameLoader::Region::ptr_t &region, const ZipArchive &zip)
{
  bool error = false;
  for (auto &file: region->files)
  {
    std::shared_ptr<uint8_t> tmp;
    size_t file_size;
    error |= LoadZippedFile(&tmp, &file_size, file, zip);
    if (!error)
    {
      size_t num_chunks = file_size / region->chunk_size;
      for (size_t i = 0; i < num_chunks; i++)
      {
        /*
         * We have to check bounds because LoadROMs() may attempt to load
         * regions whose size was computed incorrectly because a file was
         * missing. 
         *
         * It is also possible for ROM memory not to have been allocated at
         * all in such a case, hence the check for that.
         */
        size_t dest_offset = file->offset + i * region->stride;
        size_t src_offset = i * region->chunk_size;
        size_t bytes_to_copy = region->chunk_size;
        if ((dest_offset + bytes_to_copy) > rom->size || (src_offset + bytes_to_copy) > file_size)
        {
          ErrorLog("ROM region '%s' could not be created or loaded.", region->region_name.c_str());
          error |= true;
          break;
        }
        if (!rom->data.get() || !tmp.get())
        {
          ErrorLog("ROM region '%s' could not be created or loaded.", region->region_name.c_str());
          error |= true;
          break;
        }
        CopyBytes(rom->data.get(), dest_offset, tmp.get(), src_offset, region->chunk_size, region->byte_swap);
      }
    }
  }
  return error;
}

bool GameLoader::LoadROMs(ROMSet *rom_set, const std::string &game_name, const ZipArchive *zip, const std::string &parent_name, const ZipArchive *parent_zip)
{
  // First pass: scan child set and create ROM structures (but without 
  // allocating data)
  auto it = m_regions_by_game.find(game_name);
  if (it == m_regions_by_game.end())
  {
    ErrorLog("Game '%s' not found in '%s'.", game_name.c_str(), zip->zipfilename.c_str());
    return true;
  }
  auto *regions_by_name = &it->second;
  bool error = false;
  for (auto &v: *regions_by_name)
  {
    auto &region = v.second;
    uint32_t region_size = 0;
    if (ComputeRegionSize(&region_size, region, *zip))
      error |= true;
    else
    {
      auto &rom = rom_set->rom_by_region[region->region_name];
      rom.size = region_size; // get size only
      rom.data = nullptr;     // don't allocate yet
    }
  }
    
  // Second pass: scan parent set and create ROMs or resize existing ones.
  // Memory is allocated here and parent ROMs are loaded.
  if (parent_zip)
  {
    it = m_regions_by_game.find(parent_name);
    if (it == m_regions_by_game.end())
    {
      ErrorLog("Parent game '%s' not found in '%s'.", parent_name.c_str(), parent_zip->zipfilename.c_str());
      return true;
    }
    regions_by_name = &it->second;
    for (auto &v: *regions_by_name)
    {
      auto &region = v.second;
      uint32_t region_size = 0;
      if (ComputeRegionSize(&region_size, region, *parent_zip))
        error |= true;
      else
      {
        // Caution: if region size computation fails above, ROM buffer will not
        // be allocated here!
        auto &rom = rom_set->rom_by_region[region->region_name];
        rom.size = std::max(rom.size, size_t(region_size));
        rom.data.reset(new uint8_t[rom.size], std::default_delete<uint8_t[]>());
        error |= LoadRegion(&rom, region, *parent_zip);
      }
    }
  }
  
  // Third pass: load child ROMs atop existing parent ROMs. If there is no
  // parent, memory is allocated here.
  regions_by_name = &(m_regions_by_game.find(game_name)->second);
  for (auto &v: *regions_by_name)
  {
    auto &region = v.second;
    auto &rom = rom_set->rom_by_region[region->region_name];
    if (!rom.data)  // not yet allocated
      rom.data.reset(new uint8_t[rom.size], std::default_delete<uint8_t[]>());
    error |= LoadRegion(&rom, region, *zip);
  }
  
  return error;
}

bool GameLoader::LoadDefinitionXML(const std::string &filename)
{
  m_xml_filename = filename;
  Util::Config::Node xml("xml");
  if (Util::Config::FromXMLFile(&xml, filename))
    return true;
  return ParseXML(xml);
}

std::string StripFilename(const std::string &filepath)
{
  // Search for last '/' or '\', if any
  size_t last_slash = std::string::npos;
  for (size_t i = filepath.length() - 1; i < filepath.length(); i--)
  {
    if (filepath[i] == '/' || filepath[i] =='\\')
    {
      last_slash = i;
      break;
    }
  }
  
  // If none found, there is directory component here
  if (last_slash == std::string::npos)
    return "";

  // Otherwise, strip everything after the slash
  return std::string(filepath, 0, last_slash + 1);
}

bool GameLoader::Load(Game *game, ROMSet *rom_set, const std::string &zipfilename)
{
  *game = Game();
  
  // Load the zip file and identify all games in it
  ZipArchive zip;
  if (LoadZipArchive(&zip, zipfilename))
    return true;
  std::set<std::string> games_found = IdentifyCompleteGamesInZipArchive(zip);
  if (games_found.empty())
  {
    ErrorLog("No complete Model 3 games found in '%s'.", zipfilename.c_str());
    return true;
  }
  else if (games_found.size() > 1)
    ErrorLog("Multiple games found in '%s' (%s). Loading '%s'.", zipfilename.c_str(), std::string(Util::Format(", ").Join(games_found)).c_str(), games_found.begin()->c_str());

  // Pick the first game in the first zip file
  *game = m_game_info_by_game[*games_found.begin()];
  
  // If there is a parent ROM set, determine where it is 1) contained in the
  // same zip file or 2) try loading it from the same directory
  ZipArchive zip2;
  ZipArchive *parent_zip = nullptr;
  if (!game->parent.empty())
  {
    if (games_found.count(game->parent) > 0)
      parent_zip = &zip;
    else
    {
      std::string parent_zipfilename = StripFilename(zipfilename) + game->parent + ".zip";
      if (LoadZipArchive(&zip2, parent_zipfilename))
      {
        ErrorLog("Could not load parent ROM set of '%s' from '%s'.", game->name.c_str(), parent_zipfilename.c_str());
        return true;
      }
      parent_zip = &zip2;
    }
  }
  
  // Sanity check: a parent set should not itself have a parent
  if (!game->parent.empty())
  {
    auto it = m_game_info_by_game.find(game->parent);
    if (it != m_game_info_by_game.end() && !it->second.parent.empty())
      ErrorLog("Parent ROM set '%s' also has parent defined in '%s', which is invalid and ignored.", game->parent.c_str(), m_xml_filename.c_str());
  }

  // Load
  bool error = LoadROMs(rom_set, game->name, &zip, game->parent, parent_zip);
  if (error)
    *game = Game();
  return error;
}

GameLoader::GameLoader(const std::string &xml_file)
{
  LoadDefinitionXML(xml_file);
}
