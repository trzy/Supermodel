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

/*
 * DATFile.cpp
 *
 * Writes a Logiqx XML DAT file. A clone has cloneof and romof attributes, and
 * each file that it shares with its parent has a merge attribute. With these,
 * ROM auditing tools can make merged, split, and non-merged ROM sets.
 */

#include "DATFile.h"
#include "Version.h"
#include "OSD/Logger.h"
#include <cstdio>
#include <map>

static std::string EscapeXML(const std::string &text)
{
  std::string escaped;
  for (char c: text)
  {
    switch (c)
    {
    case '&':   escaped += "&amp;"; break;
    case '<':   escaped += "&lt;"; break;
    case '>':   escaped += "&gt;"; break;
    case '"':   escaped += "&quot;"; break;
    case '\'':  escaped += "&apos;"; break;
    default:    escaped += c; break;
    }
  }
  return escaped;
}

// The parent's file that has the same contents, or nullptr
static const GameLoader::ROMSetFile *FindParentFile(const GameLoader::ROMSetFile &file, const GameLoader::ROMSetFiles &parent)
{
  for (auto &parent_file: parent.files)
  {
    if (file.has_crc32 && parent_file.has_crc32)
    {
      if (file.crc32 == parent_file.crc32 && file.size == parent_file.size)
        return &parent_file;
    }
    else if (file.name == parent_file.name)
      return &parent_file;
  }
  return nullptr;
}

bool WriteDATFile(const std::string &filename, const GameLoader &loader)
{
  if (loader.HasXMLErrors())
  {
    ErrorLog("Not writing '%s' because the game definitions have errors.", filename.c_str());
    return true;
  }

  FILE *fp = fopen(filename.c_str(), "w");
  if (!fp)
  {
    ErrorLog("Unable to open '%s' for writing.", filename.c_str());
    return true;
  }

  std::vector<GameLoader::ROMSetFiles> rom_sets = loader.GetROMSetFiles();
  std::map<std::string, const GameLoader::ROMSetFiles *> rom_sets_by_name;
  for (auto &rom_set: rom_sets)
    rom_sets_by_name[rom_set.game->name] = &rom_set;

  std::string version = EscapeXML(SupermodelVersion());
  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<!DOCTYPE datafile PUBLIC \"-//Logiqx//DTD ROM Management Datafile//EN\" \"http://www.logiqx.com/Dats/datafile.dtd\">\n");
  fprintf(fp, "<datafile>\n");
  fprintf(fp, "\t<header>\n");
  fprintf(fp, "\t\t<name>Supermodel</name>\n");
  fprintf(fp, "\t\t<description>Supermodel %s</description>\n", version.c_str());
  fprintf(fp, "\t\t<version>%s</version>\n", version.c_str());
  fprintf(fp, "\t\t<author>The Supermodel Team</author>\n");
  fprintf(fp, "\t\t<homepage>Supermodel</homepage>\n");
  fprintf(fp, "\t\t<url>https://github.com/trzy/Supermodel</url>\n");
  fprintf(fp, "\t</header>\n");

  for (auto &rom_set: rom_sets)
  {
    const Game &game = *rom_set.game;
    const GameLoader::ROMSetFiles *parent = nullptr;
    if (!game.parent.empty())
    {
      auto it = rom_sets_by_name.find(game.parent);
      if (it != rom_sets_by_name.end())
        parent = it->second;
    }

    std::string name = EscapeXML(game.name);
    if (parent)
    {
      std::string parent_name = EscapeXML(game.parent);
      fprintf(fp, "\t<game name=\"%s\" cloneof=\"%s\" romof=\"%s\">\n", name.c_str(), parent_name.c_str(), parent_name.c_str());
    }
    else
      fprintf(fp, "\t<game name=\"%s\">\n", name.c_str());

    std::string description = game.version.empty() ? game.title : game.title + " (" + game.version + ")";
    fprintf(fp, "\t\t<description>%s</description>\n", EscapeXML(description).c_str());
    if (game.year != 0)
      fprintf(fp, "\t\t<year>%u</year>\n", game.year);
    fprintf(fp, "\t\t<manufacturer>%s</manufacturer>\n", EscapeXML(game.manufacturer).c_str());

    for (auto &file: rom_set.files)
    {
      fprintf(fp, "\t\t<rom name=\"%s\"", EscapeXML(file.name).c_str());
      const GameLoader::ROMSetFile *parent_file = parent ? FindParentFile(file, *parent) : nullptr;
      if (parent_file)
        fprintf(fp, " merge=\"%s\"", EscapeXML(parent_file->name).c_str());
      fprintf(fp, " size=\"%u\"", file.size);
      if (file.has_crc32)
        fprintf(fp, " crc=\"%08x\"", file.crc32);
      fprintf(fp, "/>\n");
    }
    fprintf(fp, "\t</game>\n");
  }

  fprintf(fp, "</datafile>\n");
  bool error = ferror(fp) != 0;
  if (fclose(fp) != 0 || error)
  {
    ErrorLog("Unable to write '%s'.", filename.c_str());
    return true;
  }
  return false;
}
