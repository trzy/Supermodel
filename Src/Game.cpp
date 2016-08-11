#include "Game.h"
#include "OSD/Logger.h"
#include "Util/NewConfig.h"
#include "Util/ByteSwap.h"
#include <algorithm>

#include <iostream>

void Game::ROM::CopyTo(uint8_t *dest, size_t dest_size) const
{
  if (!data || !size || !dest || !dest_size)
    return;
  size_t bytes_to_copy = std::min(size, dest_size);
  const uint8_t *src = data.get();
  memcpy(dest, src, bytes_to_copy);
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

GameLoader::File::Ptr_t GameLoader::File::Create(const GameLoader &loader, const Util::Config::Node &file_node)
{
  if (GameLoader::MissingAttrib(loader, file_node, "name") | GameLoader::MissingAttrib(loader, file_node, "offset"))
    return Ptr_t();
  Ptr_t file = std::make_shared<File>();
  file->offset = file_node["offset"].ValueAsUnsigned();
  file->filename = Util::ToLower(file_node["name"].Value());
  file->has_crc32 = file_node["crc32"].Exists();
  file->crc32 = file->has_crc32 ? file_node["crc32"].ValueAsUnsigned() : 0;
  return file;
}

bool GameLoader::File::Matches(const std::string &filename_to_match, uint32_t crc32_to_match) const
{
  if (has_crc32)
    return crc32_to_match == crc32;
  return Util::ToLower(filename_to_match) == filename;
}

GameLoader::Region::Ptr_t GameLoader::Region::Create(const GameLoader &loader, const Util::Config::Node &region_node)
{
  if (GameLoader::MissingAttrib(loader, region_node, "name") | MissingAttrib(loader, region_node, "stride") | GameLoader::MissingAttrib(loader, region_node, "chunk_size"))
    return Ptr_t();
  Ptr_t region = std::make_shared<Region>();
  region->region_name = region_node["name"].Value();
  region->stride = region_node["stride"].ValueAsUnsigned();
  region->chunk_size = region_node["chunk_size"].ValueAsUnsigned();
  region->byte_swap = region_node["byte_swap"].ValueAsBoolWithDefault(false);
  return region;
}

static void PopulateGameInfo(Game *game, const Util::Config::Node &game_node)
{
  game->name = game_node["name"].Value();
  game->title = game_node["identity/title"].Value();
  game->version = game_node["identity/version"].Value();
  game->manufacturer = game_node["identity/manufacturer"].Value();
  game->year = game_node["identity/year"].ValueAsUnsigned();
  game->stepping = game_node["hardware/stepping"].Value();
}

bool GameLoader::ParseXML(const Util::Config::Node::ConstPtr_t &xml)
{
  for (auto it = xml->begin(); it != xml->end(); ++it)
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
    std::string game_name = game_node["name"].Value();
    if (m_regions_by_game.find(game_name) != m_regions_by_game.end())
    {
      ErrorLog("%s: Ignoring redefinition of game '%s'.", m_xml_filename.c_str(), game_name.c_str());
      continue;
    }
    RegionsByName_t &regions_by_name = m_regions_by_game[game_name];
    PopulateGameInfo(&m_game_info_by_game[game_name], game_node);

    for (auto it = game_node.begin(); it != game_node.end(); ++it)
    {
      auto &roms_node = *it;
      if (roms_node.Key() != "roms")
        continue;

      // Regions defined uniquely per game
      for (auto it = roms_node.begin(); it != roms_node.end(); ++it)
      {
        auto &region_node = *it;
        if (region_node.Key() != "region")
          continue;
        Region::Ptr_t region = Region::Create(*this, region_node);
        if (!region)
          continue;
        if (regions_by_name.find(region->region_name) != regions_by_name.end())
        {
          ErrorLog("%s: Ignoring redefinition of region '%s' of '%s'.", m_xml_filename.c_str(), region->region_name.c_str(), game_name.c_str());
          continue;
        }
        Region::FilesByOffset_t &files_by_offset = region->files_by_offset;

        // Files defined uniquely per region
        for (auto it = region_node.begin(); it != region_node.end(); ++it)
        {
          auto &file_node = *it;
          if (file_node.Key() != "file")
            continue;
          File::Ptr_t file = File::Create(*this, file_node);
          if (!file)
            continue;
          // Ensure file offset not defined multiple times. We allow the same
          // file to be reused, however (e.g., a blank file loaded at multiple
          // offsets).
          if (files_by_offset.find(file->offset) != files_by_offset.end())
          {
            ErrorLog("%s: Ignoring redefinition of offset 0x%x in region '%s' of '%s'.", m_xml_filename.c_str(), file->offset, region->region_name.c_str(), game_name.c_str());
            continue;
          }
          files_by_offset[file->offset] = file;
        }
        
        // Check to ensure that some files were defined in the region
        if (files_by_offset.empty())
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

std::set<std::string> GameLoader::IdentifyGamesFileBelongsTo(const std::string &filename, uint32_t crc32) const
{
  std::set<std::string> games;
  for (auto &v_game: m_regions_by_game)
  {
    const std::string &game_name = v_game.first;
    auto &regions_by_name = v_game.second;
    for (auto &v_region: regions_by_name)
    {
      Region::Ptr_t region = v_region.second;
      for (auto &v_file: region->files_by_offset)
      {
        File::Ptr_t file = v_file.second;
        if (file->Matches(filename, crc32))
          games.insert(game_name);
      }
    }
  }
  return games;
}

const unz_file_info *GameLoader::LookupZippedFile(const File::Ptr_t &file) const
{
  if (file->has_crc32)
  {
    auto it = m_zip_info_by_crc32.find(file->crc32);
    if (it != m_zip_info_by_crc32.end())
      return &it->second;
    ErrorLog("'%s' with CRC32 0x%08x not found in '%s'.", file->filename.c_str(), file->crc32, m_zipfilename.c_str());
    return 0;
  }
  auto it = m_zip_info_by_filename.find(file->filename);
  if (it != m_zip_info_by_filename.end())
    return &it->second;
  ErrorLog("'%s' not found in '%s'.", file->filename.c_str(), m_zipfilename.c_str());
  return 0;
}

bool GameLoader::ComputeRegionSize(uint32_t *region_size, const GameLoader::Region::Ptr_t &region) const
{
  // Files in region need not be loaded contiguously. To find region size,
  // use maximum end_addr = offset + stride * (num_chunks - 1) + chunk_size.
  std::vector<uint32_t> end_addr;
  bool error = false;
  for (auto &v_file: region->files_by_offset)
  {
    auto &file = v_file.second;
    const unz_file_info *info = LookupZippedFile(file);
    if (info)
    {
      if (info->uncompressed_size % region->chunk_size != 0)
      {
        std::string filename = file->filename;
        auto it = m_filename_by_crc32.find(info->crc);
        if (it != m_filename_by_crc32.end())
          filename = it->second;
        ErrorLog("File '%s' in '%s' is not sized in %d-byte chunks.", filename.c_str(), m_zipfilename.c_str(), region->chunk_size);
        error = true;
      }
      uint32_t num_chunks = info->uncompressed_size / region->chunk_size;
      end_addr.push_back(file->offset + region->stride * (num_chunks - 1) + region->chunk_size);
    }
    else
      error = true;
  }
  if (!error)
    *region_size = *std::max_element(end_addr.begin(), end_addr.end());
  return error;
}

bool GameLoader::LoadZippedFile(std::shared_ptr<uint8_t> *buffer, size_t *file_size, const GameLoader::File::Ptr_t &file)
{
  unz_file_info info;
  for (int err = unzGoToFirstFile(m_zf); err == UNZ_OK; err = unzGoToNextFile(m_zf))
  {
    char current_filename[256];
    if (UNZ_OK != unzGetCurrentFileInfo(m_zf, &info, current_filename, sizeof(current_filename), NULL, 0, NULL, 0))
      continue;
    if (file->Matches(current_filename, info.crc))
    {
      // Found file, load it!
      err = unzOpenCurrentFile(m_zf);
      if (UNZ_OK != err)
      {
        ErrorLog("Unable to read '%s' from '%s'. Is zip file corrupt?", current_filename, m_zipfilename.c_str());
        return true;
      }
      *file_size = info.uncompressed_size;
      buffer->reset(new uint8_t[*file_size], std::default_delete<uint8_t[]>());
      ZPOS64_T bytes_read = unzReadCurrentFile(m_zf, buffer->get(), *file_size);
      if (bytes_read != *file_size)
      {
        ErrorLog("Unable to read '%s' from '%s'. Is zip file corrupt?", current_filename, m_zipfilename.c_str());
        unzCloseCurrentFile(m_zf);
        return true;
      }
      err = unzCloseCurrentFile(m_zf);
      if (UNZ_CRCERROR == err)
        ErrorLog("CRC error reading '%s' from '%s'. File may be corrupt.", current_filename, m_zipfilename.c_str());
      return false;
    }
  }
  if (file->has_crc32)
    ErrorLog("'%s' with CRC32 0x%08x not found in '%s'.", file->filename.c_str(), file->crc32, m_zipfilename.c_str());
  else
    ErrorLog("'%s' not found in '%s'.", file->filename.c_str(), m_zipfilename.c_str());
  return true;
}

bool GameLoader::LoadRegion(Game::ROM *buffer, const GameLoader::Region::Ptr_t &region)
{
  bool error = false;
  for (auto &v_file: region->files_by_offset)
  {
    auto &file = v_file.second;
    std::shared_ptr<uint8_t> tmp;
    size_t file_size;
    error |= LoadZippedFile(&tmp, &file_size, file);
    if (!error)
    {
      size_t num_chunks = file_size / region->chunk_size;
      for (size_t i = 0; i < num_chunks; i++)
      {
        uint8_t *dest = buffer->data.get() + file->offset + i * region->stride;
        uint8_t *src = tmp.get() + i * region->chunk_size;
        memcpy(dest, src, region->chunk_size);
      }
    }
  }
  if (region->byte_swap)
    Util::FlipEndian16(buffer->data.get(), buffer->size);
  return error;
}

bool GameLoader::LoadROMs(std::map<std::string, Game::ROM> *roms, const std::string &game_name)
{
  auto it = m_regions_by_game.find(game_name);
  if (it == m_regions_by_game.end())
  {
    ErrorLog("Game '%s' not found in '%s'.", game_name.c_str(), m_zipfilename.c_str());
    return true;
  }
  bool error = false;
  auto &regions_by_name = it->second;
  for (auto &v_region: regions_by_name)
  {
    auto &region = v_region.second;
    uint32_t region_size = 0;
    if (ComputeRegionSize(&region_size, region))
      error |= true;
    else
    {
      std::cout << region->region_name << " -> " << Util::Hex(region_size) << std::endl;
      auto &buffer = (*roms)[region->region_name];
      buffer.size = region_size;
      buffer.data.reset(new uint8_t[region_size], std::default_delete<uint8_t[]>());
      error |= LoadRegion(&buffer, region);
    }
  }
  return error;
}

bool GameLoader::LoadDefinitionXML(const std::string &filename)
{
  m_xml_filename = filename;
  Util::Config::Node::ConstPtr_t xml = std::const_pointer_cast<const Util::Config::Node>(Util::Config::FromXMLFile(filename));
  return ParseXML(xml);
}

bool GameLoader::Load(Game *game, const std::string &zipfilename)
{
  *game = Game();
  m_zf = NULL;
  m_filename_by_crc32.clear();
  m_zip_info_by_filename.clear();
  m_zip_info_by_crc32.clear();

  m_zipfilename = zipfilename;
  m_zf = unzOpen(zipfilename.c_str());
  if (NULL == m_zf)
  {
    ErrorLog("Could not open '%s'.", zipfilename.c_str());
    return true;
  }

  // Identify all files in zip archive
  int err = UNZ_OK;
  for (err = unzGoToFirstFile(m_zf); err == UNZ_OK; err = unzGoToNextFile(m_zf))
  {
    unz_file_info file_info;
    char filename_buffer[256];
    if (UNZ_OK != unzGetCurrentFileInfo(m_zf, &file_info, filename_buffer, sizeof(filename_buffer), NULL, 0, NULL, 0))
      continue;
    std::string filename = Util::ToLower(filename_buffer);
    m_zip_info_by_filename[filename] = file_info;
    m_zip_info_by_crc32[file_info.crc] = file_info;
    m_filename_by_crc32[file_info.crc] = filename;
  }
  if (err != UNZ_END_OF_LIST_OF_FILE)
  {
    ErrorLog("Unable to read the contents of '%s' (code 0x%x).", zipfilename.c_str(), err);
    return true;
  }

  // Which game is this?
  std::map<std::string, size_t> files_per_game;
  std::set<std::string> all_games_found;
  using value_type = std::pair<std::string, size_t>;
  for (auto &v: m_filename_by_crc32)
  {
    std::set<std::string> games = IdentifyGamesFileBelongsTo(v.second, v.first);
    all_games_found.insert(games.begin(), games.end());
    for (auto &game: games)
      files_per_game[game]++;
  }
  auto v = std::max_element(files_per_game.begin(), files_per_game.end(), [](const value_type &v1, const value_type &v2) { return v1.second < v2.second; });
  if (v == files_per_game.end())
  {
    ErrorLog("No valid Model 3 ROMs found in '%s'.", zipfilename.c_str());
    return true;
  }
  std::string game_name = v->first;
  if (files_per_game.size() != 1)
    ErrorLog("Multiple games found in '%s' (%s). Loading '%s'.", zipfilename.c_str(), std::string(Util::Format(", ").Join(all_games_found)).c_str(), game_name.c_str());

  // Load it
  *game = m_game_info_by_game[game_name];
  bool error = LoadROMs(&game->roms, game_name);
  if (error)
    *game = Game();
  unzClose(m_zf);
  return error;
}

GameLoader::GameLoader(const Util::Config::Node &config)
{
  LoadDefinitionXML(config["GameXMLFile"].Value());
}
