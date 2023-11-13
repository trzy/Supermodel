#include "GameLoader.h"
#include "OSD/Logger.h"
#include "Util/NewConfig.h"
#include "Util/ConfigBuilders.h"
#include "Util/ByteSwap.h"
#include "Util/Format.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>

bool GameLoader::LoadZipArchive(ZipArchive *zip, const std::string &zipfilename) const
{
  unzFile zf = unzOpen(zipfilename.c_str());
  if (NULL == zf)
  {
    ErrorLog("Could not open '%s'.", zipfilename.c_str());
    return true;
  }
  zip->zipfilenames.push_back(zipfilename);
  zip->zfs.push_back(zf);

  // Identify all files in zip archive
  int err;
  for (err = unzGoToFirstFile(zf); err == UNZ_OK; err = unzGoToNextFile(zf))
  {
    unz_file_info file_info;
    char filename_buffer[256];
    if (UNZ_OK != unzGetCurrentFileInfo(zf, &file_info, filename_buffer, sizeof(filename_buffer), NULL, 0, NULL, 0))
      continue;
    zip->files_by_crc[file_info.crc].zf = zf;
    zip->files_by_crc[file_info.crc].zipfilename = filename_buffer;
    zip->files_by_crc[file_info.crc].filename = filename_buffer;
    zip->files_by_crc[file_info.crc].uncompressed_size = file_info.uncompressed_size;
    zip->files_by_crc[file_info.crc].crc32 = file_info.crc;
  }

  if (err != UNZ_END_OF_LIST_OF_FILE)
  {
    ErrorLog("Unable to read the contents of '%s' (code 0x%x).", zipfilename.c_str(), err);
    return true;
  }
  InfoLog("Opened %s.", zipfilename.c_str());
  return false;
}

bool GameLoader::FileExistsInZipArchive(const File::ptr_t &file, const ZipArchive &zip) const
{
  if (file->has_crc32)
  {
    auto it = zip.files_by_crc.find(file->crc32);
    return it != zip.files_by_crc.end();
  }

  // Try to lookup by name
  for (auto &v: zip.files_by_crc)
  {
    if (Util::ToLower(v.second.filename) == file->filename)
      return true;
  }
  return false;
}

const GameLoader::ZippedFile *GameLoader::LookupFile(const File::ptr_t &file, const ZipArchive &zip) const
{
  if (file->has_crc32)
  {
    auto it = zip.files_by_crc.find(file->crc32);
    if (it == zip.files_by_crc.end())
    {
      if (zip.zfs.size() == 1)
        ErrorLog("'%s' with CRC32 0x%08x not found in '%s'.", file->filename.c_str(), file->crc32, zip.zipfilenames[0].c_str());
      else
        ErrorLog("'%s' with CRC32 0x%08x not found in '%s'.", file->filename.c_str(), file->crc32, Util::Format("', '").Join(zip.zipfilenames).str().c_str());
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
  if (zip.zfs.size() == 1)
    ErrorLog("'%s' not found in '%s'.", file->filename.c_str(), zip.zipfilenames[0].c_str());
  else
    ErrorLog("'%s' not found in '%s'.", file->filename.c_str(), Util::Format("', '").Join(zip.zipfilenames).str().c_str());
  return nullptr;
}

bool GameLoader::LoadZippedFile(std::shared_ptr<uint8_t> *buffer, size_t *file_size, const GameLoader::File::ptr_t &file, const ZipArchive &zip) const
{
  // Locate file
  const ZippedFile *zipped_file = LookupFile(file, zip);
  if (!zipped_file)
    return true;
  if (UNZ_OK != unzLocateFile(zipped_file->zf, zipped_file->filename.c_str(), 2))
  {
    ErrorLog("Unable to locate '%s' in '%s'. Is zip file corrupt?", zipped_file->filename.c_str(), zipped_file->zipfilename.c_str());
    return true;
  }

  // Read it in
  if (UNZ_OK != unzOpenCurrentFile(zipped_file->zf))
  {
    ErrorLog("Unable to read '%s' from '%s'. Is zip file corrupt?", zipped_file->filename.c_str(), zipped_file->zipfilename.c_str());
    return true;
  }
  *file_size = zipped_file->uncompressed_size;
  buffer->reset(new uint8_t[*file_size], std::default_delete<uint8_t[]>());
  size_t bytes_read = (size_t) unzReadCurrentFile(zipped_file->zf, buffer->get(), *file_size);
  if (bytes_read != *file_size)
  {
    ErrorLog("Unable to read '%s' from '%s'. Is zip file corrupt?", zipped_file->filename.c_str(), zipped_file->zipfilename.c_str());
    unzCloseCurrentFile(zipped_file->zf);
    return true;
  }

  // And close it
  if (UNZ_CRCERROR == unzCloseCurrentFile(zipped_file->zf))
    ErrorLog("CRC error reading '%s' from '%s'. File may be corrupt.", zipped_file->filename.c_str(), zipped_file->zipfilename.c_str());
  return false;
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
  if (GameLoader::MissingAttrib(loader, file_node, "name") | GameLoader::MissingAttrib(loader, file_node, "offset")) // no || to easier detect errors
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

bool GameLoader::File::operator==(const File &rhs) const
{
  return Matches(rhs.filename, rhs.crc32);
}

GameLoader::Region::ptr_t GameLoader::Region::Create(const GameLoader &loader, const Util::Config::Node &region_node)
{
  if (GameLoader::MissingAttrib(loader, region_node, "name") | MissingAttrib(loader, region_node, "stride") | GameLoader::MissingAttrib(loader, region_node, "chunk_size")) // no || to easier detect errors
    return ptr_t();

  if (region_node["byte_swap"].Exists() && region_node["byte_layout"].Exists())
  {
    ErrorLog("%s: '%s' region has both 'byte_swap' and 'byte_layout' attributes. Use one or the other.", loader.m_xml_filename.c_str(), region_node["name"].Value<std::string>().c_str());
    return ptr_t();
  }

  ptr_t region = std::make_shared<Region>();

  region->region_name = region_node["name"].Value<std::string>();

  region->stride = region_node["stride"].ValueAs<size_t>();
  if (region->stride == 0)
  {
    ErrorLog("%s: '%s' stride length must be greater than 0.", loader.m_xml_filename.c_str(), region->region_name.c_str());
    return ptr_t();
  }

  region->chunk_size = region_node["chunk_size"].ValueAs<size_t>();
  if (region->chunk_size == 0)
  {
    ErrorLog("%s: '%s' chunk size must be greater than 0.", loader.m_xml_filename.c_str(), region->region_name.c_str());
    return ptr_t();
  }

  region->required = region_node["required"].ValueAsDefault<bool>(true);

  // Byte layout. If byte_swap was specified, construct the byte swapped layout string based on
  // stride size. If byte_swap is set to false, empty layout string is fine.
  if (region_node["byte_swap"].Exists())
  {
    if (region_node["byte_swap"].ValueAs<bool>())
    {
      // Special case: if chunk size and stride are both 1, change them both to 2 so we can allow byte
      // swapping (these values are used for singular ROMs that don't need to be merged; technically,
      // the stride and chunk size should be 2 since they are 16-bit ROMs).
      if (region->stride == 1 && region->chunk_size == 1)
      {
        region->stride = 2;
        region->chunk_size = 2;
      }

      std::string byte_layout;
      for (size_t i = 0; i < region->stride; i++)
      {
        byte_layout += '0' + (i ^ 1);
      }
      region->byte_layout = byte_layout;
    }
  }
  else
  {
    region->byte_layout = region_node["byte_layout"].ValueAsDefault<std::string>(std::string());

  }

  return region;
}

bool GameLoader::Region::AttribsMatch(const ptr_t &other) const
{
  return stride == other->stride && chunk_size == other->chunk_size && byte_layout == other->byte_layout;
}

bool GameLoader::Region::FindFileIndexByOffset(size_t *idx, uint32_t offset) const
{
  for (size_t i = 0; i < files.size(); i++)
  {
    if (files[i]->offset == offset)
    {
      *idx = i;
      return true;
    }
  }
  return false;
}

static void PopulateGameInfo(Game *game, const Util::Config::Node &game_node)
{
  game->name = game_node["name"].ValueAs<std::string>();
  game->parent = game_node["parent"].ValueAsDefault<std::string>("");
  game->title = game_node["identity/title"].ValueAsDefault<std::string>("Unknown");
  game->version = game_node["identity/version"].ValueAsDefault<std::string>("");
  game->manufacturer = game_node["identity/manufacturer"].ValueAsDefault<std::string>("Unknown");
  game->year = game_node["identity/year"].ValueAsDefault<unsigned>(0);
  game->stepping = game_node["hardware/stepping"].ValueAsDefault<std::string>("");
  game->mpeg_board = game_node["hardware/mpeg_board"].ValueAsDefault<std::string>("");
  std::map<std::string, Game::AudioTypes> audio_types
  {
    { "",                      Game::STEREO_LR }, // default to stereo
    { "Mono",                  Game::MONO },
    { "Stereo",                Game::STEREO_LR },
    { "StereoReversed",        Game::STEREO_RL },
    { "QuadFrontRear",         Game::QUAD_1_FLR_2_RLR },
    { "QuadFrontRearReversed", Game::QUAD_1_FRL_2_RRL },
    { "QuadRearFront",         Game::QUAD_1_RLR_2_FLR },
    { "QuadRearFrontReversed", Game::QUAD_1_RRL_2_FRL },
    { "QuadMix",               Game::QUAD_1_LR_2_FR_MIX}
  };
  std::string audio_type = game_node["hardware/audio"].ValueAsDefault<std::string>("");
  game->audio = audio_types[audio_type];
  game->pci_bridge = game_node["hardware/pci_bridge"].ValueAsDefault<std::string>("");
  game->real3d_pci_id = game_node["hardware/real3d_pci_id"].ValueAsDefault<uint32_t>(0);
  game->real3d_status_bit_set_percent_of_frame = game_node["hardware/real3d_status_bit_set_percent_of_frame"].ValueAsDefault<float>(0);
  game->encryption_key = game_node["hardware/encryption_key"].ValueAsDefault<uint32_t>(0);
  game->netboard_present = game_node["hardware/netboard"].ValueAsDefault<bool>(false);

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

  std::map<std::string, Game::DriveBoardType> drive_board_types
  {
    { "Wheel",      Game::DRIVE_BOARD_WHEEL },
    { "Joystick",   Game::DRIVE_BOARD_JOYSTICK },
    { "Ski",        Game::DRIVE_BOARD_SKI },
    { "Billboard",  Game::DRIVE_BOARD_BILLBOARD}
  };
  std::string drive_board_type = game_node["hardware/drive_board"].ValueAsDefault<std::string>(std::string());
  game->driveboard_type = drive_board_types[drive_board_type];
}

bool GameLoader::LoadGamesFromXML(const Util::Config::Node &xml)
{
  for (auto it = xml.begin(); it != xml.end(); ++it)
  {
    // Root games node
    auto &root_node = *it;
    if (root_node.Key() != "games")
      continue;
    for (auto &game_node: root_node)
    {
      // Game node
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
      PatchesByRegion_t &patches_by_region = m_patches_by_game[game_name];
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

        // ROM patches, if any
        for (auto &patches_node: roms_node)
        {
          if (patches_node.Key() != "patches")
            continue;

          for (auto &patch_node: patches_node)
          {
            if (MissingAttrib(*this, patch_node, "region") ||
                MissingAttrib(*this, patch_node, "bits") ||
                MissingAttrib(*this, patch_node, "offset") ||
                MissingAttrib(*this, patch_node, "value"))
              continue;
            std::string region = patch_node["region"].ValueAs<std::string>();
            unsigned bits = patch_node["bits"].ValueAs<unsigned>();
            uint32_t offset = patch_node["offset"].ValueAs<uint32_t>();
            uint64_t value = patch_node["value"].ValueAs<uint64_t>();
            if (bits != 8 && bits != 16 && bits != 32 && bits != 64)
              ErrorLog("%s: Ignoring ROM patch in '%s' with invalid bit length. Must be 8, 16, 32, or 64!", m_xml_filename.c_str(), game_name.c_str());
            else
              patches_by_region[region].push_back(ROM::BigEndianPatch(offset, value, bits));
          }
        }
      }

      // Check to ensure that some ROM regions were defined for the game
      if (regions_by_name.empty())
        ErrorLog("%s: No ROM regions defined for '%s'.", m_xml_filename.c_str(), game_name.c_str());
    }
  }
  // Check to ensure some games were defined
  if (m_regions_by_game.empty())
  {
    ErrorLog("%s: No games defined.", m_xml_filename.c_str());
    return true;
  }
  return false;
}

static bool IsChildSet(const Game &game)
{
  return game.parent.length() > 0;
}

bool GameLoader::MergeChildrenWithParents()
{
  bool error = false;

  for (auto &v1: m_regions_by_game)
  {
    auto &game = m_game_info_by_game[v1.first];
    if (!IsChildSet(game))  // we want child sets
      continue;

    auto &child_regions = v1.second;
    auto &parent_regions = m_regions_by_game[game.parent];

    // Rebuild child regions by copying over all parent regions first, then
    // merge in files from equivalent child regions
    RegionsByName_t new_regions;
    for (auto &v2: parent_regions)
    {
      // Copy over parent region (shallow copy is sufficient, vector of files
      // will be copied over correctly)
      auto &region_name = v2.first;
      new_regions[region_name] = std::make_shared<Region>(*v2.second);
      auto &new_region = new_regions[region_name];

      // Replace equivalent files from child in parent region, appending any
      // new ones
      if (child_regions.find(region_name) != child_regions.end())
      {
        auto &child_region = child_regions[region_name];
        if (!new_region->AttribsMatch(child_region))
        {
          ErrorLog("%s: Attributes of region '%s' in parent '%s' and child '%s' do not match.", m_xml_filename.c_str(), region_name.c_str(), game.parent.c_str(), game.name.c_str());
          error = true;
        }
        for (size_t i = 0; i < child_region->files.size(); i++)
        {
          size_t idx;
          if (new_region->FindFileIndexByOffset(&idx, child_region->files[i]->offset))
            new_region->files[idx] = child_region->files[i];
          else
            new_region->files.push_back(child_region->files[i]);
        }
      }
    }

    // Simply append any region in child that does *not* exist in parent
    for (auto &v2: child_regions)
    {
      if (new_regions.find(v2.first) == new_regions.end())
      {
        // Since these are pointers anyway, just insert directly
        new_regions[v2.first] = v2.second;
      }
    }

    // Save the final result
    m_regions_by_merged_game[v1.first] = new_regions;
  }

  return error;
}

void GameLoader::LogROMDefinition(const std::string &game_name, const RegionsByName_t &regions_by_name) const
{
  InfoLog("%s:", game_name.c_str());
  for (auto &v2: regions_by_name)
  {
    InfoLog("  %s: stride=%zu, chunk size=%zu, byte layout=%s", v2.first.c_str(), v2.second->stride, v2.second->chunk_size, v2.second->byte_layout.c_str());
    for (auto &file: v2.second->files)
    {
      InfoLog("    %s, crc32=0x%08x, offset=0x%08x", file->filename.c_str(), file->crc32, file->offset);
    }
  }
}

bool GameLoader::ParseXML(const Util::Config::Node &xml)
{
  if (LoadGamesFromXML(xml))
    return true;

  // More than one level of parents not allowed
  bool error = false;
  std::set<std::string> parents_with_parents;
  for (auto &v: m_game_info_by_game)
  {
    if (IsChildSet(v.second))
    {
      if (IsChildSet(m_game_info_by_game[v.second.parent]))
      {
        parents_with_parents.insert(v.second.parent);
        error = true;
      }
    }
  }
  for (auto &game_name: parents_with_parents)
  {
    ErrorLog("%s: Parent ROM set '%s' also has parent defined, which is not allowed.", m_xml_filename.c_str(), game_name.c_str());
  }

  if (MergeChildrenWithParents())
    return true;

  return error;
}

bool GameLoader::LoadDefinitionXML(const std::string &filename)
{
  m_xml_filename = filename;
  Util::Config::Node xml("xml");
  if (Util::Config::FromXMLFile(&xml, filename))
  {
    ErrorLog("Game and ROM set definitions could not be loaded! ROMs will not be detected.");
    return true;
  }
  return ParseXML(xml);
}

void GameLoader::FindEquivalentFiles(std::set<File::ptr_t> *equivalent_files, const std::set<File::ptr_t> &a, const std::set<File::ptr_t> &b)
{
  // Copy files that are equivalent between a and b from a (doesn't matter
  // which we actually use) to output
  for (auto &file1: a)
  {
    for (auto &file2: b)
    {
      if (*file1 == *file2)
        equivalent_files->insert(file1);
    }
  }
}

void GameLoader::IdentifyGamesInZipArchive(
  std::set<std::string> *complete_games,
  std::map<std::string, std::set<File::ptr_t>> *files_missing_by_game,
  const ZipArchive &zip,
  const std::map<std::string, RegionsByName_t> &regions_by_game) const
{
  std::map<std::string, std::set<File::ptr_t>> files_required_by_game;
  std::map<std::string, std::set<File::ptr_t>> files_found_by_game;

  // Determine which files each game requires and which files are present in
  // the zip archive. Files belonging to optional regions cannot be used to
  // identify games.
  for (auto &v1: regions_by_game)
  {
    const std::string &game_name = v1.first;
    auto &regions_by_name = v1.second;
    for (auto &v2: regions_by_name)
    {
      Region::ptr_t region = v2.second;
      if (!region->required)
        continue;
      for (auto file: region->files)
      {
        // Add each file to the set of required files per game
        files_required_by_game[game_name].insert(file);
        // Check file in ROM definition against all files in zip
        if (FileExistsInZipArchive(file, zip))
          files_found_by_game[game_name].insert(file);
      }
    }
  }

  /*
   * Corner case: some child ROM sets legitimately share files, which can fool
   * us into thinking two games are partially present. Need to remove the one
   * that is not really there by detecting case when only overlapping files
   * exist (the ROM set with more present files is the intended one).
   */
  std::vector<std::string> to_remove;
  for (auto &v1: files_found_by_game)
  {
    auto &game1_name = v1.first;
    auto &game1_files = v1.second;
    for (auto &v2: files_found_by_game)
    {
      auto &game2_name = v2.first;
      auto &game2_files = v2.second;
      if (game1_name == game2_name)
        continue;
      std::set<File::ptr_t> equivalent_files;
      FindEquivalentFiles(&equivalent_files, game1_files, game2_files);
      /*
       * If the these two games have a different number of files in the zip
       * archive, but one consists only of the overlapping files, we can safely
       * conclude that these files represent only the game with the larger
       * number of files present. Otherwise, if only the overlapping files are
       * present for both, we have a genuine ambiguity and hence do nothing.
       */
      if (game1_files.size() != game2_files.size() && equivalent_files.size() == game2_files.size())
        to_remove.push_back(game2_name);
    }
  }
  for (auto &game_name: to_remove)
  {
    files_found_by_game.erase(game_name);
  }

  // Find the missing files for each game we found in the zip archive, then use
  // this to determine whether the complete game exists
  auto compare = [](const File::ptr_t &a, const File::ptr_t &b) { return a->filename < b->filename; };
  for (auto &v: files_found_by_game)
  {
    auto &files_found = v.second;
    auto &files_required = files_required_by_game[v.first];
    auto &files_missing = (*files_missing_by_game)[v.first];
    // Need to sort by filename for set_difference to work
    std::vector<File::ptr_t> files_found_v(files_found.begin(), files_found.end());
    std::vector<File::ptr_t> files_required_v(files_required.begin(), files_required.end());
    std::sort(files_found_v.begin(), files_found_v.end(), compare);
    std::sort(files_required_v.begin(), files_required_v.end(), compare);
    // Use set difference to find missing files
    std::set_difference(
      files_required_v.begin(), files_required_v.end(),
      files_found_v.begin(), files_found_v.end(),
      std::inserter(files_missing, files_missing.end()),
      compare);
    // Is the whole game present?
    if (files_found == files_required)
      complete_games->insert(v.first);
    // Clean up: if no files missing, don't want empty entry in map
    if (files_missing.empty())
      files_missing_by_game->erase(v.first);
  }
}

void GameLoader::ChooseGameInZipArchive(std::string *chosen_game, bool *missing_parent_roms, const ZipArchive &zip, const std::string &zipfilename) const
{
  chosen_game->clear();
  *missing_parent_roms = false;

  // Find complete unmerged games (those that do not need to be merged with a
  // parent). This will pick up child-only ROMs, too, which we prune out later.
  std::set<std::string> complete_games;
  std::map<std::string, std::set<File::ptr_t>> files_missing_by_game;
  IdentifyGamesInZipArchive(&complete_games, &files_missing_by_game, zip, m_regions_by_game);

  // Find complete, merged games
  std::set<std::string> complete_merged_games;
  std::map<std::string, std::set<File::ptr_t>> files_missing_by_merged_game;
  IdentifyGamesInZipArchive(&complete_merged_games, &files_missing_by_merged_game, zip, m_regions_by_merged_game);

  /*
   * Find incomplete child games by sorting child games out from the unmerged
   * games results and pruning out complete merged games. Don't care about
   * missing files because they are not neccessarily an error for these games.
   * If one ends up being chosen, we would try to load from a second, parent
   * ROM set.
   */
  std::set<std::string> incomplete_child_games;
  for (auto &v: m_game_info_by_game)
  {
    auto &game_name = v.first;
    if (IsChildSet(v.second))
    {
      if (complete_games.count(game_name) || files_missing_by_game.find(game_name) != files_missing_by_game.end())
      {
        incomplete_child_games.insert(game_name);
        complete_games.erase(game_name);
        files_missing_by_game.erase(game_name);
      }
    }
  }
  for (auto &game_name: complete_merged_games)
  {
    incomplete_child_games.erase(game_name);
  }

  // Complete merged games take highest precedence
  for (auto &game_name: complete_merged_games)
  {
    const std::string &parent = m_game_info_by_game.find(game_name)->second.parent;
    // Complete merged game will be used, so ignore the parent entirely
    complete_games.erase(parent);
    // Complete merged sets will often have some parent ROMs missing (those
    // replaced by the child games). This is not an error, so remove parents of
    // complete merged games from missing file list.
    files_missing_by_game.erase(parent);
  }

  // Any remaining incomplete games from the unmerged set are legitimate errors
  for (auto &v: files_missing_by_game)
  {
    for (auto &file: v.second)
    {
      ErrorLog("'%s' (CRC32 0x%08x) not found in '%s' for game '%s'.", file->filename.c_str(), file->crc32, zipfilename.c_str(), v.first.c_str());
    }
    ErrorLog("Ignoring game '%s' in '%s' because it is missing files.", v.first.c_str(), zipfilename.c_str());
  }

  // Choose game: complete merged game > incomplete child game > complete
  // unmerged game
  if (!complete_merged_games.empty())
    chosen_game->assign(*complete_merged_games.begin());
  else if (!incomplete_child_games.empty())
  {
    // TODO: could use scoring to pick game with most files?
    chosen_game->assign(*incomplete_child_games.begin());
    *missing_parent_roms = true;  // try to find missing files in parent ROM zip file
  }
  else if (!complete_games.empty())
    chosen_game->assign(*complete_games.begin());
  else
  {
    ErrorLog("No complete Model 3 games found in '%s'.", zipfilename.c_str());
    return;
  }

  // Print out which game we chose from valid candidates in the zip file
  std::set<std::string> candidates(complete_games);
  candidates.insert(complete_merged_games.begin(), complete_merged_games.end());
  candidates.insert(incomplete_child_games.begin(), incomplete_child_games.end());
  if (candidates.size() > 1)
    ErrorLog("Multiple games found in '%s' (%s). Loading '%s'.", zipfilename.c_str(), Util::Format(", ").Join(candidates).str().c_str(), chosen_game->c_str());
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
        ErrorLog("File '%s' in '%s' is not sized in %d-byte chunks.", zipped_file->filename.c_str(), zipped_file->zipfilename.c_str(), region->chunk_size);
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

static bool ApplyLayout(ROM *rom, const std::string &byte_layout, size_t stride, const std::string &region_name)
{
  // Empty layout means do nothing
  if (byte_layout.size() == 0)
    return false;

  // Validate that the layout string includes the same number of bytes as the region stride. The
  // stride is block size that the ROM files all contribute to. We also verify that each byte is
  // used once and only once.
  if (byte_layout.size() != stride)
  {
    ErrorLog("Byte layout of '%s' region does not match the stride length (%d bytes but should be %d bytes).", region_name.c_str(), byte_layout.size(), stride);
    return true;
  }

  if (stride > 8)
  {
    ErrorLog("Region '%s' has stride larger than 8 (%d), which is currently unsupported.", region_name.c_str(), stride);
    return true;
  }

  std::vector<size_t> byte_offsets;
  for (char c: byte_layout)
  {
    if (isdigit(c))
    {
      byte_offsets.push_back(c - '0');
    }
    else
    {
      ErrorLog("Byte layout of '%s' region contains non-numeric characters. Use single-digit byte indices only.", region_name.c_str());
      return true;
    }
  }

  // Check all byte indices 0..N-1 are present
  std::vector<size_t> sorted(byte_offsets);
  std::sort(sorted.begin(), sorted.end());  // ascending order
  size_t expected_offset = 0;
  for (size_t offset: sorted)
  {
    if (offset != expected_offset)
    {
      ErrorLog("Byte layout of '%s' region must specify all byte offsets exactly once.", region_name.c_str());
      return true;
    }
    expected_offset += 1;
  }

  // Okay, all good. Now we can reshuffle the region memory according to layout.
  uint8_t *buffer = new uint8_t[stride];
  uint8_t *dest = rom->data.get();
  for (size_t dest_offset = 0; (dest_offset + stride) <= rom->size; dest_offset += stride)
  {
    // Copy current region bytes to temporary buffer. The layout offsets refer to this original layout.
    memcpy(buffer, dest + dest_offset, stride);

    // Place the bytes back into the ROM region in the layout order specified.
    for (size_t i = 0; i < stride; i++)
    {
      dest[dest_offset + i] = buffer[byte_offsets[i]];
    }
  }
  delete [] buffer;

  return false; // no error
}

bool GameLoader::LoadRegion(ROM *rom, const GameLoader::Region::ptr_t &region, const ZipArchive &zip) const
{
  bool error = false;

  for (auto &file: region->files)
  {
    std::shared_ptr<uint8_t> tmp;
    size_t file_size;
    error |= LoadZippedFile(&tmp, &file_size, file, zip);
    if (!error)
    {
      uint8_t *dest = rom->data.get();
      uint8_t *src = tmp.get();
      if (region->chunk_size == region->stride)
      {
        memcpy(dest + file->offset, src, file_size);
      }
      else
      {
        uint32_t num_chunks = (uint32_t)file_size / region->chunk_size;
        uint32_t dest_offset = file->offset;
        uint32_t src_offset = 0;
        uint32_t chunk_size = (uint32_t)region->chunk_size;		// cache these as pointer dereferencing cripples performance in a tight loop
        uint32_t stride = (uint32_t)region->stride;
        for (uint32_t i = 0; i < num_chunks; i++)
        {
          memcpy(dest + dest_offset, src + src_offset, chunk_size);
          dest_offset += stride;
          src_offset += chunk_size;
        }
      }
    }
  }

  if (!error)
  {
    error = ApplyLayout(rom, region->byte_layout, region->stride, region->region_name);
  }

  return error;
}

bool GameLoader::LoadROMs(ROMSet *rom_set, const std::string &game_name, const ZipArchive &zip) const
{
  auto it = m_game_info_by_game.find(game_name);
  if (it == m_game_info_by_game.end())
  {
    ErrorLog("Cannot load unknown game '%s'. Is it defined in '%s'?", game_name.c_str(), m_xml_filename.c_str());
    return true;
  }

  // Load up the ROMs
  auto &regions_by_name = IsChildSet(it->second) ? m_regions_by_merged_game.find(game_name)->second : m_regions_by_game.find(game_name)->second;
  LogROMDefinition(game_name, regions_by_name);
  bool error = false;
  for (auto &v: regions_by_name)
  {
    auto &region = v.second;
    uint32_t region_size = 0;
    bool error_loading_region = false;

    // Attempt to load the region
    if (ComputeRegionSize(&region_size, region, zip))
      error_loading_region = true;
    else
    {
      // Load up the ROM region
      auto &rom = rom_set->rom_by_region[region->region_name];
      rom.data.reset(new uint8_t[region_size], std::default_delete<uint8_t[]>());
      rom.size = region_size;
      error_loading_region = LoadRegion(&rom, region, zip);
    }

    if (error_loading_region && !region->required)
    {
      // Failed to load the region but it wasn't required anyway, so remove it
      // and proceed
      rom_set->rom_by_region.erase(region->region_name);
      ErrorLog("Optional ROM region '%s' in '%s' could not be loaded.", region->region_name.c_str(), game_name.c_str());
    }
    else
    {
      // Proceed normally: accumulate errors
      error |= error_loading_region;
    }
  }

  // Attach the patches and do some more error checking here
  auto &patches_by_region = m_patches_by_game.find(game_name)->second;
  for (auto &v: patches_by_region)
  {
    auto &region_name = v.first;
    auto &patches = v.second;
    if (regions_by_name.find(region_name) == regions_by_name.end())
      ErrorLog("%s: Ignoring ROM patch for undefined region '%s' in '%s'.", m_xml_filename.c_str(), region_name.c_str(), game_name.c_str());
    else if (rom_set->rom_by_region.find(region_name) != rom_set->rom_by_region.end())
      rom_set->rom_by_region[region_name].patches = patches;
  }
  return error;
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
    return std::string();

  // Otherwise, strip everything after the slash
  return std::string(filepath, 0, last_slash + 1);
}

bool GameLoader::Load(Game *game, ROMSet *rom_set, const std::string &zipfilename) const
{
  *game = Game();

  // Read the zip contents
  ZipArchive zip;
  if (LoadZipArchive(&zip, zipfilename))
    return true;

  // Pick the game to load (there could be multiple ROM sets in a zip file)
  std::string chosen_game;
  bool missing_parent_roms = false;
  ChooseGameInZipArchive(&chosen_game, &missing_parent_roms, zip, zipfilename);
  if (chosen_game.empty())
    return true;

  // Return game information to caller
  *game = m_game_info_by_game.find(chosen_game)->second;

  // Bring in additional parent ROM set if needed
  if (missing_parent_roms)
  {
    std::string parent_zipfilename = StripFilename(zipfilename) + game->parent + ".zip";
    if (LoadZipArchive(&zip, parent_zipfilename))
    {
      ErrorLog("Expected to find parent ROM set of '%s' at '%s'.", game->name.c_str(), parent_zipfilename.c_str());
      return true;
    }
  }

  // Load
  bool error = LoadROMs(rom_set, game->name, zip);
  if (error)
    *game = Game();
  return error;
}

GameLoader::GameLoader(const std::string &xml_file)
{
  LoadDefinitionXML(xml_file);
}
