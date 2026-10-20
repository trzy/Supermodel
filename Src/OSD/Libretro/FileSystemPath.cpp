/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * FileSystemPath.cpp (libretro)
 *
 * File locations for the libretro core. Read-only data (Games.xml, Music.xml,
 * crosshair bitmaps and other assets) lives in the frontend's system
 * directory, everything the core writes lives in the frontend's save
 * directory:
 *
 *   <system>/supermodel/            Games.xml, Music.xml
 *   <system>/supermodel/Assets/     p1crosshair.bmp, p2crosshair.bmp, ...
 *   <save>/supermodel/NVRAM/        <game>.nv
 *   <save>/supermodel/<type>/       everything else
 *
 * Only portable C++17 (std::filesystem) is used.
 */

#include "FileSystemPath.h"
#include "LibretroPaths.h"

#include <filesystem>
#include <system_error>

namespace
{
  std::string s_systemDir;
  std::string s_saveDir;

  std::string WithSeparator(std::string dir)
  {
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
      dir += '/';
    return dir;
  }
}

namespace LibretroPaths
{
  void SetSystemDirectory(const std::string &dir)
  {
    s_systemDir = WithSeparator(dir);
  }

  void SetSaveDirectory(const std::string &dir)
  {
    s_saveDir = WithSeparator(dir);
  }

  std::string GetDataDirectory()
  {
    return s_systemDir + "supermodel/";
  }

  std::string GetSaveDirectory()
  {
    // If the frontend does not provide a save directory, fall back to the
    // system directory, as recommended by the libretro API
    return (s_saveDir.empty() ? s_systemDir : s_saveDir) + "supermodel/";
  }
}

namespace FileSystemPath
{
  bool PathExists(std::string fileSystemPath)
  {
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::u8path(fileSystemPath), ec);
  }

  int MakeDir(std::string dir)
  {
    std::error_code ec;
    if (PathExists(dir))
      return 0;
    std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
    return ec ? -1 : 0;
  }

  std::string GetPath(PathType pathType)
  {
    std::string path;
    switch (pathType)
    {
    case Config:      path = LibretroPaths::GetDataDirectory(); break;
    case Assets:      path = LibretroPaths::GetDataDirectory() + "Assets/"; break;
    case ROMs:        path = LibretroPaths::GetDataDirectory() + "ROMs/"; break;
    case NVRAM:       path = LibretroPaths::GetSaveDirectory() + "NVRAM/"; break;
    case Saves:       path = LibretroPaths::GetSaveDirectory() + "Saves/"; break;
    case Screenshots: path = LibretroPaths::GetSaveDirectory() + "Screenshots/"; break;
    case Log:         path = LibretroPaths::GetSaveDirectory() + "Log/"; break;
    case Analysis:    path = LibretroPaths::GetSaveDirectory() + "Analysis/"; break;
    }

    // Only the writable locations are created; read-only data is never
    // created on the frontend's behalf
    if (pathType != Config && pathType != Assets && pathType != ROMs)
      MakeDir(path);
    return path;
  }
}
