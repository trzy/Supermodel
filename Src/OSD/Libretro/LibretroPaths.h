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
 * LibretroPaths.h
 *
 * Directories supplied by the libretro frontend. FileSystemPath (libretro
 * implementation) builds every Supermodel path from these.
 */

#ifndef INCLUDED_LIBRETROPATHS_H
#define INCLUDED_LIBRETROPATHS_H

#include <string>

namespace LibretroPaths
{
  void SetSystemDirectory(const std::string &dir);
  void SetSaveDirectory(const std::string &dir);

  // <system>/supermodel/ : read-only data (Games.xml, Music.xml, Assets/)
  std::string GetDataDirectory();

  // <save>/supermodel/ : data written by the core (NVRAM, ...)
  std::string GetSaveDirectory();
}

#endif  // INCLUDED_LIBRETROPATHS_H
