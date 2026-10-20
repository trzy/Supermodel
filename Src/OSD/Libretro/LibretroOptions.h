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
 * LibretroOptions.h
 *
 * Core options of the libretro core. They replace Supermodel's command line
 * options and Supermodel.ini settings: each option is translated into the
 * corresponding Supermodel configuration key, so the emulator itself reads
 * its settings exactly as in the standalone build.
 */

#ifndef INCLUDED_LIBRETROOPTIONS_H
#define INCLUDED_LIBRETROOPTIONS_H

#include "Util/NewConfig.h"
#include "libretro.h"

namespace LibretroOptions
{
  /*
   * Registers the core options with the frontend (core options v2 when
   * available, otherwise the legacy variable interface).
   */
  void Register(retro_environment_t environ);

  /*
   * Returns the Supermodel configuration with every key the emulator reads,
   * set to the libretro core's defaults.
   */
  Util::Config::Node DefaultConfig();

  /*
   * Reads the core options from the frontend into config. When initial is
   * false, only options that can change while a game runs are updated (the
   * others take effect the next time a game is loaded).
   */
  void Apply(retro_environment_t environ, Util::Config::Node *config, bool initial);

  // Values that are not Supermodel configuration keys
  unsigned GetRumbleStrength(retro_environment_t environ);
  bool GetClearNVRAM(retro_environment_t environ);
  bool GetGunAutoReload(retro_environment_t environ);
}

#endif  // INCLUDED_LIBRETROOPTIONS_H
